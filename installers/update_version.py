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


#
# Update these values to match the checked in version.
#
_LANGUAGES_NOT_IN_BUILD = []


# Sets the version and plug-in filenames to the values for the checked in build.
# Removes languages that are in the current mk_common but not in the checked in
# build.
# Returns list of removed languages.
def UpdateSettingsForCheckedInVersion(env,
                                      version_major,
                                      version_minor,
                                      version_build,
                                      version_patch,
                                      oneclick_plugin_version):
  print
  print '******* CHANGING VERSION! *******'
  env.SetProductVersion(
      version_major, version_minor, version_build, version_patch)

  print
  print '*********************************'
  print 'Plug-in version: ' + oneclick_plugin_version
  print '*********************************'
  env.SetActiveXFilenames(oneclick_plugin_version)

  for language in _LANGUAGES_NOT_IN_BUILD:
    if language in env['languages']:
      env['languages'].remove(language)
      print '*** Removing language: ' + language

  return _LANGUAGES_NOT_IN_BUILD


