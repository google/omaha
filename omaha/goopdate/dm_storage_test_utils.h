// Copyright 2019 Google LLC.
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

#ifndef OMAHA_GOOPDATE_DM_STORAGE_TEST_UTILS_H_
#define OMAHA_GOOPDATE_DM_STORAGE_TEST_UTILS_H_

#include <tchar.h>

namespace omaha {

// Test helper functions for writing enrollment and device management tokens
// into various locations in the Windows registry. All functions use Google
// Test ASSERTs, and must therefore be wrapped in ASSERT_NO_FATAL_FAILURE.

void WriteInstallToken(const TCHAR* enrollment_token);
void WriteCompanyPolicyToken(const TCHAR* enrollment_token);
void WriteCompanyDmToken(const char* dm_token);

#if defined(HAS_LEGACY_DM_CLIENT)
void WriteLegacyPolicyToken(const TCHAR* enrollment_token);
void WriteOldLegacyPolicyToken(const TCHAR* enrollment_token);
void WriteLegacyDmToken(const char* dm_token);
#endif  // defined(HAS_LEGACY_DM_CLIENT)

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_DM_STORAGE_TEST_UTILS_H_

