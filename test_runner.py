import json
import subprocess
import sys
import unittest

def run(limit, timeout, jobs, code):
    result=subprocess.run(["./batch-runner",str(limit),str(timeout),str(jobs),"--",sys.executable,"-c",code],capture_output=True,text=True,timeout=6)
    return result.returncode,json.loads(result.stdout.splitlines()[-1])

class RunnerTests(unittest.TestCase):
    def test_concurrency_bound_and_job_indices(self):
        status,r=run(3,1000,7,"import time, os; assert int(os.environ['BATCH_JOB_INDEX']) > 0; time.sleep(.04)")
        self.assertEqual((status,r["ok"],r["peak_concurrency"]),(0,7,3))

    def test_timeout_and_exit_status(self):
        status,r=run(2,80,2,"import time; time.sleep(2)")
        self.assertEqual((status,r["timed_out"],r["ok"]),(1,2,0))

    def test_nonzero_exit(self):
        status,r=run(1,1000,1,"raise SystemExit(3)")
        self.assertEqual((status,r["failed"]),(1,1))

if __name__=="__main__":unittest.main()
