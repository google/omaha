#!/usr/bin/python2.4
#
# Copyright 2008-2009 Google Inc.
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

"""
mage.exe does not provide a way to add the trustURLParameters attribute to an
application manifest. This script fills that gap. It also adds in the
localized display name, to get around issues with the Python commands
module.
"""

import sys
import os
import getopt
import commands


def _AddTrustURLParametersAndName(manifest_file, output_file, display_name):
  f_in = open(manifest_file, 'r')
  manifest_contents = f_in.read()
  f_in.close()

  manifest_contents = manifest_contents.replace('<deployment ', \
      '<deployment trustURLParameters="true" ')
  manifest_contents = manifest_contents.replace('\"xxxXXXxxx', \
      '\"%s' % display_name)

  f_out = open(output_file, 'w')
  # Works without needing to write the codecs.BOM_UTF8 at the beginning of the
  # file. May need to write this at some point though.
  f_out.write(manifest_contents)
  f_out.close()


def _Usage():
  """Prints out script usage information."""
  print """
add_trusturlparams.py: Modify the given manifest file by adding in a
trustURLParameters=true to the deployment section. Also substitutes
the dummy name xxxXXXxxx with the localized display name.

Usage:
  add_trusturlparams.py [--help
                         | --manifest_file filename
                         | --output_file filename
                           --display_name {i18n display name}]

Options:
  --help                    Show this information.
  --manifest_file filename     Path/name of input/output manifest file.
  --output_file filename       Path/name of an optional output manifest file.
  --display_name  name         i18n display name.
"""


def _Main():
  # use getopt to parse the option and argument list; this may raise, but
  # don't catch it
  _ARGUMENT_LIST = ["help", "manifest_file=", "output_file=", "display_name="]
  (opts, args) = getopt.getopt(sys.argv[1:], "", _ARGUMENT_LIST)
  if not opts or ("--help", "") in opts:
    _Usage()
    sys.exit()

  manifest_file = ""
  output_file = ""
  display_name = ""

  for (o, v) in opts:
    if o == "--manifest_file":
      manifest_file = v
    if o == "--output_file":
      output_file = v
    if o == "--display_name":
      display_name = v

  # make sure we have work to do
  if not manifest_file:
    raise SystemExit("no manifest_filename specified")
  if not display_name:
    raise SystemExit("no display_name specified")

  # overwrite existing file if no separate output specified
  if not output_file:
    output_file = manifest_file

  _AddTrustURLParametersAndName(manifest_file, output_file, display_name)
  sys.exit()


if __name__ == "__main__":
  _Main()

