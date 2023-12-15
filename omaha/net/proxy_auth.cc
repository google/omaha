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
#include "omaha/net/proxy_auth.h"
#include <atlcom.h>
#if (_MSC_VER < 1800)
#include <pstore.h>
#endif
#include <wincred.h>
#include <wincrypt.h>
#include "omaha/base/commontypes.h"
#include "omaha/base/encrypt.h"
#include "omaha/base/logging.h"
#include "omaha/base/smart_handle.h"
#include "omaha/base/string.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/cred_dialog.h"
#include "omaha/third_party/smartany/scoped_any.h"

using omaha::encrypt::EncryptData;
using omaha::encrypt::DecryptData;

namespace omaha {

#define kIE7CredKey "abe2869f-9b47-4cd9-a358-c22904dba7f7"

bool ProxyAuth::IsPromptAllowed() {
  __mutexScope(lock_);
  return prompt_cancelled_ < cancel_prompt_threshold_;
}

void ProxyAuth::PromptCancelled() {
  __mutexScope(lock_);
  prompt_cancelled_++;
}

CString ProxyAuth::ExtractProxy(const CString& proxy_settings,
                                bool isHttps) {
  if (proxy_settings.IsEmpty()) {
    NET_LOG(L3, (_T("[ProxyAuth::ExtractProxy][Empty settings")));
    return proxy_settings;
  }

  int equals_index = String_FindChar(proxy_settings, L'=');
  if (equals_index >= 0) {
    const wchar_t* prefix = L"http=";
    if (isHttps)
      prefix = L"https=";

    int prefix_index = String_FindString(proxy_settings, prefix);
    if (prefix_index == -1) {
      // fallback to whatever we've got after an equals sign
      prefix = L"=";
      prefix_index = equals_index;
    }

    int first = prefix_index + lstrlen(prefix);
    int length = String_FindChar(proxy_settings, L' ');
    if (length == -1) {
      return proxy_settings.Mid(first);
    } else {
      return proxy_settings.Mid(first, length);
    }
  }

  return proxy_settings;
}

void ProxyAuth::ConfigureProxyAuth(bool is_machine,
                                   uint32 cancel_prompt_threshold) {
  ASSERT1(cancel_prompt_threshold);

  proxy_prompt_is_machine_ = is_machine;
  cancel_prompt_threshold_ = cancel_prompt_threshold;
}

bool ProxyAuth::PromptUser(const CString& server,
                           const ProxyAuthConfig& proxy_auth_config) {
  NET_LOG(L3, (_T("[ProxyAuth::PromptUser][%s][%s]"),
               server, proxy_auth_config.ToString()));

  CString user;
  CString pass;

  HRESULT hr = LaunchCredentialDialog(
      proxy_prompt_is_machine_,
      proxy_auth_config.parent_hwnd,
      server,
      proxy_auth_config.prompt_caption,
      &user,
      &pass);

  if (SUCCEEDED(hr)) {
    AddCred(server, user, pass);
  } else if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
    PromptCancelled();
  }

  SecureZeroMemory(user.GetBuffer(), user.GetAllocLength() * sizeof(TCHAR));
  SecureZeroMemory(pass.GetBuffer(), pass.GetAllocLength() * sizeof(TCHAR));

  return SUCCEEDED(hr);
}

bool ProxyAuth::GetProxyCredentials(bool allow_ui, bool force_ui,
                                    const CString& proxy_server,
                                    const ProxyAuthConfig& proxy_auth_config,
                                    CString* username, CString* password,
                                    uint32* auth_scheme) {
  NET_LOG(L3, (_T("[ProxyAuth::GetProxyCredentials][%d][%s]"),
               allow_ui, proxy_auth_config.ToString()));

  CString server(proxy_server);
  if (server.IsEmpty()) {
    server = kDefaultProxyServer;
  }

  __mutexScope(lock_);
  int i = -1;
  if (!force_ui)
    i = servers_.Find(server);

  if (i == -1) {
    if (ReadFromIE7(server) || ReadFromPreIE7(server) ||
        (allow_ui && proxy_auth_config.parent_hwnd && IsPromptAllowed() &&
         PromptUser(server, proxy_auth_config))) {
      i = servers_.GetSize() - 1;
    }
  }

  if (i >= 0) {
    ASSERT1(!usernames_[i].IsEmpty() && !passwords_[i].empty());
    std::vector<uint8> decrypted_password;
    HRESULT hr = DecryptData(NULL,
                             0,
                             &passwords_[i].front(),
                             passwords_[i].size(),
                             &decrypted_password);
    if (FAILED(hr)) {
      NET_LOG(LE, (_T("[DecryptData failed][0x%x]"), hr));
      return false;
    }

    *username = usernames_[i];
    *password = reinterpret_cast<TCHAR*>(&decrypted_password.front());
    *auth_scheme = auth_schemes_[i];
  }

  NET_LOG(L3, (_T("[ProxyAuth::GetProxyCredentials][%d][%s][%s][%d]"),
               i, proxy_server, *username, *auth_scheme));
  return i >= 0;
}

static bool ParseCredsFromRawBuffer(const BYTE* buffer, const DWORD bytes,
                                    CString* username, CString* password) {
  ASSERT1(bytes > 0);
  if (bytes <= 0)
    return false;

  const char* ascii_buffer = reinterpret_cast<const char*>(buffer);
  const unsigned ascii_length =
    static_cast<const unsigned>(strlen(ascii_buffer));

  // The buffer could be ascii or wide characters, so we detect which one it
  // is and copy the ascii characters to a wide string if necessary
  const wchar_t* user_pass = NULL;
  CString temp;
  if (ascii_length == bytes - 1) {
    temp = AnsiToWideString(ascii_buffer, ascii_length);
    user_pass = temp.GetString();
  } else {
    user_pass = reinterpret_cast<const wchar_t*>(buffer);
  }

  int colon_pos = String_FindChar(user_pass, L':');
  if (colon_pos >= 0) {
    username->SetString(user_pass, colon_pos);
    password->SetString(user_pass + colon_pos + 1);
  }

  return colon_pos >= 0;
}

void ProxyAuth::AddCred(const CString& server, const CString& username,
                        const CString& password) {
  std::vector<uint8> encrypted_password;
  HRESULT hr = EncryptData(NULL,
                           0,
                           password,
                           (password.GetLength() + 1) * sizeof(TCHAR),
                           &encrypted_password);
  if (FAILED(hr)) {
    NET_LOG(LE, (_T("[EncryptData failed][0x%x]"), hr));
    return;
  }

  __mutexScope(lock_);
  int i = servers_.Find(server);
  if (i == -1) {
    servers_.Add(server);
    usernames_.Add(username);
    passwords_.Add(encrypted_password);
    auth_schemes_.Add(UNKNOWN_AUTH_SCHEME);
  } else {
    usernames_[i] = username;
    passwords_[i] = encrypted_password;
    auth_schemes_[i] = UNKNOWN_AUTH_SCHEME;
  }

  NET_LOG(L3, (_T("[ProxyAuth::AddCred][%s][%s]"), server, username));
}

HRESULT ProxyAuth::SetProxyAuthScheme(const CString& proxy_server,
                                      uint32 scheme) {
  CString server(proxy_server);
  if (server.IsEmpty()) {
    server = kDefaultProxyServer;
  }

  __mutexScope(lock_);
  int i = servers_.Find(server);
  if (i == -1) {
    NET_LOG(LE, (_T("[ProxyAuth::SetProxyAuthScheme][%s not found]"), server));
    return E_INVALIDARG;
  }

  auth_schemes_[i] = scheme;
  NET_LOG(L3, (_T("[ProxyAuth::SetProxyAuthScheme][%s][%s][%d]"),
               server, usernames_[i], scheme));
  return S_OK;
}

// This approach (the key in particular) comes from a securityfocus posting:
// http://www.securityfocus.com/archive/1/458115/30/0/threaded
bool ProxyAuth::ReadFromIE7(const CString& server) {
  scoped_library crypt_lib(LoadSystemLibrary(_T("crypt32.dll")));
  ASSERT1(crypt_lib);
  if (!crypt_lib)
    return false;

  typedef BOOL (__stdcall *CryptUnprotectData_type)(DATA_BLOB*, LPWSTR*,
    DATA_BLOB*, PVOID, CRYPTPROTECT_PROMPTSTRUCT*, DWORD, DATA_BLOB*);
  CryptUnprotectData_type CryptUnprotectData_fn =
      reinterpret_cast<CryptUnprotectData_type>(
          GetProcAddress(get(crypt_lib), "CryptUnprotectData"));
  ASSERT1(CryptUnprotectData_fn);
  if (!CryptUnprotectData_fn)
    return false;

  // Load CredEnumerate and CredFree dynamically because they don't exist on
  // Win2K and so loading the GoogleDesktopCommon.dll otherwise.
  scoped_library advapi_lib(LoadSystemLibrary(_T("advapi32.dll")));
  ASSERT1(advapi_lib);
  if (!advapi_lib)
    return false;

  typedef BOOL (__stdcall *CredEnumerateW_type)(LPCWSTR, DWORD, DWORD*,
                                                PCREDENTIAL**);
  CredEnumerateW_type CredEnumerateW_fn =
      reinterpret_cast<CredEnumerateW_type>(
          GetProcAddress(get(advapi_lib), "CredEnumerateW"));
  ASSERT1(CredEnumerateW_fn || SystemInfo::IsRunningOnW2K());
  if (!CredEnumerateW_fn)
    return false;

  typedef VOID (__stdcall *CredFree_type)(PVOID);
  CredFree_type CredFree_fn = reinterpret_cast<CredFree_type>(
      GetProcAddress(get(advapi_lib), "CredFree"));
  ASSERT1(CredFree_fn || SystemInfo::IsRunningOnW2K());
  if (!CredFree_fn)
    return false;

  // Done with dynamically loading methods.  CredEnumerate (and CredFree if
  // we didn't return) will have failed to load on Win2K

  DATA_BLOB optional_entropy;

  char key[ARRAYSIZE(kIE7CredKey)] = kIE7CredKey;
  int16 temp[ARRAYSIZE(key)];
  for (int i = 0; i < ARRAYSIZE(key); ++i)
    temp[i] = static_cast<int16>(key[i] * 4);

  optional_entropy.pbData = reinterpret_cast<BYTE*>(&temp);
  optional_entropy.cbData = sizeof(temp);

  CString target(NOTRANSL(L"Microsoft_WinInet_"));
  target += server;
  target += L"*";

  DWORD count = 0;
  CREDENTIAL** credentials = NULL;
  CString username;
  CString password;

  bool found = false;
  if (CredEnumerateW_fn(target, 0, &count, &credentials)) {
    for (unsigned i = 0; i < count; ++i) {
      if (credentials[i]->Type == CRED_TYPE_GENERIC) {
        DATA_BLOB data_in;
        DATA_BLOB data_out = { 0 };
        data_in.pbData = static_cast<BYTE*>(credentials[i]->CredentialBlob);
        data_in.cbData = credentials[i]->CredentialBlobSize;

        if (CryptUnprotectData_fn(&data_in, NULL, &optional_entropy, NULL, NULL,
                                  0, &data_out)) {
          found = ParseCredsFromRawBuffer(data_out.pbData, data_out.cbData,
                                          &username, &password);
          LocalFree(data_out.pbData);
          if (found) {
            AddCred(server, username, password);
            break;
          }
        }
      }
    }
    CredFree_fn(credentials);
  }

  return found;
}

// The pstore.lib has been deprecated in the newer Windows platforms in favor of
// using the CryptProtectData and CryptUnprotectData functions. Therefore,
// reading credentials from the IE6 is not supported anymore.
bool ProxyAuth::ReadFromPreIE7(const CString& server) {
#if (_MSC_VER < 1800)
  scoped_library pstore_lib(LoadSystemLibrary(_T("pstorec.dll"));
  ASSERT1(pstore_lib);
  if (!pstore_lib)
    return false;

  typedef HRESULT (__stdcall *PStoreCreateInstance_type)(IPStore**,
    PST_PROVIDERID*, void*, DWORD);
  PStoreCreateInstance_type PStoreCreateInstance_fn =
      reinterpret_cast<PStoreCreateInstance_type>(
          GetProcAddress(get(pstore_lib), "PStoreCreateInstance"));
  ASSERT1(PStoreCreateInstance_fn);
  if (!PStoreCreateInstance_fn)
    return false;

  CString username;
  CString password;
  bool found = false;

  scoped_co_init initializer(COINIT_APARTMENTTHREADED);
  HRESULT hr = E_FAIL;

  // The best reference I found about these iterators, especially how to free
  // the item_name returned by this iterator was a microsoft patent application:
  // http://www.patentstorm.us/patents/6272631-description.html
  CComPtr<IPStore> pstore;
  VERIFY_SUCCEEDED(hr = PStoreCreateInstance_fn(&pstore, NULL, NULL, 0));
  if (SUCCEEDED(hr)) {
    CComPtr<IEnumPStoreTypes> enum_types;
    VERIFY_SUCCEEDED(hr = pstore->EnumTypes(PST_KEY_CURRENT_USER, 0,
                                             &enum_types));
    if (SUCCEEDED(hr)) {
      GUID type_guid = { 0 };
      // Get the types one at a time
      while (enum_types->Next(1, &type_guid, NULL) == S_OK) {
        if (type_guid != kPreIE7CredTypeGuid)
          continue;

        CComPtr<IEnumPStoreTypes> enum_subtypes;
        VERIFY_SUCCEEDED(hr = pstore->EnumSubtypes(PST_KEY_CURRENT_USER,
          &type_guid, 0, &enum_subtypes));
        if (SUCCEEDED(hr)) {
          GUID subtype_guid = { 0 };
          // Get the subtypes one at a time
          while (enum_subtypes->Next(1, &subtype_guid, NULL) == S_OK) {
            if (subtype_guid != kPreIE7CredSubtypeGuid)
              continue;

            CComPtr<IEnumPStoreItems> enum_items;
            VERIFY_SUCCEEDED(hr = pstore->EnumItems(PST_KEY_CURRENT_USER,
              &type_guid, &subtype_guid, 0, &enum_items));
            if (SUCCEEDED(hr)) {
              wchar_t* item_name = NULL;
              // Get the items one at a time
              while (enum_items->Next(1, &item_name, NULL) == S_OK) {
                DWORD data_length = 0;
                byte* data = NULL;

                VERIFY_SUCCEEDED(hr = pstore->ReadItem(PST_KEY_CURRENT_USER,
                  &type_guid, &subtype_guid, item_name, &data_length, &data,
                  NULL, 0));
                if (SUCCEEDED(hr)) {
                  found = ParseCredsFromRawBuffer(data, data_length,
                                                  &username, &password);
                  CoTaskMemFree(data);
                }

                CoTaskMemFree(item_name);
                if (found) {
                  AddCred(server, username, password);
                  break;
                }
              }  // end enum_items loop
            }
          }  // end enum_subtypes loop
        }
      }  // end enum_types loop
    }
  }

  return found;
#else
  UNREFERENCED_PARAMETER(server);
  NET_LOG(L3, (_T("[ProxyAuth::ReadFromPreIE7][unsupported in this build]")));
  return false;
#endif
}

}  // namespace omaha

