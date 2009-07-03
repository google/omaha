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

#include "omaha/goopdate/extra_args_parser.h"

#include "omaha/common/const_cmd_line.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

// TODO(omaha): There is no enforcement of required or optional values of
// the extra arguments. Come up with a way to enforce this.
HRESULT ExtraArgsParser::Parse(const TCHAR* extra_args,
                               const TCHAR* app_args,
                               CommandLineExtraArgs* args) {
  // TODO(omaha): If we prefix extra_args with '/' and replace all '=' with ' '
  // and all & with '/' then we should be able to use the CommandLineParser
  // class to pull out the values here.  We'd need to define scenarios for all
  // permutations of ExtraArgs, but this shouldn't be difficult to get the right
  // ones.
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
      return hr;
    }

    // Continue parsing
    token = input_str.Tokenize(kExtraArgsSeparators, pos);
  }

  // Save the arguments for the last application.
  args->apps.push_back(cur_extra_app_args_);
  return ParseAppArgs(app_args, args);
}

HRESULT ExtraArgsParser::ParseAppArgs(const TCHAR* app_args,
                                      CommandLineExtraArgs* args) {
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
    CORE_LOG(L2, (_T("[ExtraArgsParser::ParseAppArgs][token=%s]"), token));
    hr = HandleAppArgsToken(token, args, &cur_app_args_index);
    if (FAILED(hr)) {
      return hr;
    }

    token = input_str.Tokenize(kExtraArgsSeparators, pos);
  }

  return S_OK;
}

HRESULT ExtraArgsParser::Validate(const TCHAR* extra_args) {
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
  if (name.CompareNoCase(kExtraArgInstallationId) == 0) {
    ASSERT1(!value.IsEmpty());
    if (FAILED(::CLSIDFromString(const_cast<TCHAR*>(value.GetString()),
                                 &args->installation_id))) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgBrandCode) == 0) {
    if (value.GetLength() > kBrandIdLength) {
      return E_INVALIDARG;
    }
    args->brand_code = value;
  } else if (name.CompareNoCase(kExtraArgClientId) == 0) {
    args->client_id = value;
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

    // The following args are per app.
  } else if (name.CompareNoCase(kExtraArgAdditionalParameters) == 0) {
    cur_extra_app_args_.ap = value;
  } else if (name.CompareNoCase(kExtraArgTTToken) == 0) {
    cur_extra_app_args_.tt_token = value;
  } else if (name.CompareNoCase(kExtraArgAppGuid) == 0) {
    if (!first_app_) {
      // Save the arguments for the application we have been processing.
      args->apps.push_back(cur_extra_app_args_);
    }
    cur_extra_app_args_ = CommandLineAppArgs();

    cur_extra_app_args_.app_guid = StringToGuid(value);
    if (cur_extra_app_args_.app_guid == GUID_NULL) {
      return E_INVALIDARG;
    }
    first_app_ = false;
  } else if (name.CompareNoCase(kExtraArgAppName) == 0) {
    if (value.GetLength() > kMaxAppNameLength) {
      return E_INVALIDARG;
    }
    CString trimmed_val = value.Trim();
    if (trimmed_val.IsEmpty()) {
      return E_INVALIDARG;
    }

    // The value is a utf8 encoded url escaped string that is stored as a
    // unicode string, convert it into a wide string.
    CString app_name;
    HRESULT hr = Utf8UrlEncodedStringToWideString(trimmed_val, &app_name);
    if (FAILED(hr)) {
      return hr;
    }
    cur_extra_app_args_.app_name = app_name;
  } else if (name.CompareNoCase(kExtraArgNeedsAdmin) == 0) {
    if (FAILED(String_StringToBool(value, &cur_extra_app_args_.needs_admin))) {
      return E_INVALIDARG;
    }
  } else if (name.CompareNoCase(kExtraArgInstallDataIndex) == 0) {
    cur_extra_app_args_.install_data_index = value;
  } else {
    // Unrecognized token
    return E_INVALIDARG;
  }

  return S_OK;
}

// Handles tokens from the app arguments string.
HRESULT ExtraArgsParser::HandleAppArgsToken(const CString& token,
                                            CommandLineExtraArgs* args,
                                            int* cur_app_args_index) {
  ASSERT1(args);
  ASSERT1(cur_app_args_index);
  ASSERT1(*cur_app_args_index < static_cast<int>(args->apps.size()));

  CString name;
  CString value;
  if (!ParseNameValuePair(token, kNameValueSeparatorChar, &name, &value)) {
    return E_INVALIDARG;
  }

  if (name.CompareNoCase(kExtraArgAppGuid) == 0) {
    *cur_app_args_index = -1;
    for (size_t i = 0; i < args->apps.size(); ++i) {
      if (!value.CompareNoCase(GuidToString(args->apps[i].app_guid))) {
        *cur_app_args_index = i;
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

}  // namespace omaha

