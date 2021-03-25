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

#include "omaha/common/extra_args_parser.h"
#include <intsafe.h>
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

namespace {

HRESULT ConvertUtf8UrlEncodedString(const CString& encoded_string,
                                    CString* unencoded_string) {
  CString trimmed_val = encoded_string;
  trimmed_val.Trim();
  if (trimmed_val.IsEmpty()) {
    return E_INVALIDARG;
  }

  // The value is a utf8 encoded url escaped string that is stored as a
  // unicode string, convert it into a wide string.
  CString app_name;
  HRESULT hr = Utf8UrlEncodedStringToWideString(trimmed_val, unencoded_string);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT Validate(const TCHAR* extra_args) {
  CString extra_args_str(extra_args);
  if (extra_args_str.IsEmpty()) {
    return E_INVALIDARG;
  }

  if (-1 != extra_args_str.FindOneOf(kDisallowedCharsInExtraArgs)) {
    // A '/' was found in the "extra" arguments or "extra" arguments were
    // not specified before the next command.
    return E_INVALIDARG;
  }

  return S_OK;
}

// Handles tokens from the app arguments string.
HRESULT HandleAppArgsToken(const CString& token,
                           CommandLineExtraArgs* args,
                           int* cur_app_args_index) {
  ASSERT1(args);
  ASSERT1(cur_app_args_index);
  ASSERT1(*cur_app_args_index < static_cast<int>(args->apps.size()));

  if (args->apps.size() > INT_MAX) {
    return E_INVALIDARG;
  }

  CString name;
  CString value;
  if (!ParseNameValuePair(token, kNameValueSeparatorChar, &name, &value)) {
    return E_INVALIDARG;
  }

  if (name.CompareNoCase(kExtraArgAppGuid) == 0) {
    *cur_app_args_index = -1;
    for (size_t i = 0; i < args->apps.size(); ++i) {
      if (!value.CompareNoCase(GuidToString(args->apps[i].app_guid))) {
        *cur_app_args_index = static_cast<int>(i);
        break;
      }
    }

    if (-1 == *cur_app_args_index) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgInstallerData) == 0) {
    if (-1 == *cur_app_args_index) {
      return E_INVALIDARG;
    }
    args->apps[*cur_app_args_index].encoded_installer_data = value;
  } else {
    // Unrecognized token
    return E_INVALIDARG;
  }

  return S_OK;
}

HRESULT ParseAppArgs(const TCHAR* app_args, CommandLineExtraArgs* args) {
  if (!app_args || !*app_args) {
    return S_OK;
  }

  HRESULT hr = Validate(app_args);
  if (FAILED(hr)) {
    return hr;
  }

  int cur_app_args_index = -1;
  int pos = 0;
  CString input_str = app_args;
  CString token = input_str.Tokenize(kExtraArgsSeparators, pos);
  while (!token.IsEmpty()) {
    hr = HandleAppArgsToken(token, args, &cur_app_args_index);
    if (FAILED(hr)) {
      return hr;
    }

    token = input_str.Tokenize(kExtraArgsSeparators, pos);
  }

  return S_OK;
}

HRESULT StringToNeedsAdmin(const TCHAR* str, NeedsAdmin* value) {
  ASSERT1(str);
  ASSERT1(value);

  const TCHAR* const kFalse   = _T("false");
  const TCHAR* const kTrue    = _T("true");
  const TCHAR* const kPrefers = _T("prefers");

  if (_tcsicmp(kFalse, str) == 0) {
    *value = NEEDS_ADMIN_NO;
  } else if (_tcsicmp(kTrue, str) == 0) {
    *value = NEEDS_ADMIN_YES;
  } else if (_tcsicmp(kPrefers, str) == 0) {
    *value = NEEDS_ADMIN_PREFERS;
  } else {
    return E_INVALIDARG;
  }
  return S_OK;
}

HRESULT StringToRuntimeMode(const TCHAR* str, RuntimeMode* value) {
  ASSERT1(str);
  ASSERT1(value);

  const TCHAR* const kFalse   = _T("false");
  const TCHAR* const kTrue    = _T("true");
  const TCHAR* const kPersist = _T("persist");

  if (!_tcsicmp(kFalse, str)) {
    *value = RUNTIME_MODE_FALSE;
  } else if (!_tcsicmp(kTrue, str)) {
    *value = RUNTIME_MODE_TRUE;
  } else if (!_tcsicmp(kPersist, str)) {
    *value = RUNTIME_MODE_PERSIST;
  } else {
    return E_INVALIDARG;
  }
  return S_OK;
}

}  // namespace

// If no bundle name is specified, the first app's name is used.
// TODO(omaha): There is no enforcement of required or optional values of
// the extra arguments. Come up with a way to enforce this.
// TODO(omaha): If we prefix extra_args with '/' and replace all '=' with ' '
// and all & with '/' then we should be able to use the CommandLineParser
// class to pull out the values here.  We'd need to define scenarios for all
// permutations of ExtraArgs, but this shouldn't be difficult to get the right
// ones.

HRESULT ExtraArgsParser::Parse(const TCHAR* extra_args,
                               const TCHAR* app_args,
                               CommandLineExtraArgs* args) {
  HRESULT hr = ParseExtraArgs(extra_args, args);
  if (FAILED(hr)) {
    return hr;
  }

  return ParseAppArgs(app_args, args);
}

HRESULT ExtraArgsParser::ParseExtraArgs(const TCHAR* extra_args,
                                        CommandLineExtraArgs* args) {
  HRESULT hr = Validate(extra_args);
  if (FAILED(hr)) {
    return hr;
  }

  first_app_ = true;
  int pos = 0;
  CString input_str(extra_args);
  CString token = input_str.Tokenize(kExtraArgsSeparators, pos);
  while (!token.IsEmpty()) {
    CORE_LOG(L2, (_T("[ExtraArgsParser::Parse][token=%s]"), token));
    hr = HandleToken(token, args);
    if (FAILED(hr)) {
      OPT_LOG(L2, (_T("[Failed to parse extra argument][%s]"), token));
      return hr;
    }

    // Continue parsing
    token = input_str.Tokenize(kExtraArgsSeparators, pos);
  }

  // Save the arguments for the last application.
  args->apps.push_back(cur_extra_app_args_);

  ASSERT1(args->runtime_mode == RUNTIME_MODE_NOT_SET ||
          args->apps[0].app_guid == GUID_NULL);

  if (args->bundle_name.IsEmpty()) {
    ASSERT1(!args->apps.empty());
    args->bundle_name = args->apps[0].app_name;
  }

  return S_OK;
}

// Handles tokens from the extra arguments string.
HRESULT ExtraArgsParser::HandleToken(const CString& token,
                                     CommandLineExtraArgs* args) {
  CString name;
  CString value;
  if (!ParseNameValuePair(token, kNameValueSeparatorChar, &name, &value)) {
    return E_INVALIDARG;
  }

  // The first set of args apply to all apps. They may occur at any point, but
  // only the last occurrence is recorded.
  if (name.CompareNoCase(kExtraArgBundleName) == 0) {
    if (value.GetLength() > kMaxNameLength) {
      return E_INVALIDARG;
    }
    HRESULT hr = ConvertUtf8UrlEncodedString(value,
                                             &args->bundle_name);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (name.CompareNoCase(kExtraArgInstallationId) == 0) {
    ASSERT1(!value.IsEmpty());
    if (FAILED(StringToGuidSafe(value, &args->installation_id))) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgBrandCode) == 0) {
    if (value.GetLength() > kBrandIdLength) {
      return E_INVALIDARG;
    }
    args->brand_code = value;
  } else if (name.CompareNoCase(kExtraArgClientId) == 0) {
    args->client_id = value;
  } else if (name.CompareNoCase(kExtraArgOmahaExperimentLabels) == 0) {
    HRESULT hr = ConvertUtf8UrlEncodedString(value,
                                             &args->experiment_labels);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (name.CompareNoCase(kExtraArgReferralId) == 0) {
    args->referral_id = value;
  } else if (name.CompareNoCase(kExtraArgBrowserType) == 0) {
    BrowserType type = BROWSER_UNKNOWN;
    if (SUCCEEDED(goopdate_utils::ConvertStringToBrowserType(value, &type))) {
      args->browser_type = type;
    }
  } else if (name.CompareNoCase(kExtraArgLanguage) == 0) {
    if (value.GetLength() > kLangMaxLength) {
      return E_INVALIDARG;
    }
    // Even if we don't support the language, we want to pass it to the
    // installer. Omaha will pick its language later. See http://b/1336966.
    args->language = value;
  } else if (name.CompareNoCase(kExtraArgUsageStats) == 0) {
    if (!String_StringToTristate(value, &args->usage_stats_enable)) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgRuntimeMode) == 0) {
    if (!args->apps.empty() || cur_extra_app_args_.app_guid != GUID_NULL) {
      return E_INVALIDARG;
    }

    if (FAILED(StringToRuntimeMode(value, &args->runtime_mode))) {
      return E_INVALIDARG;
    }
#if defined(HAS_DEVICE_MANAGEMENT)
  } else if (name.CompareNoCase(kExtraArgEnrollmentToken) == 0) {
    if (!IsUuid(value)) {
      return E_INVALIDARG;
    }
    args->enrollment_token = value;
#endif

  // The following args are per-app.
  } else if (name.CompareNoCase(kExtraArgAdditionalParameters) == 0) {
    cur_extra_app_args_.ap = value;
  } else if (name.CompareNoCase(kExtraArgTTToken) == 0) {
    cur_extra_app_args_.tt_token = value;
  } else if (name.CompareNoCase(kExtraArgExperimentLabels) == 0) {
    HRESULT hr = ConvertUtf8UrlEncodedString(
        value,
        &cur_extra_app_args_.experiment_labels);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (name.CompareNoCase(kExtraArgAppGuid) == 0) {
    if (!first_app_) {
      // Save the arguments for the application we have been processing.
      args->apps.push_back(cur_extra_app_args_);
    }
    cur_extra_app_args_ = CommandLineAppArgs();

    HRESULT hr = StringToGuidSafe(value, &cur_extra_app_args_.app_guid);
    if (FAILED(hr)) {
      return hr;
    }
    if (cur_extra_app_args_.app_guid == GUID_NULL ||
        args->runtime_mode != RUNTIME_MODE_NOT_SET) {
      return E_INVALIDARG;
    }
    first_app_ = false;
  } else if (name.CompareNoCase(kExtraArgAppName) == 0) {
    if (value.GetLength() > kMaxNameLength) {
      return E_INVALIDARG;
    }
    HRESULT hr = ConvertUtf8UrlEncodedString(value,
                                             &cur_extra_app_args_.app_name);
    if (FAILED(hr)) {
      return hr;
    }
  } else if (name.CompareNoCase(kExtraArgNeedsAdmin) == 0) {
    if (FAILED(StringToNeedsAdmin(value, &cur_extra_app_args_.needs_admin))) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgInstallDataIndex) == 0) {
    cur_extra_app_args_.install_data_index = value;
  } else if (name.CompareNoCase(kExtraArgUntrustedData) == 0) {
    cur_extra_app_args_.untrusted_data = value;
  } else {
    // Unrecognized token
    return E_INVALIDARG;
  }

  return S_OK;
}

}  // namespace omaha
