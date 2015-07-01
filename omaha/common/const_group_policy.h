// Copyright 2008-2009 Google Inc.
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

#ifndef OMAHA_COMMON_CONST_GROUP_POLICY_H_
#define OMAHA_COMMON_CONST_GROUP_POLICY_H_

#include <tchar.h>
#include "omaha/base/constants.h"

namespace omaha {

// Key containing Omaha Group Policy settings. All policies are in HKLM.
// TODO(omaha): Rename Omaha.
const TCHAR* const kRegKeyGoopdateGroupPolicy =
    MACHINE_KEY GOOPDATE_POLICIES_RELATIVE;

// Preferences Categroy.
const TCHAR* const kRegValueAutoUpdateCheckPeriodOverrideMinutes =
    _T("AutoUpdateCheckPeriodMinutes");

// Applications Categroy.
// The prefix strings have the app's GUID appended to them.
const TCHAR* const kRegValueInstallAppsDefault  = _T("InstallDefault");
const TCHAR* const kRegValueInstallAppPrefix    = _T("Install");
const TCHAR* const kRegValueUpdateAppsDefault   = _T("UpdateDefault");
const TCHAR* const kRegValueUpdateAppPrefix     = _T("Update");

const bool kInstallPolicyDefault    = true;
const bool kUpdatePolicyDefault     = true;

const int kPolicyDisabled           = 0;
const int kPolicyEnabled            = 1;
const int kPolicyManualUpdatesOnly  = 2;

}  // namespace omaha

#endif  // OMAHA_COMMON_CONST_GROUP_POLICY_H_
