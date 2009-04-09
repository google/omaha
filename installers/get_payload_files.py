#!/usr/bin/python2.4
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


def GetListOfPayloadFiles(prefix,
                          activex_filename,
                          bho_filename,
                          languages,
                          product_version):
  payload_files = [
      'GoogleUpdate.exe',
      '%sgoopdate.dll' % (prefix),
      # One-Click DLL
      '%s%s' % (prefix, activex_filename),
      # BHO proxy DLL
      '%s%s' % (prefix, bho_filename),
      'GoogleUpdateHelper.msi',
      ]

  # TODO(omaha): Eliminate this and add GoogleCrashHandler.exe above once we no
  # longer need to support versions less than 1.2.147.x.
  # Note: check will be incorrect for versions such as 0.3.0.0, but it is
  # assumed that such versions will not be used.
  if (product_version[0] > 1 or
      product_version[1] > 2 or
      product_version[2] > 146):
    payload_files += ['GoogleCrashHandler.exe']

  for language in languages:
    payload_files += ['%sgoopdateres_%s.dll' % (prefix, language)]

  return payload_files


