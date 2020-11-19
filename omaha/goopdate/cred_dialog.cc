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

#include <windows.h>
#include <wincred.h>
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/client/resource.h"
#include "omaha/goopdate/cred_dialog.h"

namespace omaha {

STDMETHODIMP CredentialDialogBase::QueryUserForCredentials(
    ULONG_PTR owner_hwnd,
    BSTR server,
    BSTR caption,
    BSTR* username,
    BSTR* password) {
  ASSERT1(server && wcslen(server));
  ASSERT1(caption);
  ASSERT1(username);
  ASSERT1(password);

  CORE_LOG(L3, (_T("[LaunchCredentialDialog][COM server launching dialog]")));

  if (!::IsWindow(reinterpret_cast<HWND>(owner_hwnd))) {
    return E_INVALIDARG;
  }

  if (!server || !wcslen(server) || !caption) {
    return E_INVALIDARG;
  }

  if (!username || !password) {
    return E_POINTER;
  }

  CString message;
  message.LoadString(IDS_PROXY_PROMPT_MESSAGE);
  message.FormatMessage(message, server);

  CString capt_cstr(caption);
  if (capt_cstr.IsEmpty()) {
    capt_cstr.LoadString(IDS_PRODUCT_DISPLAY_NAME);
  }

  CString user;
  CString pass;
  DWORD result = DisplayDialog(reinterpret_cast<HWND>(owner_hwnd),
                               server,
                               message,
                               capt_cstr,
                               &user,
                               &pass);

  if (result == NO_ERROR) {
    user.SetSysString(username);
    ::SecureZeroMemory(user.GetBuffer(), user.GetAllocLength() * sizeof(TCHAR));
    pass.SetSysString(password);
    ::SecureZeroMemory(pass.GetBuffer(), pass.GetAllocLength() * sizeof(TCHAR));
    return S_OK;
  }

  return HRESULT_FROM_WIN32(result);
}

DWORD CredentialDialogBase::DisplayDialog(
    HWND hwnd,
    LPCTSTR server,
    LPCTSTR message,
    LPCTSTR caption,
    CString* username_out,
    CString* password_out) {
  scoped_library credui_lib(LoadSystemLibrary(_T("credui.dll")));
  ASSERT1(credui_lib);
  if (!credui_lib) {
    CORE_LOG(L3, (_T("[CredUIPromptForCredentialsW not available]")));
    return ERROR_NOT_READY;
  }

  typedef BOOL (__stdcall *CredUIPromptForCredentialsW_type)(
      PCREDUI_INFO pUiInfo,
      PCTSTR pszTargetName,
      PCtxtHandle Reserved,
      DWORD dwAuthError,
      PCTSTR pszUserName,
      ULONG ulUserNameMaxChars,
      PCTSTR pszPassword,
      ULONG ulPasswordMaxChars,
      PBOOL pfSave,
      DWORD dwFlags);
  CredUIPromptForCredentialsW_type CredUIPromptForCredentialsW_fn =
      reinterpret_cast<CredUIPromptForCredentialsW_type>(
          GetProcAddress(get(credui_lib), "CredUIPromptForCredentialsW"));
  ASSERT1(CredUIPromptForCredentialsW_fn || SystemInfo::IsRunningOnW2K());
  if (!CredUIPromptForCredentialsW_fn) {
    CORE_LOG(L3, (_T("[CredUIPromptForCredentialsW not available]")));
    return ERROR_NOT_READY;
  }

  wchar_t temp_username[CREDUI_MAX_USERNAME_LENGTH + 1] = {};
  wchar_t temp_password[CREDUI_MAX_PASSWORD_LENGTH + 1] = {};
  BOOL check;
  CREDUI_INFO info = {0};
  info.cbSize = sizeof(info);
  info.hwndParent = hwnd;
  info.pszMessageText = message;
  info.pszCaptionText = caption;

  DWORD result;
  do {
    temp_username[0] = L'\0';
    temp_password[0] = L'\0';
    result = CredUIPromptForCredentialsW_fn(
        &info,
        server,
        NULL,
        0,
        temp_username,
        CREDUI_MAX_USERNAME_LENGTH,
        temp_password,
        CREDUI_MAX_PASSWORD_LENGTH,
        &check,
        CREDUI_FLAGS_ALWAYS_SHOW_UI | CREDUI_FLAGS_GENERIC_CREDENTIALS |
        CREDUI_FLAGS_DO_NOT_PERSIST);
    CORE_LOG(L3, (_T("[CredUIPromptForCredentialsW returned %d]"), result));
  } while (result == NO_ERROR && (!temp_username[0] || !temp_password[0]));

  if (result == NO_ERROR) {
    username_out->SetString(temp_username);
    password_out->SetString(temp_password);
  }

  ::SecureZeroMemory(temp_username, sizeof(temp_username));
  ::SecureZeroMemory(temp_password, sizeof(temp_password));

  return result;
}

}  // namespace omaha

