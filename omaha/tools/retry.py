#!/usr/bin/python

import os
import subprocess
import sys
import time


times = int(sys.argv[1])
duration = float(sys.argv[2])
cmd = sys.argv[3:]

# Open stdout with no buffering.
sys.stdout = os.fdopen(sys.stdout.fileno(), 'wb', 0)

for i in range(times):
  if i:
    print('Retrying %d...' % i)
  retcode = subprocess.call(cmd, stderr=subprocess.STDOUT)
  if retcode == 0:
    sys.exit(0)
  time.sleep(duration)

sys.exit(retcode)
