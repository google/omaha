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

// OneClick worker class shared by the different plugin architectures.
// This class does most of the OneClick heavy lifting.

#ifndef OMAHA_PLUGINS_ONECLICK_WORKER_H__
#define OMAHA_PLUGINS_ONECLICK_WORKER_H__

#include <windows.h>

#include "base/scoped_ptr.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/synchronized.h"
#include "omaha/plugins/oneclick_browser_callback.h"

namespace omaha {

class SiteLockPatterns;

class OneClickWorker {
 public:
  OneClickWorker();
  ~OneClickWorker();

  HRESULT Initialize();
  HRESULT Shutdown();

  // Performs a OneClick install.
  // cmd_line_args - arguments to eventually be passed to googleupdate.exe.
  // browser_callback - Callback class to fire success/failure events to.
  // NOTE:  OneClickWorker assumes memory ownership of browser_callback.
  HRESULT DoOneClickInstall(const TCHAR* cmd_line_args,
                            OneClickBrowserCallback* browser_callback);
  // The incoming extra_args are used to construct an "/install" command line.
  HRESULT DoOneClickInstall2(const TCHAR* extra_args);

  HRESULT GetInstalledVersion(const TCHAR* guid_string,
                              bool is_machine,
                              CString* version_string);

  HRESULT GetOneClickVersion(long* version);

  bool InApprovedDomain();

  void set_browser_url(const TCHAR* browser_url) {
    browser_url_ = browser_url;
    browser_url_.MakeLower();
  }

 private:
  HRESULT DoOneClickInstallInternal(const TCHAR* cmd_line_args);
  HRESULT GetUrlDomain(const CString& url, CString* url_domain);

  CString browser_url_;
  scoped_ptr<SiteLockPatterns> site_lock_patterns_;

  DISALLOW_EVIL_CONSTRUCTORS(OneClickWorker);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_ONECLICK_WORKER_H__

