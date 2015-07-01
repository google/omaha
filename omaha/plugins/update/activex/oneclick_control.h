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

// One-click support for Omaha returning users.

#ifndef OMAHA_PLUGINS_UPDATE_ACTIVEX_ONECLICK_CONTROL_H_
#define OMAHA_PLUGINS_UPDATE_ACTIVEX_ONECLICK_CONTROL_H_

// TODO(omaha): We may want to move sitelock.h to be the "standard" sitelock.h
// file from Microsoft (and move that file to omaha/external) and then have our
// modifications to sitelock be in a derived class within the plugins
// directory.

#include <objsafe.h>
#include <shellapi.h>
#include <winhttp.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include "base/scoped_ptr.h"
#include "omaha/base/ATLRegMapEx.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/plugins/update/config.h"
#include "omaha/plugins/update/resource.h"
#include "omaha/plugins/update/site_lock.h"
#include "plugins/update/activex/update_control_idl.h"

namespace omaha {

class OneClickControl;

typedef IObjectSafetyImpl<OneClickControl, INTERFACESAFE_FOR_UNTRUSTED_CALLER>
            OneClickControlSafety;

// Using 0xffff for the major/minor versions in the IDispatchImpl template will
// make ATL load the typelib directly from the DLL instead of looking up typelib
// registration in registry. The big benefit is that we do not need to register
// the typelib. Also, this is needed for Vista SP1 with UAC off, in which
// oleaut32 does not read typelib information from HKCU, because of a bug.
class ATL_NO_VTABLE OneClickControl
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<OneClickControl,
                         &__uuidof(GoogleUpdateOneClickControlCoClass)>,
      public IDispatchImpl<IGoogleUpdateOneClick,
                           &__uuidof(IGoogleUpdateOneClick),
                           &LIBID_GoogleUpdateControlLib, 0xffff, 0xffff>,
      public OneClickControlSafety,
      public IObjectWithSiteImpl<OneClickControl> {
 public:
  OneClickControl();
  virtual ~OneClickControl();

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_ONECLICK_RGS)

#pragma warning(push)
// C4640: construction of local static object is not thread-safe
#pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("CLSID"),             GetObjectCLSID())
    REGMAP_ENTRY(_T("PROGID"),            kOneclickControlProgId)
    REGMAP_ENTRY(_T("HKROOT"),            goopdate_utils::GetHKRoot())
    REGMAP_ENTRY(_T("SHELLNAME"),         kOmahaWebPluginFileName)
    REGMAP_ENTRY(_T("SHELLPATH"),         GetShellPathForRegMap())

    // The following entries are actually for the NPAPI plugin
    REGMAP_ENTRY(_T("PLUGINDESCRIPTION"), kAppName)
    REGMAP_ENTRY(_T("PLUGINDOMAIN"),      kGoopdateServer)
    REGMAP_ENTRY(_T("PLUGINVENDOR"),      kFullCompanyName)
    REGMAP_ENTRY(_T("PLUGINVERSION"),     kOneclickPluginVersion)
    REGMAP_ENTRY(_T("PLUGINPRODUCT"),     kAppName)
    REGMAP_ENTRY(_T("PLUGINMIMETYPE"),    ONECLICK_MIME_TYPE)
  END_REGISTRY_MAP()
#pragma warning(pop)

  BEGIN_COM_MAP(OneClickControl)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IObjectSafety)
  END_COM_MAP()

  DECLARE_NOT_AGGREGATABLE(OneClickControl)
  DECLARE_PROTECT_FINAL_CONSTRUCT();

  // Installs the application that the passed-in manifest corresponds to.
  STDMETHOD(Install)(BSTR cmd_line_args,
                     VARIANT* success_callback,
                     VARIANT* failure_callback);

  STDMETHOD(Install2)(BSTR extra_args);

  // Gets the version of the passed in application guid. If the application is
  // not installed, returns an empty string.
  STDMETHOD(GetInstalledVersion)(BSTR guid_string,
                                 VARIANT_BOOL is_machine,
                                 BSTR* version_string);

  // Gets the version of the plugin. The value is ONECLICK_PLUGIN_VERSION_ANSI.
  // TODO(omaha3): If possible without causing incompatibilities, change version
  // to a preferred type here and in OneClickWorker.
  STDMETHOD(GetOneClickVersion)(long* version);  // NOLINT

  // Launches a command defined by an installed application. Fails if the
  // command is not successfully started, succeeds otherwise. Returns without
  // waiting for the command to complete.
  STDMETHOD(LaunchAppCommand)(BSTR app_guid,
                              VARIANT_BOOL is_machine,
                              BSTR cmd_id);

 private:
  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  static CString GetShellPathForRegMap() {
    return goopdate_utils::BuildInstallDirectory(is_machine(),
                                                 GetVersionString());
  }

  HRESULT DoGetInstalledVersion(const TCHAR* guid_string,
                                bool is_machine,
                                CString* version_string);

  static bool VariantIsValidCallback(const VARIANT* callback);
  static HRESULT InvokeJavascriptCallback(VARIANT* callback,
                                          const HRESULT* opt_param);

  SiteLock site_lock_;

  // If Admin, returns the path for Machine Goopdate. Else returns path for User
  // Goopdate.
  static CString GetGoopdateShellPathForRegMap();
};

}  // namespace omaha

#endif  // OMAHA_PLUGINS_UPDATE_ACTIVEX_ONECLICK_CONTROL_H_

