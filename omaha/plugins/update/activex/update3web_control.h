// Copyright 2009 Google Inc.
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

#ifndef OMAHA_PLUGINS_UPDATE_ACTIVEX_UPDATE3WEB_CONTROL_H_
#define OMAHA_PLUGINS_UPDATE_ACTIVEX_UPDATE3WEB_CONTROL_H_

#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>

#include "base/basictypes.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/constants.h"
#include "omaha/base/omaha_version.h"
#include "common/goopdate_utils.h"
#include "omaha/plugins/update/config.h"
#include "omaha/plugins/update/resource.h"
#include "omaha/plugins/update/site_lock.h"
#include "plugins/update/activex/update_control_idl.h"

namespace omaha {

class Update3WebControl;

typedef IObjectSafetyImpl<Update3WebControl, INTERFACESAFE_FOR_UNTRUSTED_CALLER>
    Update3WebControlSafety;

class ATL_NO_VTABLE Update3WebControl
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<Update3WebControl,
                         &__uuidof(GoogleUpdate3WebControlCoClass)>,
      public IDispatchImpl<IGoogleUpdate3WebControl,
                           &__uuidof(IGoogleUpdate3WebControl),
                           &LIBID_GoogleUpdateControlLib, 0xffff, 0xffff>,
      public Update3WebControlSafety,
      public IObjectWithSiteImpl<Update3WebControl> {
 public:
  Update3WebControl();

  DECLARE_NOT_AGGREGATABLE(Update3WebControl)
  DECLARE_REGISTRY_RESOURCEID_EX(IDR_ONECLICK_RGS)

#pragma warning(push)
// Construction of local static object is not thread-safe
#pragma warning(disable:4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(L"CLSID",              GetObjectCLSID())
    REGMAP_ENTRY(L"PROGID",             kUpdate3WebControlProgId)
    REGMAP_ENTRY(L"HKROOT",             goopdate_utils::GetHKRoot())
    REGMAP_ENTRY(L"SHELLNAME",          is_machine() ? kOmahaBrokerFileName :
                                                       kOmahaOnDemandFileName)
    REGMAP_ENTRY(L"SHELLPATH",          GetShellPathForRegMap())
    // The following entries are actually for the NPAPI plugin
    REGMAP_ENTRY(L"PLUGINDESCRIPTION",  kAppName)
    REGMAP_ENTRY(L"PLUGINDOMAIN",       kGoopdateServer)
    REGMAP_ENTRY(L"PLUGINVENDOR",       kFullCompanyName)
    REGMAP_ENTRY(L"PLUGINVERSION",      kUpdate3WebPluginVersion)
    REGMAP_ENTRY(L"PLUGINPRODUCT",      kAppName)
    REGMAP_ENTRY(L"PLUGINMIMETYPE",     UPDATE3WEB_MIME_TYPE)
  END_REGISTRY_MAP()
#pragma warning(pop)

  BEGIN_COM_MAP(Update3WebControl)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IObjectSafety)
    COM_INTERFACE_ENTRY(IObjectWithSite)
  END_COM_MAP()

  // IGoogleUpdate3WebControl methods.
  STDMETHOD(createOmahaMachineServerAsync)(VARIANT_BOOL create_elevated,
                                           IDispatch** async_status);
  STDMETHOD(createOmahaUserServer)(IDispatch** server);

  // Gets the version of the passed in application guid. If the application is
  // not installed, returns an empty string.
  STDMETHOD(getInstalledVersion)(BSTR guid_string,
                                 VARIANT_BOOL is_machine,
                                 BSTR* version_string);

  // OneClick-equivalent API, used for cross-installs.
  STDMETHOD(crossInstall)(BSTR extra_args);

  // Launches a command defined by an installed application. Fails if the
  // command is not successfully started, succeeds otherwise. Returns without
  // waiting for the command to complete.
  STDMETHOD(launchAppCommand)(BSTR guid_string,
                              VARIANT_BOOL is_machine,
                              BSTR cmd_id);

 protected:
  virtual ~Update3WebControl();

 private:
  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  static CString GetShellPathForRegMap() {
    return goopdate_utils::BuildInstallDirectory(is_machine(),
                                                 GetVersionString());
  }

  HRESULT GetVersionUsingCOMServer(const TCHAR* guid_string,
                                   bool is_machine,
                                   CString* version_string);
  HRESULT GetVersionUsingRegistry(const TCHAR* guid_string,
                                  bool is_machine,
                                  CString* version_string);

  SiteLock site_lock_;

  friend class Update3WebControlTest;

  DISALLOW_COPY_AND_ASSIGN(Update3WebControl);
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_ACTIVEX_UPDATE3WEB_CONTROL_H_
