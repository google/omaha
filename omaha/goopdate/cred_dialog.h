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

#ifndef OMAHA_GOOPDATE_CRED_DIALOG_H_
#define OMAHA_GOOPDATE_CRED_DIALOG_H_

#include <atlbase.h>
#include <atlcom.h>
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/goopdate/com_proxy.h"
#include "omaha/goopdate/non_localized_resource.h"

namespace omaha {

class ATL_NO_VTABLE CredentialDialogBase
  : public CComObjectRootEx<CComObjectThreadModel>,
    public ICredentialDialog,
    public StdMarshalInfo {
 public:
  explicit CredentialDialogBase(bool is_machine)
      : StdMarshalInfo(is_machine),
        is_machine_(is_machine) {}

  BEGIN_COM_MAP(CredentialDialogBase)
    COM_INTERFACE_ENTRY(IStdMarshalInfo)
    COM_INTERFACE_ENTRY(ICredentialDialog)
  END_COM_MAP()

  // ICredentialDialog methods.
  STDMETHOD(QueryUserForCredentials)(ULONG_PTR owner_hwnd,
                                     BSTR server,
                                     BSTR caption,
                                     BSTR* username,
                                     BSTR* password);

 protected:
  virtual ~CredentialDialogBase() {}

 private:
  bool is_machine_;

  static HRESULT DoQueryUserForCredentials(
      HWND hwnd,
      BSTR server,
      BSTR caption,
      BSTR* username,
      BSTR* password);

  static DWORD DisplayDialog(
      HWND hwnd,
      LPCTSTR server,
      LPCTSTR message,
      LPCTSTR caption,
      CString* username_out,
      CString* password_out);

  DISALLOW_COPY_AND_ASSIGN(CredentialDialogBase);
};

template <typename T>
class ATL_NO_VTABLE CredentialDialog
    : public CredentialDialogBase,
      public CComCoClass<CredentialDialog<T> > {
 public:
  CredentialDialog() : CredentialDialogBase(T::is_machine()) {}

  DECLARE_NOT_AGGREGATABLE(CredentialDialog);
  DECLARE_REGISTRY_RESOURCEID_EX(T::registry_res_id())

  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), T::hk_root())
    REGMAP_ENTRY(_T("VERSION"), _T("1.0"))
    REGMAP_ENTRY(_T("PROGID"), T::prog_id())
    REGMAP_ENTRY(_T("DESCRIPTION"), MAIN_EXE_BASE_NAME _T(" CredentialDialog"))
    REGMAP_ENTRY(_T("CLSID"), T::class_id())
    REGMAP_MODULE2(_T("MODULE"), kOmahaOnDemandFileName)
  END_REGISTRY_MAP()

 protected:
  virtual ~CredentialDialog() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CredentialDialog);
};


struct CredentialDialogModeUser {
  static bool is_machine() { return false; }
  static const TCHAR* prog_id() { return kProgIDCredentialDialogUser; }
  static GUID class_id() { return __uuidof(CredentialDialogUserClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_RGS; }
  static const TCHAR* hk_root() { return _T("HKCU"); }
};

struct CredentialDialogModeMachine {
  static bool is_machine() { return true; }
  static const TCHAR* prog_id() { return kProgIDCredentialDialogMachine; }
  static GUID class_id() { return __uuidof(CredentialDialogMachineClass); }
  static UINT registry_res_id() { return IDR_LOCAL_SERVER_RGS; }
  static const TCHAR* hk_root() { return _T("HKLM"); }
};

typedef CredentialDialog<CredentialDialogModeUser> CredentialDialogUser;
typedef CredentialDialog<CredentialDialogModeMachine> CredentialDialogMachine;

// A static function that decides whether to display the dialog in-process
// or launch an out-of-process COM server for showing it, and automatically
// handles BSTR/CString conversion.
inline HRESULT LaunchCredentialDialog(
    bool is_machine,
    HWND owner_hwnd,
    const CString& server,
    const CString& caption,
    CString* username_out,
    CString* password_out) {
  ASSERT1(username_out);
  ASSERT1(password_out);

  CAccessToken access_token;
  if (!access_token.GetThreadToken(TOKEN_READ)) {
    // If this thread is currently impersonating a user, that's perfect, as the
    // COM server will be started under that user.  If not, verify that the
    // process isn't running as LocalSystem/LocalService - we cannot show UI
    // in that scenario without impersonating.
    bool is_system = true;
    HRESULT hr = IsSystemProcess(&is_system);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[CredDialog][IsSystemProcess failed][0x%08x]"), hr));
      return hr;
    }
    if (is_system) {
      CORE_LOG(LE, (_T("[CredDialog][Process running as SYSTEM - aborting]")));
      return E_ABORT;
    }
  }

  CComPtr<ICredentialDialog> dialog;
  REFCLSID clsid = is_machine ? __uuidof(CredentialDialogMachineClass) :
                                __uuidof(CredentialDialogUserClass);
  HRESULT hr = dialog.CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[LaunchCredentialDialog][CoCreate failed][0x%08x]"), hr));
    return hr;
  }

  CComBSTR server_bstr(server);
  CComBSTR caption_bstr(caption);
  CComBSTR username_bstr;
  CComBSTR password_bstr;
  hr = dialog->QueryUserForCredentials(reinterpret_cast<ULONG_PTR>(owner_hwnd),
                                       server_bstr,
                                       caption_bstr,
                                       &username_bstr,
                                       &password_bstr);

  if (SUCCEEDED(hr)) {
    username_out->SetString(username_bstr);
    password_out->SetString(password_bstr);
  }
  ::SecureZeroMemory(username_bstr.m_str, username_bstr.ByteLength());
  ::SecureZeroMemory(password_bstr.m_str, password_bstr.ByteLength());

  return hr;
}

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CRED_DIALOG_H_

