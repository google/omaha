// Copyright 2011 Google Inc.
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

#include "omaha/client/bundle_creator.h"
#include <atlsafe.h>
#include <algorithm>
#include <vector>
#include "omaha/base/cgi.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/vista_utils.h"
#include "omaha/client/client_utils.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/command_line.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/experiment_labels.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"
#include "omaha/common/update3_utils.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

namespace bundle_creator {

namespace internal {

bool IsNotPrintable(TCHAR c) {
  return c < 32 || c >= 127;
}

HRESULT UnescapeAndStoreUntrustedData(const CString& untrusted_data_escaped,
                                      IApp2* app) {
  int escaped_len = untrusted_data_escaped.GetLength();
  ASSERT1(escaped_len > 0);

  // Example: Converts from
  //   key1%3dvalue1%26key2%3dhas%2520space
  // to
  //   key=value1&key2=has%20space
  // Decoded untrusted data string consists of key-value pairs in URL encoding.
  std::vector<TCHAR> buf(escaped_len + 2, _T('\0'));  // Added sentinel '\0'.
  CGI::UnescapeString(static_cast<const TCHAR*>(untrusted_data_escaped),
                      escaped_len,
                      &buf.front(),
                      escaped_len + 1);  // Includes terminating '\0'.
  // Note: A string containing "%00" would terminate early. We let this go.
  size_t size = _tcslen(&buf.front());
  if (size > kUntrustedDataMaxLength) {
    CORE_LOG(LE, (_T("[Untrusted data too long]")));
    return GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS;
  }

  if (std::find_if(buf.begin(), buf.begin() + size, IsNotPrintable)
        != buf.begin() + size) {  // Excluding terminating '\0'.
    CORE_LOG(LE, (_T("[Untrusted data have invalid characters]")));
    return GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS;
  }

  return app->put_untrustedData(CComBSTR(CString(&buf.front()).GetString()));
}

// display_language and install_source can be empty.
HRESULT SetBundleProperties(const CString& display_language,
                            const CString& display_name,
                            const CString& install_source,
                            const CString& session_id,
                            bool send_pings,
                            IAppBundle* app_bundle) {
  ASSERT1(!display_name.IsEmpty());
  ASSERT1(app_bundle);

  CString process_language = lang::GetLanguageForProcess(display_language);
  HRESULT hr = app_bundle->put_displayLanguage(CComBSTR(process_language));
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle->put_displayName(CComBSTR(display_name));
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle->put_sessionId(CComBSTR(session_id));
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle->put_sendPings(send_pings ? VARIANT_TRUE : VARIANT_FALSE);
  if (FAILED(hr)) {
    return hr;
  }

  if (!install_source.IsEmpty()) {
    hr = app_bundle->put_installSource(CComBSTR(install_source));
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

// Do not use the apps member of extra_args. Those values are handled by
// PopulateAppSpecificData.
HRESULT PopulateCommonData(const CommandLineExtraArgs& extra_args,
                           bool is_eula_accepted,
                           IApp* app) {
  ASSERT1(app);

  HRESULT hr = S_OK;

  // Set as soon as possible so pings can occur in error cases.
  hr = app->put_isEulaAccepted(is_eula_accepted ? VARIANT_TRUE : VARIANT_FALSE);
  if (FAILED(hr)) {
    return hr;
  }

  if (!extra_args.language.IsEmpty()) {
    hr = app->put_language(CComBSTR(extra_args.language));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!IsEqualGUID(GUID_NULL, extra_args.installation_id)) {
    hr = app->put_iid(CComBSTR(GuidToString(extra_args.installation_id)));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!extra_args.brand_code.IsEmpty()) {
    hr = app->put_brandCode(CComBSTR(extra_args.brand_code));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!extra_args.client_id.IsEmpty()) {
    hr = app->put_clientId(CComBSTR(extra_args.client_id));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!extra_args.referral_id.IsEmpty()) {
    hr = app->put_referralId(CComBSTR(extra_args.referral_id));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (extra_args.browser_type != BROWSER_UNKNOWN) {
    hr = app->put_browserType(extra_args.browser_type);
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = app->put_usageStatsEnable(extra_args.usage_stats_enable);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT PopulateAppSpecificData(const CommandLineAppArgs& app_args,
                                IApp* app) {
  ASSERT1(app);

  HRESULT hr = app->put_displayName(CComBSTR(app_args.app_name));
  if (FAILED(hr)) {
    return hr;
  }

  if (!app_args.ap.IsEmpty()) {
    hr = app->put_ap(CComBSTR(app_args.ap));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app_args.tt_token.IsEmpty()) {
    hr = app->put_ttToken(CComBSTR(app_args.tt_token));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app_args.encoded_installer_data.IsEmpty()) {
    CString decoded_installer_data;
    hr = Utf8UrlEncodedStringToWideString(app_args.encoded_installer_data,
                                          &decoded_installer_data);
    ASSERT(SUCCEEDED(hr), (_T("[Utf8UrlEncodedStringToWideString][0x%x]"), hr));
    if (FAILED(hr) || CString(decoded_installer_data).Trim().IsEmpty()) {
      return GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS;
    }

    hr = app->put_clientInstallData(CComBSTR(decoded_installer_data));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app_args.install_data_index.IsEmpty()) {
    hr = app->put_serverInstallDataIndex(CComBSTR(app_args.install_data_index));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app_args.experiment_labels.IsEmpty()) {
    hr = app->put_labels(CComBSTR(app_args.experiment_labels));
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[put_labels() failed][%s][%#x]"),
                    app_args.experiment_labels, hr));
      return hr;
    }
  }

  if (!app_args.untrusted_data.IsEmpty()) {
    CComQIPtr<IApp2> app_2(app);
    if (!app_2) {
      CORE_LOG(LE, (_T("[IApp could not be cast to IApp2]")));
      return E_NOINTERFACE;
    }

    hr = UnescapeAndStoreUntrustedData(app_args.untrusted_data, app_2);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

// Obtains tokens and passes them to put_altTokens when running as Local System.
// Does not do anything for per-user instances or when not run as Local System.
HRESULT SetAltTokens(bool is_machine, IAppBundle* app_bundle) {
  ASSERT1(app_bundle);

  if (!is_machine) {
    return S_OK;
  }

  bool is_local_system = false;
  HRESULT hr = IsSystemProcess(&is_local_system);
  if (FAILED(hr)) {
    return hr;
  }

  if (!is_local_system) {
    // Do not need alternate tokens.
    return S_OK;
  }

  CAccessToken primary_token;
  hr = primary_token.GetProcessToken(TOKEN_ALL_ACCESS) ?
           S_OK : HRESULTFromLastError();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetProcessToken failed][0x%x]"), hr));
    return hr;
  }

  CAccessToken impersonation_token;
  HANDLE user_token = NULL;

  vista::GetLoggedOnUserToken(&user_token);

  if (user_token) {
    impersonation_token.Attach(user_token);
  } else if (!primary_token.CreateImpersonationToken(&impersonation_token)) {
    hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[CreateImpersonationToken failed][0x%x]"), hr));
    return hr;
  }

  hr = app_bundle->put_altTokens(
      reinterpret_cast<ULONG_PTR>(impersonation_token.GetHandle()),
      reinterpret_cast<ULONG_PTR>(primary_token.GetHandle()),
      ::GetCurrentProcessId());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[put_altTokens failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

}  // namespace internal

HRESULT Create(bool is_machine,
               const CString& display_language,
               const CString& install_source,
               const CString& session_id,
               bool is_interactive,
               bool send_pings,
               IAppBundle** app_bundle) {
  CORE_LOG(L2, (_T("[bundle_creator::Create]")));
  ASSERT1(app_bundle);

  CComPtr<IGoogleUpdate3> server;
  HRESULT hr = update3_utils::CreateGoogleUpdate3Class(is_machine, &server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateGoogleUpdate3Class][0x%08x]"), hr));
    return hr;
  }

  CComPtr<IAppBundle> app_bundle_ptr;
  hr = update3_utils::CreateAppBundle(server, &app_bundle_ptr);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAppBundle failed][0x%08x]"), hr));
    return hr;
  }

  hr = internal::SetBundleProperties(display_language,
                                     client_utils::GetUpdateAllAppsBundleName(),
                                     install_source,
                                     session_id,
                                     send_pings,
                                     app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = internal::SetAltTokens(is_machine, app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->put_priority(is_interactive ?
                                        INSTALL_PRIORITY_HIGH :
                                        INSTALL_PRIORITY_LOW);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->initialize();
  if (FAILED(hr)) {
    return hr;
  }

  *app_bundle = app_bundle_ptr.Detach();
  return S_OK;
}

HRESULT CreateFromCommandLine(bool is_machine,
                              bool is_eula_accepted,
                              bool is_offline,
                              const CString& offline_dir_name,
                              const CommandLineExtraArgs& extra_args,
                              const CString& install_source,
                              const CString& session_id,
                              bool is_interactive,
                              bool send_pings,
                              IAppBundle** app_bundle) {
  CORE_LOG(L2, (_T("[bundle_creator::CreateFromCommandLine]")));
  ASSERT1(app_bundle);
  UNREFERENCED_PARAMETER(is_interactive);

  CComPtr<IGoogleUpdate3> server;
  HRESULT hr = update3_utils::CreateGoogleUpdate3Class(is_machine, &server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateGoogleUpdate3Class][0x%08x]"), hr));
    return hr;
  }

  CComPtr<IAppBundle> app_bundle_ptr;
  hr = update3_utils::CreateAppBundle(server, &app_bundle_ptr);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAppBundle failed][0x%08x]"), hr));
    return hr;
  }

  hr = internal::SetBundleProperties(extra_args.language,
                                     extra_args.bundle_name,
                                     install_source,
                                     session_id,
                                     send_pings,
                                     app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  if (is_offline) {
    CString parent_offline_dir(
        is_machine ?
        ConfigManager::Instance()->GetMachineSecureOfflineStorageDir() :
        ConfigManager::Instance()->GetUserOfflineStorageDir());
    CString offline_dir(ConcatenatePath(parent_offline_dir, offline_dir_name));

    hr = app_bundle_ptr->put_offlineDirectory(CComBSTR(offline_dir));
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = internal::SetAltTokens(is_machine, app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  // CreateFromCommandLine is the primary install use-case and always runs at
  // high priority. Even if the install is silent. Most install use-cases
  // require the install to complete quickly. For instance, Avast launching the
  // Omaha metainstaller with /silent while showing their own UI.
  hr = app_bundle_ptr->put_priority(INSTALL_PRIORITY_HIGH);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->initialize();
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i < extra_args.apps.size(); ++i) {
    const CommandLineAppArgs& app_args(extra_args.apps[i]);

    const CComBSTR app_id(GuidToString(app_args.app_guid));

    CComPtr<IApp> app;
    hr = update3_utils::CreateApp(app_id, app_bundle_ptr, &app);
    if (FAILED(hr)) {
      return hr;
    }

    hr = internal::PopulateCommonData(extra_args, is_eula_accepted, app);
    if (FAILED(hr)) {
      return hr;
    }

    hr = internal::PopulateAppSpecificData(app_args, app);
    if (FAILED(hr)) {
      return hr;
    }
  }

  *app_bundle = app_bundle_ptr.Detach();
  return S_OK;
}

HRESULT CreateForceInstallBundle(bool is_machine,
                                 const CString& display_language,
                                 const CString& install_source,
                                 const CString& session_id,
                                 bool is_interactive,
                                 bool send_pings,
                                 IAppBundle** app_bundle) {
  CORE_LOG(L2, (_T("[bundle_creator::CreateForceInstallBundle]")));
  ASSERT1(app_bundle);

  std::vector<CString> force_install_app_ids;
  HRESULT hr = ConfigManager::Instance()->GetForceInstallApps(
      is_machine,
      &force_install_app_ids,
      NULL);
  if (FAILED(hr)) {
    return S_FALSE;
  }

  std::vector<CString> app_ids;
  for (const auto& app_id : force_install_app_ids) {
    const CString app_id_key =
          app_registry_utils::GetAppClientsKey(is_machine, app_id);

    if (!RegKey::HasKey(app_id_key)) {
      app_ids.push_back(app_id);
    }
  }

  if (app_ids.empty()) {
    return S_FALSE;
  }

  CComPtr<IGoogleUpdate3> server;
  hr = update3_utils::CreateGoogleUpdate3Class(is_machine, &server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateGoogleUpdate3Class][%#x]"), hr));
    return hr;
  }

  CComPtr<IAppBundle> app_bundle_ptr;
  hr = update3_utils::CreateAppBundle(server, &app_bundle_ptr);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAppBundle failed][%#x]"), hr));
    return hr;
  }

  hr = internal::SetBundleProperties(display_language,
                                     client_utils::GetDefaultBundleName(),
                                     install_source,
                                     session_id,
                                     send_pings,
                                     app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = internal::SetAltTokens(is_machine, app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->put_priority(is_interactive ?
                                        INSTALL_PRIORITY_HIGH :
                                        INSTALL_PRIORITY_LOW);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->initialize();
  if (FAILED(hr)) {
    return hr;
  }

  for (const auto& app_id_string : app_ids) {
    CComBSTR app_id(app_id_string);
    CComPtr<IApp> app;
    hr = update3_utils::CreateApp(app_id, app_bundle_ptr, &app);
    if (FAILED(hr)) {
      return hr;
    }

    hr = app->put_displayName(
        CComBSTR(client_utils::GetDefaultApplicationName()));
    if (FAILED(hr)) {
      return hr;
    }

    const bool is_eula_accepted =
          app_registry_utils::IsAppEulaAccepted(is_machine,
                                                app_id_string,
                                                false);
    hr = app->put_isEulaAccepted(is_eula_accepted ? VARIANT_TRUE :
                                                    VARIANT_FALSE);
    if (FAILED(hr)) {
      return hr;
    }
  }

  *app_bundle = app_bundle_ptr.Detach();
  return S_OK;
}

HRESULT CreateForOnDemand(bool is_machine,
                          const CString& app_id,
                          const CString& install_source,
                          const CString& session_id,
                          HANDLE impersonation_token,
                          HANDLE primary_token,
                          IAppBundle** app_bundle) {
  CORE_LOG(L2, (_T("[bundle_creator::CreateForOnDemand]")));
  ASSERT1(app_bundle);

  CComPtr<IGoogleUpdate3> server;
  HRESULT hr = update3_utils::CreateGoogleUpdate3Class(is_machine, &server);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateGoogleUpdate3Class failed][0x%x]"), hr));
    return hr;
  }

  CComPtr<IAppBundle> app_bundle_ptr;
  hr = update3_utils::CreateAppBundle(server, &app_bundle_ptr);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateAppBundle failed][0x%x]"), hr));
    return hr;
  }

  // ::CoSetProxyBlanket() settings are per proxy. For OnDemand, after
  // unmarshaling the interface, we need to set the blanket on this new proxy.
  // The proxy blanket on the IAppBundle interface are set explicitly only for
  // OnDemand, because OnDemand is a unique case of being a COM server as well
  // as a COM client. The default security settings set for the OnDemand COM
  // server are more restrictive and rightly so, as compared to the settings
  // that we set for a COM client such as our Omaha3 UI. Hence the need to
  // explicitly set the proxy blanket settings and lower the security
  // requirements only when calling out on this interface.
  hr = update3_utils::SetProxyBlanketAllowImpersonate(app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  if (is_machine) {
    hr = app_bundle_ptr->put_altTokens(
        reinterpret_cast<ULONG_PTR>(impersonation_token),
        reinterpret_cast<ULONG_PTR>(primary_token),
        ::GetCurrentProcessId());
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[put_altTokens failed][0x%x]"), hr));
      return hr;
    }
  }

  CString lang;
  CString name;
  app_registry_utils::GetAppLang(is_machine, app_id, &lang);
  app_registry_utils::GetAppName(is_machine, app_id, &name);
  const bool send_pings = true;

  hr = internal::SetBundleProperties(
      lang,
      name.IsEmpty() ? CString(_T("On Demand Bundle")) : name,
      install_source,
      session_id,
      send_pings,
      app_bundle_ptr);
  if (FAILED(hr)) {
    return hr;
  }

  hr = app_bundle_ptr->initialize();
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IApp> app;
  hr = update3_utils::CreateInstalledApp(CComBSTR(app_id),
                                         app_bundle_ptr,
                                         &app);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateInstalledApp failed][0x%x]"), hr));
    return hr;
  }

  *app_bundle = app_bundle_ptr.Detach();
  return S_OK;
}

}  // namespace bundle_creator

}  // namespace omaha
