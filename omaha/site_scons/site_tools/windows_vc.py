#!/usr/bin/python2.4
#
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http:#www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# ========================================================================

"""Tool-specific initialization for Visual Studio 2013.

NOTE: This tool is order-dependent, and must be run before
any other tools that modify the binary, include, or lib paths because it will
wipe out any existing values for those variables.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.
"""

import os
import SCons


def SetMsvcCompilerVersion(
    env, version_num, vc_flavor='x86'):
  """When run on a non-Windows system, this function does nothing.

     When run on a Windows system, this function wipes the binary, include
     and lib paths so that any non-hermetic tools won't be used.
     These paths will be set up by target_platform_windows and other tools.
     By wiping the paths here, we make the adding the other tools
     order-independent.
  Args:
    env: The SCons environment in question.
    version_num: Either 15.0 or 16.0 to select the Visual Studion 2017 or 2019
                 toolchain. Any other value will raise an exception.

    vc_flavor: Defaults to x64_x86. It can be 'x86_x86', 'x86_x64', 'x64_x64,
               or 'x64_x86'. The latter is using 64-bit compiler host to
               generate x86 binaries.
  Returns:
    Nothing.
  Raises:
    ValueError: An error if vc_flavor is not valid.
  """
  if version_num in ('15.0', '16.0', '17.0'):
    if not vc_flavor:
      vc_flavor = 'x64_x86'
    if vc_flavor not in ['x86_x86', 'x86_x64', 'x64_x86', 'x64_x64']:
      raise ValueError('Invalid vc_flavor %s.' % str(vc_flavor))

  if version_num == '17.0':
    env.Replace(VC17_0_DIR=os.environ.get('VCToolsInstallDir'))
    _SetMsvcCompilerVersion(env,
                            vc_version='17.0',
                            vc_flavor=vc_flavor)
  elif version_num == '16.0':
    env.Replace(VC16_0_DIR=os.environ.get('VCToolsInstallDir'))
    _SetMsvcCompilerVersion(env,
                            vc_version='16.0',
                            vc_flavor=vc_flavor)
  elif version_num == '15.0':
    env.Replace(VC15_0_DIR=os.environ.get('VCToolsInstallDir')),
    _SetMsvcCompilerVersion(env,
                            vc_version='15.0',
                            vc_flavor=vc_flavor)
  else:
    raise ValueError('Unknown MSVC compiler version: "%s".' % version_num)


def _SetMsvcCompilerVersion(env, vc_version, vc_flavor='x64_x86'):
  if (vc_flavor == 'x86_x64' or vc_flavor == 'x64_x64'):
    flavor = 'amd64'
  else:
    flavor = 'x86'

  vc_install_dir = os.environ.get('VCToolsInstallDir')
  vc_redist_dir = os.environ.get('VCToolsRedistDir')

  platform_sdk_dir = os.environ.get('OMAHA_PLATFORM_SDK_DIR')
  platform_sdk_version = os.environ.get('OMAHA_WINDOWS_SDK_10_0_VERSION')
  platform_sdk_include_dir = (platform_sdk_dir + 'include/' +
                              platform_sdk_version)
  platform_sdk_lib_dir = platform_sdk_dir + 'lib/' + platform_sdk_version

  env['GOOGLECLIENT'] = '$MAIN_DIR/..'
  env['GOOGLE3'] = '$GOOGLECLIENT'
  env['THIRD_PARTY'] = '$GOOGLECLIENT/third_party/'

  env.Replace(
      PLATFORM_SDK_DIR=platform_sdk_dir,
      WINDOWS_SDK_10_0_VERSION=platform_sdk_version,
      WINDOWS_SDK_10_0_LIB_DIR=platform_sdk_lib_dir,

      # The msvc tool has a nasty habit of searching the registry for
      # installed versions of MSVC and prepending them to the path.  Flag
      # that it shouldn't be allowed to do that.
      MSVC_BLOCK_ENVIRONMENT_CHANGES=True,
      MSVC_FLAVOR=('amd64', 'x86')[vc_flavor == 'x86'],
  )

  # Clear any existing paths.
  env['ENV']['PATH'] = ''
  env['ENV']['INCLUDE'] = ''
  env['ENV']['LIB'] = ''

  tools_paths = []
  include_paths = []
  lib_paths = []

  vc_bin_dir = vc_install_dir + '/bin'
  vc_first_bin_dir = ''
  vc_second_bin_dir = ''
  if vc_flavor == 'x86_x86':
    vc_first_bin_dir = vc_bin_dir + '/Hostx86/x86'
  elif vc_flavor == 'x64_x64':
    vc_first_bin_dir = vc_bin_dir + '/Hostx64/x64'
  elif vc_flavor == 'x86_x64':
    vc_first_bin_dir = vc_bin_dir + '/Hostx86/x64'
    vc_second_bin_dir = vc_bin_dir + '/Hostx86/x86'
  elif vc_flavor == 'x64_x86':
    vc_first_bin_dir = vc_bin_dir + '/Hostx64/x86'
    vc_second_bin_dir = vc_bin_dir + '/HostX64/x64'

  tools_paths += [vc_first_bin_dir,
                  vc_second_bin_dir,  # can be empty
                  vc_install_dir + '/team_tools/performance_tools',
                  vc_install_dir + '/Shared/Common/VSPerfCollectionTools']
  include_paths.append(vc_install_dir + '/include')
  if (vc_flavor == 'x64_x86' or vc_flavor == 'x86_x86'):
    tools_paths += [
        vc_redist_dir + '/x86/Microsoft.VC141.CRT',
    ]
    lib_paths.append(vc_install_dir + '/lib/x86')
    tools_paths.append(platform_sdk_dir + '/bin/x86')
  elif (vc_flavor == 'x64_x64' or vc_flavor == 'x86_x64'):
    tools_paths += [
        vc_redist_dir + '/x64/Microsoft.VC141.CRT',
    ]
    lib_paths.append(vc_install_dir + '/lib/x64')
    tools_paths.append(platform_sdk_dir + '/bin/x64')

  include_paths += [
      platform_sdk_include_dir + '/um',
      platform_sdk_include_dir + '/shared',
      platform_sdk_include_dir + '/ucrt',
  ]

  if vc_flavor == 'x86_x86':
    lib_paths += [
        platform_sdk_lib_dir + '/um/x86',
        platform_sdk_lib_dir + '/ucrt/x86',
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x86')
  elif vc_flavor == 'x64_x64':
    lib_paths += [
        platform_sdk_lib_dir + '/um/x64',
        platform_sdk_lib_dir + '/ucrt/x64',
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x64')
  elif vc_flavor == 'x64_x86':
    lib_paths += [
        platform_sdk_lib_dir + '/um/x86',
        platform_sdk_lib_dir + '/ucrt/x86',
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x64')
  elif vc_flavor == 'x86_x64':
    lib_paths += [
        platform_sdk_lib_dir + '/um/x64',
        platform_sdk_lib_dir + '/ucrt/x64',
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x86')

  for p in tools_paths:
    env.AppendENVPath('PATH', env.Dir(p).abspath)
  for p in include_paths:
    env.AppendENVPath('INCLUDE', env.Dir(p).abspath)
  for p in lib_paths:
    env.AppendENVPath('LIB', env.Dir(p).abspath)
