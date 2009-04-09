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

#ifndef OMAHA_WORKER_WORKER_COM_WRAPPER_H__
#define OMAHA_WORKER_WORKER_COM_WRAPPER_H__

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "goopdate/google_update_idl.h"
#include "omaha/common/atlregmapex.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/preprocessor_fun.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/resource.h"
#include "omaha/goopdate/resources/goopdate_dll/goopdate_dll.grh"

namespace omaha {

const TCHAR* const kOnDemandCOMClassUserProgId =
    _T("GoogleUpdate.OnDemandCOMClassUser");
const TCHAR* const kOnDemandCOMClassMachineProgId =
    _T("GoogleUpdate.OnDemandCOMClassMachine");
const TCHAR* const kOnDemandCOMClassDescription =
    _T("GoogleUpdate.OnDemandCOMClass");

class Worker;

class OnDemandCOMClass
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<OnDemandCOMClass>,
      public IGoogleUpdate {
 public:
  OnDemandCOMClass()
      : worker_(NULL) {
  }
  virtual ~OnDemandCOMClass() {}

  DECLARE_NOT_AGGREGATABLE(OnDemandCOMClass)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_GOOGLE_UPDATE_WORKER_CLASS)

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"),       goopdate_utils::GetHKRoot())
    REGMAP_EXE_MODULE(_T("MODULE"))
    REGMAP_ENTRY(_T("VERSION"),      _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"),
                 goopdate_utils::IsRunningFromOfficialGoopdateDir(true) ?
                     kOnDemandCOMClassMachineProgId :
                     kOnDemandCOMClassUserProgId)
    REGMAP_ENTRY(_T("DESCRIPTION"),  kOnDemandCOMClassDescription)
    REGMAP_UUID(_T("CLSID"),
                goopdate_utils::IsRunningFromOfficialGoopdateDir(true) ?
                    __uuidof(OnDemandMachineAppsClass) :
                    __uuidof(OnDemandUserAppsClass))
    REGMAP_UUID(_T("LIBID"),         LIBID_GoogleUpdateLib)
    REGMAP_ENTRY(_T("STRINGRESID"),
                 PP_STRINGIZE(IDS_ELEVATION_MONIKER_DISPLAYNAME))
    REGMAP_ENTRY(_T("ICONRESID"), PP_STRINGIZE(IDI_ELEVATION_MONIKER_ICON))
  END_REGISTRY_MAP()
  #pragma warning(pop)

  // C4505: unreferenced IUnknown local functions have been removed
  #pragma warning(disable : 4505)
  BEGIN_COM_MAP(OnDemandCOMClass)
    COM_INTERFACE_ENTRY(IGoogleUpdate)
  END_COM_MAP()

  STDMETHOD(CheckForUpdate)(const WCHAR* guid, IJobObserver* observer);
  STDMETHOD(Update)(const WCHAR* guid, IJobObserver* observer);
  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  void AddRefIgnoreShutdownEvent() const;
  HRESULT DoOnDemand(bool is_update_check_only,
                     const TCHAR* guid,
                     IJobObserver* observer);

  // Uninitializes the observer and allows COM calls.
  void ResetStateOnError() const;

  Worker* worker_;
};

class OnDemandCOMClassMachine : public OnDemandCOMClass {
 public:
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_GOOGLE_UPDATE_WORKER_CLASS_MACHINE)
};

}  // namespace omaha

#endif  // OMAHA_WORKER_WORKER_COM_WRAPPER_H__

