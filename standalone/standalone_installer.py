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

# This is very close to the logic within installers\build.scons. The difference
# is that we have an additional file standalone_installers.txt. This file
# contains a list of standalone installers that we create within this mk_file.
# The corresponding binaries for the standalone installers are checked into the
# standalone\binaries sub-directory, and referenced from
# standalone_installers.txt. For each entry in standalone_installers.txt, we
# create a corresponding "standalone" installer, which is the meta-installer,
# plus the offline setup binaries, tarred together.

import codecs
import os

from installers import tagged_installer
from installers import build_metainstaller
from installers import tag_meta_installers


class OffLineInstaller:
  """Represents the information for a bundle"""
  def __init__(self, installers_txt_filename, exe_name, binaries):
    self.installers_txt_filename = installers_txt_filename
    self.name = exe_name
    self.binaries = binaries


def ReadOffLineInstallersFile(env, offline_installers_file_name):
  """Enumerates the entries in the offline installers file.
  Returns:
    Returns a list of structures used for creating the prestamped binaries.
  """
  offline_installers = []
  offline_abs_path = env.File(offline_installers_file_name).abspath
  installer_file = codecs.open(offline_abs_path, 'r')
  for line in installer_file.readlines():
    line = line.strip()
    if len(line) and not line.startswith('#'):
      (installers_txt_filename, exe_name, binaries) = eval(line)
      installer = OffLineInstaller(installers_txt_filename, exe_name, binaries)
      offline_installers.append(installer)
  return offline_installers


def BuildOfflineInstallersVersion(env,
                                  google_update_files_path,
                                  source_binary,
                                  offline_installers_file_name,
                                  manifest_files_path,
                                  prefix = '',
                                  is_official = False):
  offline_installers = ReadOffLineInstallersFile(env,
      offline_installers_file_name)

  for offline_installer in offline_installers:
    BuildOfflineInstaller(
        env,
        offline_installer,
        google_update_files_path,
        source_binary,
        offline_installers_file_name,
        manifest_files_path,
        prefix,
        is_official
    )


def BuildOfflineInstaller(env,
                          offline_installer,
                          google_update_files_path,
                          source_binary,
                          offline_installers_file_name,
                          manifest_files_path,
                          prefix = '',
                          is_official = False):
  product_name = offline_installer.name
  if not is_official:
    product_name = 'UNOFFICIAL_' + product_name

  target_base = prefix + product_name
  target_name = target_base + '.exe'
  log_name = target_base + '_Contents.txt'

  # Write Omaha's VERSION file.
  if prefix:
    version_index = 1
  else:
    version_index = 0

  v = env['product_version'][version_index]
  version_string = '%d.%d.%d.%d' % (v[0], v[1], v[2], v[3])

  log_text = '*** Omaha Version ***\n\n'
  log_text += version_string + '\n'

  # Rename the checked in binaries by adding the application guid as the
  # extension. This is needed as the meta-installer expects the
  # extension.
  # Also, log information about each app.
  additional_payload_contents = []
  for binary in offline_installer.binaries:
    (binary_name, guid) = binary
    output_file = os.path.basename(binary_name) + '.' + guid

    # Have to use Command('copy') here instead of replicate, as the
    # file is being renamed in the process.
    env.Command(
        target=output_file,
        source=binary_name,
        action='@copy /y $SOURCES $TARGET'
    )

    manifest_file_path = manifest_files_path + '/' + guid + '.gup'
    additional_payload_contents += [output_file, manifest_file_path]

    # Log info about the app.
    log_text += '\n\n*** App: ' + guid + ' ***\n'
    log_text += '\nINSTALLER:\n' + binary_name + '\n'

    manifest_file = open(env.File(manifest_file_path).abspath, 'r', -1)
    manifest_file_contents = manifest_file.read()
    manifest_file.close()
    log_text += '\nMANIFEST:\n'
    log_text += manifest_file_contents

  def WriteLog(target, source, env):
    f = open(env.File(target[0]).abspath, 'w')
    f.write(env['write_data'])
    f.close()
    return 0

  env.Command(
      target='$STAGING_DIR/' + log_name,
      source=[],
      action=WriteLog,
      write_data=log_text
  )

  build_metainstaller.BuildMetaInstaller(
      env=env,
      target_name=target_name,
      empty_metainstaller_path=source_binary,
      google_update_files_path=google_update_files_path,
      prefix=prefix,
      suffix='_' + product_name,
      additional_payload_contents=additional_payload_contents,
      additional_payload_contents_dependencies=offline_installers_file_name
  )

  # Tag the meta-installer if an installers.txt file was specified.
  if offline_installer.installers_txt_filename:
    installers_txt_path = (env.File('$MAIN_DIR/' +
        offline_installer.installers_txt_filename).abspath)
    app_bundles = tag_meta_installers.ReadBundleInstallerFile(
        installers_txt_path)

    bundles = {}
    for (key, bundle_list) in app_bundles.items():
      if not bundle_list or not key:
        continue
      if not bundles.has_key(key):
        bundles[key] = bundle_list
      else:
        new_bundles_list = bundles[key] + bundle_list
        bundles[key] = new_bundles_list

    tag_meta_installers.SetOutputFileNames(target_name, bundles, '')
    for bundles_lang in bundles.itervalues():
      for bundle in bundles_lang:
        tagged_installer.TagOneBundle(
            env=env,
            bundle=bundle,
            untagged_binary=target_name,
            output_dir='$TARGET_ROOT/Tagged_Offline_Installers',
        )
