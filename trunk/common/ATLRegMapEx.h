// Copyright 2005-2009 Google Inc.
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
// Extension to DECLARE_REGISTRY_RESOURCEID that makes adding stuff to
// your reg file as simple as using an atl macro map.
//
// Adapted from http://thecodeproject.com/atl/RegistryMap.asp
/*
 * Defines a 'registry' map for adding variables to rgs files.
 * Original Code Copyright 2001-2003 Michael Geddes.  All rights reserved.
 * Modified Code Copyright 2005 Google Inc.
 */

/* use this as your RGS file -- remove or add parameters as you see fit

HKCR
{
  %PROGID%.%VERSION% = s '%DESCRIPTION%'
  {
    CLSID = s '%CLSID%'
  }
  %PROGID% = s '%DESCRIPTION%'
  {
    CLSID = s '%CLSID%'
    CurVer = s '%PROGID%.%VERSION%'
  }
  NoRemove CLSID
  {
    ForceRemove %CLSID% = s '%DESCRIPTION%'
    {
      ProgID = s '%PROGID%.%VERSION%'
       VersionIndependentProgID = s '%PROGID%'
      ForceRemove 'Programmable'
      InprocServer32 = s '%MODULE%'
      {
        val ThreadingModel = s '%THREADING%'
      }
      'TypeLib' = s '%LIBID%'
    }
  }
}

*/

#ifndef OMAHA_COMMON_ATLREGMAPEX_H__
#define OMAHA_COMMON_ATLREGMAPEX_H__

#include <atlbase.h>
#include <atlconv.h>
#include <atlstr.h>
#include "omaha/common/app_util.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"

namespace omaha {

struct _ATL_REGMAP_ENTRYKeeper : public _ATL_REGMAP_ENTRY {
  // Returns a new Olestr that needs to be freed by caller.
  LPCOLESTR NewOlestrFromTstr(LPCTSTR tstr)  {
    CT2COLE olestr(tstr);
    int alloc_length = lstrlen(olestr) + 1;
    LPOLESTR new_olestr =  new OLECHAR[alloc_length];
    if (new_olestr) {
      lstrcpyn(new_olestr, olestr, alloc_length);
    }
    return new_olestr;
  }

  _ATL_REGMAP_ENTRYKeeper() {
    szKey = NULL;
    szData = NULL;
  }

  // REGMAP_ENTRY(x, y)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr, LPCWSTR data_tstr)  {
    szKey = NewOlestrFromTstr(key_tstr);
    szData = NewOlestrFromTstr(CW2T(data_tstr));
  }

  // REGMAP_ENTRY(x, y)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr, LPCSTR data_tstr)  {
    szKey = NewOlestrFromTstr(key_tstr);
    szData = NewOlestrFromTstr(CA2T(data_tstr));
  }

  // REGMAP_MODULE(x)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr) {
    szKey = NewOlestrFromTstr(key_tstr);
    szData =
        NewOlestrFromTstr(EnclosePathIfExe(app_util::GetCurrentModulePath()));
  }

  // REGMAP_MODULE2(x, modulename)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr,
                          LPCTSTR module_name_tstr,
                          bool /* is_relative_to_current_module */)  {
    szKey = NewOlestrFromTstr(key_tstr);
    szData = NULL;

    CStringW full_module_name(app_util::GetCurrentModuleDirectory());
    full_module_name += _T("\\");
    full_module_name += module_name_tstr;
    szData = NewOlestrFromTstr(EnclosePathIfExe(full_module_name));
  }

  // REGMAP_EXE_MODULE(x)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr,
                          bool /* is_current_exe_module */)  {
    szKey = NewOlestrFromTstr(key_tstr);
    szData = NewOlestrFromTstr(EnclosePathIfExe(app_util::GetModulePath(NULL)));
  }

  // REGMAP_RESOURCE(x, resid)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr, UINT resid, bool /* resource */)  {
    szKey = NewOlestrFromTstr(key_tstr);
    CStringW res_name;
    BOOL success = res_name.LoadString(resid);
    ATLASSERT(success);
    szData = NewOlestrFromTstr(res_name);
  }

  // REGMAP_UUID(x, clsid)
  _ATL_REGMAP_ENTRYKeeper(LPCTSTR key_tstr, REFGUID guid)  {
    szKey = NewOlestrFromTstr(key_tstr);
    szData = NewOlestrFromTstr(GuidToString(guid));
  }

  ~_ATL_REGMAP_ENTRYKeeper()  {
    delete [] szKey;
    delete [] szData;
  }
};

// This now supports DECLARE_OLEMISC_STATUS()
#define BEGIN_REGISTRY_MAP()                                                \
  __if_exists(_GetMiscStatus) {                                             \
    static LPCTSTR _GetMiscStatusString()  {                                \
      static TCHAR misc_string[32] = {0}                                    \
      if (!misc_string[0])  {                                               \
        wsprintf(misc_string, _T("%d"), _GetMiscStatus());                  \
      }                                                                     \
                                                                            \
      return misc_string;                                                   \
    }                                                                       \
  }                                                                         \
                                                                            \
  static struct _ATL_REGMAP_ENTRY *_GetRegistryMap() {                      \
    static const _ATL_REGMAP_ENTRYKeeper map[] = {                          \
      __if_exists(_GetMiscStatusString) {                                   \
        _ATL_REGMAP_ENTRYKeeper(_T("OLEMISC"), _GetMiscStatusString()),     \
      }                                                                     \
      __if_exists(GetAppIdT) {                                              \
        _ATL_REGMAP_ENTRYKeeper(_T("APPID"), GetAppIdT()),                  \
      }                                                                     \

#define REGMAP_ENTRY(x, y) _ATL_REGMAP_ENTRYKeeper((x), (y)),

#define REGMAP_RESOURCE(x, resid) _ATL_REGMAP_ENTRYKeeper((x), (resid), true),

#define REGMAP_UUID(x, clsid) _ATL_REGMAP_ENTRYKeeper((x), (clsid)),

// Add in an entry with key x, and value being the current module path.
// For example, REGMAP_MODULE("foo"), with the current module being
// "goopdate.dll" will result in the entry:
// "foo", "{blah}\\Google\\Update\\1.2.71.7\\goopdate.dll"
#define REGMAP_MODULE(x) _ATL_REGMAP_ENTRYKeeper((x)),

// Add in an entry with key x, and value being modulename, fully qualified with
// the current module path. For example, REGMAP_MODULE2("foo", "npClick7.dll")
// with the current module being "goopdate.dll" will result in the entry:
// "foo", "{blah}\\Google\\Update\\1.2.71.7\\npClick7.dll"
#define REGMAP_MODULE2(x, modulename)                                       \
    _ATL_REGMAP_ENTRYKeeper((x), (modulename), true),

// Add in an entry with key x, and value being the currently running EXE's
// module path. For example, REGMAP_EXE_MODULE("foo"), with the current process
// being googleupdate.exe will result in the entry:
// "foo", "{blah}\\Google\\Update\\googleupdate.exe"
#define REGMAP_EXE_MODULE(x) _ATL_REGMAP_ENTRYKeeper((x), true),

#define END_REGISTRY_MAP() _ATL_REGMAP_ENTRYKeeper()                        \
    };                                                                      \
    return (_ATL_REGMAP_ENTRY *)map;                                        \
  }

#define DECLARE_REGISTRY_RESOURCEID_EX(x)                                   \
  static HRESULT WINAPI UpdateRegistry(BOOL reg) {                          \
  __if_exists(_Module) {                                                    \
    return _Module.UpdateRegistryFromResource((UINT)(x),                    \
                                               (reg),                       \
                                               _GetRegistryMap());          \
  }                                                                         \
  __if_not_exists(_Module) {                                                \
    return ATL::_pAtlModule->UpdateRegistryFromResource((UINT)(x),          \
                                                        (reg),              \
                                                        _GetRegistryMap()); \
  }                                                                         \
}

#define DECLARE_REGISTRY_APPID_RESOURCEID_EX(resid, appid)                  \
  static LPCOLESTR GetAppId() throw() {                                     \
    static const CString app_id(appid);                                     \
    return app_id;                                                          \
  }                                                                         \
  static LPCTSTR GetAppIdT() throw() {                                      \
    return GetAppId();                                                      \
  }                                                                         \
  static HRESULT WINAPI UpdateRegistryAppId(BOOL reg) throw() {             \
    return ATL::_pAtlModule->UpdateRegistryFromResource(resid,              \
                                                        (reg),              \
                                                        _GetRegistryMap()); \
  }
// END registry map

}  // namespace omaha

#endif  // OMAHA_COMMON_ATLREGMAPEX_H__

