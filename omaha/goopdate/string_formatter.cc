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

#include "omaha/goopdate/string_formatter.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/resource_manager.h"

namespace omaha {

StringFormatter::StringFormatter(const CString& language)
    : language_(language) {
  ASSERT1(!language.IsEmpty());
}

HRESULT StringFormatter::LoadString(int32 resource_id, CString* result) {
  ASSERT1(result);

  HINSTANCE resource_handle = NULL;
  HRESULT hr = ResourceManager::Instance().GetResourceDll(language_,
                                                          &resource_handle);
  if (FAILED(hr)) {
    return hr;
  }

  const TCHAR* resource_string = NULL;
  int string_length = ::LoadString(
      resource_handle,
      resource_id,
      reinterpret_cast<TCHAR*>(&resource_string),
      0);
  if (string_length <= 0) {
    return HRESULTFromLastError();
  }
  ASSERT1(resource_string && *resource_string);

  // resource_string is the string starting point but not null-terminated, so
  // explicitly copy from it for string_length characters.
  result->SetString(resource_string, string_length);

  return S_OK;
}

HRESULT StringFormatter::FormatMessage(CString* result, int32 format_id, ...) {
  ASSERT1(result);
  ASSERT1(format_id != 0);

  CString format_string;
  HRESULT hr = LoadString(format_id, &format_string);
  if (FAILED(hr)) {
    return hr;
  }

  va_list arguments;
  va_start(arguments, format_id);
  result->FormatMessageV(format_string, &arguments);
  va_end(arguments);

  return S_OK;
}

}  // namespace omaha
