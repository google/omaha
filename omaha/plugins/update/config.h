// Copyright 2009 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ========================================================================

#ifndef OMAHA_PLUGINS_UPDATE_CONFIG_H_
#define OMAHA_PLUGINS_UPDATE_CONFIG_H_

#include <windows.h>

namespace omaha {

// TODO(omaha3): The OneClick MIME type is currently defined one other place in
// the codebase, in base\const_config.h.  I'm putting our own definition here;
// we should probably delete the copy in base, and potentially moving this file
// to base as plugin_constants.h.
//
// Note that COMPANY_DOMAIN_BASE_ANSI and *_PLUGIN_VERSION_ANSI are defined
// in omaha\main.scons.

#define UPDATE3WEB_MIME_TYPE   "application/x-vnd." COMPANY_DOMAIN_BASE_ANSI \
                               ".update3webcontrol." UPDATE_PLUGIN_VERSION_ANSI

#define ONECLICK_MIME_TYPE     "application/x-vnd." COMPANY_DOMAIN_BASE_ANSI \
                               ".oneclickctrl." ONECLICK_PLUGIN_VERSION_ANSI

#define MERGED_MIME_TYPE UPDATE3WEB_MIME_TYPE "|" ONECLICK_MIME_TYPE

extern const TCHAR kUpdate3WebPluginVersion[];
extern const TCHAR kUpdate3WebControlProgId[];

extern const TCHAR kOneclickPluginVersion[];
extern const TCHAR kOneclickControlProgId[];

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_CONFIG_H_
