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

#ifndef OMAHA_GOOPDATE_CODE_RED_CHECK_H_
#define OMAHA_GOOPDATE_CODE_RED_CHECK_H_

#include <windows.h>
#include <atlstr.h>

namespace omaha {

HRESULT CheckForCodeRed(bool is_machine, const CString& omaha_version);

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CODE_RED_CHECK_H_
