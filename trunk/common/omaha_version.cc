// Copyright 2007-2009 Google Inc.
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

#include "omaha/common/omaha_version.h"
#include "omaha/common/app_util.h"
#include "omaha/common/debug.h"
#include "omaha/common/file_ver.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"

namespace {

// The version string is not visible outside this module and it is only
// accessible through the omaha::GetVersionString accessor.
CString version_string;

ULONGLONG omaha_version = 0;

}  // namespace

namespace omaha {

// In both GetVersion* methods, we assert only that InitializeVersion* was
// already called and not that they weren't called twice, which is OK.
// There is no detection that the version_ variables aren't accessed before
// being initialized because this would require accessor methods to enforce and
// lead to bloat.

const TCHAR* GetVersionString() {
  ASSERT1(!version_string.IsEmpty());
  return version_string;
}

ULONGLONG GetVersion() {
  ASSERT1(!version_string.IsEmpty());
  return omaha_version;
}

void InitializeVersionFromModule(HINSTANCE instance) {
  ULONGLONG module_version = app_util::GetVersionFromModule(instance);

  InitializeVersion(module_version);
}

void InitializeVersion(ULONGLONG version) {
  omaha_version = version;

  version_string = StringFromVersion(omaha_version);
}

}  // namespace omaha
