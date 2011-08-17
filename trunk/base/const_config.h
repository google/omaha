// Copyright 2003-2010 Google Inc.
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

#ifndef OMAHA_BASE_CONST_CONFIG_H_
#define OMAHA_BASE_CONST_CONFIG_H_

#include "omaha/base/constants.h"

namespace omaha {

// TODO(omaha): Move these values used by debug.cc someplace else.
#define kRegKeyShared        _T("Shared")
#define kCiRegKeyShared      GOOPDATE_MAIN_KEY kRegKeyShared
#define kRegValueReportIds   _T("report_ids")

// TODO(omaha): Move these plugin values someplace else. Since we're building
// constants, that should probably be the customization header. Move the Omaha 3
// plugin equivalents from config.cc there as well.

// NOTE: ONECLICK_PLUGIN_VERSION_ANSI is defined in main.scons
// For example: kOneClickProgId == "Google.OneClickCtrl.1"
const TCHAR* const kOneClickProgId = COMPANY_NAME_IDENTIFIER
                                     _T(".OneClickCtrl.")
                                     _T(ONECLICK_PLUGIN_VERSION_ANSI);
// The plug-in MIME type.
// For example:
//     kOneClickPluginMimeTypeAnsi == "application/x-vnd.google.oneclickctrl.1"
// TODO(omaha): Deal with the "Google.OneClickCtrl.%d") in
// tools\goopdump\data_dumper_oneclick.cc after integrating goopdump.
#define kOneClickPluginMimeTypeAnsi \
    "application/x-vnd." COMPANY_DOMAIN_BASE_ANSI ".oneclickctrl." \
     ONECLICK_PLUGIN_VERSION_ANSI

}  // namespace omaha

#endif  // OMAHA_BASE_CONST_CONFIG_H_
