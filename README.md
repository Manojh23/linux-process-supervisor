# Linux Process Supervisor

A small C/POSIX runner for executing a batch of trusted local commands with bounded concurrency. Each job runs in its own process group. The supervisor enforces timeouts, reaps children, and prints one JSON summary of exits and cancellations.

## Build and run

Use Linux, a C compiler, Make, and Python 3 for the tests.

```sh
make
./batch-runner 3 1000 7 -- /bin/sleep 0.04
make test
```

The arguments are **max concurrency**, **timeout in milliseconds**, **job count**, then the executable and its arguments after `--`. A job receives its one-based index in `BATCH_JOB_INDEX`.

- `fork` and `execvp` start workers without shell parsing.
- Process groups let the supervisor terminate a timed-out worker and its group.
- `waitpid` collects child outcomes, and the JSON summary records batch results.

Exit status 0 means every job succeeded, 1 means at least one job failed, timed out, or was cancelled, and 2 indicates invalid arguments. This is a local process runner for commands you control.
