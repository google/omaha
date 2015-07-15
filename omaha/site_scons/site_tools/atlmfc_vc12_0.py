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

"""Windows ATL MFC for VC12 (Visual Studio 2013) tool for SCons.

Note that ATL MFC requires the commercial (non-free) version of Visual Studio
2013.
"""

import os


def _FindLocalInstall():
  """Returns the directory containing the local install of the tool.

  Returns:
    Path to tool (as a string), or None if not found.
  """
  # TODO: Should use a better search.  Probably needs to be based on msvs tool,
  # as msvc detection is.
  default_dir = 'C:/Program Files (x86)/Microsoft Visual Studio 12.0/VC/atlmfc'
  if os.path.exists(default_dir):
    return default_dir
  else:
    return None


def generate(env):
  # NOTE: SCons requires the use of this name, which fails gpylint.
  """SCons entry point for this tool."""

  if not env.get('ATLMFC_VC12_0_DIR'):
    env['ATLMFC_VC12_0_DIR'] = _FindLocalInstall()

  env.AppendENVPath('INCLUDE', env.Dir('$ATLMFC_VC12_0_DIR/include').abspath)
  env.AppendENVPath('LIB', env.Dir('$ATLMFC_VC12_0_DIR/lib').abspath)
