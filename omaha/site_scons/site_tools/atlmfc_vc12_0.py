#!/usr/bin/python2.4
# Copyright 2014, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Windows ATL MFC for VC12 (Visual Studio 2013) tool for SCons.

Note that ATL MFC requires the commercial (non-free) version of Visual Studio
2013.  Using this in an open-source project thus limits the size of the
developer community to those with the commercial version.
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
