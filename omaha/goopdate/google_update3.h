// Copyright 2009-2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_GOOGLE_UPDATE3_H_
#define OMAHA_GOOPDATE_GOOGLE_UPDATE3_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <memory>
#include <vector>

#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/preprocessor_fun.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/non_localized_resource.h"
#include "omaha/goopdate/worker.h"

namespace omaha {

template <bool machine, const TCHAR* const progid, const GUID& clsid,
          UINT registry_resid, const TCHAR* const hkroot>
struct Update3COMClassMode {
  static bool is_machine() { return machine; }
  static const TCHAR* prog_id() { return progid; }
  static GUID class_id() { return clsid; }
  static UINT registry_res_id() { return registry_resid; }
  static const TCHAR* hk_root() { return hkroot; }
};

template <typename T>
class ATL_NO_VTABLE Update3COMClass
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<Update3COMClass<T> >,
      public IDispatchImpl<IGoogleUpdate3,
                           &__uuidof(IGoogleUpdate3),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion>,
      public StdMarshalInfo {
 public:
  typedef Update3COMClass<T> Update3COMClassT;

  Update3COMClass() : StdMarshalInfo(T::is_machine()), model_(NULL) {}
  virtual ~Update3COMClass() {}

  DECLARE_CLASSFACTORY()
  DECLARE_NOT_AGGREGATABLE(Update3COMClassT)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  DECLARE_REGISTRY_RESOURCEID_EX(T::registry_res_id())

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), T::hk_root())
    REGMAP_EXE_MODULE(_T("MODULE"))
    REGMAP_ENTRY(_T("VERSION"), _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"), T::prog_id())
    REGMAP_ENTRY(_T("DESCRIPTION"), _T("Update3COMClass"))
    REGMAP_UUID(_T("CLSID"), T::class_id())
  END_REGISTRY_MAP()

  BEGIN_COM_MAP(Update3COMClassT)
    COM_INTERFACE_ENTRY(IGoogleUpdate3)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IStdMarshalInfo)
  END_COM_MAP()

  STDMETHODIMP get_Count(long* count) {  // NOLINT
    ASSERT1(count);

    HRESULT hr = InitializeWorker(this);
    if (FAILED(hr)) {
      return hr;
    }

    __mutexScope(model()->lock());

    const size_t num_app_bundles = model()->GetNumberOfAppBundles();
    if (num_app_bundles > LONG_MAX) {
      return E_FAIL;
    }

    *count = static_cast<long>(num_app_bundles);  // NOLINT

    return S_OK;
  }

  STDMETHODIMP get_Item(long index, IDispatch** app_bundle_wrapper) {  // NOLINT
    ASSERT1(app_bundle_wrapper);

    if (::IsUserAnAdmin() && !UserRights::VerifyCallerIsAdmin()) {
      CORE_LOG(LE, (_T("[User is not an admin]")));
      return E_ACCESSDENIED;
    }

    HRESULT hr = InitializeWorker(this);
    if (FAILED(hr)) {
      return hr;
    }

    __mutexScope(model()->lock());

    const size_t num_app_bundles(model()->GetNumberOfAppBundles());
    if (index < 0 || static_cast<size_t>(index) >= num_app_bundles) {
      return HRESULT_FROM_WIN32(ERROR_INVALID_INDEX);
    }
    std::shared_ptr<AppBundle> app_bundle(model()->GetAppBundle(index));
    return AppBundleWrapper::Create(app_bundle->controlling_ptr(),
                                    app_bundle.get(),
                                    app_bundle_wrapper);
  }

  // Creates an AppBundle object and its corresponding COM wrapper.
  STDMETHODIMP createAppBundle(IDispatch** app_bundle_wrapper) {
    ASSERT1(app_bundle_wrapper);

    HRESULT hr = InitializeWorker(this);
    if (FAILED(hr)) {
      return hr;
    }

    __mutexScope(model()->lock());

    std::shared_ptr<AppBundle> app_bundle(model()->CreateAppBundle(T::is_machine()));
    return AppBundleWrapper::Create(app_bundle->controlling_ptr(),
                                    app_bundle.get(),
                                    app_bundle_wrapper);
  }

  HRESULT FinalConstruct() {
    CORE_LOG(L2, (_T("[Update3COMClass::FinalConstruct]")));

    Worker::Instance().Lock();
    return S_OK;
  }

  void FinalRelease() {
    CORE_LOG(L2, (_T("[Update3COMClass::FinalRelease]")));
    Worker::Instance().Unlock();
  }

 private:
  static HRESULT InitializeWorker(Update3COMClass* instance) {
    ASSERT1(instance);

    static LLock lock;
    static bool is_initialized = false;
    __mutexScope(lock);

    if (!is_initialized) {
      CORE_LOG(L2, (_T("[InitializeWorker][%d]"), T::is_machine()));
      HRESULT hr = Worker::Instance().Initialize(T::is_machine());
      if (FAILED(hr)) {
        return hr;
      }

      is_initialized = true;
    }

    if (!instance->model()) {
      omaha::interlocked_exchange_pointer(&instance->model_,
                                          Worker::Instance().model());
    }
    ASSERT1(instance->model());

    return S_OK;
  }

  Model* model() {
    return omaha::interlocked_exchange_pointer(&model_, model_);
  }

  // C++ root of the object model. Not owned by this instance.
  mutable Model* volatile model_;

  DISALLOW_COPY_AND_ASSIGN(Update3COMClass);
};

extern TCHAR kHKRootUser[];
extern TCHAR kHKRootMachine[];
extern TCHAR kHKRootService[];
extern TCHAR kProgIDUpdate3COMClassUserLocal[];
extern TCHAR kProgIDUpdate3COMClassMachineLocal[];
extern TCHAR kProgIDUpdate3COMClassServiceLocal[];

typedef Update3COMClassMode<false,
                            kProgIDUpdate3COMClassUserLocal,
                            __uuidof(GoogleUpdate3UserClass),
                            IDR_LOCAL_SERVER_RGS,
                            kHKRootUser> Update3COMClassModeUser;

typedef Update3COMClassMode<true,
                            kProgIDUpdate3COMClassServiceLocal,
                            __uuidof(GoogleUpdate3ServiceClass),
                            IDR_LOCAL_SERVICE_RGS,
                            kHKRootService> Update3COMClassModeService;

typedef Update3COMClass<Update3COMClassModeUser> Update3COMClassUser;
typedef Update3COMClass<Update3COMClassModeService> Update3COMClassService;

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOGLE_UPDATE3_H_
