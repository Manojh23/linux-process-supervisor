#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct { pid_t pid; long job; long long deadline; int timed_out; } Worker;
static volatile sig_atomic_t stop_requested;
static void on_signal(int signal_number) { (void)signal_number; stop_requested = 1; }

static long long now_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) { perror("clock_gettime"); exit(2); }
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static long number(const char *s, long min, long max) {
    char *end;
    errno = 0;
    long n = strtol(s, &end, 10);
    if (errno || !*s || *end || n < min || n > max) return -1;
    return n;
}
int main(int argc, char **argv) {
    long limit, jobs, timeout;
    if (argc < 6 || (limit=number(argv[1],1,64))<0 ||
        (timeout=number(argv[2],10,600000))<0 || (jobs=number(argv[3],1,10000))<0 ||
        argv[4][0]!='-' || argv[4][1]!='-' || argv[4][2]!='\0') {
        fprintf(stderr,"usage: %s CONCURRENCY TIMEOUT_MS JOBS -- COMMAND [ARGS...]\n",argv[0]);
        return 2;
    }
    struct sigaction action={0};
    action.sa_handler=on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT,&action,NULL) || sigaction(SIGTERM,&action,NULL)) { perror("sigaction"); return 2; }
    Worker workers[64]={0};
    long launched=0, active=0, peak=0, ok=0, failed=0, expired=0, cancelled=0;
    while ((!stop_requested && launched<jobs) || active) {
        for (long i=0;i<limit&&!stop_requested&&launched<jobs;i++) {
            if (workers[i].pid) continue;
            long job=++launched;
            pid_t pid=fork();
            if (pid<0) { perror("fork"); failed++; stop_requested=1; break; }
            if (pid==0) {
                if (setpgid(0,0)!=0) _exit(126);
                char index[32];
                snprintf(index,sizeof index,"%ld",job);
                if (setenv("BATCH_JOB_INDEX",index,1)!=0) _exit(126);
                execvp(argv[5],&argv[5]);
                _exit(errno==ENOENT?127:126);
            }
            // Child also establishes the group before exec; this closes the parent-side race.
            if (setpgid(pid,pid)!=0 && errno!=EACCES && errno!=ESRCH) perror("setpgid");
            workers[i]=(Worker){pid,job,now_ms()+timeout,0};
            active++;
            if (active>peak) peak=active;
        }
        for(long i=0;i<limit;i++) {
            Worker *w=&workers[i];
            if(!w->pid) continue;
            int status;
            pid_t child=waitpid(w->pid,&status,WNOHANG);
            if(child<0 && errno==EINTR) continue;
            if(child<0) { perror("waitpid"); failed++; w->pid=0; active--; continue; }
            if(child>0) {
                if(w->timed_out==1) expired++;
                else if(w->timed_out==2) cancelled++;
                else if(WIFEXITED(status)&&WEXITSTATUS(status)==0) ok++;
                else {failed++;fprintf(stderr,"job %ld failed (wait status %d)\n",w->job,status);}
                w->pid=0;
                active--;
                continue;
            }
            if(!w->timed_out && (stop_requested || now_ms()>=w->deadline)) {
                w->timed_out=stop_requested?2:1;
                if(kill(-w->pid,SIGKILL)!=0 && errno==ESRCH) (void)kill(w->pid,SIGKILL);
            }
        }
        if(active) {
            struct timespec pause={0,5000000};
            nanosleep(&pause,NULL);
        }
    }
    printf("{\"submitted\":%ld,\"launched\":%ld,\"ok\":%ld,\"failed\":%ld,\"timed_out\":%ld,\"cancelled\":%ld,\"peak_concurrency\":%ld}\n",
           jobs,launched,ok,failed,expired,cancelled,peak);
    return failed||expired||cancelled||stop_requested?1:0;
}
