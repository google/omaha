#!/usr/bin/python2.4
# Copyright 2009-2010 Google Inc.
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

import os
import re

from installers import tag_meta_installers


def TagOneBundle(env, bundle, untagged_binary_path, output_dir):
  tag_str = tag_meta_installers.BuildTagStringForBundle(bundle)

  # Need to find relative path to output file under source dir, to allow
  # it to be redirected under the output directory.
  indx = bundle.output_file_name.find('installers')
  relative_filepath = bundle.output_file_name[indx+len('installers')+1:]

  tag_exe = '$TESTS_DIR/ApplyTag.exe'

  tag_output = env.Command(
      target='%s/%s' % (output_dir, relative_filepath),
      source=untagged_binary_path,
      action='%s $SOURCES $TARGET %s' % (
          env.File(tag_exe).abspath, tag_str)
  )

  # Add extra (hidden) dependency plus a dependency on the tag executable.
  env.Depends(tag_output, [bundle.installers_txt_filename, tag_exe])


def _ReadAllBundleInstallerFiles(installers_txt_files_path):
  """Enumerates all the .*_installers.txt files in the installers_txt_files_path
     directory, and creates bundles corresponding to the info in each line in
     the *_installers.txt file.
  Returns:
    Returns a dictionary of Bundles with key=lang.
  """
  bundles = {}
  files = os.listdir(installers_txt_files_path)
  for file in files:
    regex = re.compile('^(.*)_installers.txt$')
    if not regex.match(file):
      continue

    installer_file = os.path.join(installers_txt_files_path, file)

    # Read in the installer file.
    read_bundles = tag_meta_installers.ReadBundleInstallerFile(installer_file)

    for (key, bundle_list) in read_bundles.items():
      if not bundle_list or not key:
        continue
      if not bundles.has_key(key):
        bundles[key] = bundle_list
      else:
        new_bundles_list = bundles[key] + bundle_list
        bundles[key] = new_bundles_list
  return bundles


def CreateTaggedInstallers(env, installers_txt_files_path, product_name,
                           prefix = ''):
  """For each application with an installers.txt file in installer_files_path,
     create tagged metainstaller(s).
  """
  bundles = _ReadAllBundleInstallerFiles(installers_txt_files_path)

  untagged_binary = '%s%sSetup.exe' % (prefix, product_name)

  tag_meta_installers.SetOutputFileNames(untagged_binary, bundles, '')
  for bundles_lang in bundles.itervalues():
    for bundle in bundles_lang:
      TagOneBundle(
          env=env,
          bundle=bundle,
          untagged_binary_path='$STAGING_DIR/%s' % (untagged_binary),
          output_dir='$TARGET_ROOT/Tagged_Installers',
      )

