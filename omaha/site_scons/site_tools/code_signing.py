# Copyright 2016 Google Inc.
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

"""Code signing build tool.

This module sets up code signing.
It is used as follows:
  env = Environment(tools = ["code_signing"])
To sign an EXE/DLL do:
  env.SignedBinary('hello_signed.exe', 'hello.exe',
                   CERTIFICATE_FILE='bob.pfx',
                   CERTIFICATE_PASSWORD='123',
                   TIMESTAMP_SERVER='')
If no certificate file is specified, copying instead of signing will occur.
If an empty timestamp server string is specified, there will be no timestamp.
"""

import optparse
from SCons.compat._scons_optparse import OptionConflictError
import SCons.Script


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  try:
    SCons.Script.AddOption('--certificate-name',
                           dest='certificate_name',
                           help='select which certificate to use')
    SCons.Script.Help(
        '  --certificate-name <NAME>   select which signing certificate to use')
  except (OptionConflictError, optparse.OptionConflictError):
    # This gets catch to prevent duplicate help being added for this option
    # for each build type.
    pass

  env.SetDefault(
      # Path to Microsoft signtool.exe
      SIGNTOOL='"$THIRD_PARTY/code_signing/signtool.exe"',
      # No certificate by default.
      CERTIFICATE_PATH='',
      # No sha1 certificate by default.
      SHA1_CERTIFICATE_PATH='',
      # No sha256 certificate by default.
      SHA2_CERTIFICATE_PATH='',
      # No certificate password by default.
      CERTIFICATE_PASSWORD='',
      # The default timestamp server.
      TIMESTAMP_SERVER='http://timestamp.digicert.com',
      # The default timestamp server when dual-signing.
      SHA1_TIMESTAMP_SERVER='http://timestamp.digicert.com',
      # The default timestamp server for sha256 timestamps.
      SHA2_TIMESTAMP_SERVER='http://timestamp.digicert.com',
      # The default certificate store.
      CERTIFICATE_STORE='my',
      # Set the certificate name from the command line.
      CERTIFICATE_NAME=SCons.Script.GetOption('certificate_name'),
      # The name (substring) of the certificate issuer, when needed to
      # differentiate between multiple certificates.
      SHA1_CERTIFICATE_ISSUER='Verisign',
      SHA2_CERTIFICATE_ISSUER='Symantec',
      # Or differentiate based on the cert's hash.
      CERTIFICATE_HASH='5A9272CE76A9415A4A3A5002A2589A049312AA40',
      SHA1_CERTIFICATE_HASH='',
      SHA2_CERTIFICATE_HASH='',
  )

  # Setup Builder for Signing
  env['BUILDERS']['SignedBinary'] = SCons.Script.Builder(
      generator=SignedBinaryGenerator,
      emitter=SignedBinaryEmitter)
  env['BUILDERS']['DualSignedBinary'] = SCons.Script.Builder(
      generator=DualSignedBinaryGenerator,
      emitter=SignedBinaryEmitter)


def SignedBinaryEmitter(target, source, env):
  """Add the signing certificate (if any) to the source dependencies."""
  if env.subst('$CERTIFICATE_PATH'):
    source.append(env.subst('$CERTIFICATE_PATH'))
  return target, source


def SignedBinaryGenerator(source, target, env, for_signature):
  """A builder generator for code signing."""
  source = source                # Silence gpylint.
  target = target                # Silence gpylint.
  for_signature = for_signature  # Silence gpylint.

  # Alway copy and make writable.
  commands = [
      SCons.Script.Copy('$TARGET', '$SOURCE'),
      SCons.Script.Chmod('$TARGET', 0755),
  ]

  # Only do signing if there is a certificate file or certificate name.
  if env.subst('$CERTIFICATE_PATH') or env.subst('$CERTIFICATE_NAME'):
    # The command used to do signing (target added on below).
    signing_cmd = '$SIGNTOOL sign /fd sha1'
    # Add in certificate file if any.
    if env.subst('$CERTIFICATE_PATH'):
      signing_cmd += ' /f "$CERTIFICATE_PATH"'
      # Add certificate password if any.
      if env.subst('$CERTIFICATE_PASSWORD'):
        signing_cmd += ' /p "$CERTIFICATE_PASSWORD"'
    # Add certificate store if any.
    if env.subst('$CERTIFICATE_NAME'):
      # The command used to do signing (target added on below).
      signing_cmd += ' /s "$CERTIFICATE_STORE" /n "$CERTIFICATE_NAME"'
    # Add cert hash if any.
    if env.subst('$CERTIFICATE_HASH'):
      signing_cmd += ' /sha1 "$CERTIFICATE_HASH"'
    # Add timestamp server if any.
    if env.subst('$TIMESTAMP_SERVER'):
      signing_cmd += ' /t "$TIMESTAMP_SERVER"'
    # Add in target name
    signing_cmd += ' "$TARGET"'
    # Add the signing to the list of commands to perform.
    commands.append(signing_cmd)

  return commands

def DualSignedBinaryGenerator(source, target, env, for_signature):
  """A builder generator for code signing with two certs."""
  source = source                # Silence gpylint.
  target = target                # Silence gpylint.
  for_signature = for_signature  # Silence gpylint.

  # Alway copy and make writable.
  commands = [
      SCons.Script.Copy('$TARGET', '$SOURCE'),
      SCons.Script.Chmod('$TARGET', 0755),
  ]

  # Only do signing if there are certificate files or a certificate name. The
  # CERTIFICATE_NAME is expected to be the same for both SHA1 and SHA2.
  if (env.subst('$SHA1_CERTIFICATE_PATH') and
      env.subst('$SHA2_CERTIFICATE_PATH')) or env.subst('$CERTIFICATE_NAME'):
    # Setup common signing command options (same as single signing).
    base_signing_cmd = '$SIGNTOOL sign /v '
    # Add certificate store if any.
    if env.subst('$CERTIFICATE_NAME'):
      # The command used to do signing (target added on below).
      base_signing_cmd += ' /s "$CERTIFICATE_STORE" /n "$CERTIFICATE_NAME"'

    # SHA1-specific options, e.g.:
    # "signtool.exe" sign /v /n "Google Inc"
    #   /t http://timestamp.globalsign.com/scripts/timstamp.dll
    #   /i "Verisign" someFile.exe
    sha1_signing_cmd = base_signing_cmd
    sha1_signing_cmd += ' /fd sha1'
    # Add in certificate file if any.
    if env.subst('$SHA1_CERTIFICATE_PATH'):
      sha1_signing_cmd += ' /f "$SHA1_CERTIFICATE_PATH"'
      # Add certificate password if any.
      if env.subst('$SHA1_CERTIFICATE_PASSWORD'):
        sha1_signing_cmd += ' /p "$SHA1_CERTIFICATE_PASSWORD"'
    # Add timestamp server if any.
    if env.subst('$SHA1_TIMESTAMP_SERVER'):
      sha1_signing_cmd += ' /t "$SHA1_TIMESTAMP_SERVER"'
    # Add issuer if any.
    if env.subst('$SHA1_CERTIFICATE_ISSUER'):
      sha1_signing_cmd += ' /i "$SHA1_CERTIFICATE_ISSUER"'
    # Add cert hash if any.
    if env.subst('$SHA1_CERTIFICATE_HASH'):
      sha1_signing_cmd += ' /sha1 "$SHA1_CERTIFICATE_HASH"'
    # Add in target name
    sha1_signing_cmd += ' "$TARGET"'
    # Add the SHA1 signing to the list of commands to perform.
    commands.append(sha1_signing_cmd)

    # SHA2-specific options, e.g.:
    # "signtool.exe" sign /v /n "Google Inc"
    #   /tr http://timestamp.globalsign.com/?signature=sha2 /td "SHA256"
    #   /i "Symantec" /as /fd "SHA256" someFile.exe
    sha2_signing_cmd = base_signing_cmd
    # Add in certificate file if any.
    if env.subst('$SHA2_CERTIFICATE_PATH'):
      sha2_signing_cmd += ' /f "$SHA2_CERTIFICATE_PATH"'
      # Add certificate password if any.
      if env.subst('$SHA2_CERTIFICATE_PASSWORD'):
        sha2_signing_cmd += ' /p "$SHA2_CERTIFICATE_PASSWORD"'
    # Add timestamp server if any.
    if env.subst('$SHA2_TIMESTAMP_SERVER'):
      sha2_signing_cmd += ' /tr "$SHA2_TIMESTAMP_SERVER" /td "SHA256"'
    # Add issuer if any.
    if env.subst('$SHA2_CERTIFICATE_ISSUER'):
      sha2_signing_cmd += ' /i "$SHA2_CERTIFICATE_ISSUER"'
    # Add cert hash if any.
    if env.subst('$SHA2_CERTIFICATE_HASH'):
      sha2_signing_cmd += ' /sha1 "$SHA2_CERTIFICATE_HASH"'
    # Other options needed when adding a second, sha2 signature.
    sha2_signing_cmd += ' /as /fd "SHA256"'
    # Add in target name
    sha2_signing_cmd += ' "$TARGET"'
    # Add the SHA2 signing to the list of commands to perform.
    commands.append(sha2_signing_cmd)

  return commands
