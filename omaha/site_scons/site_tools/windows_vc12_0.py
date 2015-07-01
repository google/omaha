"""Tool-specific initialization for Visual Studio 2013.

NOTE: This tool is order-dependent, and must be run before
any other tools that modify the binary, include, or lib paths because it will
wipe out any existing values for those variables.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.
"""

import os
import sys
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
    vc_dir: The base directory for the hermetic Visual Studio installation.
    platform_sdk_dir: The base directory for the hermetic platform SDK.
    vc_flavor: Defaults to x86, can be 'x86', 'amd64' or 'x86_amd64'. The
        latter is using the 32-bit executable of the 64-bit compiler.
  Returns:
    Nothing.
  Raises:
    ValueError: An error if vc_flavor is not valid.
  """
  vc_dir=os.environ.get('VC12_0_DIR')
  vc_version='12.0'
  platform_sdk_dir=os.environ.get('PLATFORM_SDK_DIR')
  WINDOWS_SDK_8_1_DIR='$THIRD_PARTY/windows_sdk_8_1/files',

  env['GOOGLECLIENT'] = '$MAIN_DIR/..'
  env['THIRD_PARTY'] = '$GOOGLECLIENT/third_party/'

  if not sys.platform in ('win32', 'cygwin'):
    return

  if not vc_flavor in ('x86', 'amd64', 'x86_amd64'):
    raise ValueError('vc_flavor must be one of: amd64, x86, x86_amd64.')

  env.Replace(
      PLATFORM_SDK_DIR=platform_sdk_dir,

      # The msvc tool has a nasty habit of searching the registry for
      # installed versions of MSVC and prepending them to the path.  Flag
      # that it shouldn't be allowed to do that.
      MSVC_BLOCK_ENVIRONMENT_CHANGES=True,
      # Tell msvs/msvs/mslink tools to use defaults for VC 8.0
      MSVS_VERSION=vc_version,
      COVERAGE_VSPERFCMD=vc_dir + '/team_tools/performance_tools/VSPerfCmd.exe',
      COVERAGE_VSINSTR=vc_dir + '/team_tools/performance_tools/vsinstr.exe',

      # Be explicit about what flavor we want to use. When cross-compiling
      # amd64 on x86, we want 'amd64' here, not 'x86_amd64'. Not using inline
      # if here, since some projects (Omaha, etc.) are still on Python 2.4.
      MSVC_FLAVOR=('amd64', 'x86')[vc_flavor == 'x86'],
      VC12_0_DIR=vc_dir,
      WINDOWS_SDK_8_1_DIR=platform_sdk_dir,
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
      platform_sdk_dir + '/include',         # Platform SDKs up to Vista (incl.)
      platform_sdk_dir + '/include/um',      # Windows SDKs
      platform_sdk_dir + '/include/shared',  # Windows SDKs
  ]
  if vc_flavor == 'x86':
    lib_paths += [
        platform_sdk_dir + '/lib',  # Platform SDKs up to Vista (incl.)
        platform_sdk_dir + '/lib/winv6.3/um/x86',  # Windows SDK 8.1
    ]
    tools_paths.append(platform_sdk_dir + '/bin/x86')  # VC 12 only
  else:
    lib_paths += [
        platform_sdk_dir + '/lib/x64',  # Platform SDKs up to Vista (incl.)
        platform_sdk_dir + '/lib/winv6.3/um/x64',  # Windows SDK 8.1
    ]
    # VC 12 needs this, otherwise mspdb120.dll will not be found.
    tools_paths.append(vc_dir + '/vc/bin')
    tools_paths.append(platform_sdk_dir + '/bin/x64')  # VC 12 only

  # TODO(rspangler): AppendENVPath() should evaluate SCons variables in its
  # inputs and convert them to absolute paths.
  for p in tools_paths:
    env.AppendENVPath('PATH', env.Dir(p).abspath)
  for p in include_paths:
    env.AppendENVPath('INCLUDE', env.Dir(p).abspath)
  for p in lib_paths:
    env.AppendENVPath('LIB', env.Dir(p).abspath)

def generate(env):
  _SetMsvcCompiler(
      env=env, vc_flavor='x86')
