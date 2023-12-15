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
//
// Defines the Google Update 3 web broker. It defines a narrow set of interfaces
// to reduce the attack surface from low and medium integrity processes.
// The web broker used to be a COM elevation point as well, but that
// functionality has moved into the broker class factory. Note that since
// Update3Web is a COM service now, ::CoSetProxyBlanket must be called on any
// interfaces that need to impersonate.

#ifndef OMAHA_GOOPDATE_UPDATE3WEB_H_
#define OMAHA_GOOPDATE_UPDATE3WEB_H_

#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/preprocessor_fun.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/non_localized_resource.h"

namespace omaha {

class ATL_NO_VTABLE Update3WebBase
    : public CComObjectRootEx<CComObjectThreadModel>,
      public IDispatchImpl<IGoogleUpdate3Web,
                           &__uuidof(IGoogleUpdate3Web),
                           &CAtlModule::m_libid,
                           kMajorTypeLibVersion,
                           kMinorTypeLibVersion>,
      public IGoogleUpdate3WebSecurity,
      public StdMarshalInfo {
 public:
  explicit Update3WebBase(bool is_machine) : StdMarshalInfo(is_machine),
                                             is_machine_(is_machine) {}

  BEGIN_COM_MAP(Update3WebBase)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IGoogleUpdate3Web)
    COM_INTERFACE_ENTRY(IGoogleUpdate3WebSecurity)
    COM_INTERFACE_ENTRY(IStdMarshalInfo)
  END_COM_MAP()

  HRESULT FinalConstruct();

  // IGoogleUpdate3Web
  STDMETHOD(createAppBundleWeb)(IDispatch** app_bundle_web);

  // IGoogleUpdate3WebSecurity
  STDMETHOD(setOriginURL)(BSTR origin_url);

  IGoogleUpdate3* omaha_server() const { return omaha_server_.p; }
  HANDLE impersonation_token() const {
    return impersonation_token_.GetHandle();
  }
  HANDLE primary_token() const { return primary_token_.GetHandle(); }
  bool is_machine_install() const { return is_machine_; }
  CString origin_url() const { return origin_url_; }

 protected:
  virtual ~Update3WebBase() {}

 private:
  CComPtr<IGoogleUpdate3> omaha_server_;
  CAccessToken impersonation_token_;
  CAccessToken primary_token_;
  bool is_machine_;
  CString origin_url_;

  DISALLOW_COPY_AND_ASSIGN(Update3WebBase);
};

template <typename T>
class ATL_NO_VTABLE Update3Web
    : public Update3WebBase,
      public CComCoClass<Update3Web<T> > {
 public:
  Update3Web() : Update3WebBase(T::is_machine()) {}

  DECLARE_NOT_AGGREGATABLE(Update3Web);
  DECLARE_REGISTRY_RESOURCEID_EX(T::registry_res_id())

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), T::hk_root())
    REGMAP_ENTRY(_T("VERSION"), _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"), T::prog_id())
    REGMAP_ENTRY(_T("DESCRIPTION"), MAIN_EXE_BASE_NAME _T(" Update3Web"))
    REGMAP_ENTRY(_T("CLSID"), T::class_id())
    REGMAP_MODULE2(_T("MODULE"), kOmahaOnDemandFileName)
    REGMAP_ENTRY(_T("ICONRESID"), PP_STRINGIZE(IDI_ELEVATION_MONIKER_ICON))
    REGMAP_ENTRY(_T("STRINGRESID"),
                 PP_STRINGIZE(IDS_ELEVATION_MONIKER_DISPLAYNAME))
  END_REGISTRY_MAP()

 protected:
  virtual ~Update3Web() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(Update3Web);
};

struct Update3WebModeUser {
  static bool is_machine() { return false; }
  static const TCHAR* prog_id() { return kProgIDUpdate3WebUser; }
  static GUID class_id() { return __uuidof(GoogleUpdate3WebUserClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_RGS; }
  static const TCHAR* hk_root() { return _T("HKCU"); }
};

struct Update3WebModeMachineFallback {
  static bool is_machine() { return true; }
  static const TCHAR* prog_id() {
    return kProgIDUpdate3WebMachineFallback;
  }
  static GUID class_id() {
    return __uuidof(GoogleUpdate3WebMachineFallbackClass);
  }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_ELEVATION_RGS; }
  static const TCHAR* hk_root() { return _T("HKLM"); }
};

struct Update3WebModeService {
  static bool is_machine() { return true; }
  static const TCHAR* prog_id() { return kProgIDUpdate3WebSvc; }
  static GUID class_id() { return __uuidof(GoogleUpdate3WebServiceClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVICE_RGS; }
  static const TCHAR* hk_root() { return _T("HKLM"); }
};

typedef Update3Web<Update3WebModeUser> Update3WebUser;
typedef Update3Web<Update3WebModeMachineFallback> Update3WebMachineFallback;
typedef Update3Web<Update3WebModeService> Update3WebService;

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_UPDATE3WEB_H_
