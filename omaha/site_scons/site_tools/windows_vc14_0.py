#!/usr/bin/python2.4
#
# Copyright 2015 Google Inc.
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


def _SetMsvcCompiler(
    env, vc_flavor='x86'):
  """When run on a non-Windows system, this function does nothing.

     When run on a Windows system, this function wipes the binary, include
     and lib paths so that any non-hermetic tools won't be used.
     These paths will be set up by target_platform_windows and other tools.
     By wiping the paths here, we make the adding the other tools
     order-independent.
  Args:
    env: The SCons environment in question.
    vc_flavor: Defaults to x86, can be 'x86', 'amd64' or 'x86_amd64'. The
        latter is using the 32-bit executable of the 64-bit compiler.
  Returns:
    Nothing.
  Raises:
    ValueError: An error if vc_flavor is not valid.
  """
  if vc_flavor not in ('x86', 'amd64', 'x86_amd64'):
    raise ValueError('vc_flavor must be one of: amd64, x86, x86_amd64.')

  vc_dir = os.environ.get('VSINSTALLDIR')
  platform_sdk_dir = os.environ.get('OMAHA_PLATFORM_SDK_DIR')
  platform_sdk_version = os.environ.get('WindowsSDKVersion')
  platform_sdk_include_dir = platform_sdk_dir + 'include/' + \
      platform_sdk_version
  platform_sdk_lib_dir = platform_sdk_dir + 'lib/' + platform_sdk_version

  env['GOOGLECLIENT'] = '$MAIN_DIR/..'
  env['GOOGLE3'] = '$GOOGLECLIENT'
  env['THIRD_PARTY'] = '$GOOGLECLIENT/third_party/'

  env.Replace(
      PLATFORM_SDK_DIR=platform_sdk_dir,
      MSVC_FLAVOR=('amd64', 'x86')[vc_flavor == 'x86'],
      VC14_0_DIR=vc_dir,
      WINDOWS_SDK_10_0_DIR=platform_sdk_dir,
  )

  # Clear any existing paths.
  env['ENV']['PATH'] = ''
  env['ENV']['INCLUDE'] = ''
  env['ENV']['LIB'] = ''

  tools_paths = []
  include_paths = []
  lib_paths = []
  vc_bin_dir = vc_dir + '/vc/bin'
  if vc_flavor == 'amd64':
    vc_bin_dir += '/amd64'
  elif vc_flavor == 'x86_amd64':
    vc_bin_dir += '/x86_amd64'

  tools_paths += [vc_bin_dir,]

  include_paths.append(vc_dir + '/vc/include')
  if vc_flavor == 'x86':
    lib_paths.append(vc_dir + '/vc/lib')
  else:
    lib_paths.append(vc_dir + '/vc/lib/amd64')

  # Add explicit location of platform SDK to tools path
  tools_paths.append(platform_sdk_dir + '/bin')
  include_paths += [
      platform_sdk_include_dir + 'um',      # Windows SDKs
      platform_sdk_include_dir + 'shared',  # Windows SDKs
      platform_sdk_include_dir + 'ucrt',    # Windows CRT
  ]
  if vc_flavor == 'x86':
    lib_paths += [
        platform_sdk_lib_dir + 'um/x86',    # Windows SDK
        platform_sdk_lib_dir + 'ucrt/x86',  # Windows CRT
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x86')  # VC 12 only
  else:
    lib_paths += [
        platform_sdk_lib_dir + 'um/x64',    # Windows SDK
        platform_sdk_lib_dir + 'ucrt/x64',  # Windows CRT
    ]

  for p in tools_paths:
    env.AppendENVPath('PATH', env.Dir(p).abspath)
  for p in include_paths:
    env.AppendENVPath('INCLUDE', env.Dir(p).abspath)
  for p in lib_paths:
    env.AppendENVPath('LIB', env.Dir(p).abspath)


def generate(env):
  _SetMsvcCompiler(
      env=env, vc_flavor='x86')
