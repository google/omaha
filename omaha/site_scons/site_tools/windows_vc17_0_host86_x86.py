# Copyright 2023 Google Inc.
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

"""Tool-specific initialization for Visual Studio 2017.

NOTE: This tool is order-dependent, and must be run before
any other tools that modify the binary, include, or lib paths because it will
wipe out any existing values for those variables.

There normally shouldn't be any need to import this module directly.
It will usually be imported through the generic SCons.Tool.Tool()
selection method.

"""

import windows_vc


def generate(env):
  windows_vc.SetMsvcCompilerVersion(env=env,
                                    version_num='17.0',
                                    vc_flavor='x86_x86')
