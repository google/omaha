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

// TODO(omaha): provide a way to remember the last good network configuration.

// TODO(omaha): maybe reconsider the singleton and allow multiple instances
// of it to exist, to allow for testability. The app will most likely use it as
// a singleton but the class itself should allow for multiple instances.

// TODO(omaha): might need to remove dependency on winhttp.h when implementing
// support for wininet; see http://b/1119232

#ifndef OMAHA_NET_NETWORK_CONFIG_H__
#define OMAHA_NET_NETWORK_CONFIG_H__

#include <windows.h>
#include <winhttp.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/synchronized.h"
#include "omaha/net/detector.h"
#include "omaha/net/http_client.h"
#include "omaha/net/proxy_auth.h"

namespace ATL {

class CSecurityDesc;

}  // namespace ATL

namespace omaha {

// There are three ways by which an application could connect to the Internet:
// 1. Direct connection.
//    The config for the direction connection must not specify WPAD information
//    nor named proxy information.
// 2. Named proxy.
//    The config for named proxy only includes proxy and proxy_bypass.
// 3. Proxy auto detection.
//    The config for proxy auto detection should include either the auto-detect
//    flag or the auto configuration url. Named proxy information is discarded
//    if present.
struct Config {
  Config() : auto_detect(false) {}

  // Mostly used for debugging purposes to identify who created the instance.
  CString source;

  // Specifies the configuration is WPAD.
  bool auto_detect;

  // The url of the proxy configuration script, if known.
  CString auto_config_url;

  // Named proxy information.
  // The proxy string is usually something as "http=foo:80;https=bar:8080".
  // According to the documentation for WINHTTP_PROXY_INFO, multiple proxies
  // are separated by semicolons or whitespace. The documentation for
  // IBackgroundCopyJob::SetProxySettings says that the list is
  // space-delimited.
  // TODO(omaha): our proxy information is semicolon-separated. This may
  // result in compatibility problems with BITS. Fix this.
  CString proxy;
  CString proxy_bypass;
};

struct CupCredentials;

// Manages the network configurations.
class NetworkConfig {
  public:

  // Abstracts the Internet session, as provided by winhttp or wininet.
  // A winhttp session should map to one and only one identity. in other words,
  // a winhttp session is used to manage the network traffic of a single
  // authenticated user, or a group of anonymous users.
  struct Session {
    Session()
        : session_handle(NULL),
          impersonation_token(NULL) {}

    HINTERNET session_handle;
    HANDLE    impersonation_token;
  };

  // Instance, Initialize, and DeleteInstance methods below are not thread safe.
  // The caller must initialize and cleanup the instance before going
  // multithreaded.
  //
  // Gets the singleton instance of the class.
  // TODO(omaha): rename to GetInstance for consistency
  static NetworkConfig& Instance();

  // Initializes the instance. It takes ownership of the impersonation token
  // provided.
  HRESULT Initialize(bool is_machine, HANDLE impersonation_token);

  bool is_initialized() const { return is_initialized_; }

  // Cleans up the class instance.
  static void DeleteInstance();

  // Hooks up a proxy detector. The class takes ownership of the detector.
  void Add(ProxyDetectorInterface* detector);

  // Clears all detectors and configurations. It does not clear the session.
  // TODO(omaha): rename to avoid the confusion that Clear clears the sessions
  // as well.
  void Clear();

  // Detects the network configuration for each of the registered detectors.
  HRESULT Detect();

  // Returns the detected configurations.
  std::vector<Config> GetConfigurations() const;

  // Gets the persisted CUP credentials.
  HRESULT GetCupCredentials(CupCredentials* cup_credentials);

  // Saves the CUP credentials in persistent storage. If the parameter is null,
  // it clears the credentials.
  HRESULT SetCupCredentials(const CupCredentials* cup_credentials);

  // The caption is an unformatted string. The message is a formatted string
  // that accepts one FormatMessage parameter, the proxy server. The
  // cancel_prompt_threshold is the max number of times the user will see
  // prompts for this process.
  void ConfigureProxyAuth(const CString& caption, const CString& message,
                          HWND parent, uint32 cancel_prompt_threshold);

  // Prompts for credentials, or gets cached credentials if they exist.
  bool GetProxyCredentials(bool allow_ui,
                           bool force_ui,
                           const CString& proxy_settings,
                           bool is_https,
                           CString* username,
                           CString* password,
                           uint32* auth_scheme);

  // Once a auth scheme has been verified against a proxy, this allows a client
  // to record the auth scheme that was used and was successful, so it can be
  // cached for future use within this process.
  HRESULT SetProxyAuthScheme(const CString& proxy_settings,
                             bool is_https,
                             uint32 auth_scheme);

  // Runs the WPAD protocol to compute the proxy information to be used
  // for the given url. The ProxyInfo pointer members must be freed using
  // GlobalFree.
  HRESULT GetProxyForUrl(const CString& url,
                         const CString& auto_config_url,
                         HttpClient::ProxyInfo* proxy_info);

  Session session() const { return session_; }

  // Returns the global configuration override if available.
  HRESULT GetConfigurationOverride(Config* configuration_override);

  // Sets the global configuration override. The function clears the existing
  // configuration if the parameter is NULL.
  void SetConfigurationOverride(const Config* configuration_override);

  // True if the CUP test keys are being used to negotiate the CUP
  // credentials.
  bool static IsUsingCupTestKeys();

  // Returns the prefix of the user agent string.
  static CString GetUserAgent();

  // Eliminates the redundant configurations, for example, if multiple
  // direct connection or proxy auto-detect occur.
  static void RemoveDuplicates(std::vector<Config>*);

  // Parses a network configuration string. The format of the string is:
  // wpad=[false|true];script=script_url;proxy=host:port
  // Ignores the names and the values it does not understand.
  static Config ParseNetConfig(const CString& net_config);

  // Serializes configurations for debugging purposes.
  static CString ToString(const std::vector<Config>& configurations);
  static CString ToString(const Config& configuration);

  static int GetAccessType(const Config& config);

  // Returns s1 + delim + s2. Consider making it an utility function if
  // more usage patterns are found.
  static CString JoinStrings(const TCHAR* s1,
                             const TCHAR* s2,
                             const TCHAR* delim);

 private:
  NetworkConfig();
  ~NetworkConfig();

  HRESULT InitializeLock();
  HRESULT InitializeRegistryKey();

  static NetworkConfig* const kInvalidInstance;
  static NetworkConfig* instance_;

  // Registry sub key where network configuration is persisted.
  static const TCHAR* const kNetworkSubkey;

  // Registry sub key where CUP configuration is persisted.
  static const TCHAR* const kNetworkCupSubkey;

  // The secret key must be encrypted by the caller. This class does not do any
  // encryption.
  static const TCHAR* const kCupClientSecretKey;      // CUP sk.
  static const TCHAR* const kCupClientCookie;         // CUP c.

  static const TCHAR* const kUserAgent;

  bool is_machine_;     // True if the instance is initialized for machine.

  std::vector<Config> configurations_;
  std::vector<ProxyDetectorInterface*> detectors_;

  CString sid_;   // The user sid.

  // The registry path under which to store persistent network configuration.
  CString registry_update_network_path_;

  // Synchronizes access to per-process instance data, which includes
  // the detectors and configurations.
  LLock lock_;

  // Synchronizes access to per-session instance data, such as the
  // CUP credentials.
  GLock global_lock_;

  bool is_initialized_;

  scoped_ptr<Config> configuration_override_;

  Session session_;
  scoped_ptr<HttpClient> http_client_;

  // Manages the proxy auth credentials. Typically a http client tries to
  // use autologon via Negotiate/NTLM with a proxy server. If that fails, the
  // Http client then calls GetProxyCredentials() on NetworkConfig.
  // GetProxyCredentials() gets credentials by either prompting the user, or
  // cached credentials. Then the http client tries again. Options are set via
  // ConfigureProxyAuth().
  ProxyAuth proxy_auth_;

  DISALLOW_EVIL_CONSTRUCTORS(NetworkConfig);
};

// For unittests, where creation and termination of NetworkConfig instances
// is required, the implementation disables the dead reference detection. This
// forces an inline of the code below for unit tests, so different behavior
// can be achieved, even though the rest of the implementation compiles in
// a library. It is somehow brittle but good enough for now.

#ifdef UNITTEST
__forceinline
#else
  inline
#endif
void NetworkConfig::DeleteInstance() {
  delete instance_;
#ifdef UNITTEST
  instance_ = NULL;
#else
  instance_ = kInvalidInstance;
#endif
}

}   // namespace omaha

#endif  // OMAHA_NET_NETWORK_CONFIG_H__

