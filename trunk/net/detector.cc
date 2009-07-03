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

// TODO(omaha): move EnclosePath and UnenclosePath functions from path.h to
//               string.h
// TODO(omaha): Firefox detector does not handle proxy bypass.

#include "omaha/net/detector.h"

#include "base/scoped_ptr.h"
#include "omaha/common/atl_regexp.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/file_reader.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/string.h"
#include "omaha/common/user_info.h"
#include "omaha/common/utils.h"
#include "omaha/common/time.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_config.h"

namespace omaha {

namespace  {

// Returns true if the caller's impersonation or process access token user
// is LOCAL_SYSTEM.
bool IsRunningAsSystem() {
  CString sid;
  HRESULT hr = user_info::GetCurrentThreadUser(&sid);
  if (SUCCEEDED(hr)) {
    return IsLocalSystemSid(sid);
  }
  sid.Empty();
  hr = user_info::GetCurrentUser(NULL, NULL, &sid);
  if (SUCCEEDED(hr)) {
    return IsLocalSystemSid(sid);
  }
  return false;
}

}  // namespace

HRESULT GoogleProxyDetector::Detect(Config* config) {
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
  *config = Config();
  config->proxy.Format(_T("%s:%d"), proxy_host, proxy_port);
  config->source = _T("Google");
  return S_OK;
}

FirefoxProxyDetector::FirefoxProxyDetector()
    : cached_prefs_last_modified_(0),
      cached_config_(new Config) {}

FirefoxProxyDetector::~FirefoxProxyDetector() {}

HRESULT FirefoxProxyDetector::Detect(Config* config) {
  ASSERT1(config);

  // The Firefox profile is not available when running as a local system.
  if (IsRunningAsSystem()) {
    return E_FAIL;
  }

  const TCHAR* const kFirefoxPrefsJsFile = _T("\\prefs.js");

  CString name, path;
  HRESULT hr = GetFirefoxDefaultProfile(&name, &path);
  if (FAILED(hr)) {
    return hr;
  }
  path.Append(kFirefoxPrefsJsFile);

  // Has the current profile been modified? Check the name, path, and
  // last modified time of the profile are the same as their cached values.
  FILETIME filetime_last_modified = {0};
  int64 last_modified = 0;
  if (SUCCEEDED(File::GetFileTime(path,
                                  NULL,
                                  NULL,
                                  &filetime_last_modified))) {
    last_modified = FileTimeToInt64(filetime_last_modified);
  }
  if (name.CompareNoCase(cached_prefs_name_)      == 0 &&
      path.CompareNoCase(cached_prefs_file_path_) == 0 &&
      last_modified == cached_prefs_last_modified_     &&
      last_modified) {
    NET_LOG(L4, (_T("[using FF cached profile][%s]"), path));
    *config = *cached_config_;
    return S_OK;
  }

  hr = ParsePrefsFile(name, path, config);
  if (SUCCEEDED(hr) && last_modified) {
    NET_LOG(L4, (_T("[cache FF profile][%s]"), path));
    cached_prefs_name_          = name;
    cached_prefs_file_path_     = path;
    cached_prefs_last_modified_ = last_modified;
    *cached_config_             = *config;
  }
  return hr;
}

// This is what the proxy configuration in Firefox looks like:
// user_pref("network.proxy.autoconfig_url", "http://wpad/wpad.dat");
// user_pref("network.proxy.ftp", "127.0.0.1");
// user_pref("network.proxy.ftp_port", 8888);
// user_pref("network.proxy.gopher", "127.0.0.1");
// user_pref("network.proxy.gopher_port", 8888);
// user_pref("network.proxy.http", "127.0.0.1");
// user_pref("network.proxy.http_port", 8888);
// user_pref("network.proxy.share_proxy_settings", true);
// user_pref("network.proxy.socks", "127.0.0.1");
// user_pref("network.proxy.socks_port", 8888);
// user_pref("network.proxy.ssl", "127.0.0.1");
// user_pref("network.proxy.ssl_port", 8888);
// user_pref("network.proxy.type", 4);
HRESULT FirefoxProxyDetector::ParsePrefsFile(const TCHAR* name,
                                             const TCHAR* file_path,
                                             Config* config) {
  ASSERT1(name);
  ASSERT1(file_path);
  ASSERT1(config);

  *config = Config();
  config->source = _T("FireFox");

  // TODO(omaha): implement optimization not to parse the file again if it
  // did not change.
  UNREFERENCED_PARAMETER(name);

  // There were issues in production where the code fails to allocate
  // the 1MB memory buffer as it had been initially requested by a previous
  // version of the code.
  //
  // The assert below is somehow flaky but useful to detect the unlikely cases
  // when the prefs file can't be opened.
  FileReader prefs_file;
  const size_t kBufferSize = 0x10000;      // 64KB buffer.
  HRESULT hr = prefs_file.Init(file_path, kBufferSize);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  CString proxy_type;
  CString proxy_config_url;
  CString proxy_http_host;
  CString proxy_http_port;
  CString proxy_ssl_host;
  CString proxy_ssl_port;

  // For each line in the prefs.js, try to parse the proxy information out.
  char line[1024] = {0};
  while (SUCCEEDED(prefs_file.ReadLineAnsi(arraysize(line), line))) {
    ParsePrefsLine(line,
                   &proxy_type,
                   &proxy_config_url,
                   &proxy_http_host,
                   &proxy_http_port,
                   &proxy_ssl_host,
                   &proxy_ssl_port);
  }

  // The default in FireFox is direct connection so it may be that the
  // network.proxy.type is missing.
  int type = PROXY_TYPE_NO_PROXY;
  if (!proxy_type.IsEmpty() &&
      !String_StringToDecimalIntChecked(proxy_type, &type)) {
    return E_UNEXPECTED;
  }

  // Direct connection.
  if (type == PROXY_TYPE_NO_PROXY) {
    return S_OK;
  }

  // We look for both proxy auto-detect and proxy config url, to emulate
  // the IE behavior, where when the auto-detect fails it defaults to the
  // auto config url. Firefox remembers the auto config url even if not used,
  // so it might not hurt to try it out.
  if (type & PROXY_TYPE_AUTO_DETECT) {
    config->auto_detect = true;
  }
  if ((type & PROXY_TYPE_AUTO_CONFIG_URL) && !proxy_config_url.IsEmpty()) {
    UnenclosePath(&proxy_config_url);
    config->auto_config_url = proxy_config_url;
  }

  // Named proxy.
  if (!(type & PROXY_TYPE_NAMED_PROXY)) {
    return S_OK;
  }

  CString proxy;
  hr = BuildProxyString(proxy_http_host,
                        proxy_http_port,
                        proxy_ssl_host,
                        proxy_ssl_port,
                        &proxy);
  if (FAILED(hr)) {
    return hr;
  }

  config->proxy = proxy;
  return S_OK;
}

HRESULT FirefoxProxyDetector::BuildProxyString(const CString& proxy_http_host,
                                               const CString& http_port,
                                               const CString& proxy_ssl_host,
                                               const CString& ssl_port,
                                               CString* proxy) {
  ASSERT1(proxy);

  CString http_host = proxy_http_host;
  CString ssl_host  = proxy_ssl_host;

  // The host names in the prefs file are strings literals.
  UnenclosePath(&http_host);
  UnenclosePath(&ssl_host);

  // Validate the port values.
  if (!http_port.IsEmpty()) {
    int http_port_num = 0;
    if (!String_StringToDecimalIntChecked(http_port, &http_port_num) ||
        http_port_num <= 0 &&
        http_port_num > INTERNET_MAX_PORT_NUMBER_VALUE) {
      return E_INVALIDARG;
    }
  }
  if (!ssl_port.IsEmpty()) {
    int ssl_port_num = 0;
    if (!String_StringToDecimalIntChecked(ssl_port, &ssl_port_num) ||
        ssl_port_num <= 0 ||
        ssl_port_num > INTERNET_MAX_PORT_NUMBER_VALUE) {
      return E_INVALIDARG;
    }
  }

  // Format the proxy string.
  CString str;
  if (!http_host.IsEmpty()) {
    str.AppendFormat(_T("http=%s"), http_host);
    if (!http_port.IsEmpty()) {
      str.AppendFormat(_T(":%s"), http_port);
    }
  }
  if (!ssl_host.IsEmpty()) {
    // Append a separator if needed.
    if (!str.IsEmpty()) {
      str += _T(';');
    }
    str.AppendFormat(_T("https=%s"), ssl_host);
    if (!ssl_port.IsEmpty()) {
      str.AppendFormat(_T(":%s"), ssl_port);
    }
  }

  *proxy = str;
  return S_OK;
}

// Parses a line from the prefs.js. An example of line to parse is:
// user_pref("network.proxy.http", "foo");
void FirefoxProxyDetector::ParsePrefsLine(const char* ansi_line,
                                          CString* proxy_type,
                                          CString* proxy_config_url,
                                          CString* proxy_http_host,
                                          CString* proxy_http_port,
                                          CString* proxy_ssl_host,
                                          CString* proxy_ssl_port) {
  // Skip the lines that do not contain "network.proxy" to speed up the
  // parsing. This is important for large prefs files.
  if (strstr(ansi_line, "network.proxy.") == NULL) {
    return;
  }

  AtlRE proxy_type_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.type\\\"\\b*,\\b*{\\d+}\\)"), false);                    // NOLINT
  AtlRE proxy_config_url_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.autoconfig_url\\\"\\b*,\\b*{\\q}\\)"), false);     // NOLINT
  AtlRE proxy_http_host_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.http\\\"\\b*,\\b*{\\q}\\)"), false);                // NOLINT
  AtlRE proxy_http_port_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.http_port\\\"\\b*,\\b*{\\d+}\\)"), false);          // NOLINT
  AtlRE proxy_ssl_host_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.ssl\\\"\\b*,\\b*{\\q}\\)"), false);                  // NOLINT
  AtlRE proxy_ssl_port_regex(_T("^\\b*user_pref\\b*\\(\\b*\\\"network\\.proxy\\.ssl_port\\\"\\b*,\\b*{\\d+}\\)"), false);            // NOLINT

  CString line(ansi_line);
  if (AtlRE::PartialMatch(line, proxy_type_regex, proxy_type)) {
    return;
  }
  if (AtlRE::PartialMatch(line, proxy_config_url_regex, proxy_config_url)) {
    return;
  }
  if (AtlRE::PartialMatch(line, proxy_http_host_regex, proxy_http_host)) {
    return;
  }
  if (AtlRE::PartialMatch(line, proxy_http_port_regex, proxy_http_port)) {
    return;
  }
  if (AtlRE::PartialMatch(line, proxy_ssl_host_regex, proxy_ssl_host)) {
    return;
  }
  if (AtlRE::PartialMatch(line, proxy_ssl_port_regex, proxy_ssl_port)) {
    return;
  }
}

HRESULT DefaultProxyDetector::Detect(Config* config) {
  ASSERT1(config);

  scoped_ptr<HttpClient> http_client(CreateHttpClient());

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
    Config proxy_config;
    proxy_config.source = _T("winhttp");
    proxy_config.proxy = proxy_info.proxy;
    proxy_config.proxy_bypass = proxy_info.proxy_bypass;
    *config = proxy_config;
    return S_OK;
  } else {
    return E_FAIL;
  }
}

HRESULT IEProxyDetector::Detect(Config* config) {
  ASSERT1(config);

  // Internet Explorer proxy configuration is not available when running as
  // local system.
  if (IsRunningAsSystem()) {
    return E_FAIL;
  }

  scoped_ptr<HttpClient> http_client(CreateHttpClient());

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
  config->source = _T("IE");
  config->auto_detect = ie_proxy_config.auto_detect;
  config->auto_config_url = ie_proxy_config.auto_config_url;
  config->proxy = ie_proxy_config.proxy;
  config->proxy_bypass = ie_proxy_config.proxy_bypass;

  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.auto_config_url));
  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.proxy));
  ::GlobalFree(const_cast<TCHAR*>(ie_proxy_config.proxy_bypass));

  return S_OK;
};

}  // namespace omaha

