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

#include <winhttp.h>
#include <atlconv.h>
#include <atlsecurity.h>
#include <algorithm>
#include <hash_set>
#include <vector>
#include "base/error.h"
#include "base/scoped_ptr.h"
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
#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/user_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/http_client.h"

using omaha::encrypt::EncryptData;
using omaha::encrypt::DecryptData;

namespace omaha {

// Computes the hash value of a ProxyConfig object. Names in the stdext
// namespace are not currently part of the ISO C++ standard.
uint32 hash_value(const ProxyConfig& config) {
  uint32 hash = stdext::hash_value(config.auto_detect)                 ^
                stdext::hash_value(config.auto_config_url.GetString()) ^
                stdext::hash_value(config.proxy.GetString())           ^
                stdext::hash_value(config.proxy_bypass.GetString());
  return hash;
}

const TCHAR* const NetworkConfigManager::kNetworkSubkey      = _T("network");
const TCHAR* const NetworkConfigManager::kNetworkCupSubkey   = _T("secure");
const TCHAR* const NetworkConfigManager::kCupClientSecretKey = _T("sk");
const TCHAR* const NetworkConfigManager::kCupClientCookie    = _T("c");

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

// Initialize creates or opens a global lock to synchronize access to
// registry where CUP credentials are stored. Each user including non-elevated
// admins stores network configuration data, such as the CUP password in
// its HKCU. The admin users, including the LOCAL_SYSTEM, store data in HKLM.
// Therefore, the naming of the global lock is different: users have their
// lock postfixed with their sid, so the serialization only occurs within the
// same user's programs. Admin users use the same named lock since they store
// data in a shared HKLM. The data of the admin users is disambiguated by
// postfixing their registry sub key with sids.
// In conclusion, users have sid-postfixed locks and their data goes in
// their respective HKCU. Admin users have the same lock and their data goes
// under HKLM in sid-postfixed stores.
//
// The named lock is created in the global namespace to account for users
// logging in from different TS sessions.
//
// The CUP credentials must be protected with ACLs so non-elevated admins can't
// read elevated-admins' keys and attack the protocol.
//
// Also, an Internet session is created.
HRESULT NetworkConfig::Initialize() {
  ASSERT1(!is_initialized_);

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

  Add(new UpdateDevProxyDetector);
  BrowserType browser_type(BROWSER_UNKNOWN);
  GetDefaultBrowserType(&browser_type);
  if (browser_type == BROWSER_FIREFOX) {
    Add(new FirefoxProxyDetector);
  }
  // There is no Chrome detector because it uses the same proxy settings as IE.
  Add(new IEProxyDetector);
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

HRESULT NetworkConfig::GetCupCredentials(
    CupCredentials* cup_credentials) const {
  return NetworkConfigManager::Instance().GetCupCredentials(cup_credentials);
}

HRESULT NetworkConfig::SetCupCredentials(
    const CupCredentials* cup_credentials) const {
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  if (cup_credentials == NULL) {
    network_manager.ClearCupCredentials();
    return S_OK;
  }

  return network_manager.SetCupCredentials(*cup_credentials);
}

// Serializes configurations for debugging purposes.
CString NetworkConfig::ToString(const ProxyConfig& config) {
  CString result;
  result.AppendFormat(_T("priority=%u, source=%s, "),
                      config.priority, config.source);

  switch (GetAccessType(config)) {
    case WINHTTP_ACCESS_TYPE_NO_PROXY:
      result.AppendFormat(_T("direct connection"));
      break;
    case WINHTTP_ACCESS_TYPE_NAMED_PROXY:
      result.AppendFormat(_T("named proxy=%s, bypass=%s"),
                          config.proxy, config.proxy_bypass);
      break;
    case WINHTTP_ACCESS_TYPE_AUTO_DETECT:
      result.AppendFormat(_T("wpad=%d, script=%s"),
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

// TODO(omaha): the code does WPAD auto detect in all cases. It is possible for
// a configuration to specify no auto detection but provide the proxy script
// url. The current code does not account for this yet.
HRESULT NetworkConfig::GetProxyForUrl(const CString& url,
                                      const CString& auto_config_url,
                                      HttpClient::ProxyInfo* proxy_info) {
  ASSERT1(proxy_info);

  NET_LOG(L3, (_T("[NetworkConfig::GetProxyForUrl][%s]"), url));

  HttpClient::AutoProxyOptions auto_proxy_options = {0};
  auto_proxy_options.flags = WINHTTP_AUTOPROXY_AUTO_DETECT;
  auto_proxy_options.auto_detect_flags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                                         WINHTTP_AUTO_DETECT_TYPE_DNS_A;
  if (!auto_config_url.IsEmpty()) {
    auto_proxy_options.auto_config_url = auto_config_url;
    auto_proxy_options.flags |= WINHTTP_AUTOPROXY_CONFIG_URL;
  }
  auto_proxy_options.auto_logon_if_challenged = true;

  HRESULT hr = http_client_->GetProxyForUrl(session_.session_handle,
                                            url,
                                            &auto_proxy_options,
                                            proxy_info);

  if (FAILED(hr) && ::UrlIsFileUrl(auto_config_url)) {
    // Some HttpClient implementations, namely WinHTTP, only support PAC files
    // with http or https schemes.  Attempt an alternate resolution scheme using
    // jsproxy.dll if the initial attempt fails.
    ASSERT1(user_info::IsThreadImpersonating());

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
  user_agent.Format(kUserAgent, GetVersionString());
  return user_agent;
}

CString NetworkConfig::GetMID() {
  CString mid;
  RegKey::GetValue(MACHINE_REG_UPDATE_DEV, kRegValueMID, &mid);
  return mid;
}

CString NetworkConfig::JoinStrings(const TCHAR* s1,
                                   const TCHAR* s2,
                                   const TCHAR* delim) {
  CString result;
  const TCHAR* components[] = {s1, s2};
  JoinStringsInArray(components, arraysize(components), delim, &result);
  return result;
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

  typedef stdext::hash_set<uint32> Keys;
  Keys keys;
  for (size_t i = 0; i != input.size(); ++i) {
    std::pair<Keys::iterator, bool> result(keys.insert(hash_value(input[i])));
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
  scoped_library jsproxy_lib(::LoadLibrary(_T("jsproxy.dll")));
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

const NetworkConfigManager* const NetworkConfigManager::kInvalidInstance  =
    reinterpret_cast<const NetworkConfigManager* const>(-1);
NetworkConfigManager* NetworkConfigManager::instance_ = NULL;
LLock NetworkConfigManager::instance_lock_;
bool NetworkConfigManager::is_machine_ = false;

NetworkConfigManager::NetworkConfigManager() {
}

NetworkConfigManager::~NetworkConfigManager() {
  SaveCupCredentialsToRegistry();
}

HRESULT NetworkConfigManager::CreateInstance() {
  __mutexScope(instance_lock_);
  ASSERT1(instance_ != kInvalidInstance);
  if (!instance_) {
    NET_LOG(L1, (_T("[NetworkConfigManager::CreateInstance][is_machine: %d]"),
         is_machine_));
    instance_ = new NetworkConfigManager();
    VERIFY1(SUCCEEDED(instance_->InitializeLock()));
    VERIFY1(SUCCEEDED(instance_->InitializeRegistryKey()));
    instance_->LoadCupCredentialsFromRegistry();
  }

  return S_OK;
}

void NetworkConfigManager::DeleteInstance() {
  ASSERT1(instance_ != kInvalidInstance);

  NetworkConfigManager* instance =
      omaha::interlocked_exchange_pointer(&instance_, kInvalidInstance);

  if (kInvalidInstance != instance && NULL != instance) {
    instance->DeleteInstanceInternal();
    delete instance;
  }
}

NetworkConfigManager& NetworkConfigManager::Instance() {
  __mutexScope(instance_lock_);
  if (!instance_) {
    VERIFY1(SUCCEEDED(NetworkConfigManager::CreateInstance()));
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

HRESULT NetworkConfigManager::InitializeLock() {
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kNetworkConfigLock, is_machine_, &lock_attr);
  return global_lock_.InitializeWithSecAttr(lock_attr.name, &lock_attr.sa) ?
         S_OK : E_FAIL;
}

HRESULT NetworkConfigManager::InitializeRegistryKey() {
  // The registry path under which to store persistent network configuration.
  // The "network" subkey is created with default security. Below "network",
  // the "secure" key is created so that only system and administrators have
  // access to it.
  CString reg_path = is_machine_ ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  reg_path = AppendRegKeyPath(reg_path, kNetworkSubkey);
  RegKey reg_key_network;
  DWORD disposition = 0;
  HRESULT hr = reg_key_network.Create(reg_path,
                                      NULL,                 // Class.
                                      0,                    // Options.
                                      KEY_CREATE_SUB_KEY,   // SAM desired.
                                      NULL,                 // Security attrs.
                                      &disposition);
  if (FAILED(hr)) {
    return hr;
  }

  // When initializing for machine, grant access to administrators and system.
  scoped_ptr<CSecurityAttributes> sa;
  if (is_machine_) {
    sa.reset(new CSecurityAttributes);
    GetAdminDaclSecurityAttributes(sa.get(), GENERIC_ALL);
  }

  disposition = 0;
  RegKey reg_key_network_secure;
  hr = reg_key_network_secure.Create(reg_key_network.Key(),  // Parent.
                                     kNetworkCupSubkey,      // Subkey name.
                                     NULL,                   // Class.
                                     0,                      // Options.
                                     KEY_READ,               // SAM desired.
                                     sa.get(),               // Security attrs.
                                     &disposition);
  if (FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

HRESULT NetworkConfigManager::SetCupCredentials(
    const CupCredentials& cup_credentials) {
  __mutexScope(lock_);

  const std::vector<uint8>& sk_in(cup_credentials.sk);

  if (sk_in.empty()) {
    return E_INVALIDARG;
  }

  std::vector<uint8> sk_out;
  HRESULT hr = EncryptData(NULL, 0, &sk_in.front(), sk_in.size(), &sk_out);
  if (FAILED(hr)) {
    return hr;
  }

  cup_credentials_.reset(new CupCredentials);

  cup_credentials_->sk.swap(sk_out);
  cup_credentials_->c.SetString(cup_credentials.c);

  return S_OK;
}

HRESULT NetworkConfigManager::GetCupCredentials(
    CupCredentials* cup_credentials) {
  ASSERT1(cup_credentials);
  __mutexScope(lock_);
  if (cup_credentials_ == NULL || cup_credentials_->sk.empty()) {
    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  }

  std::vector<uint8> decrypted_sk;
  HRESULT hr = DecryptData(NULL,
                           0,
                           &cup_credentials_->sk.front(),
                           cup_credentials_->sk.size(),
                           &decrypted_sk);
  if (FAILED(hr)) {
    return hr;
  }
  cup_credentials->sk.swap(decrypted_sk);
  cup_credentials->c.SetString(cup_credentials_->c);

  return S_OK;
}

// This function should be called in singleton creation stage,
// thus no lock is needed.
HRESULT NetworkConfigManager::LoadCupCredentialsFromRegistry() {
  __mutexScope(global_lock_);

  CString reg_path = is_machine_ ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  reg_path = AppendRegKeyPath(reg_path, kNetworkSubkey);
  CString key_name = AppendRegKeyPath(reg_path, kNetworkCupSubkey);
  RegKey reg_key;
  HRESULT hr = reg_key.Open(key_name, KEY_READ);
  if (FAILED(hr)) {
    return hr;
  }
  scoped_array<byte> buf;
  DWORD buf_length = 0;
  hr = reg_key.GetValue(kCupClientSecretKey, address(buf), &buf_length);
  if (FAILED(hr)) {
    return hr;
  }
  CString cookie;
  hr = reg_key.GetValue(kCupClientCookie, &cookie);
  if (FAILED(hr)) {
    return hr;
  }
  if (buf_length == 0) {
    return E_FAIL;
  }
  cup_credentials_.reset(new CupCredentials);
  cup_credentials_->sk.resize(buf_length);
  memcpy(&cup_credentials_->sk.front(), buf.get(), buf_length);
  cup_credentials_->c = CT2A(cookie);

  return S_OK;
}

// This function is called in the destructor only thus no lock is needed.
HRESULT NetworkConfigManager::SaveCupCredentialsToRegistry() {
  __mutexScope(global_lock_);

  CString reg_path = is_machine_ ? MACHINE_REG_UPDATE : USER_REG_UPDATE;
  reg_path = AppendRegKeyPath(reg_path, kNetworkSubkey);
  CString key_name = AppendRegKeyPath(reg_path, kNetworkCupSubkey);
  RegKey reg_key;
  HRESULT hr = reg_key.Open(key_name, KEY_WRITE);
  if (FAILED(hr)) {
    NET_LOG(L2, (_T("[Registry key open failed][%s][0x%08x]"), key_name, hr));
    return hr;
  }

  if (cup_credentials_ == NULL || cup_credentials_->sk.empty()) {
    HRESULT hr1 = reg_key.DeleteValue(kCupClientSecretKey);
    HRESULT hr2 = reg_key.DeleteValue(kCupClientCookie);
    return (SUCCEEDED(hr1) && SUCCEEDED(hr2)) ? S_OK : HRESULTFromLastError();
  }

  hr = reg_key.SetValue(kCupClientSecretKey,
           static_cast<const byte*>(&cup_credentials_->sk.front()),
           cup_credentials_->sk.size());
  if (FAILED(hr)) {
    return hr;
  }
  hr = reg_key.SetValue(kCupClientCookie, CA2T(cup_credentials_->c));
  if (FAILED(hr)) {
    return hr;
  }
  return S_OK;
}

void NetworkConfigManager::ClearCupCredentials() {
  __mutexScope(lock_);
  cup_credentials_.reset(NULL);
}

}  // namespace omaha

