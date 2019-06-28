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

# Windows ATL MFC for VC16 (Visual Studio 2019) tool for SCons.

import os


def _FindLocalInstall():
  """Returns the directory containing the local install of the tool.

  Returns:
    Path to tool (as a string), or None if not found.
  """
  default_dir = os.environ['VCToolsInstallDir'] + 'atlmfc'
  if os.path.exists(default_dir):
    return default_dir
  else:
    return None


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  if not env.get('ATLMFC_VC16_0_DIR'):
    env['ATLMFC_VC16_0_DIR'] = _FindLocalInstall()

  env.AppendENVPath('INCLUDE', env.Dir('$ATLMFC_VC16_0_DIR/include').abspath)
  env.AppendENVPath('LIB', env.Dir('$ATLMFC_VC16_0_DIR/lib/x86').abspath)
