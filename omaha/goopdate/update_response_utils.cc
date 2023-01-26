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

#include "omaha/goopdate/update_response_utils.h"

#include <algorithm>
#include <regex>
#include <string>
#include <vector>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/system_info.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/lang.h"
#include "omaha/common/xml_const.h"
#include "omaha/common/xml_parser.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace update_response_utils {

namespace {

// This function is called with a dotted pair, which is the minimum OS
// version and it comes from the update response, or with the a dotted quad,
// which is the OS version of the host.
ULONGLONG OSVersionFromString(const CString& s) {
  if (std::regex_search(std::wstring(s), std::wregex(_T("^\\d+\\.\\d+$")))) {
    // Convert from "x.y" to "x.y.0.0" format so we can use the existing
    // VersionFromString utility function.
    return VersionFromString(s + _T(".0.0"));
  }

  // The string is a dotted quad version. VersionFromString handles the error if
  // the parameter is something else.
  ASSERT1(std::regex_search(std::wstring(s),
                            std::wregex(_T("^\\d+\\.\\d+\\.\\d+\\.\\d+$"))));
  return VersionFromString(s);
}

bool IsPlatformCompatible(const CString& platform) {
  return platform.IsEmpty() || !platform.CompareNoCase(kPlatformWin);
}

// Checks if the current architecture is compatible with the entries in
// `arch_list`. `arch_list` can be a single entry, or multiple entries separated
// with `,`. Entries prefixed with `-` (negative entries) indicate
// non-compatible hosts. Non-prefixed entries indicate compatible guests.
//
// Returns `true` if:
// * `arch_list` is empty, or
// * none of the negative entries within `arch_list` match the current host
//   architecture exactly, and there are no non-negative entries, or
// * one of the non-negative entries within `arch_list` matches the current
//   architecture, or is compatible with the current architecture (i.e., it is a
//   compatible guest for the current host) as determined by
//   `::IsWow64GuestMachineSupported()`.
//   * If `::IsWow64GuestMachineSupported()` is not available, returns `true`
//     if `arch` is x86.
//
// Examples:
// * `arch_list` == "x86": returns `true` if run on all systems, because Omaha3
//   is x86, and is running the logic to determine compatibility).
// * `arch_list` == "x64": returns `true` if run on x64 or many arm64 systems.
// * `arch_list` == "x86,x64,-arm64": returns `false` if the underlying host is
// arm64.
// * `arch_list` == "-arm64": returns `false` if the underlying host is arm64.
bool IsArchCompatible(const CString& arch_list) {
  std::vector<CString> architectures;
  int pos = 0;
  do {
    const CString arch = arch_list.Tokenize(_T(","), pos).Trim().MakeLower();
    if (!arch.IsEmpty()) {
      architectures.push_back(arch);
    }
  } while (pos != -1);

  if (architectures.empty()) {
    return true;
  }

  std::sort(architectures.begin(), architectures.end());
  if (std::find(architectures.begin(), architectures.end(),
                _T('-') + SystemInfo::GetArchitecture().MakeLower()) !=
      architectures.end()) {
    return false;
  }

  architectures.erase(
      std::remove_if(architectures.begin(), architectures.end(),
                     [](const CString& arch) { return arch[0] == '-'; }),
      architectures.end());

  return architectures.empty() ||
         std::find_if(architectures.begin(), architectures.end(),
                      SystemInfo::IsArchitectureSupported) !=
             architectures.end();
}

bool IsOSVersionCompatible(const CString& min_os_version) {
  CString current_os_ver;
  CString current_sp;
  return min_os_version.IsEmpty() ||
         FAILED(goopdate_utils::GetOSInfo(&current_os_ver, &current_sp)) ||
         (OSVersionFromString(current_os_ver) >=
          OSVersionFromString(min_os_version));
}

}  // namespace

// TODO(omaha): unit test the functions below.

// Returns a pointer to the app object corresponding to the appid in the
// response object. Returns NULL if the app is not found.
const xml::response::App* GetApp(const xml::response::Response& response,
                                 const CString& appid) {
  size_t app_index = 0;
  for (; app_index < response.apps.size(); ++app_index) {
    if (!appid.CompareNoCase(response.apps[app_index].appid)) {
      return &response.apps[app_index];
    }
  }
  return NULL;
}

// Checks the response only includes one "untrusted" data item and the
// status of that item is "ok".
HRESULT ValidateUntrustedData(const std::vector<xml::response::Data>& data) {
  size_t num_untrusted_data_elements = 0;
  bool is_valid = false;
  std::vector<xml::response::Data>::const_iterator it;
  for (it = data.begin(); it != data.end(); ++it) {
    if (it->name == xml::value::kUntrusted) {
      ++num_untrusted_data_elements;
      is_valid = it->status == xml::response::kStatusOkValue;
    }
  }
  return (num_untrusted_data_elements == 1 && is_valid) ?
      S_OK : GOOPDATEINSTALL_E_INVALID_UNTRUSTED_DATA;
}

HRESULT GetInstallData(const std::vector<xml::response::Data>& data,
                       const CString& install_data_index,
                       CString* install_data) {
  ASSERT1(install_data);
  if (install_data_index.IsEmpty()) {
    return S_OK;
  }

  std::vector<xml::response::Data>::const_iterator it;
  for (it = data.begin(); it != data.end(); ++it) {
    if (install_data_index != it->install_data_index) {
      continue;
    }

    if (it->status != xml::response::kStatusOkValue) {
      ASSERT1(it->status == xml::response::kStatusNoData);
      return GOOPDATE_E_INVALID_INSTALL_DATA_INDEX;
    }

    ASSERT1(!it->install_data.IsEmpty());
    *install_data = it->install_data;
    return S_OK;
  }

  return GOOPDATE_E_INVALID_INSTALL_DATA_INDEX;
}

// Check the outer elements first, then check any child elements only if the
// outer element was successful.
CString GetAppResponseStatus(const xml::response::App& app) {
  if (_tcsicmp(xml::response::kStatusOkValue, app.status) != 0) {
    ASSERT1(!app.status.IsEmpty());
    return app.status;
  }

  if (!app.update_check.status.IsEmpty() &&
      _tcsicmp(xml::response::kStatusOkValue, app.update_check.status) != 0) {
    return app.update_check.status;
  }

  std::vector<xml::response::Data>::const_iterator data;
  for (data = app.data.begin(); data != app.data.end(); ++data) {
    if (!data->status.IsEmpty() &&
        _tcsicmp(xml::response::kStatusOkValue, data->status) != 0) {
      return data->status;
    }
  }

  if (!app.ping.status.IsEmpty() &&
      _tcsicmp(xml::response::kStatusOkValue, app.ping.status) != 0) {
    return app.ping.status;
  }

  std::vector<xml::response::Event>::const_iterator it;
  for (it = app.events.begin(); it != app.events.end(); ++it) {
    if (!it->status.IsEmpty() &&
        _tcsicmp(xml::response::kStatusOkValue, it->status) != 0) {
      return it->status;
    }
  }

  // TODO(omaha): verify that no other elements can report errors
  // once we've finalized the protocol.

  return app.status;
}

HRESULT BuildApp(const xml::UpdateResponse* update_response,
                 HRESULT code,
                 App* app) {
  ASSERT1(update_response);
  ASSERT1(SUCCEEDED(code) || code == GOOPDATE_E_NO_UPDATE_RESPONSE);
  ASSERT1(app);

  AppVersion* next_version = app->next_version();

  const CString& app_id = app->app_guid_string();

  const xml::response::App* response_app(GetApp(update_response->response(),
                                                app_id));
  ASSERT1(response_app);
  const xml::response::UpdateCheck& update_check = response_app->update_check;

  VERIFY_SUCCEEDED(app->put_ttToken(CComBSTR(update_check.tt_token)));

  Cohort cohort;
  cohort.cohort = response_app->cohort;
  cohort.hint = response_app->cohort_hint;
  cohort.name = response_app->cohort_name;
  app->set_cohort(cohort);

  if (code == GOOPDATE_E_NO_UPDATE_RESPONSE) {
    return S_OK;
  }

  for (size_t i = 0; i < update_check.urls.size(); ++i) {
    HRESULT hr = next_version->AddDownloadBaseUrl(update_check.urls[i]);
    if (FAILED(hr)) {
      return hr;
    }
  }

  for (size_t i = 0; i < update_check.install_manifest.packages.size(); ++i) {
    const xml::InstallPackage& package(
        update_check.install_manifest.packages[i]);
    HRESULT hr = next_version->AddPackage(package.name,
                                          package.size,
                                          package.hash_sha256);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app->untrusted_data().IsEmpty()) {
    HRESULT hr = ValidateUntrustedData(response_app->data);
    if (FAILED(hr)) {
      return hr;
    }
  }

  CString server_install_data;
  HRESULT hr = GetInstallData(response_app->data,
                              app->server_install_data_index(),
                              &server_install_data);
  if (FAILED(hr)) {
    return hr;
  }
  app->set_server_install_data(server_install_data);

  ASSERT1(!next_version->install_manifest());
  next_version->set_install_manifest(
      new xml::InstallManifest(update_check.install_manifest));

  // TODO(omaha): it appears the version_ below holds either the manifest
  // version or the "pv" version, written by the installer. If this is the case,
  // then it is confusing and perhaps we need to have two different members to
  // hold these values.
  ASSERT1(next_version->version().IsEmpty());
  next_version->set_version(next_version->install_manifest()->version);

  return S_OK;
}

// "noupdate" is an error for fresh installs, but a successful completion in
// the cases of silent and on demand updates. The caller is responsible for
// interpreting "noupdate" as it sees fit.
xml::UpdateResponseResult GetResult(const xml::UpdateResponse* update_response,
                                    const CString& appid,
                                    const CString& app_name,
                                    const CString& language) {
  ASSERT1(update_response);
  const xml::response::App* response_app(GetApp(update_response->response(),
                                                appid));

  StringFormatter formatter(language);
  CString text;

  if (!response_app) {
    CORE_LOG(L1, (_T("[UpdateResponse::GetResult][app not found][%s]"), appid));
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_UNKNOWN_APPLICATION, &text));
    return std::make_pair(GOOPDATE_E_NO_SERVER_RESPONSE, text);
  }

  const xml::response::UpdateCheck& update_check = response_app->update_check;
  const CString& status = GetAppResponseStatus(*response_app);
  const CString& display_name = update_check.install_manifest.name;

  ASSERT1(!status.IsEmpty());
  CORE_LOG(L1, (_T("[UpdateResponse::GetResult][%s][%s][%s][%s]"),
                appid, app_name, status, display_name));

  // ok
  if (_tcsicmp(xml::response::kStatusOkValue, status) == 0) {
    return std::make_pair(S_OK, CString());
  }

  // noupdate
  if (_tcsicmp(xml::response::kStatusNoUpdate, status) == 0) {
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_NO_UPDATE_RESPONSE, &text));
    return std::make_pair(GOOPDATE_E_NO_UPDATE_RESPONSE, text);
  }

  // "restricted"
  if (_tcsicmp(xml::response::kStatusRestrictedExportCountry, status) == 0) {
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_RESTRICTED_RESPONSE_FROM_SERVER,
                                           &text));
    return std::make_pair(GOOPDATE_E_RESTRICTED_SERVER_RESPONSE, text);
  }

  // "error-UnKnownApplication"
  if (_tcsicmp(xml::response::kStatusUnKnownApplication, status) == 0) {
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_UNKNOWN_APPLICATION, &text));
    return std::make_pair(GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE, text);
  }

  // "error-hwnotsupported"
  if (_tcsicmp(xml::response::kStatusHwNotSupported, status) == 0) {
    VERIFY_SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_HW_NOT_SUPPORTED,
                                              app_name));
    return std::make_pair(GOOPDATE_E_HW_NOT_SUPPORTED, text);
  }

  // "error-osnotsupported"
  if (_tcsicmp(xml::response::kStatusOsNotSupported, status) == 0) {
    VERIFY_SUCCEEDED(formatter.LoadString(IDS_OS_NOT_SUPPORTED, &text));
    return std::make_pair(GOOPDATE_E_OS_NOT_SUPPORTED, text);
  }

  // "error-internal"
  if (_tcsicmp(xml::response::kStatusInternalError, status) == 0) {
    VERIFY_SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_NON_OK_RESPONSE_FROM_SERVER,
                                              status));
    return std::make_pair(GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE, text);
  }

  // "error-hash"
  if (_tcsicmp(xml::response::kStatusHashError, status) == 0) {
    VERIFY_SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_NON_OK_RESPONSE_FROM_SERVER,
                                              status));
    return std::make_pair(GOOPDATE_E_SERVER_RESPONSE_NO_HASH, text);
  }

  // "error-unsupportedprotocol"
  if (_tcsicmp(xml::response::kStatusUnsupportedProtocol, status) == 0) {
    // TODO(omaha): Ideally, we would provide an app-specific URL instead of
    // just the publisher name. If it was a link, we could use point to a
    // redirect URL and provide the app GUID rather than somehow obtaining the
    // app-specific URL.
    VERIFY_SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_INSTALLER_OLD,
                                              kShortCompanyName));
    return std::make_pair(GOOPDATE_E_SERVER_RESPONSE_UNSUPPORTED_PROTOCOL,
                          text);
  }

  VERIFY_SUCCEEDED(formatter.FormatMessage(&text,
                                            IDS_NON_OK_RESPONSE_FROM_SERVER,
                                            status));
  return std::make_pair(GOOPDATE_E_UNKNOWN_SERVER_RESPONSE, text);
}

bool IsOmahaUpdateAvailable(const xml::UpdateResponse* update_response) {
  ASSERT1(update_response);
  xml::UpdateResponseResult update_response_result(
      update_response_utils::GetResult(update_response,
                                       kGoogleUpdateAppId,
                                       CString(),
                                       lang::GetDefaultLanguage(true)));
  return update_response_result.first == S_OK;
}

HRESULT ApplyExperimentLabelDeltas(bool is_machine,
                                   const xml::UpdateResponse* update_response) {
  ASSERT1(update_response);

  for (size_t app_index = 0;
       app_index < update_response->response().apps.size();
       ++app_index) {
    const xml::response::App& app = update_response->response().apps[app_index];
    if (!app.experiments.IsEmpty()) {
      VERIFY1(IsGuid(app.appid));

      HRESULT hr = ExperimentLabels::WriteRegistry(is_machine,
                                                   app.appid,
                                                   app.experiments);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  return S_OK;
}

xml::UpdateResponseResult CheckSystemRequirements(
    const xml::UpdateResponse* update_response, const CString& language) {
  ASSERT1(update_response);

  const xml::response::SystemRequirements& sys_req(
      update_response->response().sys_req);
  StringFormatter formatter(language);
  CString text;

  if (IsPlatformCompatible(sys_req.platform) &&
      IsArchCompatible(sys_req.arch) &&
      IsOSVersionCompatible(sys_req.min_os_version)) {
    return std::make_pair(S_OK, CString());
  }

  VERIFY_SUCCEEDED(formatter.LoadString(IDS_OS_NOT_SUPPORTED, &text));
  return std::make_pair(GOOPDATE_E_OS_NOT_SUPPORTED, text);
}


}  // namespace update_response_utils

}  // namespace omaha

