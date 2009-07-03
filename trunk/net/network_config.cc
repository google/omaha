// Copyright 2007-2009 Google Inc.
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
#include <hash_set>
#include <vector>
#include "base/scoped_ptr.h"
#include "omaha/common/const_object_names.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/common/string.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/vistautil.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/http_client.h"
#include "omaha/goopdate/resource.h"

using omaha::user_info::GetCurrentUser;

namespace omaha {

// Computes the hash value of a Config object. Names in the stdext namespace are
// not currently part of the ISO C++ standard.
uint32 hash_value(const Config& config) {
  uint32 hash = stdext::hash_value(config.auto_detect)                 ^
                stdext::hash_value(config.auto_config_url.GetString()) ^
                stdext::hash_value(config.proxy.GetString())           ^
                stdext::hash_value(config.proxy_bypass.GetString());
  return hash;
}

NetworkConfig* const NetworkConfig::kInvalidInstance  =
    reinterpret_cast<NetworkConfig* const>(-1);
NetworkConfig* NetworkConfig::instance_               = NULL;

const TCHAR* const NetworkConfig::kNetworkSubkey      = _T("network");
const TCHAR* const NetworkConfig::kNetworkCupSubkey   = _T("secure");
const TCHAR* const NetworkConfig::kCupClientSecretKey = _T("sk");
const TCHAR* const NetworkConfig::kCupClientCookie    = _T("c");

const TCHAR* const NetworkConfig::kUserAgent = _T("Google Update/%s");

NetworkConfig::NetworkConfig()
    : is_machine_(false),
      is_initialized_(false) {}

NetworkConfig::~NetworkConfig() {
  if (session_.session_handle && http_client_.get()) {
    http_client_->Close(session_.session_handle);
    session_.session_handle = NULL;
  }
  if (session_.impersonation_token) {
    ::CloseHandle(session_.impersonation_token);
    session_.impersonation_token = NULL;
  }
  Clear();
}

NetworkConfig& NetworkConfig::Instance() {
  // Getting the instance after the instance has been deleted is a bug in
  // the logic of the program.
  ASSERT1(instance_ != kInvalidInstance);
  if (!instance_) {
    instance_ = new NetworkConfig();
  }
  return *instance_;
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
// Also, an Internet session is created and associated with the impersonation
// token.
HRESULT NetworkConfig::Initialize(bool is_machine,
                                  HANDLE impersonation_token) {
  ASSERT1(!is_initialized_);
  is_machine_ = is_machine;
  HRESULT hr = GetCurrentUser(NULL, NULL, &sid_);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[GetCurrentUser failed][0x%x]"), hr));
    return hr;
  }
  hr = InitializeLock();
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[InitializeLock failed][0x%x]"), hr));
    return hr;
  }
  hr = InitializeRegistryKey();
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[InitializeRegistryKey failed][0x%x]"), hr));
    return hr;
  }

  http_client_.reset(CreateHttpClient());
  ASSERT1(http_client_.get());
  if (!http_client_.get()) {
    NET_LOG(LE, (_T("[CreateHttpClient failed]")));
    return E_UNEXPECTED;
  }
  hr = http_client_->Initialize();
  if (FAILED(hr)) {
    // TODO(omaha): This makes an assumption that only WinHttp is
    // supported by the network code.
    NET_LOG(LE, (_T("[http_client_->Initialize() failed][0x%x]"), hr));
    return OMAHA_NET_E_WINHTTP_NOT_AVAILABLE;
  }

  hr = http_client_->Open(NULL,
                          WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS,
                          &session_.session_handle);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[http_client_->Open() failed][0x%x]"), hr));
    return hr;
  }

  session_.impersonation_token = impersonation_token;
  is_initialized_ = true;
  return S_OK;
}

HRESULT NetworkConfig::InitializeLock() {
  NamedObjectAttributes lock_attr;
  GetNamedObjectAttributes(kNetworkConfigLock, is_machine_, &lock_attr);
  return global_lock_.InitializeWithSecAttr(lock_attr.name, &lock_attr.sa) ?
         S_OK : E_FAIL;
}

HRESULT NetworkConfig::InitializeRegistryKey() {
  // The registry path under which to store persistent network configuration.
  // The "network" subkey is created with default security. Below "network",
  // the "secure" key is created so that only system and administrators have
  // access to it.
  registry_update_network_path_ = is_machine_ ? MACHINE_REG_UPDATE :
                                                USER_REG_UPDATE;
  registry_update_network_path_ =
      AppendRegKeyPath(registry_update_network_path_, kNetworkSubkey);
  RegKey reg_key_network;
  DWORD disposition = 0;
  HRESULT hr = reg_key_network.Create(registry_update_network_path_,
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
  CString subkey_name = is_machine_ ?
                        JoinStrings(kNetworkCupSubkey, sid_, _T("-")) :
                        kNetworkCupSubkey;
  hr = reg_key_network_secure.Create(reg_key_network.Key(),  // Parent.
                                     subkey_name,            // Subkey name.
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
    std::vector<Config> configurations;
    for (size_t i = 0; i != detectors_.size(); ++i) {
      Config config;
      if (SUCCEEDED(detectors_[i]->Detect(&config))) {
        configurations.push_back(config);
      }
    }
    configurations_.swap(configurations);
  }
  return S_OK;
}

std::vector<Config> NetworkConfig::GetConfigurations() const {
  std::vector<Config> configurations;
  __mutexBlock(lock_) {
    configurations = configurations_;
  }
  return configurations;
}

HRESULT NetworkConfig::GetConfigurationOverride(Config* config) {
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
    const Config* configuration_override) {
  __mutexBlock(lock_) {
    if (configuration_override) {
      configuration_override_.reset(new Config);
      *configuration_override_ = *configuration_override;
    } else {
      configuration_override_.reset();
    }
  }
}

// Serializes configurations for debugging purposes.

CString NetworkConfig::ToString(const Config& config) {
  CString result;
  result.AppendFormat(_T("source=%s, "), config.source);
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

CString NetworkConfig::ToString(const std::vector<Config>& configurations) {
  CString result;
  for (size_t i = 0; i != configurations.size(); ++i) {
    result.Append(NetworkConfig::ToString(configurations[i]));
    result.Append(_T("\r\n"));
  }
  return result;
}

int NetworkConfig::GetAccessType(const Config& config) {
  if (config.auto_detect || !config.auto_config_url.IsEmpty()) {
    return WINHTTP_ACCESS_TYPE_AUTO_DETECT;
  } else if (!config.proxy.IsEmpty()) {
    return WINHTTP_ACCESS_TYPE_NAMED_PROXY;
  } else {
    return WINHTTP_ACCESS_TYPE_NO_PROXY;
  }
}

HRESULT NetworkConfig::GetCupCredentials(CupCredentials* cup_credentials) {
  ASSERT1(cup_credentials);
  ASSERT1(is_initialized_);
  __mutexBlock(global_lock_) {
    CString subkey_name = is_machine_ ?
                          JoinStrings(kNetworkCupSubkey, sid_, _T("-")) :
                          kNetworkCupSubkey;
    CString key_name = AppendRegKeyPath(registry_update_network_path_,
                                        subkey_name);
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
    cup_credentials->sk.resize(buf_length);
    memcpy(&cup_credentials->sk.front(), buf.get(), buf_length);
    cup_credentials->c = CT2A(cookie);
  }
  return S_OK;
}

HRESULT NetworkConfig::SetCupCredentials(
            const CupCredentials* cup_credentials) {
  ASSERT1(is_initialized_);
  __mutexBlock(global_lock_) {
    CString subkey_name = is_machine_ ?
                          JoinStrings(kNetworkCupSubkey, sid_, _T("-")) :
                          kNetworkCupSubkey;
    CString key_name = AppendRegKeyPath(registry_update_network_path_,
                                        subkey_name);
    RegKey reg_key;
    HRESULT hr = reg_key.Open(key_name, KEY_WRITE);
    if (FAILED(hr)) {
      NET_LOG(L2, (_T("[Registry key open failed][%s][0x%08x]"), key_name, hr));
      return hr;
    }
    if (!cup_credentials) {
      HRESULT hr1 = reg_key.DeleteValue(kCupClientSecretKey);
      HRESULT hr2 = reg_key.DeleteValue(kCupClientCookie);
      return (SUCCEEDED(hr1) && SUCCEEDED(hr2)) ? S_OK : HRESULTFromLastError();
    }
    hr = reg_key.SetValue(kCupClientSecretKey,
             static_cast<const byte*>(&cup_credentials->sk.front()),
             cup_credentials->sk.size());
    if (FAILED(hr)) {
      return hr;
    }
    hr = reg_key.SetValue(kCupClientCookie, CA2T(cup_credentials->c));
    if (FAILED(hr)) {
      return hr;
    }
  }
  return S_OK;
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

void NetworkConfig::ConfigureProxyAuth(const CString& caption,
                                       const CString& message,
                                       HWND parent,
                                       uint32 cancel_prompt_threshold) {
  return proxy_auth_.ConfigureProxyAuth(caption, message, parent,
                                        cancel_prompt_threshold);
}

bool NetworkConfig::GetProxyCredentials(bool allow_ui,
                                        bool force_ui,
                                        const CString& proxy_settings,
                                        bool is_https,
                                        CString* username,
                                        CString* password,
                                        uint32* auth_scheme) {
  ASSERT1(username);
  ASSERT1(password);
  ASSERT1(auth_scheme);
  const CString& proxy = ProxyAuth::ExtractProxy(proxy_settings, is_https);
  return proxy_auth_.GetProxyCredentials(allow_ui, force_ui, proxy,
                                         username, password, auth_scheme);
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
  HttpClient::AutoProxyOptions auto_proxy_options = {0};
  auto_proxy_options.flags = WINHTTP_AUTOPROXY_AUTO_DETECT;
  auto_proxy_options.auto_detect_flags = WINHTTP_AUTO_DETECT_TYPE_DHCP |
                                         WINHTTP_AUTO_DETECT_TYPE_DNS_A;
  if (!auto_config_url.IsEmpty()) {
    auto_proxy_options.auto_config_url = auto_config_url;
    auto_proxy_options.flags |= WINHTTP_AUTOPROXY_CONFIG_URL;
  }
  auto_proxy_options.auto_logon_if_challenged = true;

  return http_client_->GetProxyForUrl(session_.session_handle,
                                      url,
                                      &auto_proxy_options,
                                      proxy_info);
}

CString NetworkConfig::GetUserAgent() {
  CString user_agent;
  user_agent.Format(kUserAgent, GetVersionString());
  return user_agent;
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
// are not expected, due to how the Config structure is being used.
// TODO(omaha): consider not using the hash_set and save about 1K of code.
void NetworkConfig::RemoveDuplicates(std::vector<Config>* config) {
  ASSERT1(config);

  // Iterate over the input configurations, remember the hash of each
  // distinct configuration, and remove the duplicates by skipping the
  // configurations seen before.
  std::vector<Config> input(*config);
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

Config NetworkConfig::ParseNetConfig(const CString& net_config) {
  Config config;
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

}  // namespace omaha

