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

#ifndef OMAHA_PLUGINS_ONECLICK_H__
#define OMAHA_PLUGINS_ONECLICK_H__

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
#include "omaha/common/ATLRegMapEx.h"
#include "omaha/common/const_addresses.h"
#include "omaha/common/const_config.h"
#include "omaha/common/logging.h"
#include "omaha/common/debug.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/plugins/resource.h"
#include "omaha/plugins/oneclick_worker.h"
#include "plugins/sitelock.h"
#include "plugins/oneclick_idl.h"

// This doesn't get generated into the oneclick_idl.h as an extern and our
// build environment doesn't let us include oneclick_idl_i.c
// since we do it differently so extern define this here.
extern "C" const IID DIID__GoogleUpdateOneClickEvents;

namespace omaha {

class GoopdateCtrl;

typedef IObjectSafetyImpl<GoopdateCtrl, INTERFACESAFE_FOR_UNTRUSTED_CALLER>
            ObjectSafety;

typedef CSiteLock<GoopdateCtrl> SiteLock;

// Using 0xffff for the major/minor versions in the IDispatchImpl template will
// make ATL load the typelib directly from the DLL instead of looking up typelib
// registration in registry. The big benefit is that we do not need to register
// the typelib. Also, this is needed for Vista SP1 with UAC off, in which
// oleaut32 does not read typelib information from HKCU, because of a bug.
class ATL_NO_VTABLE GoopdateCtrl
    : public CComObjectRootEx<CComObjectThreadModel>,
      public CComCoClass<GoopdateCtrl, &__uuidof(GoopdateOneClickControl)>,
      public IDispatchImpl<IGoogleUpdateOneClick,
                           &__uuidof(IGoogleUpdateOneClick),
                           &LIBID_OneClickLib, 0xffff, 0xffff>,
      public ObjectSafety,
      public SiteLock,
      public IObjectWithSiteImpl<GoopdateCtrl> {
 public:
  GoopdateCtrl();
  virtual ~GoopdateCtrl();

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_ONECLICK)

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"),            goopdate_utils::GetHKRoot())
    REGMAP_ENTRY(_T("PROGID"),            kOneClickProgId)
    REGMAP_ENTRY(_T("CLSID"),             __uuidof(GoopdateOneClickControl))
    REGMAP_ENTRY(_T("PLUGINDOMAIN"),      kGoopdateServer)
    REGMAP_ENTRY(_T("PLUGINVERSION"),     ACTIVEX_VERSION_ANSI)
    REGMAP_ENTRY(_T("PLUGINDESCRIPTION"), kCiProgram)
    REGMAP_ENTRY(_T("PLUGINPRODUCT"),     kCiProgram)
    REGMAP_ENTRY(_T("PLUGINVENDOR"),      PUBLISHER_NAME_ANSI)
    REGMAP_ENTRY(_T("PLUGINMIMETYPE"),    kOneClickPluginMimeTypeAnsi)
    REGMAP_ENTRY(_T("SHELLNAME"),         kGoopdateFileName)
    // Not fatal if "SHELLPATH" is empty because the side-effect would be that
    // on Vista, the user will get prompted on invoking one-click.
    REGMAP_ENTRY(_T("SHELLPATH"),         GetGoopdateShellPathForRegMap())
    REGMAP_MODULE2(_T("NPONECLICK.DLL"),  ACTIVEX_FILENAME)
  END_REGISTRY_MAP()
  #pragma warning(pop)

  BEGIN_COM_MAP(GoopdateCtrl)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IObjectWithSite)
    COM_INTERFACE_ENTRY(IObjectSafety)
  END_COM_MAP()

  DECLARE_NOT_AGGREGATABLE(GoopdateCtrl)
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

  // Gets the version of the plugin.  This value will be ACTIVEX_VERSION_ANSI.
  STDMETHOD(GetOneClickVersion)(long* version);

  HRESULT FinalConstruct();
  void FinalRelease();

 private:
  void EnsureWorkerUrlSet();

  scoped_ptr<OneClickWorker> oneclick_worker_;
  bool is_worker_url_set_;

  // If Admin, returns the path for Machine Goopdate. Else returns path for User
  // Goopdate.
  static CString GetGoopdateShellPathForRegMap();
};

}  // namespace omaha.

#endif  // OMAHA_PLUGINS_ONECLICK_H__

