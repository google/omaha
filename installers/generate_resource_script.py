#!/usr/bin/python2.4
#
# Copyright 2009 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ========================================================================

import getopt
import os.path
import re
import sys

def GenerateResourceScript(input_filename, output_filename, payload_filename,
                           manifest_filename, resource_filename):
  """
  Given a template name, output filename, payload filename, manifest filename,
  and resource_filename, and creates a resource script.
  """
  f_int = open(input_filename, 'r')
  f_out = open(output_filename, 'w')

  for line in f_int.readlines():
    f_out.write(re.sub(r'__RESOURCE_FILENAME__', resource_filename,
                re.sub(r'__MANIFEST_FILENAME__', manifest_filename,
                re.sub(r'__PAYLOAD_FILENAME__', payload_filename, line))))

(opts, args) = getopt.getopt(sys.argv[1:], 'i:o:p:m:r:')

input_filename = ''
output_filename = ''
payload_filename = ''
manifest_filename = ''
resource_filename = ''

for (o, v) in opts:
  if o == '-i':
    input_filename = v
  if o == '-o':
    output_filename = v
  if o == '-p':
    payload_filename = v
  if o == '-m':
    manifest_filename = v
  if o == '-r':
    resource_filename = v

# The forward slashes prevent the RC compiler from trying to interpret
# backslashes in the quoted path.
GenerateResourceScript(input_filename, output_filename,
  re.sub(r'\\', r'/', payload_filename),
  re.sub(r'\\', r'/', manifest_filename),
  re.sub(r'\\', r'/', resource_filename))
