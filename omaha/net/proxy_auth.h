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


#ifndef OMAHA_NET_PROXY_AUTH_H__
#define OMAHA_NET_PROXY_AUTH_H__

#include <windows.h>
#include <tchar.h>
#include <atlsimpcoll.h>
#include <atlstr.h>
#include <vector>
#include "omaha/base/synchronized.h"
#include "omaha/common/goopdate_utils.h"

#define UNKNOWN_AUTH_SCHEME  0x0000FFFF
const TCHAR* const kDefaultProxyServer = _T("<Default Proxy>");
const uint32 kDefaultCancelPromptThreshold = 1;

namespace omaha {

struct ProxyAuthConfig {
  ProxyAuthConfig() : parent_hwnd(NULL) {}
  ProxyAuthConfig(HWND hwnd, const CString& caption)
      : parent_hwnd(hwnd), prompt_caption(caption) {}

  CString ToString() const {
    CString result;
    result.Format(_T("[ProxyAuthConfig][%p][%s]"),
                  parent_hwnd, prompt_caption.GetString());
    return result;
  }

  HWND parent_hwnd;
  CString prompt_caption;
};

// A class that reads and stores the Internet Explorer saved proxy
// authentication info.  Works with versions of IE up to and including 7.
class ProxyAuth {
 public:
  ProxyAuth() : prompt_cancelled_(0),
                proxy_prompt_is_machine_(
                    goopdate_utils::IsRunningFromOfficialGoopdateDir(true)),
                cancel_prompt_threshold_(kDefaultCancelPromptThreshold) {}
  ~ProxyAuth() {}

  void ConfigureProxyAuth(bool is_machine, uint32 cancel_prompt_threshold);

  // Retrieves the saved proxy credentials for Internet Explorer currently.
  // In the future, there may be other sources of credentials.
  //
  // @param allow_ui Whether to allow a ui prompt
  // @param server The proxy server in domain:port format (e.g., foo.com:8080)
  // @param username The stored username for this proxy server
  // @param password The stored password for this proxy server
  // @returns true if credentials were found, otherwise false
  bool GetProxyCredentials(bool allow_ui, bool force_ui, const CString& server,
                           const ProxyAuthConfig& config, CString* username,
                           CString* password, uint32* auth_scheme);

  static CString ExtractProxy(const CString& proxy_settings, bool isHttps);

  // This function adds a credential entry, or updates an existing server's
  // credential entry if it already exists
  void AddCred(const CString& server, const CString& username,
               const CString& password);
  HRESULT SetProxyAuthScheme(const CString& server, uint32 scheme);

  bool IsPromptAllowed();
  void PromptCancelled();

 private:
  // The servers_, usernames_, and passwords_ lists form a map from server to
  // (username, password) tuple.  They're implemented here using SimplyArrays
  // because the otherhead of a map is thought to be too much, since most
  // users will have one proxy server at most, not dozens.  Accesses are
  // protected with the lock_.
  omaha::LLock lock_;
  CSimpleArray<CString> servers_;
  CSimpleArray<CString> usernames_;
  CSimpleArray<std::vector<uint8> > passwords_;
  CSimpleArray<uint32> auth_schemes_;

  // counts how many times the user has cancelled the authentication prompt.
  uint32 prompt_cancelled_;

  // after this many authentication prompt cancellations, stop prompting.
  uint32 cancel_prompt_threshold_;

  bool proxy_prompt_is_machine_;

  bool ReadFromIE7(const CString& server);
  bool ReadFromPreIE7(const CString& server);
  bool PromptUser(const CString& server, const ProxyAuthConfig& config);

  DISALLOW_EVIL_CONSTRUCTORS(ProxyAuth);
};

}  // namespace omaha

#endif  // OMAHA_NET_PROXY_AUTH_H__

