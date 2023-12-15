// Copyright 2010 Google Inc.
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

// TODO(omaha3): This really is a client. Where should it and similar code live?

#ifndef OMAHA_GOOPDATE_ONDEMAND_H_
#define OMAHA_GOOPDATE_ONDEMAND_H_

#include <atlbase.h>
#include <oaidl.h>
#include <oleauto.h>
#include <memory>

#include "base/basictypes.h"
#include "base/debug.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/preprocessor_fun.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/thread_pool_callback.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/elevation_moniker_resource.h"
#include "goopdate/omaha3_idl.h"
// TODO(omaha3): Worker is convenient, but it is very odd to include part of the
// Omaha 3 COM server in one of its clients.
#include "omaha/goopdate/goopdate.h"

namespace omaha {

namespace internal {

struct OnDemandParameters {
 public:
  OnDemandParameters(const CString& guid,
                     DWORD job_observer_cookie,
                     bool is_check_only,
                     const CString& sess_id,
                     HANDLE caller_impersonation_token,
                     HANDLE caller_primary_token)
      : app_id(guid),
        job_observer_git_cookie(job_observer_cookie),
        is_update_check_only(is_check_only),
        session_id(sess_id),
        impersonation_token(caller_impersonation_token),
        primary_token(caller_primary_token) {
    ASSERT1(guid.GetLength() > 0);
    ASSERT1(IsGuid(session_id));
    ASSERT1(job_observer_cookie);
  }

  CString app_id;
  DWORD job_observer_git_cookie;
  bool is_update_check_only;
  CString session_id;
  HANDLE impersonation_token;
  HANDLE primary_token;
};

HRESULT DoOnDemand(bool is_machine,
                   OnDemandParameters on_demand_params);

}  // namespace internal


template <typename T>
class ATL_NO_VTABLE OnDemand
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<OnDemand<T> >,
      public IGoogleUpdate,
      public StdMarshalInfo {
 public:
  // IGoogleUpdate::CheckForUpdate().
  STDMETHOD(CheckForUpdate)(const WCHAR* guid, IJobObserver* observer) {
    return DoOnDemandInternalAsync(guid, observer, true);
  }

  // IGoogleUpdate::Update().
  STDMETHOD(Update)(const WCHAR* guid, IJobObserver* observer) {
    return DoOnDemandInternalAsync(guid, observer, false);
  }

  HRESULT FinalConstruct() {
    CORE_LOG(L2, (_T("[OnDemand::FinalConstruct]")));

    if (!T::is_machine()) {
      return S_OK;
    }

    HRESULT hr = UserRights::GetCallerToken(&impersonation_token_);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetCallerToken failed][0x%x]"), hr));
      return hr;
    }

    if (!primary_token_.GetProcessToken(TOKEN_ALL_ACCESS)) {
      hr = HRESULTFromLastError();
      CORE_LOG(LE, (_T("[OnDemand::FC][GetProcessToken failed][%#x]"), hr));
      return hr;
    }

    return S_OK;
  }

 protected:
  OnDemand() : StdMarshalInfo(T::is_machine()) {
    CORE_LOG(L2, (_T("[OnDemand::OnDemand]")));
  }

  virtual ~OnDemand() {
    CORE_LOG(L2, (_T("[OnDemand::~OnDemand]")));
  }

  void FinalRelease() {
    CORE_LOG(L2, (_T("[OnDemand::FinalRelease]")));
  }

  HRESULT DoOnDemandInternalAsync(const WCHAR* guid,
                                  IJobObserver* observer,
                                  bool is_update_check_only) {
    CORE_LOG(L2, (_T("[DoOnDemandInternalAsync][%s][%d]"),
                  guid, is_update_check_only));

    CComGITPtr<IJobObserver> job_observer_git;
    HRESULT hr = job_observer_git.Attach(observer);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[job_observer_git.Attach failed][0x%x]"), hr));
      return hr;
    }

    CAccessToken dup_impersonation_token;
    CAccessToken dup_primary_token;
    if (T::is_machine()) {
      hr = DuplicateTokenIntoCurrentProcess(::GetCurrentProcess(),
                                            impersonation_token_.GetHandle(),
                                            &dup_impersonation_token);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[Failed to duplicate impersonation token][0x%x]"),
                      hr));
        return hr;
      }

      hr = DuplicateTokenIntoCurrentProcess(::GetCurrentProcess(),
                                            primary_token_.GetHandle(),
                                            &dup_primary_token);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[Failed to duplicate primary token][0x%x]"), hr));
        return hr;
      }
    }

    if (session_id_.IsEmpty()) {
      VERIFY_SUCCEEDED(GetGuid(&session_id_));
    }

    // We Lock the ATL Module here since we want the process to stick around
    // until the newly created threadpool item below starts and also completes
    // execution. The corresponding Unlock of the ATL Module is done at the end
    // of the threadpool proc.
    _pAtlModule->Lock();
    ScopeGuard atl_module_unlock = MakeObjGuard(*_pAtlModule,
                                                &CAtlModule::Unlock);

    // Create a thread pool work item for deferred execution of the on demand
    // check. The thread pool owns this call back object. The thread owns the
    // impersonation and primary tokens.
    using Callback = StaticThreadPoolCallBack1<internal::OnDemandParameters>;
    hr = Goopdate::Instance().QueueUserWorkItem(
        std::make_unique<Callback>(
            &OnDemand::DoOnDemandInternal,
            internal::OnDemandParameters(
                guid,
                job_observer_git.Detach(),
                is_update_check_only,
                session_id_,
                dup_impersonation_token.GetHandle(),
                dup_primary_token.GetHandle())),
        COINIT_APARTMENTTHREADED,
        WT_EXECUTELONGFUNCTION);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[QueueUserWorkItem failed][0x%x]"), hr));
      return hr;
    }

    atl_module_unlock.Dismiss();
    if (T::is_machine()) {
      dup_impersonation_token.Detach();
      dup_primary_token.Detach();
    }

    return S_OK;
  }

  static void DoOnDemandInternal(
      internal::OnDemandParameters on_demand_params) {
    CORE_LOG(L2, (_T("[DoOnDemandInternal][%d]"),
                  on_demand_params.is_update_check_only));

    ON_SCOPE_EXIT_OBJ(*_pAtlModule, &CAtlModule::Unlock);

    scoped_handle impersonation_token(on_demand_params.impersonation_token);
    scoped_handle primary_token(on_demand_params.primary_token);

    HRESULT hr = internal::DoOnDemand(T::is_machine(), on_demand_params);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[DoOnDemand failed][0x%x]"), hr));
      return;
    }
  }

  DECLARE_NOT_AGGREGATABLE(OnDemand)
  DECLARE_REGISTRY_RESOURCEID_EX(T::registry_res_id())

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), T::hk_root())
    REGMAP_ENTRY(_T("VERSION"), _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"), T::prog_id())
    REGMAP_ENTRY(_T("DESCRIPTION"), _T("Google Update Legacy On Demand"))
    REGMAP_ENTRY(_T("CLSID"), T::class_id())
    REGMAP_MODULE2(_T("MODULE"), kOmahaOnDemandFileName)
    REGMAP_ENTRY(_T("ICONRESID"), PP_STRINGIZE(IDI_ELEVATION_MONIKER_ICON))
    REGMAP_ENTRY(_T("STRINGRESID"),
                 PP_STRINGIZE(IDS_ELEVATION_MONIKER_DISPLAYNAME))
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(OnDemand)
    COM_INTERFACE_ENTRY(IGoogleUpdate)
    COM_INTERFACE_ENTRY(IStdMarshalInfo)
  END_COM_MAP()

 private:
  CAccessToken impersonation_token_;
  CAccessToken primary_token_;
  CString session_id_;

  DISALLOW_COPY_AND_ASSIGN(OnDemand);
};

struct OnDemandModeUser {
  static bool is_machine() { return false; }
  static const TCHAR* prog_id() { return kProgIDOnDemandUser; }
  static GUID class_id() { return __uuidof(OnDemandUserAppsClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_RGS; }
  static const TCHAR* hk_root() { return _T("HKCU"); }
};

struct OnDemandModeMachineFallback {
  static bool is_machine() { return true; }
  static const TCHAR* prog_id() { return kProgIDOnDemandMachineFallback; }
  static GUID class_id() { return __uuidof(OnDemandMachineAppsFallbackClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_ELEVATION_RGS; }
  static const TCHAR* hk_root() { return _T("HKLM"); }
};

struct OnDemandModeService {
  static bool is_machine() { return true; }
  static const TCHAR* prog_id() { return kProgIDOnDemandSvc; }
  static GUID class_id() { return __uuidof(OnDemandMachineAppsServiceClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVICE_RGS; }
  static const TCHAR* hk_root() { return _T("HKLM"); }
};

typedef OnDemand<OnDemandModeUser> OnDemandUser;
typedef OnDemand<OnDemandModeMachineFallback> OnDemandMachineFallback;
typedef OnDemand<OnDemandModeService> OnDemandService;

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_ONDEMAND_H_
