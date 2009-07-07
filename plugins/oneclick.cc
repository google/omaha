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
// Implementation of the OneClick Plugin.

#include "omaha/plugins/oneclick.h"

#include <atlsafe.h>

#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/string.h"
#include "omaha/common/vistautil.h"
#include "omaha/plugins/oneclick_browser_callback_activex.h"

namespace omaha {

GoopdateCtrl::GoopdateCtrl() : is_worker_url_set_(false) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GoopdateCtrl]")));
}

GoopdateCtrl::~GoopdateCtrl() {
  CORE_LOG(L2, (_T("[GoopdateCtrl::~GoopdateCtrl]")));
}

void GoopdateCtrl::EnsureWorkerUrlSet() {
  if (is_worker_url_set_) {
    return;
  }

  CComBSTR browser_url_bstr;
  if (!GetOurUrl(&browser_url_bstr)) {
    ASSERT(false, (_T("[SetSite] failed GetOurUrl() call")));
    return;
  }

  CORE_LOG(L2, (_T("[EnsureWorkerUrlSet][url=%s]"), browser_url_bstr));
  oneclick_worker_->set_browser_url(browser_url_bstr);
  is_worker_url_set_ = true;
}

STDMETHODIMP GoopdateCtrl::Install(BSTR cmd_line_args,
                                   VARIANT* success_callback,
                                   VARIANT* failure_callback) {
  EnsureWorkerUrlSet();
  ASSERT1(cmd_line_args && cmd_line_args[0]);

  if (!cmd_line_args || !cmd_line_args[0]) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[GoopdateCtrl::Install][cmd_line \"%s\"]"),
                CW2CT(cmd_line_args)));

  OneClickBrowserCallbackActiveX browser_callback;
  HRESULT hr = browser_callback.Initialize(success_callback, failure_callback);
  if (FAILED(hr)) {
    return hr;
  }

  CString cmd_line_args_str(cmd_line_args);
  hr = oneclick_worker_->DoOneClickInstall(cmd_line_args_str,
                                           &browser_callback);

  return hr;
}

STDMETHODIMP GoopdateCtrl::Install2(BSTR extra_args) {
  EnsureWorkerUrlSet();
  ASSERT1(extra_args && extra_args[0]);

  if (!extra_args || !extra_args[0]) {
    return E_INVALIDARG;
  }

  CORE_LOG(L2, (_T("[GoopdateCtrl::Install2][extra_args \"%s\"]"),
                CW2CT(extra_args)));

  CString extra_args_str(extra_args);
  return oneclick_worker_->DoOneClickInstall2(extra_args_str);
}

STDMETHODIMP GoopdateCtrl::GetInstalledVersion(BSTR guid_string,
                                               VARIANT_BOOL is_machine,
                                               BSTR* version_string) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GetInstalledVersion][%s][%d]"),
                guid_string, is_machine));
  EnsureWorkerUrlSet();

  if (!version_string) {
    return E_POINTER;
  }
  *version_string = NULL;

  CString version;
  HRESULT hr = oneclick_worker_->GetInstalledVersion(guid_string,
                                                     is_machine == VARIANT_TRUE,
                                                     &version);
  if (FAILED(hr)) {
    return hr;
  }

  *version_string = version.AllocSysString();
  return S_OK;
}

STDMETHODIMP GoopdateCtrl::GetOneClickVersion(long* version) {
  CORE_LOG(L2, (_T("[GoopdateCtrl::GetOneClickVersion]")));
  EnsureWorkerUrlSet();

  return oneclick_worker_->GetOneClickVersion(version);
}

HRESULT GoopdateCtrl::FinalConstruct() {
  CORE_LOG(L2, (_T("[GoopdateCtrl::FinalConstruct]")));
  oneclick_worker_.reset(new OneClickWorker());
  return oneclick_worker_->Initialize();
}

void GoopdateCtrl::FinalRelease() {
  CORE_LOG(L2, (_T("[GoopdateCtrl::FinalRelease]")));
}

CString GoopdateCtrl::GetGoopdateShellPathForRegMap() {
  return goopdate_utils::BuildGoogleUpdateExeDir(
      goopdate_utils::IsRunningFromOfficialGoopdateDir(true));
}

OBJECT_ENTRY_AUTO(__uuidof(GoopdateOneClickControl), GoopdateCtrl)

}  // namespace omaha

// 4505: unreferenced local function has been removed
#pragma warning(disable : 4505)

