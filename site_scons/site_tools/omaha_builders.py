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

import SCons.Action
import SCons.Builder
import SCons.Tool


def _EnablePrecompile(env, target_name):
  """Enable use of precompiled headers for target_name."""
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


def _SignDotNetManifest(env, target, unsigned_manifest):
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


_all_in_one_unittest_sources = []
_all_in_one_unittest_libs = set()


def _OmahaUnittest(env,  # pylint: disable-msg=C6409
                   unused_name,
                   source,
                   LIBS=None,
                   all_in_one=True):
  """Declares a new unit test.

  Args:
    env: The environment.
    unused_name: Name of the unit test.
    source: Sources for the unittest.
    LIBS: Any libs required for the unit test.
    all_in_one: If true, the test will be added to an executable containing
        all tests.

  Returns:
    Nothing.
  """
  source = env.Flatten(source)
  if all_in_one:
    _all_in_one_unittest_sources.extend(env.File(source))
    if LIBS:
      _all_in_one_unittest_libs.update(env.File(env.Flatten(LIBS)))
    # TODO(omaha): this should return the name of the all-in-one unit test.
  else:
    # TODO(omaha): implement this.
    raise NotImplementedError('Want to volunteer?')


def _GetAllInOneUnittestSources(unused_env):
  """Returns a list of source files for the all-in-one unit test.

  Args:
    unused_env: the environment.

  Returns:
    A list of sources for the all-in-one unit test.
  """
  return _all_in_one_unittest_sources


def _GetAllInOneUnittestLibs(unused_env):
  """Returns a list of libs to be linked into the all-in-one unit test.

  Args:
    unused_env: the environment.

  Returns:
    A set of libs for the all-in-one unit test.
  """
  return list(_all_in_one_unittest_libs)


# If a .idl file does not result in any generated proxy code (no foo_p.c and
# no foo_data.c), the default TypeLibrary builder will mistakenly believe that
# the IDL needs to be run through midl.exe again to rebuild the missing files.
def _MidlEmitter(target, source, env):
  def IsNonProxyGeneratedFile(x):
    """Returns true if x is not generated proxy code, false otherwise."""
    return not (str(x).endswith('_p.c') or str(x).endswith('_data.c'))

  (t, source) = SCons.Tool.midl.midl_emitter(target, source, env)
  return (filter(IsNonProxyGeneratedFile, t), source)


# NOTE: SCons requires the use of this name, which fails gpylint.
def generate(env):  # pylint: disable-msg=C6409
  """SCons entry point for this tool."""
  env.AddMethod(_EnablePrecompile, 'EnablePrecompile')
  env.AddMethod(_SignDotNetManifest, 'SignDotNetManifest')
  # These are not used by Omaha 2.
  #env.AddMethod(_OmahaUnittest, 'OmahaUnittest')
  #env.AddMethod(_GetAllInOneUnittestSources, 'GetAllInOneUnittestSources')
  #env.AddMethod(_GetAllInOneUnittestLibs, 'GetAllInOneUnittestLibs')

  env['MIDLNOPROXYCOM'] = ('$MIDL $MIDLFLAGS /tlb ${TARGETS[0]} '
                           '/h ${TARGETS[1]} /iid ${TARGETS[2]} '
                           '$SOURCE 2> NUL')
  env['BUILDERS']['TypeLibraryWithNoProxy'] = SCons.Builder.Builder(
      action=SCons.Action.Action('$MIDLNOPROXYCOM', '$MIDLNOPROXYCOMSTR'),
      src_suffix='.idl',
      suffix='.tlb',
      emitter=_MidlEmitter,
      source_scanner=SCons.Tool.midl.idl_scanner)
