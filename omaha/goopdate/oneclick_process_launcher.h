// Copyright 2011 Google Inc.
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

#ifndef OMAHA_GOOPDATE_ONECLICK_PROCESS_LAUNCHER_H__
#define OMAHA_GOOPDATE_ONECLICK_PROCESS_LAUNCHER_H__

#include <atlbase.h>
#include <atlcom.h>
#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/non_localized_resource.h"

namespace omaha {

const TCHAR* const kOneClickProcessLauncherTlbVersion = _T("1.0");
const TCHAR* const kOneClickProcessLauncherDescription =
    _T(SHORT_COMPANY_NAME_ANSI) _T(".OneClickProcessLauncher");

class ATL_NO_VTABLE OneClickProcessLauncher
    : public CComObjectRootEx<CComMultiThreadModel>,
      public CComCoClass<OneClickProcessLauncher>,
      public IOneClickProcessLauncher {
 public:
  OneClickProcessLauncher();
  virtual ~OneClickProcessLauncher();

  DECLARE_NOT_AGGREGATABLE(OneClickProcessLauncher)
  DECLARE_PROTECT_FINAL_CONSTRUCT()

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_LOCAL_SERVER_IE_LOW_RGS)

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"),       goopdate_utils::GetHKRoot())
    REGMAP_MODULE2(_T("MODULE"),
                   is_machine() ? kOmahaBrokerFileName : kOmahaOnDemandFileName)
    REGMAP_ENTRY(_T("VERSION"),      kOneClickProcessLauncherTlbVersion)
    REGMAP_ENTRY(_T("PROGID"),
                 is_machine() ?
                     kProgIDOneClickProcessLauncherMachine :
                     kProgIDOneClickProcessLauncherUser)
    REGMAP_ENTRY(_T("DESCRIPTION"),  kOneClickProcessLauncherDescription)
    REGMAP_UUID(_T("CLSID"),
                is_machine() ?
                    __uuidof(OneClickMachineProcessLauncherClass) :
                    __uuidof(OneClickUserProcessLauncherClass))
  END_REGISTRY_MAP()
  #pragma warning(pop)

  // C4505: unreferenced IUnknown local functions have been removed
  #pragma warning(disable : 4505)
  BEGIN_COM_MAP(OneClickProcessLauncher)
    COM_INTERFACE_ENTRY(IOneClickProcessLauncher)
  END_COM_MAP()

  STDMETHOD(LaunchAppCommand)(const WCHAR* app_guid,
                              const WCHAR* cmd_id);

 private:
  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  DISALLOW_COPY_AND_ASSIGN(OneClickProcessLauncher);
};  // class OneClickProcessLauncher

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_ONECLICK_PROCESS_LAUNCHER_H__
