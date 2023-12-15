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

#include "omaha/net/network_config.h"

#include <versionhelpers.h>
#include <winhttp.h>
#include <atlconv.h>
#include <atlsecurity.h>
#include <algorithm>
#include <memory>
#include <unordered_set>  // NOLINT
#include <vector>

#include "base/error.h"
#include "base/scope_guard.h"
#include "omaha/base/browser_utils.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/encrypt.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/system_info.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/net/http_client.h"
#include "omaha/net/winhttp.h"

using omaha::encrypt::EncryptData;
using omaha::encrypt::DecryptData;

namespace omaha {

// Computes the hash value of a ProxyConfig object.
size_t hash_value(const ProxyConfig& config) {
  size_t hash = std::hash<bool>{}(config.auto_detect)                 ^
                std::hash<std::wstring>{}(config.auto_config_url.GetString()) ^
                std::hash<std::wstring>{}(config.proxy.GetString())           ^
                std::hash<std::wstring>{}(config.proxy_bypass.GetString());
  return hash;
}

const TCHAR* const NetworkConfig::kUserAgent = _T("Google Update/%s");

const TCHAR* const NetworkConfig::kRegKeyProxy = GOOPDATE_MAIN_KEY _T("proxy");
const TCHAR* const NetworkConfig::kRegValueSource = _T("source");

const TCHAR* const NetworkConfig::kWPADIdentifier = _T("auto");
const TCHAR* const NetworkConfig::kDirectConnectionIdentifier = _T("direct");

NetworkConfig::NetworkConfig(bool is_machine)
    : is_machine_(is_machine),
      is_initialized_(false) {}

NetworkConfig::~NetworkConfig() {
  if (session_.session_handle && http_client_.get()) {
    http_client_->Close(session_.session_handle);
    session_.session_handle = NULL;
  }
  Clear();
}

HRESULT NetworkConfig::Initialize() {
  if (is_initialized_) {
    NET_LOG(L3, (_T("[NetworkConfig::Initialize][already initialized]")));
    return S_OK;
  }

  http_client_.reset(CreateHttpClient());
  ASSERT1(http_client_.get());
  if (!http_client_.get()) {
    NET_LOG(LE, (_T("[CreateHttpClient failed]")));
    return E_UNEXPECTED;
  }
  HRESULT hr = http_client_->Initialize();
  if (FAILED(hr)) {
    // TODO(omaha): This makes an assumption that only WinHttp is
    // supported by the network code.
    NET_LOG(LE, (_T("[http_client_->Initialize() failed][0x%x]"), hr));
    return OMAHA_NET_E_WINHTTP_NOT_AVAILABLE;
  }

  // Initializes the WinHttp session and configures WinHttp to work in
  // asynchronous mode. In this mode, the network requests are non-blocking
  // and asynchronous events are generated when a request is complete.
  hr = http_client_->Open(NULL,
                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS,
                          WINHTTP_FLAG_ASYNC,
                          &session_.session_handle);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[http_client_->Open() failed][0x%x]"), hr));
    return hr;
  }

  // Allow TLS1.2 on Windows 7 and Windows 8. See KB3140245.
  // TLS 1.2 is enabled by default on Windows 8.1 and Windows 10.
  if (::IsWindows7OrGreater() && !::IsWindows8Point1OrGreater()) {
    constexpr int kSecureProtocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1 |
                                     WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 |
                                     WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
    http_client_->SetOptionInt(session_.session_handle,
                               WINHTTP_OPTION_SECURE_PROTOCOLS,
                               kSecureProtocols);
  }

  Add(new UpdateDevProxyDetector);
  Add(new PolicyProxyDetector);
  Add(new IEWPADProxyDetector);
  Add(new IEPACProxyDetector);
  Add(new IENamedProxyDetector);
  Add(new DefaultProxyDetector);

  // Use a global network configuration override if available.
  ConfigManager* config_manager = ConfigManager::Instance();
  CString net_config;
  if (SUCCEEDED(config_manager->GetNetConfig(&net_config))) {
    ProxyConfig config_override = NetworkConfig::ParseNetConfig(net_config);
    SetConfigurationOverride(&config_override);
  }

  ConfigureProxyAuth();

  is_initialized_ = true;
  return S_OK;
}

void NetworkConfig::Add(ProxyDetectorInterface* detector) {
  ASSERT1(detector);
  __mutexBlock(lock_) {
    detectors_.push_back(detector);
  }
}

void NetworkConfig::Clear() {
  __mutexBlock(lock_) {
    for (size_t i = 0; i != detectors_.size(); ++i) {
      delete detectors_[i];
    }
    detectors_.clear();
    configurations_.clear();
  }
}

HRESULT NetworkConfig::Detect() {
  __mutexBlock(lock_) {
    std::vector<ProxyConfig> configurations;

    for (size_t i = 0; i != detectors_.size(); ++i) {
      ProxyConfig config;
      if (SUCCEEDED(detectors_[i]->Detect(&config))) {
        configurations.push_back(config);
      }
    }
    configurations_.swap(configurations);
  }

  return S_OK;
}

void NetworkConfig::SortProxies(std::vector<ProxyConfig>* configurations) {
  ASSERT1(configurations);

  std::stable_sort(configurations->begin(), configurations->end(),
                   ProxySortPredicate);
}

HRESULT NetworkConfig::ConfigFromIdentifier(const CString& id,
                                            ProxyConfig* config) {
  ASSERT1(config);

  *config = ProxyConfig();
  if (id == kWPADIdentifier) {
    config->source = kWPADIdentifier;
    config->auto_detect = true;
  } else if (id == kDirectConnectionIdentifier) {
    config->source = kDirectConnectionIdentifier;
  } else {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  return S_OK;
}

void NetworkConfig::AppendLastKnownGoodProxyConfig(
    std::vector<ProxyConfig>* configurations) const {
  ASSERT1(configurations);
  ProxyConfig last_known_good_config;
  if (SUCCEEDED(LoadProxyConfig(&last_known_good_config))) {
    configurations->push_back(last_known_good_config);
  }
}

void NetworkConfig::AppendStaticProxyConfigs(
    std::vector<ProxyConfig>* configurations) {
  ASSERT1(configurations);
  ProxyConfig config;

  HRESULT hr = ConfigFromIdentifier(kWPADIdentifier, &config);
  if (SUCCEEDED(hr)) {
    configurations->push_back(config);
  }

  hr = ConfigFromIdentifier(kDirectConnectionIdentifier, &config);
  if (SUCCEEDED(hr)) {
    configurations->push_back(config);
  }
}

HRESULT NetworkConfig::Detect(const CString& proxy_source,
                              ProxyConfig* config) const {
  ASSERT1(config);
  __mutexBlock(lock_) {
    std::vector<ProxyConfig> configurations;
    for (size_t i = 0; i != detectors_.size(); ++i) {
      if (proxy_source == detectors_[i]->source()) {
        return detectors_[i]->Detect(config);
      }
    }
  }

  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

std::vector<ProxyConfig> NetworkConfig::GetConfigurations() const {
  std::vector<ProxyConfig> configurations;
  __mutexBlock(lock_) {
    configurations = configurations_;
  }
  return configurations;
}

HRESULT NetworkConfig::GetConfigurationOverride(ProxyConfig* config) {
  ASSERT1(config);
  __mutexBlock(lock_) {
    if (configuration_override_.get()) {
      *config = *configuration_override_;
      return S_OK;
    }
  }
  return E_FAIL;
}

void NetworkConfig::SetConfigurationOverride(
    const ProxyConfig* configuration_override) {
  __mutexBlock(lock_) {
    if (configuration_override) {
      configuration_override_.reset(new ProxyConfig);
      *configuration_override_ = *configuration_override;
      configuration_override_->source = _T("updatedev/netconfig");
    } else {
      configuration_override_.reset();
    }
  }
}

// Serializes configurations for debugging purposes.
CString NetworkConfig::ToString(const ProxyConfig& config) {
  CString result;
  SafeCStringFormat(&result, _T("priority=%d, source=%s, "),
                    config.priority, config.source);

  switch (GetAccessType(config)) {
    case WINHTTP_ACCESS_TYPE_NO_PROXY:
      SafeCStringAppendFormat(&result, _T("direct connection"));
      break;
    case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
      SafeCStringAppendFormat(&result, _T("named proxy=%s, bypass=%s"),
                              config.proxy, config.proxy_bypass);
      break;
    case WINHTTP_ACCESS_TYPE_AUTO_DETECT:
      SafeCStringAppendFormat(&result, _T("wpad=%d, script=%s"),
                              config.auto_detect, config.auto_config_url);
      break;
    default:
      ASSERT1(false);
      break;
  }
  return result;
}

CString NetworkConfig::ToString(const std::vector<ProxyConfig>& config) {
  CString result;
  for (size_t i = 0; i != config.size(); ++i) {
    result.Append(NetworkConfig::ToString(config[i]));
    result.Append(_T("\r\n"));
  }
  return result;
}

int NetworkConfig::GetAccessType(const ProxyConfig& config) {
  if (config.auto_detect || !config.auto_config_url.IsEmpty()) {
    return WINHTTP_ACCESS_TYPE_AUTO_DETECT;
  } else if (!config.proxy.IsEmpty()) {
    return WINHTTP_ACCESS_TYPE_NAMED_PROXY;
  } else {
    return WINHTTP_ACCESS_TYPE_NO_PROXY;
  }
}

bool NetworkConfig::IsUsingCupTestKeys() {
  DWORD value = 0;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueCupKeys,
                                 &value))) {
    return value != 0;
  } else {
    return false;
  }
}

void NetworkConfig::ConfigureProxyAuth() {
  const uint32 kProxyMaxPrompts = 1;
  return proxy_auth_.ConfigureProxyAuth(is_machine_, kProxyMaxPrompts);
}

bool NetworkConfig::GetProxyCredentials(bool allow_ui,
                                        bool force_ui,
                                        const CString& proxy_settings,
                                        const ProxyAuthConfig& config,
                                        bool is_https,
                                        CString* username,
                                        CString* password,
                                        uint32* auth_scheme) {
  ASSERT1(username);
  ASSERT1(password);
  ASSERT1(auth_scheme);

  const CString& proxy = ProxyAuth::ExtractProxy(proxy_settings, is_https);
  return proxy_auth_.GetProxyCredentials(allow_ui, force_ui, proxy,
                                         config, username,
                                         password, auth_scheme);
}

HRESULT NetworkConfig::SetProxyAuthScheme(const CString& proxy_settings,
                                          bool is_https,
                                          uint32 auth_scheme) {
  ASSERT1(auth_scheme != UNKNOWN_AUTH_SCHEME);
  const CString& proxy = ProxyAuth::ExtractProxy(proxy_settings, is_https);
  return proxy_auth_.SetProxyAuthScheme(proxy, auth_scheme);
}

HRESULT NetworkConfig::GetProxyForUrl(const CString& url,
                                      bool use_wpad,
                                      const CString& auto_config_url,
                                      HttpClient::ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  NET_LOG(L3, (_T("[NetworkConfig::GetProxyForUrl][%s]"), url));

  HRESULT hr = E_FAIL;

  if (use_wpad) {
    hr = GetWPADProxyForUrl(url, proxy_info);
  }

  if (FAILED(hr) && !auto_config_url.IsEmpty()) {
    hr = GetPACProxyForUrl(url, auto_config_url, proxy_info);
  }

  return hr;
}

HRESULT NetworkConfig::GetWPADProxyForUrl(const CString& url,
                                          HttpClient::ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  HttpClient::AutoProxyOptions auto_proxy_options = {0};
  auto_proxy_options.auto_logon_if_challenged = true;
  auto_proxy_options.flags = WINHTTP_AUTOPROXY_AUTO_DETECT;
  auto_proxy_options.auto_config_url = NULL;
  auto_proxy_options.auto_detect_flags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                                         WINHTTP_AUTO_DETECT_TYPE_DNS_A;

  return http_client_->GetProxyForUrl(session_.session_handle,
                                      url,
                                      &auto_proxy_options,
                                      proxy_info);
}

HRESULT NetworkConfig::GetPACProxyForUrl(const CString& url,
                                         const CString& auto_config_url,
                                         HttpClient::ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  if (auto_config_url.IsEmpty()) {
    return E_INVALIDARG;
  }

  HttpClient::AutoProxyOptions auto_proxy_options = {0};
  auto_proxy_options.auto_logon_if_challenged = true;
  auto_proxy_options.flags = WINHTTP_AUTOPROXY_CONFIG_URL;
  auto_proxy_options.auto_config_url = auto_config_url;

  HRESULT hr = http_client_->GetProxyForUrl(session_.session_handle,
                                            url,
                                            &auto_proxy_options,
                                            proxy_info);

  if (FAILED(hr) && ::UrlIsFileUrl(auto_config_url)) {
    // Some HttpClient implementations, namely WinHTTP, only support PAC files
    // with http or https schemes.  If the scheme is file and the initial
    // attempt fails, make an attempt at resolving using jsproxy.dll.

    CString local_file;
    hr = ConvertFileUriToLocalPath(auto_config_url, &local_file);
    if (FAILED(hr)) {
      NET_LOG(LE, (_T("[ConvertFileUriToLocalPath failed][0x%08x]"), hr));
      return hr;
    }

    hr = GetProxyForUrlLocal(url, local_file, proxy_info);
  }

  return hr;
}

CString NetworkConfig::GetUserAgent() {
  CString user_agent;
  SafeCStringFormat(&user_agent, kUserAgent, GetVersionString());
  return user_agent;
}

CString NetworkConfig::GetMID() {
  CString mid;
  RegKey::GetValue(MACHINE_REG_UPDATE_DEV, kRegValueMID, &mid);
  return mid;
}

// Using std::hash_set adds about 2K uncompressed code size. Using a CAtlMap
// adds about 1.5K. Usually, there are only five detected configurations so
// an O(n^2) algorithm would work well. The advantage of the current
// implementation is simplicity. It also does not handle conflicts. Conflicts
// are not expected, due to how the ProxyConfig structure is being used.
// TODO(omaha): consider not using the hash_set and save about 1K of code.
void NetworkConfig::RemoveDuplicates(std::vector<ProxyConfig>* config) {
  ASSERT1(config);

  // Iterate over the input configurations, remember the hash of each
  // distinct configuration, and remove the duplicates by skipping the
  // configurations seen before.
  std::vector<ProxyConfig> input(*config);
  config->clear();

  typedef std::unordered_set<size_t> Keys;
  Keys keys;
  for (size_t i = 0; i != input.size(); ++i) {
    const size_t hash = hash_value(input[i]);
    std::pair<Keys::iterator, bool> result(keys.insert(hash));
    if (result.second) {
      config->push_back(input[i]);
    }
  }
}

HRESULT NetworkConfig::CreateProxyConfigRegKey(RegKey* key) {
  ASSERT1(key);
  CString config_root;

  if (!user_info::IsRunningAsSystem()) {
    scoped_hkey user_root_key;
    HRESULT hr = ::RegOpenCurrentUser(KEY_READ | KEY_WRITE,
                                      address(user_root_key));
    if (FAILED(hr)) {
        return hr;
    }
    return key->Create(get(user_root_key), kRegKeyProxy);
  } else {
    return key->Create(HKEY_LOCAL_MACHINE, kRegKeyProxy);
  }
}

HRESULT NetworkConfig::SaveProxyConfig(const ProxyConfig& config) {
  const CString& new_configuration = config.source;
  NET_LOG(L3, (_T("[NetworkConfig::SaveProxyConfig][%s]"), new_configuration));

  RegKey key;
  HRESULT hr = CreateProxyConfigRegKey(&key);
  if (FAILED(hr)) {
    return hr;
  }

  CString current_configuration;
  if (SUCCEEDED(key.GetValue(kRegValueSource, &current_configuration)) &&
      current_configuration != new_configuration) {
    NET_LOG(L3, (_T("[Network configuration changed from %s to %s"),
        current_configuration, new_configuration));
  }

  return key.SetValue(kRegValueSource, new_configuration);
}

HRESULT NetworkConfig::LoadProxyConfig(ProxyConfig* config) const {
  ASSERT1(config);

  *config = ProxyConfig();

  RegKey key;
  HRESULT hr = CreateProxyConfigRegKey(&key);
  if (FAILED(hr)) {
    return hr;
  }

  CString source;
  hr = key.GetValue(kRegValueSource, &source);
  if (FAILED(hr)) {
    return hr;
  }

  hr = NetworkConfig::ConfigFromIdentifier(source, config);
  if (FAILED(hr)) {
    hr = Detect(source, config);
    if (FAILED(hr)) {
      return hr;
    }
  }

  config->priority = ProxyConfig::PROXY_PRIORITY_LAST_KNOWN_GOOD;

  return S_OK;
}

ProxyConfig NetworkConfig::ParseNetConfig(const CString& net_config) {
  ProxyConfig config;
  int pos(0);
  CString token = net_config.Tokenize(_T(";"), pos);
  while (pos != -1) {
    CString name, value;
    if (ParseNameValuePair(token, _T('='), &name, &value)) {
      bool auto_detect(false);
      if (name == _T("wpad") &&
          SUCCEEDED(String_StringToBool(value, &auto_detect))) {
        config.auto_detect = auto_detect;
      } else if (name == _T("script")) {
        config.auto_config_url = value;
      } else if (name == _T("proxy")) {
        config.proxy = value;
      }
    }
    token = net_config.Tokenize(_T(";"), pos);
  }
  return config;
}

// Note: The jsproxy functions are exposed to public users as part of the
// DOJ consent decree and are not formally supported by Microsoft.

GPA_WRAP(jsproxy.dll,
         InternetInitializeAutoProxyDll,
         (DWORD dwVersion, LPSTR lpszDownloadedTempFile, LPSTR lpszMime, LPCVOID lpAutoProxyCallbacks, LPCVOID lpAutoProxyScriptBuffer),  // NOLINT
         (dwVersion, lpszDownloadedTempFile, lpszMime, lpAutoProxyCallbacks, lpAutoProxyScriptBuffer),  // NOLINT
         WINAPI,
         BOOL,
         FALSE);

GPA_WRAP(jsproxy.dll,
         InternetGetProxyInfo,
         (LPCSTR lpszUrl, DWORD dwUrlLength, LPSTR lpszUrlHostName, DWORD dwUrlHostNameLength, LPSTR *lplpszProxyHostName, LPDWORD lpdwProxyHostNameLength),  // NOLINT
         (lpszUrl, dwUrlLength, lpszUrlHostName, dwUrlHostNameLength, lplpszProxyHostName, lpdwProxyHostNameLength),  // NOLINT
         WINAPI,
         BOOL,
         FALSE);

GPA_WRAP(jsproxy.dll,
         InternetDeInitializeAutoProxyDll,
         (LPSTR lpszMime, DWORD dwReserved),
         (lpszMime, dwReserved),
         WINAPI,
         BOOL,
         FALSE);

HRESULT NetworkConfig::GetProxyForUrlLocal(const CString& url,
                                           const CString& path_to_pac_file,
                                           HttpClient::ProxyInfo* proxy_info) {
  scoped_library jsproxy_lib(LoadSystemLibrary(_T("jsproxy.dll")));
  ASSERT1(jsproxy_lib);
  if (!jsproxy_lib) {
    HRESULT hr = HRESULTFromLastError();
    NET_LOG(LE, (_T("[GetProxyForUrlLocal][jsproxy not loaded][0x%08x]"), hr));
    return hr;
  }

  // Convert the inputs to ANSI, and call into JSProxy to execute the PAC
  // script; we should get back out a PAC-format list of proxies to use.
  //
  // TODO(omaha3): The MSDN prototypes specify LPSTR, and I've assumed this
  // implies CP_ACP.  However, depending on how this was implemented internally,
  // conversion to UTF8 might work better.  Investigate this later and confirm.
  CStringA path_a(path_to_pac_file);

  if (FALSE == InternetInitializeAutoProxyDllWrap(0, CStrBufA(path_a, MAX_PATH),
                                                  NULL, NULL, NULL)) {
    HRESULT hr = HRESULTFromLastError();
    NET_LOG(LE, (_T("[GetProxyForUrlLocal][jsproxy init failed][0x%08x]"), hr));
    return hr;
  }

  ON_SCOPE_EXIT(InternetDeInitializeAutoProxyDllWrap, (LPSTR)NULL, 0);

  CStringA url_a(url);
  CStringA url_hostname_a(GetUriHostNameHostOnly(url, false));

  scoped_hglobal proxy_ptr;
  DWORD proxy_len = 0;
  if (FALSE == InternetGetProxyInfoWrap(
      url_a,
      url_a.GetLength(),
      CStrBufA(url_hostname_a, url_hostname_a.GetLength()),
      url_hostname_a.GetLength(),
      reinterpret_cast<LPSTR*>(address(proxy_ptr)),
      &proxy_len)) {
    HRESULT hr = HRESULTFromLastError();
    NET_LOG(LE, (_T("[GetProxyForUrlLocal][jsproxy failed][0x%08x]"), hr));
    return hr;
  }

  ASSERT1(proxy_ptr && proxy_len > 0);
  CStringA proxy(reinterpret_cast<LPSTR>(get(proxy_ptr)), proxy_len);
  ConvertPacResponseToProxyInfo(proxy, proxy_info);
  return S_OK;
}

void NetworkConfig::ConvertPacResponseToProxyInfo(
    const CStringA& response,
    HttpClient::ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  NET_LOG(L4, (_T("[ConvertPacResponseToProxyInfo][%s]"), CString(response)));

  // The proxy list response from a PAC file for a file is a string of proxies
  // to attempt in order, delimited by semicolons, with a keyword denoting how
  // to use the proxy.  For example:
  //
  // PROXY prx1.samp.com; PROXY prx2.test.com:8080; SOCKS prx3.test.com; DIRECT
  //
  // We convert this to a direct semicolon-separated list of host/ports.  We
  // stop parsing if we see DIRECT; we omit any non-PROXY entries.
  CString proxy_list;
  for (int start = 0; start >= 0 && start < response.GetLength();) {
    int semi_pos = response.Find(';', start);
    if (semi_pos < 0) {
      semi_pos = response.GetLength();
    }

    CStringA entry = response.Mid(start, semi_pos - start).Trim().MakeLower();
    if (entry == "direct") {
      break;
    }
    if (0 == entry.Find("proxy ")) {
      // This is a valid proxy entry.  Strip the leading "PROXY " and add it
      // to our parsed list.
      if (!proxy_list.IsEmpty()) {
        proxy_list.AppendChar(_T(';'));
      }
      proxy_list.Append(CString(entry.Mid(6)));
    }

    start = semi_pos + 1;
  }

  if (proxy_list.IsEmpty()) {
    proxy_info->access_type = WINHTTP_ACCESS_TYPE_NO_PROXY;
    proxy_info->proxy = NULL;
    proxy_info->proxy_bypass = NULL;
  } else {
    // The convention is that any strings in a WINHTTP_PROXY_INFO are expected
    // to be freed by the caller using GlobalFree().  Convert our intermediary
    // CString to a GlobalAlloc() buffer and write that out.
    size_t list_len = (proxy_list.GetLength() + 1) * sizeof(TCHAR);
    TCHAR* list_hglob = reinterpret_cast<TCHAR*>(::GlobalAlloc(GPTR, list_len));
    memcpy(list_hglob, proxy_list.GetString(), list_len);
    proxy_info->access_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    proxy_info->proxy = list_hglob;
    proxy_info->proxy_bypass = NULL;
  }
}

NetworkConfigManager* NetworkConfigManager::instance_ = NULL;
LLock NetworkConfigManager::instance_lock_;
bool NetworkConfigManager::is_machine_ = false;

NetworkConfigManager::NetworkConfigManager() {
  const bool has_winhttp =
      HttpClient::GetFactory().Register(HttpClient::WINHTTP,
                                        internal::WinHttpClientCreator);
  ASSERT1(has_winhttp);
}

NetworkConfigManager::~NetworkConfigManager() {
  const bool has_winhttp =
      HttpClient::GetFactory().Unregister(HttpClient::WINHTTP);
  ASSERT1(has_winhttp);
}

HRESULT NetworkConfigManager::CreateInstance() {
  __mutexScope(instance_lock_);
  if (!instance_) {
    NET_LOG(L1, (_T("[NetworkConfigManager::CreateInstance][is_machine: %d]"),
         is_machine_));
    instance_ = new NetworkConfigManager();
  }

  return S_OK;
}

void NetworkConfigManager::DeleteInstance() {
  NetworkConfigManager* instance = omaha::interlocked_exchange_pointer(
      &instance_, static_cast<NetworkConfigManager*>(NULL));

  if (NULL != instance) {
    instance->DeleteInstanceInternal();
    delete instance;
  }
}

NetworkConfigManager& NetworkConfigManager::Instance() {
  __mutexScope(instance_lock_);
  if (!instance_) {
    VERIFY_SUCCEEDED(NetworkConfigManager::CreateInstance());
  }
  return *instance_;
}

void NetworkConfigManager::set_is_machine(bool is_machine) {
  __mutexScope(instance_lock_);
  if (instance_) {
    NET_LOG(LE, (_T("set_is_machine called after instance created.")));
  }

  is_machine_ = is_machine;
}

void NetworkConfigManager::DeleteInstanceInternal() {
  __mutexBlock(lock_) {
    std::map<CString, NetworkConfig*>::iterator it;

    for (it = user_network_config_map_.begin();
         it != user_network_config_map_.end();
         ++it) {
      if (NULL != it->second) {
        delete it->second;
      }
    }
    user_network_config_map_.clear();
  }
}

HRESULT NetworkConfigManager::GetUserNetworkConfig(
    NetworkConfig** network_config) {
  CString sid;
  HRESULT hr = user_info::GetEffectiveUserSid(&sid);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[GetEffectiveUserSid failed][0x%x]"), hr));
    return hr;
  }

  __mutexBlock(lock_) {
    std::map<CString, NetworkConfig*>::iterator it;
    it = user_network_config_map_.find(sid);

    if (user_network_config_map_.end() != it) {
      *network_config = it->second;
      return S_OK;
    }

    hr = CreateNetworkConfigInstance(network_config, is_machine_);
    if (SUCCEEDED(hr)) {
      user_network_config_map_.insert(std::make_pair(sid, *network_config));
    }

    return hr;
  }

  return E_FAIL;
}

HRESULT NetworkConfigManager::CreateNetworkConfigInstance(
    NetworkConfig** network_config_ptr,
    bool is_machine) {
  ASSERT1(network_config_ptr);

  NetworkConfig* network_config(new NetworkConfig(is_machine));
  HRESULT hr = network_config->Initialize();
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[NetworkConfig::Initialize() failed][0x%x]"), hr));
    delete network_config;
    return hr;
  }

  *network_config_ptr = network_config;
  return S_OK;
}

CString NetworkConfigManager::GetUserIdHistory() {
  return goopdate_utils::GetUserIdHistory(is_machine_);
}

}  // namespace omaha

