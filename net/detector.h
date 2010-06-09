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

#ifndef OMAHA_NET_DETECTOR_H__
#define OMAHA_NET_DETECTOR_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace omaha {

struct Config;

class ProxyDetectorInterface {
 public:
  // Detects proxy information.
  virtual HRESULT Detect(Config* config) = 0;
  virtual ~ProxyDetectorInterface() {}
};

// Detects proxy override information in the specified registry key.
class GoogleProxyDetector : public ProxyDetectorInterface {
 public:
  explicit GoogleProxyDetector(const CString& reg_path)
      : reg_path_(reg_path) {}

  virtual HRESULT Detect(Config* config);
 private:
  CString reg_path_;
  DISALLOW_EVIL_CONSTRUCTORS(GoogleProxyDetector);
};

// Detects winhttp proxy information. This is what the winhttp proxy
// configuration utility (proxycfg.exe) has set.
// The winhttp proxy settings are under:
// HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Internet Settings\Connections
class DefaultProxyDetector : public ProxyDetectorInterface {
 public:
  DefaultProxyDetector() {}
  virtual HRESULT Detect(Config* config);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DefaultProxyDetector);
};

// Detects proxy information for Firefox.
// http://www.mozilla.org/quality/networking/docs/netprefs.html
// It works only when the calling code runs as or it impersonates a user.
class FirefoxProxyDetector : public ProxyDetectorInterface {
 public:
  enum ProxyType {
    PROXY_TYPE_NO_PROXY         = 0,
    PROXY_TYPE_NAMED_PROXY      = 1,
    PROXY_TYPE_AUTO_CONFIG_URL  = 2,
    PROXY_TYPE_AUTO_DETECT      = 4
  };

  FirefoxProxyDetector();
  virtual ~FirefoxProxyDetector();

  virtual HRESULT Detect(Config* config);

 private:
  // Parses the prefs.js file.
  HRESULT ParsePrefsFile(const TCHAR* name,
                         const TCHAR* file_path,
                         Config* config);

  // Parse one line of the prefs file.
  void ParsePrefsLine(const char* ansi_line,
                      CString* proxy_type,
                      CString* proxy_config_url,
                      CString* proxy_http_host,
                      CString* proxy_http_port,
                      CString* proxy_ssl_host,
                      CString* proxy_ssl_port);

  // Builds a proxy string out of individual components.
  HRESULT BuildProxyString(const CString& http_host,
                           const CString& http_port,
                           const CString& ssl_host,
                           const CString& ssl_port,
                           CString* proxy);

  // Cached configuration values for the current FF profile.
  CString            cached_prefs_name_;
  CString            cached_prefs_file_path_;
  int64              cached_prefs_last_modified_;
  scoped_ptr<Config> cached_config_;

  friend class FirefoxProxyDetectorTest;
  DISALLOW_EVIL_CONSTRUCTORS(FirefoxProxyDetector);
};

// Detects wininet proxy information for the current user. The caller must
// run as user to retrieve the correct information.
// It works only when the calling code runs as or it impersonates a user.
class IEProxyDetector : public ProxyDetectorInterface {
 public:
  IEProxyDetector() {}
  virtual HRESULT Detect(Config* config);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(IEProxyDetector);
};

}  // namespace omaha

#endif  // OMAHA_NET_DETECTOR_H__

