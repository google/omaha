#!/usr/bin/python

import subprocess
import sys
import time


times = int(sys.argv[1])
duration = float(sys.argv[2])
cmd = sys.argv[3:]


for i in range(times):
  if i:
    print 'Retrying %d...' % i
  retcode = subprocess.call(cmd)
  if retcode == 0:
    sys.exit(0)
  time.sleep(duration)
sys.exit(retcode)
