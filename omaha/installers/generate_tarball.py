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
import sys
import tarfile
# NOTE: Normally this would use something like six.moves.urllib to handle the
# py2/py3 differences, but that's not available in the scons-based Omaha build
# environment (and it's probably not worth figuring out how to add it, so this
# is legacy Omaha stuff which is being replaced), so just hack in compatibility.
try:
  # py3
  from urllib.parse import unquote as url_unquote
except ImportError:
  # py2
  from urllib import unquote as url_unquote

TEST_PREFIXES = ('TEST_', 'TEST2_')

def GenerateTarball(output_filename, members):
  """
  Given a tarball name and a sequence of filenames, creates a tarball
  containing the named files.
  """
  tarball = tarfile.open(output_filename, 'w')
  for filename in members:
    # A hacky convention to get around the spaces in filenames is to
    # urlencode them. So at this point we unescape those characters.
    scrubbed_filename = url_unquote(os.path.basename(filename))
    if scrubbed_filename.startswith(TEST_PREFIXES):
      scrubbed_filename = scrubbed_filename.split('_', 1)[1]
    tarball.add(filename, scrubbed_filename)
  tarball.close()


if __name__ == '__main__':
  (opts, args) = getopt.getopt(sys.argv[1:], 'i:o:p:')

  output_filename = ''

  for (o, v) in opts:
    if o == '-o':
      output_filename = v

  GenerateTarball(output_filename, args)
