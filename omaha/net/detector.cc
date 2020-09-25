// Copyright 2007-2010 Google Inc.
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

// TODO(omaha): move EnclosePath and UnenclosePath functions from path.h to
//               string.h

#include "omaha/net/detector.h"

#include <memory>
#include "omaha/base/browser_utils.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/file_reader.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/base/time.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_config.h"
#include "omaha/goopdate/dm_messages.h"

namespace omaha {

namespace internal {

HRESULT IEProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  // Internet Explorer proxy configuration is not available when running as
  // local system.
  if (user_info::IsRunningAsSystem()) {
    return E_FAIL;
  }

  std::unique_ptr<HttpClient> http_client(CreateHttpClient());

  // We expect to be able to instantiate either of the http clients.
  ASSERT1(http_client.get());
  if (!http_client.get()) {
    return E_UNEXPECTED;
  }
  HRESULT hr = http_client->Initialize();
  if (FAILED(hr)) {
    return hr;
  }
  HttpClient::CurrentUserIEProxyConfig ie_proxy_config = {0};
  hr = http_client->GetIEProxyConfiguration(&ie_proxy_config);
  if (FAILED(hr)) {
    return hr;
  }
  config->source = source();
  config->auto_detect = ie_proxy_config.auto_detect;
  config->auto_config_url = ie_proxy_config.auto_config_url;
  config->proxy = ie_proxy_config.proxy;
  config->proxy_bypass = ie_proxy_config.proxy_bypass;

  // If IE is the default brower, promotes its proxy priority.
  BrowserType browser_type(BROWSER_UNKNOWN);
  if (SUCCEEDED(GetDefaultBrowserType(&browser_type)) &&
      browser_type == BROWSER_IE) {
    config->priority = ProxyConfig::PROXY_PRIORITY_DEFAULT_BROWSER;
  }

  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.auto_config_url));
  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.proxy));
  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.proxy_bypass));

  return S_OK;
}

}  // namespace internal

HRESULT RegistryOverrideProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);
  RegKey reg_key;
  HRESULT hr = reg_key.Open(reg_path_, KEY_READ);
  if (FAILED(hr)) {
    return hr;
  }
  CString proxy_host;
  hr = reg_key.GetValue(kRegValueProxyHost, &proxy_host);
  if (FAILED(hr)) {
    return hr;
  }
  DWORD proxy_port(0);
  hr = reg_key.GetValue(kRegValueProxyPort, &proxy_port);
  if (FAILED(hr)) {
    return hr;
  }
  *config = ProxyConfig();
  SafeCStringFormat(&config->proxy, _T("%s:%d"), proxy_host, proxy_port);
  config->source = source();
  config->priority = ProxyConfig::PROXY_PRIORITY_OVERRIDE;
  return S_OK;
}

UpdateDevProxyDetector::UpdateDevProxyDetector()
    : registry_detector_(MACHINE_REG_UPDATE_DEV) {
}

HRESULT PolicyProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  CString proxy_mode;
  HRESULT hr = GetProxyMode(&proxy_mode);
  if (FAILED(hr)) {
    return hr;
  }
  NET_LOG(L4, (_T("[%s][proxy mode][%s]"), source(), proxy_mode));

  *config = ProxyConfig();
  config->source = source();
  config->priority = ProxyConfig::PROXY_PRIORITY_OVERRIDE;

  if (proxy_mode.CompareNoCase(kProxyModeDirect) == 0) {
    return S_OK;  // Default-initialized ProxyConfig = direct connection.
  } else if (proxy_mode.CompareNoCase(kProxyModeAutoDetect) == 0) {
    config->auto_detect = true;
    return S_OK;
  } else if (proxy_mode.CompareNoCase(kProxyModePacScript) == 0) {
    return GetProxyPacUrl(&config->auto_config_url);
  } else if (proxy_mode.CompareNoCase(kProxyModeFixedServers) == 0) {
    return GetProxyServer(&config->proxy);
  } else if (proxy_mode.CompareNoCase(kProxyModeSystem) == 0) {
    // Fall through, and let the rest of the proxy detectors deal with it.
    return E_FAIL;
  } else {
    // Unrecognized ProxyMode string.
    return E_INVALIDARG;
  }
}

HRESULT PolicyProxyDetector::GetProxyMode(CString* proxy_mode) {
  return ConfigManager::Instance()->GetProxyMode(proxy_mode, NULL);
}

HRESULT PolicyProxyDetector::GetProxyPacUrl(CString* proxy_pac_url) {
  return ConfigManager::Instance()->GetProxyPacUrl(proxy_pac_url, NULL);
}

HRESULT PolicyProxyDetector::GetProxyServer(CString* proxy_server) {
  return ConfigManager::Instance()->GetProxyServer(proxy_server, NULL);
}

HRESULT DefaultProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  std::unique_ptr<HttpClient> http_client(CreateHttpClient());

  // We expect to be able to instantiate either of the http clients.
  ASSERT1(http_client.get());
  if (!http_client.get()) {
    return E_UNEXPECTED;
  }
  HRESULT hr = http_client->Initialize();
  if (FAILED(hr)) {
    return hr;
  }
  HttpClient::ProxyInfo proxy_info = {0};
  hr = http_client->GetDefaultProxyConfiguration(&proxy_info);
  if (FAILED(hr)) {
    return hr;
  }
  if (proxy_info.access_type == WINHTTP_ACCESS_TYPE_NAMED_PROXY) {
    ProxyConfig proxy_config;
    proxy_config.source = source();
    proxy_config.proxy = proxy_info.proxy;
    proxy_config.proxy_bypass = proxy_info.proxy_bypass;
    *config = proxy_config;
    return S_OK;
  } else {
    return E_FAIL;
  }
}

HRESULT IEWPADProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  ProxyConfig ie_proxy_config;
  HRESULT hr = internal::IEProxyDetector::Detect(&ie_proxy_config);
  if (FAILED(hr)) {
    return hr;
  }

  if (!ie_proxy_config.auto_detect) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  config->source = ie_proxy_config.source;
  config->auto_detect = ie_proxy_config.auto_detect;
  config->priority = ie_proxy_config.priority;
  return S_OK;
}

HRESULT IEPACProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  ProxyConfig ie_proxy_config;
  HRESULT hr = internal::IEProxyDetector::Detect(&ie_proxy_config);
  if (FAILED(hr)) {
    return hr;
  }

  if (ie_proxy_config.auto_config_url.IsEmpty()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  config->source = ie_proxy_config.source;
  config->auto_config_url = ie_proxy_config.auto_config_url;
  config->priority = ie_proxy_config.priority;
  return S_OK;
}

HRESULT IENamedProxyDetector::Detect(ProxyConfig* config) {
  ASSERT1(config);

  ProxyConfig ie_proxy_config;
  HRESULT hr = internal::IEProxyDetector::Detect(&ie_proxy_config);
  if (FAILED(hr)) {
    return hr;
  }

  if (ie_proxy_config.proxy.IsEmpty()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  config->source = ie_proxy_config.source;
  config->proxy = ie_proxy_config.proxy;
  config->proxy_bypass = ie_proxy_config.proxy_bypass;
  config->priority = ie_proxy_config.priority;
  return S_OK;
}

}  // namespace omaha

