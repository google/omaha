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

#ifndef OMAHA_GOOPDATE_COM_PROXY_H_
#define OMAHA_GOOPDATE_COM_PROXY_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include "base/basictypes.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/scope_guard.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/google_update_ps_resource.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

// All[*] coclasses in omaha3_idl.idl do the following:
// * Derive from StdMarshalInfo.
// * Construct StdMarshalInfo(is_machine).
// * Add a COM_INTERFACE_ENTRY(IStdMarshalInfo)
//
// [*] The proxy classes GoogleComProxy[XXX]Class, being proxies, do not follow
// the steps above. In addition, the CurrentStateClass has a custom marshaler
// and does not follow the steps above.
//
const IID kIIDsToRegister[] = {
  // Omaha3 IIDs:
  __uuidof(IGoogleUpdate3),
  __uuidof(IAppBundle),
  __uuidof(IApp),
  __uuidof(IApp2),
  __uuidof(IAppCommand),
  __uuidof(IAppCommand2),
  __uuidof(IAppVersion),
  __uuidof(IPackage),
  __uuidof(ICurrentState),

  __uuidof(IRegistrationUpdateHook),

  __uuidof(IGoogleUpdate3Web),
  __uuidof(IGoogleUpdate3WebSecurity),
  __uuidof(IAppBundleWeb),
  __uuidof(IAppWeb),
  __uuidof(IAppCommandWeb),
  __uuidof(IAppVersionWeb),
  __uuidof(ICoCreateAsync),
  __uuidof(ICoCreateAsyncStatus),
  __uuidof(ICredentialDialog),
  __uuidof(IPolicyStatus),
  __uuidof(IPolicyStatus2),
  __uuidof(IPolicyStatus3),
  __uuidof(IPolicyStatusValue),

  __uuidof(IProcessLauncher2),

  // Omaha2 IIDs:
  __uuidof(IBrowserHttpRequest2),
  __uuidof(IProcessLauncher),
  __uuidof(IProgressWndEvents),
  __uuidof(IJobObserver),
  __uuidof(IGoogleUpdate),
  __uuidof(IGoogleUpdateCore),
};

struct ComProxyMode {
  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  static const GUID& class_id() {
    return is_machine() ? __uuidof(GoogleComProxyMachineClass) :
                          __uuidof(GoogleComProxyUserClass);
  }

  static const GUID ps_clsid() {
    if (is_machine()) {
      GUID proxy_clsid = PROXY_CLSID_IS_MACHINE;
      return proxy_clsid;
    } else {
      GUID proxy_clsid = PROXY_CLSID_IS_USER;
      return proxy_clsid;
    }
  }

  static const TCHAR* hk_root() {
    return is_machine() ? _T("HKLM") : _T("HKCU");
  }
};


class ATL_NO_VTABLE ComProxy
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<ComProxy>,
      public IUnknown {
 public:
  ComProxy() {
    CORE_LOG(L2, (_T("[ComProxy::ComProxy]")));
  }

  DECLARE_GET_CONTROLLING_UNKNOWN()
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_COM_PROXY_RGS);

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), ComProxyMode::hk_root())
    REGMAP_ENTRY(_T("CLSID"),  ComProxyMode::class_id())
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(ComProxy)
    COM_INTERFACE_ENTRY(IUnknown)
    COM_INTERFACE_ENTRY_FUNC(__uuidof(IClientSecurity), 0, QueryInternal)
    COM_INTERFACE_ENTRY_FUNC(__uuidof(IMultiQI), 0, QueryInternal)
    COM_INTERFACE_ENTRY_AGGREGATE_BLIND(proxy_manager_.p)
  END_COM_MAP()

  static HRESULT WINAPI QueryInternal(void* ptr, REFIID iid,
                                      void** retval, DWORD_PTR) {
    ASSERT1(ptr);
    ASSERT1(retval);
    CORE_LOG(L2, (_T("[ComProxy::QueryInternal][%s]"), GuidToString(iid)));

    ComProxy* this_ptr = reinterpret_cast<ComProxy*>(ptr);
    return this_ptr->proxy_internal_unknown_->QueryInternalInterface(iid,
                                                                     retval);
  }

  HRESULT FinalConstruct() {
    CORE_LOG(L2, (_T("[ComProxy::FinalConstruct]")));

    HRESULT hr = RegisterProxyStubs();
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[RegisterProxyStubs failed][0x%x]"), hr));
      // If explicit registration failed, the registry-based proxy lookup
      // mechanism may still work. Fall through.
    }

    hr = ::CoGetStdMarshalEx(GetControllingUnknown(),
                             SMEXF_HANDLER,
                             &proxy_manager_);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[::CoGetStdMarshalEx failed][0x%x]"), hr));
      return hr;
    }

    return proxy_manager_.QueryInterface(&proxy_internal_unknown_);
  }

  void FinalRelease() {
    CORE_LOG(L2, (_T("[ComProxy::FinalRelease]")));
  }

  static HRESULT RegisterProxyStubs() {
    static LLock lock;
    static bool is_registered = false;

    __mutexScope(lock);

    if (is_registered) {
      return S_OK;
    }

    CORE_LOG(L2, (_T("[ComProxy::RegisterProxyStubs][Registering][%d]"),
                  ComProxyMode::is_machine()));

    const GUID ps_clsid = ComProxyMode::ps_clsid();
    for (size_t i = 0; i < arraysize(kIIDsToRegister); ++i) {
      HRESULT hr = ::CoRegisterPSClsid(kIIDsToRegister[i], ps_clsid);
      if (FAILED(hr)) {
        CORE_LOG(LE, (_T("[::CoRegisterPSClsid failed][%s][%s][0x%x]"),
            GuidToString(kIIDsToRegister[i]), GuidToString(ps_clsid), hr));
        return hr;
      }
    }

    is_registered = true;
    return S_OK;
  }

 protected:
  virtual ~ComProxy() {
    CORE_LOG(L2, (_T("[ComProxy::~ComProxy]")));
  }

  CComPtr<IUnknown> proxy_manager_;
  CComPtr<IInternalUnknown> proxy_internal_unknown_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ComProxy);
};

class StdMarshalInfo : public IStdMarshalInfo {
 public:
  explicit StdMarshalInfo(bool is_machine) : is_machine_(is_machine) {
    CORE_LOG(L6, (_T("[StdMarshalInfo::StdMarshalInfo][%d]"), is_machine));

    VERIFY_SUCCEEDED(ComProxy::RegisterProxyStubs());
  }

  // IStdMarshalInfo.
  STDMETHODIMP GetClassForHandler(DWORD context, void* ptr, CLSID* clsid) {
    UNREFERENCED_PARAMETER(context);
    UNREFERENCED_PARAMETER(ptr);

    *clsid = is_machine_ ? __uuidof(GoogleComProxyMachineClass) :
                           __uuidof(GoogleComProxyUserClass);
    return S_OK;
  }

 private:
  bool is_machine_;

  DISALLOW_COPY_AND_ASSIGN(StdMarshalInfo);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_COM_PROXY_H_
