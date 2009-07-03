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
//
// OneClick worker to handle threads and manage callbacks to the browser.

#include "omaha/plugins/oneclick_worker.h"

#include <dispex.h>
#include <wininet.h>
#include <atlbase.h>
#include <atlcom.h>

#include "omaha/common/app_util.h"
#include "omaha/common/atl_regexp.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/goopdate/command_line_builder.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/webplugin_utils.h"
#include "omaha/worker/application_manager.h"

#define INITGUID
#include <guiddef.h>  // NOLINT

namespace omaha {

const TCHAR* site_lock_pattern_strings[] = {
  _T("^https?://(gears)|(mail)|(tools)|(www)|(desktop)\\.google\\.com/"),
  _T("^https?://www\\.google\\.(ad)|(bg)|(ca)|(cn)|(cz)|(de)|(es)|(fi)|(fr)|(gr)|(hr)|(hu)|(it)|(ki)|(kr)|(lt)|(lv)|(nl)|(no)|(pl)|(pt)|(ro)|(ru)|(sk)|(sg)|(sl)|(sr)|(vn)/"),  // NOLINT
  _T("^https?://www\\.google\\.co\\.(hu)|(id)|(il)|(it)|(jp)|(kr)|(th)|(uk)/"),
  _T("^https?://www\\.google\\.com\\.(ar)|(au)|(br)|(cn)|(et)|(gr)|(hr)|(ki)|(lv)|(om)|(pl)|(pt)|(ru)|(sg)|(sv)|(tr)|(vn)/"),  // NOLINT
};

typedef std::vector<AtlRegExp*> HostRegexp;

class SiteLockPatterns {
 public:
  SiteLockPatterns() {}
  ~SiteLockPatterns();
  bool AddPattern(const CString& host_pattern);
  bool Match(const CString& url);

 private:
  HostRegexp hosts_;
};

bool SiteLockPatterns::AddPattern(const CString& host_pattern) {
  scoped_ptr<AtlRegExp> regex(new AtlRegExp);
  REParseError error = regex->Parse(host_pattern);
  if (error != REPARSE_ERROR_OK) {
    return false;
  }

  hosts_.push_back(regex.release());
  return true;
}

bool SiteLockPatterns::Match(const CString& url) {
  if (url.IsEmpty()) {
    return false;
  }

  ASSERT1(!hosts_.empty());
  for (size_t i = 0; i < hosts_.size(); ++i) {
    AtlMatchContext url_match;
    if (hosts_[i]->Match(url, &url_match)) {
      return true;
    }
  }

  return false;
}

SiteLockPatterns::~SiteLockPatterns() {
  for (size_t i = 0; i < hosts_.size(); ++i) {
    delete hosts_[i];
  }
}

OneClickWorker::OneClickWorker() {
  CORE_LOG(L2, (_T("OneClickWorker::OneClickWorker()")));

  site_lock_patterns_.reset(new SiteLockPatterns());
  // TODO(Omaha): Download new patterns on the fly.
  for (int i = 0; i < arraysize(site_lock_pattern_strings); ++i) {
    VERIFY1(site_lock_patterns_->AddPattern(site_lock_pattern_strings[i]));
  }

  CString update_dev_host_pattern;
  if (SUCCEEDED(RegKey::GetValue(MACHINE_REG_UPDATE_DEV,
                                 kRegValueOneClickHostPattern,
                                 &update_dev_host_pattern)) &&
      !update_dev_host_pattern.IsEmpty()) {
    VERIFY1(site_lock_patterns_->AddPattern(update_dev_host_pattern));
  }
}

OneClickWorker::~OneClickWorker() {
  CORE_LOG(L2, (_T("OneClickWorker::~OneClickWorker()")));
}

HRESULT OneClickWorker::Initialize() {
  CORE_LOG(L2, (_T("[OneClickWorker::Initialize]")));

  return S_OK;
}

HRESULT OneClickWorker::Shutdown() {
  CORE_LOG(L2, (_T("[OneClickWorker::Shutdown]")));

  return S_OK;
}

bool OneClickWorker::InApprovedDomain() {
  ASSERT1(!browser_url_.IsEmpty());
  return site_lock_patterns_->Match(browser_url_);
}

HRESULT OneClickWorker::DoOneClickInstall(
    const TCHAR* cmd_line_args,
    OneClickBrowserCallback* browser_callback) {
  CORE_LOG(L2, (_T("[OneClickWorker::DoOneClickInstall]")
                _T("[cmd_line_args=%s][browser_url=%s]"),
                cmd_line_args, browser_url_));
  ASSERT1(browser_callback);

  HRESULT hr = DoOneClickInstallInternal(cmd_line_args);
  if (SUCCEEDED(hr)) {
    browser_callback->DoSuccessCallback();
  } else {
    CORE_LOG(LE, (_T("[DoOneClickInstallInternal failed][0x%x]"), hr));
    browser_callback->DoFailureCallback(hr);
  }

  // Return success in all cases. The failure callback has already been called
  // above, and we don't want to cause a failure path to be called again when
  // the JavaScript catches the exception.
  return S_OK;
}

HRESULT OneClickWorker::DoOneClickInstall2(const TCHAR* extra_args) {
  CommandLineBuilder builder(COMMANDLINE_MODE_INSTALL);
  builder.set_extra_args(extra_args);
  return DoOneClickInstallInternal(builder.GetCommandLineArgs());
}

HRESULT OneClickWorker::DoOneClickInstallInternal(const TCHAR* cmd_line_args) {
  if (!InApprovedDomain()) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

#ifdef _DEBUG
  // If the args are exactly __DIRECTNOTIFY__ then just fire the event
  // out of this thread.  This allows for easy testing of the
  // browser interface without requiring launch of
  // google_update.exe.
  if (0 == _wcsicmp(L"__DIRECTNOTIFY__", cmd_line_args)) {
    return S_OK;
  }
#endif

  HRESULT hr = webplugin_utils::VerifyResourceLanguage(cmd_line_args);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[VerifyResourceLanguage failed][0x%08x]"), hr));
    return hr;
  }

  CString url_domain;
  hr = GetUrlDomain(browser_url_, &url_domain);
  if (FAILED(hr)) {
    return hr;
  }

  CString url_domain_encoded;
  CString cmd_line_args_encoded;
  hr = StringEscape(url_domain, false, &url_domain_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  hr = StringEscape(cmd_line_args, false, &cmd_line_args_encoded);
  if (FAILED(hr)) {
    return hr;
  }

  CommandLineBuilder builder(COMMANDLINE_MODE_WEBPLUGIN);
  builder.set_webplugin_url_domain(url_domain_encoded);
  builder.set_webplugin_args(cmd_line_args_encoded);
  builder.set_install_source(kCmdLineInstallSource_OneClick);
  CString final_cmd_line_args = builder.GetCommandLineArgs();

  CORE_LOG(L2, (_T("[OneClickWorker::DoOneClickInstallInternal]")
                _T("[Final command line params: %s]"),
                final_cmd_line_args));

  scoped_process process_goopdate;

  hr = goopdate_utils::StartGoogleUpdateWithArgs(
          goopdate_utils::IsRunningFromOfficialGoopdateDir(true),
          final_cmd_line_args,
          address(process_goopdate));
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[OneClickWorker::DoOneClickInstallInternal]")
                  _T("[Failed StartGoogleUpdateWithArgs: 0x%x"),
                  hr));
    return hr;
  }

  return S_OK;
}

HRESULT OneClickWorker::GetInstalledVersion(const TCHAR* guid_string,
                                            bool is_machine,
                                            CString* version_string) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GetInstalledVersion][%s][%d]"),
                guid_string, is_machine));
  if (!InApprovedDomain()) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  ASSERT1(version_string);
  version_string->Empty();

  AppManager app_manager(is_machine);
  ProductData product_data;
  HRESULT hr = app_manager.ReadProductDataFromStore(StringToGuid(guid_string),
                                                    &product_data);
  if (SUCCEEDED(hr) && !product_data.app_data().is_uninstalled()) {
    *version_string = product_data.app_data().version();
  }
  return S_OK;
}

HRESULT OneClickWorker::GetOneClickVersion(long* version) {   // NOLINT
  CORE_LOG(L2, (_T("[OneClickWorker::GetOneClickVersion]")));
  if (!InApprovedDomain()) {
    return GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED;
  }

  if (!version) {
    return E_POINTER;
  }

  *version = atoi(ACTIVEX_VERSION_ANSI);  // NOLINT
  return S_OK;
}

HRESULT OneClickWorker::GetUrlDomain(const CString& url, CString* url_domain) {
  ASSERT1(url_domain);
  url_domain->Empty();

  URL_COMPONENTS urlComponents = {0};
  urlComponents.dwStructSize = sizeof(urlComponents);
  urlComponents.dwSchemeLength = 1;
  urlComponents.dwHostNameLength = 1;
  if (!::InternetCrackUrl(url, 0, 0, &urlComponents)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(L2, (_T("[OneClickWorker failed InternetCrackUrl hr=0x%x]"), hr));
    return hr;
  }

  CString scheme(urlComponents.lpszScheme, urlComponents.dwSchemeLength);
  CString host_name(urlComponents.lpszHostName, urlComponents.dwHostNameLength);
  ASSERT1(!scheme.IsEmpty());
  ASSERT1(!host_name.IsEmpty());

  url_domain->Format(_T("%s://%s/"), scheme, host_name);
  return S_OK;
}

}  // namespace omaha

