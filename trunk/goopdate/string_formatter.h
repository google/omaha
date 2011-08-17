// Copyright 2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_STRING_FORMATTER_H_
#define OMAHA_GOOPDATE_STRING_FORMATTER_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class StringFormatter {
 public:
  explicit StringFormatter(const CString& language);
  ~StringFormatter() {}

  // Loads string from the language resource DLL.
  HRESULT LoadString(int32 resource_id, CString* result);

  // Loads string for format_id from the language resource DLL and then use
  // that as the format string to create the result string.
  HRESULT FormatMessage(CString* result, int32 format_id, ...);

 private:
  CString language_;

  DISALLOW_EVIL_CONSTRUCTORS(StringFormatter);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_STRING_FORMATTER_H_
