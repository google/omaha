#!/usr/bin/python2.4
#
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

"""Build a meta-installer for Omaha's client installations.

  The only function in this module creates an Omaha meta-installer, which is
  an executable whose only job is to extract its payload (the actual installer
  executable and a number of resource dlls), then launch the dropped installer
  to finish the installation.

  BuildMetaInstaller(): Build a meta-installer.
"""


def BuildMetaInstaller(env,
                       target_name,
                       omaha_version_info,
                       empty_metainstaller_path,
                       omaha_files_path,
                       prefix='',
                       suffix='',
                       additional_payload_contents=None,
                       additional_payload_contents_dependencies=None,
                       output_dir='$STAGING_DIR',
                       installers_sources_path='$MAIN_DIR/installers',
                       lzma_path='$MAIN_DIR/third_party/lzma/lzma.exe',
                       resmerge_path='$MAIN_DIR/tools/resmerge.exe'):
  """Build a meta-installer.

    Builds a full meta-installer, which is a meta-installer containing a full
    list of required files (the payload), ie. language resourse dlls, installer
    executables, etc.

  Args:
    env: environment to build with
    target_name: name to use for the target executable
    omaha_version_info: info about the version of the Omaha files
    empty_metainstaller_path: path to the base (empty) meta-installer executable
    omaha_files_path: path to the resource dlls to build into the
        target executable
    prefix: target file name prefix, used to distinguish different targets
    suffix: target file name suffix, used to distinguish different targets
    additional_payload_contents: any additional resources to build into the
        executable, beyond the normal payload files
    additional_payload_contents_dependencies: extra dependencies to be used to
        ensure the executable is rebuilt when required
    output_dir: path to the directory that will contain the metainstaller
    installers_sources_path: path to the directory containing the source files
        for building the metainstaller
    lzma_path: path to lzma.exe
    resmerge_path: path to resmerge.exe

  Returns:
    Target nodes.

  Raises:
    Nothing.
  """

  if additional_payload_contents is None:
    additional_payload_contents = []

  if additional_payload_contents_dependencies is None:
    additional_payload_contents_dependencies = []

  # Payload .tar.lzma
  tarball_filename = '%spayload%s.tar' % (prefix, suffix)
  payload_filename = tarball_filename + '.lzma'

  # Collect a list of all the files to include in the payload
  payload_file_names = omaha_version_info.GetMetainstallerPayloadFilenames()

  payload_contents = [omaha_files_path + '/' + file_name
                      for file_name in payload_file_names]
  payload_contents += additional_payload_contents

  # Create the tarball
  tarball_output = env.Command(
      target=tarball_filename,    # Archive filename
      source=payload_contents,    # List of files to include in tarball
      action='python.exe %s -o $TARGET $SOURCES' % (
          env.File(installers_sources_path + '/generate_tarball.py').abspath),
  )

  # Add potentially hidden dependencies
  if additional_payload_contents and additional_payload_contents_dependencies:
    env.Depends(tarball_output, additional_payload_contents)
    env.Depends(tarball_output, additional_payload_contents_dependencies)

  # Compress the tarball
  lzma_output = env.Command(
      target=payload_filename,
      source=tarball_output,
      action=lzma_path + ' e $SOURCES $TARGET',
  )

  # Construct the resource generation script
  manifest_path = installers_sources_path + '/installers.manifest'
  res_command = 'python.exe %s -i %s -o $TARGET -p $SOURCES -m %s -r %s' % (
      env.File(installers_sources_path + '/generate_resource_script.py'
              ).abspath,
      env.File(installers_sources_path + '/resource.rc.in').abspath,
      env.File(manifest_path).abspath,
      env.File(installers_sources_path + '/resource.h').abspath
      )

  # Generate the .rc file
  rc_output = env.Command(
      target='%sresource%s.rc' % (prefix, suffix),
      source=lzma_output,
      action=res_command,
  )

  # .res intermediate file
  res_file = env.RES(rc_output)

  # For some reason, RES() does not cause the .res file to depend on .rc input.
  # It also does not detect the dependencies in the .rc file.
  # This does not cause a rebuild for rarely changing files in res_command.
  env.Depends(res_file, [rc_output, manifest_path, lzma_output])

  # Resource DLL
  dll_env = env.Clone(COMPONENT_STATIC=False)
  dll_env['LINKFLAGS'] += ['/noentry']

  dll_inputs = [
      installers_sources_path + '/resource_only_dll.def',
      res_file
      ]

  dll_output = dll_env.ComponentLibrary(
      lib_name='%spayload%s.dll' % (prefix, suffix),
      source=dll_inputs,
  )

  # Get only the dll itself from the output (ie. ignore .pdb, etc.)
  dll_output_name = [f for f in dll_output if f.suffix == '.dll']

  # Build the target setup executable by merging the empty metafile
  # with the resource dll built above
  merged_output = env.Command(
      target='unsigned_' + target_name,
      source=[empty_metainstaller_path, dll_output_name],
      action=resmerge_path + ' --copyappend $SOURCES $TARGET'
  )

  signed_exe = env.SignedBinary(
      target=target_name,
      source=merged_output,
  )

  return env.Replicate(output_dir, signed_exe)
