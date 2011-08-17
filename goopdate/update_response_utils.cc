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
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/common/lang.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"

namespace omaha {

namespace update_response_utils {

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

    if (it->status != kResponseStatusOkValue) {
      ASSERT1(it->status == kResponseDataStatusNoData);
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
  if (_tcsicmp(kResponseStatusOkValue, app.status) != 0) {
    ASSERT1(!app.status.IsEmpty());
    return app.status;
  }

  if (!app.update_check.status.IsEmpty() &&
      _tcsicmp(kResponseStatusOkValue, app.update_check.status) != 0) {
    return app.update_check.status;
  }

  std::vector<xml::response::Data>::const_iterator data;
  for (data = app.data.begin(); data != app.data.end(); ++data) {
    if (!data->status.IsEmpty() &&
        _tcsicmp(kResponseStatusOkValue, data->status) != 0) {
      return data->status;
    }
  }

  if (!app.ping.status.IsEmpty() &&
      _tcsicmp(kResponseStatusOkValue, app.ping.status) != 0) {
    return app.ping.status;
  }

  std::vector<xml::response::Event>::const_iterator it;
  for (it = app.events.begin(); it != app.events.end(); ++it) {
    if (!it->status.IsEmpty() &&
        _tcsicmp(kResponseStatusOkValue, it->status) != 0) {
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

  VERIFY1(SUCCEEDED(app->put_ttToken(CComBSTR(update_check.tt_token))));

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
                                          package.hash);
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
                                    const CString& language) {
  ASSERT1(update_response);
  const xml::response::App* response_app(GetApp(update_response->response(),
                                                appid));

  StringFormatter formatter(language);
  CString text;

  if (!response_app) {
    CORE_LOG(L1, (_T("[UpdateResponse::GetResult][app not found][%s]"), appid));
    VERIFY1(SUCCEEDED(formatter.LoadString(IDS_UNKNOWN_APPLICATION, &text)));
    return std::make_pair(GOOPDATE_E_NO_SERVER_RESPONSE, text);
  }

  const xml::response::UpdateCheck& update_check = response_app->update_check;
  const CString& status = GetAppResponseStatus(*response_app);
  const CString& display_name = update_check.install_manifest.name;

  ASSERT1(!status.IsEmpty());
  CORE_LOG(L1, (_T("[UpdateResponse::GetResult][%s][%s][%s]"),
                appid, status, display_name));

  // ok
  if (_tcsicmp(kResponseStatusOkValue, status) == 0) {
    return std::make_pair(S_OK, CString());
  }

  // noupdate
  if (_tcsicmp(kResponseStatusNoUpdate, status) == 0) {
    VERIFY1(SUCCEEDED(formatter.LoadString(IDS_NO_UPDATE_RESPONSE, &text)));
    return std::make_pair(GOOPDATE_E_NO_UPDATE_RESPONSE, text);
  }

  // "restricted"
  if (_tcsicmp(kResponseStatusRestrictedExportCountry, status) == 0) {
    VERIFY1(SUCCEEDED(formatter.LoadString(IDS_RESTRICTED_RESPONSE_FROM_SERVER,
                                           &text)));
    return std::make_pair(GOOPDATE_E_RESTRICTED_SERVER_RESPONSE, text);
  }

  // "error-UnKnownApplication"
  if (_tcsicmp(kResponseStatusUnKnownApplication, status) == 0) {
    VERIFY1(SUCCEEDED(formatter.LoadString(IDS_UNKNOWN_APPLICATION, &text)));
    return std::make_pair(GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE, text);
  }

  // "error-OsNotSupported"
  if (_tcsicmp(kResponseStatusOsNotSupported, status) == 0) {
    const CString& error_url(update_check.error_url);
    VERIFY1(SUCCEEDED(formatter.LoadString(IDS_OS_NOT_SUPPORTED, &text)));
    if (!error_url.IsEmpty()) {
      // TODO(omaha3): The error URL is no longer in the error string. Either
      // we need to provide this URL to the client or we need to deprecate
      // error_url and put this information in the Get Help redirect.
      // Alternatively, we could have the COM server build error URLs in all
      // cases. The current UI would still need to build an URL for the entire
      // bundle, though, because it does not have per-app links.
    }
    return std::make_pair(GOOPDATE_E_OS_NOT_SUPPORTED, text);
  }

  // "error-internal"
  if (_tcsicmp(kResponseStatusInternalError, status) == 0) {
    VERIFY1(SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_NON_OK_RESPONSE_FROM_SERVER,
                                              status)));
    return std::make_pair(GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE, text);
  }

  // "error-hash"
  if (_tcsicmp(kResponseStatusHashError, status) == 0) {
    VERIFY1(SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_NON_OK_RESPONSE_FROM_SERVER,
                                              status)));
    return std::make_pair(GOOPDATE_E_SERVER_RESPONSE_NO_HASH, text);
  }

  // "error-unsupportedprotocol"
  if (_tcsicmp(kResponseStatusUnsupportedProtocol, status) == 0) {
    // TODO(omaha): Ideally, we would provide an app-specific URL instead of
    // just the publisher name. If it was a link, we could use point to a
    // redirect URL and provide the app GUID rather than somehow obtaining the
    // app-specific URL.
    VERIFY1(SUCCEEDED(formatter.FormatMessage(&text,
                                              IDS_INSTALLER_OLD,
                                              kShortCompanyName)));
    return std::make_pair(GOOPDATE_E_SERVER_RESPONSE_UNSUPPORTED_PROTOCOL,
                          text);
  }

  VERIFY1(SUCCEEDED(formatter.FormatMessage(&text,
                                            IDS_NON_OK_RESPONSE_FROM_SERVER,
                                            status)));
  return std::make_pair(GOOPDATE_E_UNKNOWN_SERVER_RESPONSE, text);
}

bool IsOmahaUpdateAvailable(const xml::UpdateResponse* update_response) {
  ASSERT1(update_response);
  xml::UpdateResponseResult update_response_result(
      update_response_utils::GetResult(update_response,
                                       kGoogleUpdateAppId,
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

      ExperimentLabels labels;
      VERIFY1(SUCCEEDED(labels.ReadFromRegistry(is_machine, app.appid)));

      if (!labels.DeserializeAndApplyDelta(app.experiments)) {
        return E_FAIL;
      }

      HRESULT hr = labels.WriteToRegistry(is_machine, app.appid);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  return S_OK;
}

}  // namespace update_response_utils

}  // namespace omaha

