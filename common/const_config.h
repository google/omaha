// Copyright 2003-2009 Google Inc.
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

#ifndef OMAHA_COMMON_CONST_CONFIG_H__
#define OMAHA_COMMON_CONST_CONFIG_H__

namespace omaha {

// Root Registry keys
//
// In this version, everything is put under HKLM.
// NOTE: PUBLISHER_NAME_ANSI and PRODUCT_NAME_ANSI are defined in mk_common
// kProgramAnsi is "Google Update"
#define kProgramAnsi      PUBLISHER_NAME_ANSI " " PRODUCT_NAME_ANSI
#define kCiProgram        _T(PUBLISHER_NAME_ANSI) _T(" ") _T(PRODUCT_NAME_ANSI)
#define kGoogleRegKey     _T("Software\\") _T(PUBLISHER_NAME_ANSI) _T("\\")
#define kGoogleFullRegKey _T("HKLM\\") kGoogleRegKey

// kProductRegKey is _T("Software\\Google\\Update")
#define kProductRegKey       kGoogleRegKey _T(PRODUCT_NAME_ANSI)
#define kCiFullRegKey        _T("HKLM\\") kProductRegKey
#define kRegKeyConfig        _T("Config")
#define kRegKeyConfigPrefix  kRegKeyConfig
#define kCiFullRegKeyConfig  kCiFullRegKey _T("\\") kRegKeyConfig
#define kRegKeyShared        _T("Shared")
#define kCiRegKeyShared      kProductRegKey _T("\\") kRegKeyShared
#define kRegValueReportIds   _T("report_ids")

// NOTE: ACTIVEX_VERSION_ANSI is defined in mk_common
// For example: kOneClickProgIdAnsi == "Google.OneClickCtrl.1"
#define kOneClickProgIdAnsi  PUBLISHER_NAME_ANSI \
                             ".OneClickCtrl." \
                             ACTIVEX_VERSION_ANSI
#define kOneClickProgId      _T(PUBLISHER_NAME_ANSI) \
                             _T(".OneClickCtrl.") \
                             _T(ACTIVEX_VERSION_ANSI)

// The plug-in MIME type.
// For example:
//     kOneClickPluginMimeTypeAnsi == "application/x-vnd.google.oneclickctrl.1"
#define kOneClickPluginMimeTypeAnsi  "application/x-vnd.google.oneclickctrl." \
                                     ACTIVEX_VERSION_ANSI

// .oii is just an arbitrary extension.
#define kOneClickPluginMimeDescriptionAnsi  kOneClickPluginMimeTypeAnsi \
                                            ":.oii:" \
                                            kProgramAnsi


}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_CONFIG_H__

