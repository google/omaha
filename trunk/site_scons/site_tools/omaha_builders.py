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

"""Omaha builders tool for SCons."""

import os.path
import SCons.Action
import SCons.Builder
import SCons.Tool


def EnablePrecompile(env, target_name):
  """Enable use of precompiled headers for target_name.

  Args:
    env: The environment.
    target_name: Name of component.

  Returns:
    The pch .obj file.
  """
  if env.Bit('use_precompiled_headers'):
    # We enable all warnings on all levels. The goal is to fix the code that
    # we have written and to programmatically disable the warnings for the
    # code we do not control. This list of warnings should shrink as the code
    # gets fixed.
    env.FilterOut(CCFLAGS=['/W3'])
    env.Append(
        CCFLAGS=[
            '/W4',
            '/Wall',
            ],
        INCLUDES=[
            '$MAIN_DIR/precompile/precompile.h'
            ],
    )

    env['PCHSTOP'] = '$MAIN_DIR/precompile/precompile.h'

    pch_env = env.Clone()
    # Must manually force-include the header, as the precompilation step does
    # not evaluate $INCLUDES
    pch_env.Append(CCFLAGS=['/FI$MAIN_DIR/precompile/precompile.h'])
    # Append '_pch' to the target base name to prevent target name collisions.
    # One case where this might have occurred is when a .cc file has the same
    # base name as the target program/library.
    pch_output = pch_env.PCH(
        target=target_name.replace('.', '_') + '_pch' + '.pch',
        source='$MAIN_DIR/precompile/precompile.cc',
    )

    env['PCH'] = pch_output[0]

    # Return the pch .obj file that is created, so it can be
    # included with the inputs of a module
    return [pch_output[1]]


def SignDotNetManifest(env, target, unsigned_manifest):
  """Signs a .NET manifest.

  Args:
    env: The environment.
    target: Name of signed manifest.
    unsigned_manifest: Unsigned manifest.

  Returns:
    Output node list from env.Command().
  """
  sign_manifest_cmd = ('@mage -Sign $SOURCE -ToFile $TARGET -TimestampUri '
                       'http://timestamp.verisign.com/scripts/timstamp.dll ')

  if env.Bit('build_server'):
    # If signing fails with the following error, the hash may not match any
    # certificates: "Internal error, please try again. Object reference not set
    # to an instance of an object."
    sign_manifest_cmd += ('-CertHash ' +
                          env['build_server_certificate_hash'])
  else:
    sign_manifest_cmd += '-CertFile %s -Password %s' % (
        env.GetOption('authenticode_file'),
        env.GetOption('authenticode_password'))

  signed_manifest = env.Command(
      target=target,
      source=unsigned_manifest,
      action=sign_manifest_cmd
  )

  return signed_manifest


#
# Custom Library and Program builders.
#
# These builders have additional cababilities, including enabling precompiled
# headers when appropriate and signing DLLs and EXEs.
#

# TODO(omaha): Make all build files use these builders instead of Hammer's.
# This will eliminate many lines in build.scons files related to enabling
# precompiled header and signing binaries.


def _ConditionallyEnablePrecompile(env, target_name, *args, **kwargs):
  """Enables precompiled headers for target_name when appropriate.

  Enables precompiled headers if they are enabled for the build unless
  use_pch_default = False. This requires that the source files are specified in
  sources or in a list as the first argument after target_name.

  Args:
    env: Environment in which we were called.
    target_name: Name of the build target.
    args: Positional arguments.
    kwargs: Keyword arguments.
  """
  use_pch_default = kwargs.get('use_pch_default', True)

  if use_pch_default and env.Bit('use_precompiled_headers'):
    pch_output = env.EnablePrecompile(target_name)

    # Search the keyworded list first.
    for key in ['source', 'sources', 'input', 'inputs']:
      if key in kwargs:
        kwargs[key] += pch_output
        return

    # If the keyword was not found, assume the sources are the first argument in
    # the non-keyworded list.
    if args:
      args[0].append(pch_output[0])


def ComponentStaticLibrary(env, lib_name, *args, **kwargs):
  """Pseudo-builder for static library.

  Enables precompiled headers if they are enabled for the build unless
  use_pch_default = False. This requires that the source files are specified in
  sources or in a list as the first argument after lib_name.

  Args:
    env: Environment in which we were called.
    lib_name: Static library name.
    args: Positional arguments.
    kwargs: Keyword arguments.

  Returns:
    Output node list from env.ComponentLibrary().
  """
  _ConditionallyEnablePrecompile(env, lib_name, *args, **kwargs)

  return env.ComponentLibrary(lib_name, *args, **kwargs)


# TODO(omaha): Add signing.
def ComponentDll(env, lib_name, *args, **kwargs):
  """Pseudo-builder for DLL.

  Enables precompiled headers if they are enabled for the build unless
  use_pch_default = False. This requires that the source files are specified in
  sources or in a list as the first argument after lib_name.

  Args:
    env: Environment in which we were called.
    lib_name: DLL name.
    args: Positional arguments.
    kwargs: Keyword arguments.

  Returns:
    Output node list from env.ComponentLibrary().
  """
  env.Append(COMPONENT_STATIC=False)

  _ConditionallyEnablePrecompile(env, lib_name, *args, **kwargs)

  return env.ComponentLibrary(lib_name, *args, **kwargs)


# TODO(omaha): Add signing.
def ComponentSignedProgram(env, prog_name, *args, **kwargs):
  """Pseudo-builder for signed EXEs.

  Enables precompiled headers if they are enabled for the build unless
  use_pch_default = False. This requires that the source files are specified in
  sources or in a list as the first argument after prog_name.

  Args:
    env: Environment in which we were called.
    prog_name: Executable name.
    args: Positional arguments.
    kwargs: Keyword arguments.

  Returns:
    Output node list from env.ComponentProgram().
  """
  _ConditionallyEnablePrecompile(env, prog_name, *args, **kwargs)

  return env.ComponentProgram(prog_name, *args, **kwargs)


# TODO(omaha): Put these in a tools/ directory instead of staging.
def ComponentTool(env, prog_name, *args, **kwargs):
  """Pseudo-builder for utility programs that do not need to be signed.

  Enables precompiled headers if they are enabled for the build unless
  use_pch_default = False. This requires that the source files are specified in
  sources or in a list as the first argument after prog_name.

  Args:
    env: Environment in which we were called.
    prog_name: Executable name.
    args: Positional arguments.
    kwargs: Keyword arguments.

  Returns:
    Output node list from env.ComponentProgram().
  """
  _ConditionallyEnablePrecompile(env, prog_name, *args, **kwargs)

  return env.ComponentProgram(prog_name, *args, **kwargs)


#
# Unit Test Builders
#


def OmahaUnittest(env,  # pylint: disable-msg=C6409
                  name,
                  source,
                  LIBS=None,
                  all_in_one=True,
                  COMPONENT_TEST_SIZE='large',
                  is_small_tests_using_resources=False):
  """Declares a new unit test.

  Args:
    env: The environment.
    name: Name of the unit test.
    source: Sources for the unittest.
    LIBS: Any libs required for the unit test.
    all_in_one: If true, the test will be added to an executable containing
        all tests.
    COMPONENT_TEST_SIZE: small, medium, or large.
    is_small_tests_using_resources: True if COMPONENT_TEST_SIZE='small' and
        the test requires resources, such as strings.

  If !all_in_one and COMPONENT_TEST_SIZE is 'small', a main is automatically
  provided. Otherwise, one must be provided in source or LIBS. The small main
  is selected based on is_small_tests_using_resources.

  Returns:
    Output node list from env.ComponentTestProgram().

  Raises:
      Exception: Invalid combination of arguments.
  """
  test_env = env.Clone()

  source = test_env.Flatten(source)

  if COMPONENT_TEST_SIZE != 'small' and is_small_tests_using_resources:
    raise Exception('is_small_tests_using_resources set for non-small test.')

  if all_in_one:
    test_env['all_in_one_unittest_sources'].extend(test_env.File(source))
    if LIBS:
      test_env['all_in_one_unittest_libs'].update(
          test_env.File(test_env.Flatten(LIBS)))
    # TODO(omaha): Get the node list automatically.
    if 'HAMMER_RUNS_TESTS' in os.environ.keys():
      test_program_dir = '$TESTS_DIR'
    else:
      test_program_dir = '$STAGING_DIR'
    output = [os.path.join(test_program_dir, 'omaha_unittest.exe'),
              os.path.join(test_program_dir, 'omaha_unittest.pdb')]
  else:
    test_env.FilterOut(LINKFLAGS=['/NODEFAULTLIB', '/SUBSYSTEM:WINDOWS'])
    if LIBS:
      test_env.Append(
          LIBS=test_env.Flatten(LIBS),
      )
    # TODO(omaha): Let's try to eliminate this giant list of Win32 .libs here.
    # They are generally dependencies of Omaha base, common, or net; it makes
    # more sense for unit test authors to stay aware of dependencies and pass
    # them in as part of the LIBS argument.
    test_env.Append(
        CPPPATH=[
            '$MAIN_DIR/third_party/gmock/include',
            '$MAIN_DIR/third_party/gtest/include',
        ],
        LIBS=[
            '$LIB_DIR/base',
            '$LIB_DIR/gmock',
            '$LIB_DIR/gtest',
            ('atls', 'atlsd')[test_env.Bit('debug')],

            # Required by base/process.h, which is used by unit_test.cc.
            'psapi',

            # Required by omaha_version.h, which is used by omaha_unittest.cc.
            'version',

            # Rquired if base/utils.h, which is used by several modules used by
            # omaha_unittest.cc, is used.
            'netapi32',
            'rasapi32',
            'shlwapi',
            'wtsapi32',
        ],
    )

    if COMPONENT_TEST_SIZE == 'small':
      if is_small_tests_using_resources:
        test_env.Append(LIBS=['$LIB_DIR/unittest_base_small_with_resources'])
      else:
        test_env.Append(LIBS=['$LIB_DIR/unittest_base_small'])

    if env.Bit('use_precompiled_headers'):
      source += test_env.EnablePrecompile(name)

    # Set environment variables specific to the tests.
    for env_var in os.environ:
      if (not env_var in test_env['ENV'] and
          (env_var.startswith('GTEST_') or env_var.startswith('OMAHA_TEST_'))):
        test_env['ENV'][env_var] = os.environ[env_var]

    output = test_env.ComponentTestProgram(
        name,
        source + ['$OBJ_ROOT/testing/run_as_invoker.res'],
        COMPONENT_TEST_SIZE=COMPONENT_TEST_SIZE,
    )

  # Add a manual dependency on the resource file used by omaha_unittest.cc to
  # ensure it is always available before the test runs, which could be during
  # the build.
  test_env.Depends(output, '$TESTS_DIR/goopdateres_en.dll')

  return output


def GetAllInOneUnittestSources(env):
  """Returns a list of source files for the all-in-one unit test.

  Args:
    env: The environment.

  Returns:
    A list of sources for the all-in-one unit test.
  """
  return env['all_in_one_unittest_sources']


def GetAllInOneUnittestLibs(env):
  """Returns a list of libs to be linked into the all-in-one unit test.

  Args:
    env: The environment.

  Returns:
    A list of libs for the all-in-one unit test.
  """
  # Sort to prevent spurious rebuilds caused by indeterminate ordering of a set.
  return sorted(env['all_in_one_unittest_libs'],
                key=SCons.Node.FS.Base.get_abspath)


# If a .idl file does not result in any generated proxy code (no foo_p.c and
# no foo_data.c), the default TypeLibrary builder will mistakenly believe that
# the IDL needs to be run through midl.exe again to rebuild the missing files.
def _MidlEmitter(target, source, env):
  def IsNonProxyGeneratedFile(x):
    """Returns true if x is not generated proxy code, false otherwise."""
    return not (str(x).endswith('_p.c') or str(x).endswith('_data.c'))

  (t, source) = SCons.Tool.midl.midl_emitter(target, source, env)
  return (filter(IsNonProxyGeneratedFile, t), source)


def IsCoverageBuild(env):
  """Returns true if this is a coverage build.

  Args:
    env: The environment.

  Returns:
    whether this is a coverage build.
  """
  return 'coverage' in env.subst('$BUILD_TYPE')


def CopyFileToDirectory(env, target, source):
  """Copies the file to the directory using the DOS copy command.

  In general, Replicate() should be used, but there are specific cases where
  an explicit copy is required.

  Args:
    env: The environment.
    target: The target directory.
    source: The full path to the source file.

  Returns:
    Output node list from env.Command().
  """
  (_, source_filename) = os.path.split(source)
  return env.Command(target=os.path.join(target, source_filename),
                     source=source,
                     action='@copy /y $SOURCE $TARGET')


# NOTE: SCons requires the use of this name, which fails gpylint.
def generate(env):  # pylint: disable-msg=C6409
  """SCons entry point for this tool."""
  env.AddMethod(EnablePrecompile)
  env.AddMethod(SignDotNetManifest)
  env.AddMethod(ComponentStaticLibrary)
  env.AddMethod(ComponentDll)
  env.AddMethod(ComponentSignedProgram)
  env.AddMethod(ComponentTool)
  env.AddMethod(OmahaUnittest)
  env.AddMethod(GetAllInOneUnittestSources)
  env.AddMethod(GetAllInOneUnittestLibs)
  env.AddMethod(IsCoverageBuild)
  env.AddMethod(CopyFileToDirectory)

  env['MIDLNOPROXYCOM'] = ('$MIDL $MIDLFLAGS /tlb ${TARGETS[0]} '
                           '/h ${TARGETS[1]} /iid ${TARGETS[2]} '
                           '$SOURCE 2> NUL')
  env['BUILDERS']['TypeLibraryWithNoProxy'] = SCons.Builder.Builder(
      action=SCons.Action.Action('$MIDLNOPROXYCOM', '$MIDLNOPROXYCOMSTR'),
      src_suffix='.idl',
      suffix='.tlb',
      emitter=_MidlEmitter,
      source_scanner=SCons.Tool.midl.idl_scanner)
