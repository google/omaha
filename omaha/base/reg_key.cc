// Copyright 2003-2009 Google Inc.
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

#include "omaha/base/reg_key.h"

#include <raserror.h>
#include <intsafe.h>
#include <tuple>

#include "omaha/base/logging.h"
#include "omaha/base/static_assert.h"
#include "omaha/base/string.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"

// The bulk of Omaha is designed to run as 32-bit only.  However, the crash
// handler is compiled in a 64-bit form, and it needs to access the 32-bit
// registry view. KEY_WOW64_32KEY is used by default for all registry access.

namespace {

// Declare `::NtDeleteKey` exported from ntdll.lib here, since no official
// header has it.
extern "C" NTSTATUS WINAPI NtDeleteKey(IN HANDLE KeyHandle);

}  // namespace

namespace omaha {

namespace {

// Returns the registry access mask |sam_desired| with the WoW view override
// indicated by |wow_override|.
REGSAM ApplyWoWOverride(REGSAM sam_desired, RegKey::WoWOverride wow_override) {
  return sam_desired | static_cast<REGSAM>(wow_override);
}

}  // namespace

HRESULT RegKey::Close() {
  HRESULT hr = S_OK;
  if (h_key_ != NULL) {
    LONG res = RegCloseKey(h_key_);
    hr = HRESULT_FROM_WIN32(res);
    h_key_ = NULL;
    wow_override_ = k32BitView;
  }
  return hr;
}

HRESULT RegKey::Create(HKEY hKeyParent,
                       const TCHAR * key_name,
                       TCHAR * lpszClass,
                       DWORD options,
                       REGSAM sam_desired,
                       LPSECURITY_ATTRIBUTES lpSecAttr,
                       LPDWORD lpdwDisposition) {
  // lpszClass may be NULL
  ASSERT1(key_name);
  ASSERT1(hKeyParent != NULL);
  ASSERT1((sam_desired & (KEY_WOW64_64KEY | KEY_WOW64_32KEY)) !=
          (KEY_WOW64_64KEY | KEY_WOW64_32KEY));
  DWORD dw;
  HKEY hKey = NULL;
  WoWOverride wow_override = k32BitView;
  if ((sam_desired & KEY_WOW64_64KEY) != 0)
    wow_override = k64BitView;
  else
    sam_desired |= KEY_WOW64_32KEY;
  LONG res = ::RegCreateKeyEx(hKeyParent,
                              key_name,
                              0,
                              lpszClass,
                              options,
                              sam_desired,
                              lpSecAttr,
                              &hKey,
                              &dw);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (lpdwDisposition != NULL)
    *lpdwDisposition = dw;
  // we have to close the currently opened key
  // before replacing it with the new one
  if (hr == S_OK) {
    hr = Close();
    ASSERT1(hr == S_OK);
    h_key_ = hKey;
    wow_override_ = wow_override;
  }
  return hr;
}

HRESULT RegKey::Create(const TCHAR * full_key_name,
                       TCHAR * lpszClass, DWORD options,
                       REGSAM sam_desired,
                       LPSECURITY_ATTRIBUTES lpSecAttr,
                       LPDWORD lpdwDisposition) {
  // lpszClass may be NULL
  ASSERT1(full_key_name);
  ASSERT1((sam_desired & (KEY_WOW64_64KEY | KEY_WOW64_32KEY)) !=
          (KEY_WOW64_64KEY | KEY_WOW64_32KEY));
  CString key_name(full_key_name);

  RootKeyInfo info = GetRootKeyInfo(&key_name);
  if (!info.key) {
    ASSERT(false, (_T("unable to get root key location %s"), full_key_name));
    return HRESULT_FROM_WIN32(ERROR_KEY_NOT_FOUND);
  }

  return Create(info.key, key_name, lpszClass, options,
                ApplyWoWOverride(sam_desired, info.wow_override), lpSecAttr,
                lpdwDisposition);
}

HRESULT RegKey::CreateKeys(const TCHAR* keys_to_create[],
                           DWORD number_of_keys,
                           TCHAR* lpszClass,
                           DWORD options,
                           LPSECURITY_ATTRIBUTES lpSecAttr) {
  ASSERT1(keys_to_create);
  ASSERT1(number_of_keys);

  for (DWORD i = 0; i < number_of_keys; i++) {
    HRESULT hr = CreateKey(keys_to_create[i], lpszClass, options, lpSecAttr);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT RegKey::CreateKey(const TCHAR* full_key_name,
                          TCHAR* lpszClass,
                          DWORD options,
                          LPSECURITY_ATTRIBUTES lpSecAttr) {
  ASSERT1(full_key_name);

  RegKey key;
  HRESULT hr = key.Create(full_key_name,
                          lpszClass,
                          options,
                          KEY_ALL_ACCESS,
                          lpSecAttr,
                          NULL);
  if (FAILED(hr)) {
    UTIL_LOG(L3, (_T("[couldn't create %s reg key]"), full_key_name));
    return hr;
  }

  return S_OK;
}

HRESULT RegKey::Open(HKEY hKeyParent,
                     const TCHAR * key_name,
                     REGSAM sam_desired,
                     DWORD options) {
  ASSERT1(key_name);
  ASSERT1(hKeyParent != NULL);
  ASSERT1((sam_desired & (KEY_WOW64_64KEY | KEY_WOW64_32KEY)) !=
          (KEY_WOW64_64KEY | KEY_WOW64_32KEY));
  HKEY hKey = NULL;
  WoWOverride wow_override = k32BitView;
  if ((sam_desired & KEY_WOW64_64KEY) != 0)
    wow_override = k64BitView;
  else
    sam_desired |= KEY_WOW64_32KEY;
  LONG res = ::RegOpenKeyEx(hKeyParent, key_name, options,
                            sam_desired, &hKey);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  // we have to close the currently opened key
  // before replacing it with the new one
  if (hr == S_OK) {
    // close the currently opened key if any
    hr = Close();
    ASSERT1(hr == S_OK);
    h_key_ = hKey;
    wow_override_ = wow_override;
  }
  return hr;
}

HRESULT RegKey::Open(const TCHAR * full_key_name, REGSAM sam_desired) {
  ASSERT1(full_key_name);
  CString key_name(full_key_name);

  RootKeyInfo info = GetRootKeyInfo(&key_name);
  if (!info.key) {
    ASSERT(false, (_T("unable to get root key for %s"), full_key_name));
    return HRESULT_FROM_WIN32(ERROR_KEY_NOT_FOUND);
  }

  return Open(info.key, key_name,
              ApplyWoWOverride(sam_desired, info.wow_override));
}

// save the key and all of its subkeys and values to a file
HRESULT RegKey::Save(const TCHAR* full_key_name, const TCHAR* file_name) {
  ASSERT1(full_key_name);
  ASSERT1(file_name);

  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);
  if (!info.key) {
    return E_FAIL;
  }

  RegKey key;
  HRESULT hr = key.Open(info.key, key_name,
                        ApplyWoWOverride(KEY_READ, info.wow_override));
  if (FAILED(hr)) {
    return hr;
  }

  System::AdjustPrivilege(SE_BACKUP_NAME, true);
  LONG res = ::RegSaveKey(key.h_key_, file_name, NULL);
  System::AdjustPrivilege(SE_BACKUP_NAME, false);

  return HRESULT_FROM_WIN32(res);
}

// restore the key and all of its subkeys and values which are saved into a file
HRESULT RegKey::Restore(const TCHAR* full_key_name, const TCHAR* file_name) {
  ASSERT1(full_key_name);
  ASSERT1(file_name);

  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);
  if (!info.key) {
    return E_FAIL;
  }

  RegKey key;
  HRESULT hr = key.Open(info.key, key_name,
                        ApplyWoWOverride(KEY_WRITE, info.wow_override));
  if (FAILED(hr)) {
    return hr;
  }

  System::AdjustPrivilege(SE_RESTORE_NAME, true);
  LONG res = ::RegRestoreKey(key.h_key_, file_name, REG_FORCE_RESTORE);
  System::AdjustPrivilege(SE_RESTORE_NAME, false);

  return HRESULT_FROM_WIN32(res);
}

// check if the current key has the specified subkey
bool RegKey::HasSubkey(const TCHAR * key_name) const {
  ASSERT1(key_name);
  ASSERT1(h_key_);

  RegKey key;
  HRESULT hr = key.Open(h_key_, key_name,
                        ApplyWoWOverride(KEY_READ, wow_override_));
  key.Close();
  return S_OK == hr;
}

// static flush key
HRESULT RegKey::FlushKey(const TCHAR * full_key_name) {
  ASSERT1(full_key_name);

  HRESULT hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  // get the root HKEY
  CString key_name(full_key_name);
  HKEY h_key = GetRootKeyInfo(&key_name).key;

  if (h_key != NULL) {
    LONG res = RegFlushKey(h_key);
    hr = HRESULT_FROM_WIN32(res);
  }
  return hr;
}

// static SET helper
HRESULT RegKey::SetValueStaticHelper(const TCHAR * full_key_name,
                                     const TCHAR * value_name,
                                     DWORD type,
                                     LPVOID value,
                                     size_t byte_count) {
  // value_name may be NULL
  ASSERT1(full_key_name);

  HRESULT hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  if (info.key != NULL) {
    RegKey key;
    hr = key.Create(info.key, key_name.GetString(), NULL,
                    REG_OPTION_NON_VOLATILE,
                    ApplyWoWOverride(KEY_ALL_ACCESS, info.wow_override));
    if (hr == S_OK) {
      switch (type) {
        case REG_DWORD:
          hr = key.SetValue(value_name, *reinterpret_cast<DWORD *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote int32 value: %s:%s = %d]"),
                          full_key_name,
                          value_name,
                          *reinterpret_cast<DWORD*>(value)));
          }
          break;
        case REG_QWORD:
          hr = key.SetValue(value_name, *reinterpret_cast<DWORD64 *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote int64 value: %s:%s = %s]"),
                          full_key_name,
                          value_name,
                          String_Int64ToString(
                              *reinterpret_cast<DWORD64*>(value), 10)));
          }
          break;
        case REG_SZ:
          hr = key.SetValue(value_name, reinterpret_cast<const TCHAR *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote string value: %s:%s = %s]"),
                          full_key_name,
                          value_name,
                          reinterpret_cast<const TCHAR *>(value)));
          }
          break;
        case REG_BINARY:
          hr = key.SetValue(value_name,
                            reinterpret_cast<const byte *>(value),
                            byte_count);
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote binary value: %s:%s, len = %d]"),
                          full_key_name, value_name, byte_count));
          }
          break;
        case REG_MULTI_SZ:
          hr = key.SetValue(value_name,
                            reinterpret_cast<const byte *>(value),
                            byte_count,
                            type);
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote multi-sz value: %s:%s, len = %d]"),
                          full_key_name, value_name, byte_count));
          }
          break;
        case REG_EXPAND_SZ:
          hr = key.SetStringValue(value_name,
                                  reinterpret_cast<const TCHAR *>(value),
                                  type);
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Wrote expandable string value: %s:%s = %s]"),
                          full_key_name, value_name, (const TCHAR *)value));
          }
          break;
        default:
          ASSERT(false, (_T("Unsupported Registry Type")));
          hr = HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
          break;
      }
      // close the key after writing
      HRESULT temp_res = key.Close();
      if (hr == S_OK) {
        hr = temp_res;
      } else {
        ASSERT(false, (_T("Failed to write reg value: %s:%s (hr=0x%x)"),
                       full_key_name, value_name, hr));
      }
    } else {
      UTIL_LOG(L3, (_T("[Failed to create reg key: %s]"), full_key_name));
    }
  }
  return hr;
}

// static GET helper
// byte_count may be NULL.
// value_name may be NULL.
HRESULT RegKey::GetValueStaticHelper(const TCHAR * full_key_name,
                                     const TCHAR * value_name,
                                     DWORD type,
                                     LPVOID value,
                                     size_t * byte_count) {
  ASSERT1(full_key_name);

  HRESULT hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  if (info.key != NULL) {
    RegKey key;
    hr = key.Open(info.key, key_name.GetString(),
                  ApplyWoWOverride(KEY_READ, info.wow_override));
    if (hr == S_OK) {
      switch (type) {
        case REG_DWORD:
          hr = key.GetValue(value_name, reinterpret_cast<DWORD *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Read int32 value: %s:%s = %d]"),
                          full_key_name,
                          value_name,
                          *reinterpret_cast<DWORD*>(value)));
          }
          break;
        case REG_QWORD:
          hr = key.GetValue(value_name, reinterpret_cast<DWORD64 *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Read int64 value: %s:%s = %s]"),
                          full_key_name,
                          value_name,
                          String_Int64ToString(
                              *(reinterpret_cast<DWORD64*>(value)), 10)));
          }
          break;
        case REG_SZ:
          hr = key.GetValue(value_name, reinterpret_cast<TCHAR * *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Read string value: %s:%s = %s]"),
                          full_key_name,
                          value_name,
                          *reinterpret_cast<TCHAR * *>(value)));
          }
          break;
        case REG_MULTI_SZ:
          hr = key.GetValue(value_name,
                            reinterpret_cast<std::vector<CString> *>(value));
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Read multi string value: %s:%s = %d]"),
                full_key_name,
                value_name,
                reinterpret_cast<std::vector<CString>*>(value)->size()));
          }
          break;
        case REG_BINARY:
          hr = key.GetValue(value_name,
                            reinterpret_cast<byte**>(value),
                            byte_count);
          if (SUCCEEDED(hr)) {
            UTIL_LOG(L6, (_T("[Read binary value: %s:%s, len = %d]"),
                          full_key_name, value_name, byte_count));
          }
          break;
        default:
          ASSERT(false, (_T("Unsupported Registry Type")));
          hr = HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
          break;
      }
      // close the key after writing
      HRESULT temp_res = key.Close();
      if (hr == S_OK) {
        hr = temp_res;
      } else {
        UTIL_LOG(L5, (_T("[Failed to read reg value: %s:%s]"),
                      full_key_name, value_name));
      }
    } else {
      UTIL_LOG(L5, (_T("[reg value does not exist: %s]"), key_name));
    }
  }
  return hr;
}

// GET helper
// value_name may be NULL.
HRESULT RegKey::GetValueHelper(const TCHAR * value_name,
                               DWORD * type,
                               byte** value,
                               size_t * byte_count) const {
  ASSERT1(byte_count);
  ASSERT1(value);
  ASSERT1(type);
  ASSERT1(h_key_);

  *value = NULL;

  DWORD num_bytes = 0;
  LONG res = ::SHQueryValueEx(h_key_, value_name, NULL, type, NULL, &num_bytes);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr == S_OK) {
    if (num_bytes != 0) {
      *value = new byte[num_bytes];
      ASSERT1(*value);

      res = ::SHQueryValueEx(h_key_,
                             value_name,
                             NULL,
                             type,
                             *value,
                             &num_bytes);
      hr = HRESULT_FROM_WIN32(res);
      ASSERT1(S_OK == hr);
    }
  }

  *byte_count = num_bytes;

  return hr;
}

// value_name may be NULL
HRESULT RegKey::GetValueType(const TCHAR* value_name,
                             DWORD* value_type) const {
  ASSERT1(value_type);

  *value_type = REG_NONE;

  LONG res = ::SHQueryValueEx(h_key_, value_name, NULL, value_type, NULL, NULL);
  if (res != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(res);
  }

  return S_OK;
}

// Int32 Get
// value_name may be NULL.
HRESULT RegKey::GetValue(const TCHAR * value_name, DWORD * value) const {
  ASSERT1(value);
  ASSERT1(h_key_);

  DWORD type = 0;
  DWORD byte_count = sizeof(DWORD);
  LONG res = ::SHQueryValueEx(h_key_,
                              value_name,
                              NULL,
                              &type,
                              reinterpret_cast<byte*>(value),
                              &byte_count);
  HRESULT hr = HRESULT_FROM_WIN32(res);
  ASSERT1((hr != S_OK) || (type == REG_DWORD));
  ASSERT1((hr != S_OK) || (byte_count == sizeof(DWORD)));
  return hr;
}

// Int64 Get
// value_name may be NULL.
HRESULT RegKey::GetValue(const TCHAR * value_name, DWORD64 * value) const {
  ASSERT1(value);
  ASSERT1(h_key_);

  DWORD type = 0;
  DWORD byte_count = sizeof(DWORD64);
  LONG res = ::SHQueryValueEx(h_key_,
                              value_name,
                              NULL,
                              &type,
                              reinterpret_cast<byte *>(value),
                              &byte_count);
  HRESULT hr = HRESULT_FROM_WIN32(res);
  ASSERT1((hr != S_OK) || (type == REG_QWORD));
  ASSERT1((hr != S_OK) || (byte_count == sizeof(DWORD64)));
  return hr;
}

// String Get
// value_name may be NULL.
HRESULT RegKey::GetValue(const TCHAR * value_name,
                         std::unique_ptr<TCHAR[]>* value_ptr) const {
  ASSERT1(value_name);
  ASSERT1(value_ptr);
  TCHAR* value;
  HRESULT hr = GetValue(value_name, &value);
  value_ptr->reset(value);
  return hr;
}

HRESULT RegKey::GetValue(const TCHAR * value_name, TCHAR * * value) const {
  ASSERT1(value);
  ASSERT1(h_key_);

  DWORD byte_count = 0;
  DWORD type = 0;

  // first get the size of the string buffer
  LONG res = ::SHQueryValueEx(h_key_,
                              value_name,
                              NULL,
                              &type,
                              NULL,
                              &byte_count);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr == S_OK) {
    // allocate room for the string and a terminating \0
    *value = new TCHAR[(byte_count / sizeof(TCHAR)) + 1];

    if ((*value) != NULL) {
      if (byte_count != 0) {
        // make the call again
        res = ::SHQueryValueEx(h_key_, value_name, NULL, &type,
                               reinterpret_cast<byte*>(*value), &byte_count);
        hr = HRESULT_FROM_WIN32(res);
      } else {
        (*value)[0] = _T('\0');
      }

      ASSERT1((hr != S_OK) || (type == REG_SZ) ||
              (type == REG_MULTI_SZ) || (type == REG_EXPAND_SZ));
    } else {
      hr = E_OUTOFMEMORY;
    }
  }

  return hr;
}

// CString Get
// value_name may be NULL.
HRESULT RegKey::GetValue(const TCHAR* value_name, OUT CString* value) const {
  ASSERT1(value);
  ASSERT1(h_key_);

  DWORD byte_count = 0;
  DWORD type = 0;

  // first get the size of the string buffer
  LONG res = ::SHQueryValueEx(h_key_,
                              value_name,
                              NULL,
                              &type,
                              NULL,
                              &byte_count);
  HRESULT hr = HRESULT_FROM_WIN32(res);

  if (hr == S_OK) {
    if (byte_count != 0) {
      // Allocate some memory and make the call again
      TCHAR* buffer = value->GetBuffer(byte_count / sizeof(TCHAR) + 1);
      if (buffer == NULL) {
        hr = E_OUTOFMEMORY;
      } else {
        res = ::SHQueryValueEx(h_key_, value_name, NULL, &type,
                               reinterpret_cast<byte*>(buffer), &byte_count);
        hr = HRESULT_FROM_WIN32(res);
      }
      value->ReleaseBuffer();
    } else {
      value->Empty();
    }

    ASSERT1((hr != S_OK) || (type == REG_SZ) ||
            (type == REG_MULTI_SZ) || (type == REG_EXPAND_SZ));
  }

  return hr;
}

// convert REG_MULTI_SZ bytes to string array
HRESULT RegKey::MultiSZBytesToStringArray(const byte * buffer,
                                          size_t byte_count,
                                          std::vector<CString> * value) {
  ASSERT1(buffer);
  ASSERT1(value);

  const TCHAR* data = reinterpret_cast<const TCHAR*>(buffer);
  size_t data_len = byte_count / sizeof(TCHAR);
  value->clear();
  if (data_len > 1) {
    // must be terminated by two null characters
    if (data[data_len - 1] != 0 || data[data_len - 2] != 0) {
      return E_INVALIDARG;
    }

    // put null-terminated strings into arrays
    while (*data) {
      CString str(data);
      value->push_back(str);
      data += str.GetLength() + 1;
    }
  }
  return S_OK;
}

// get a vector<CString> value from REG_MULTI_SZ type
HRESULT RegKey::GetValue(const TCHAR * value_name,
                         std::vector<CString> * value) const {
  ASSERT1(value);
  // value_name may be NULL

  size_t byte_count = 0;
  DWORD type = 0;
  byte* buffer = 0;

  // first get the size of the buffer
  HRESULT hr = GetValueHelper(value_name, &type, &buffer, &byte_count);
  ASSERT1((hr != S_OK) || (type == REG_MULTI_SZ));

  if (SUCCEEDED(hr)) {
    hr = MultiSZBytesToStringArray(buffer, byte_count, value);
  }

  return hr;
}

HRESULT RegKey::GetValue(const TCHAR * value_name,
                         std::unique_ptr<byte[]>* value,
                         size_t * byte_count) const {
  ASSERT1(byte_count);
  ASSERT1(value);
  // value_name may be NULL

  DWORD type = 0;
  BYTE* byte = nullptr;
  HRESULT hr = GetValueHelper(value_name, &type, &byte, byte_count);
  value->reset(byte);
  ASSERT1((hr != S_OK) || (type == REG_MULTI_SZ) || (type == REG_BINARY));
  return hr;
}

// Binary data Get
HRESULT RegKey::GetValue(const TCHAR * value_name,
                         byte** value,
                         size_t * byte_count) const {
  ASSERT1(byte_count);
  ASSERT1(value);
  // value_name may be NULL

  DWORD type = 0;
  HRESULT hr = GetValueHelper(value_name, &type, value, byte_count);
  ASSERT1((hr != S_OK) || (type == REG_MULTI_SZ) || (type == REG_BINARY));
  return hr;
}

// Raw data get
HRESULT RegKey::GetValue(const TCHAR * value_name,
                         std::unique_ptr<byte[]>* value_ptr,
                         size_t * byte_count,
                         DWORD *type) const {
  ASSERT1(type);
  ASSERT1(byte_count);
  ASSERT1(value_ptr);
  byte* value;
  HRESULT hr = GetValue(value_name, &value, byte_count, type);
  value_ptr->reset(value);
  return hr;
}

HRESULT RegKey::GetValue(const TCHAR * value_name,
                         byte** value,
                         size_t * byte_count,
                         DWORD *type) const {
  ASSERT1(type);
  ASSERT1(byte_count);
  ASSERT1(value);

  return GetValueHelper(value_name, type, value, byte_count);
}

// Int32 set
// value_name may be NULL
HRESULT RegKey::SetValue(const TCHAR * value_name, DWORD value) const {
  ASSERT1(h_key_);
  LONG res = RegSetValueEx(h_key_,
                           value_name,
                           NULL,
                           REG_DWORD,
                           reinterpret_cast<byte *>(&value),
                           sizeof(DWORD));
  return HRESULT_FROM_WIN32(res);
}

// Int64 set
// value_name may be NULL
HRESULT RegKey::SetValue(const TCHAR * value_name, DWORD64 value) const {
  ASSERT1(h_key_);
  LONG res = RegSetValueEx(h_key_,
                           value_name,
                           NULL,
                           REG_QWORD,
                           reinterpret_cast<byte *>(&value),
                           sizeof(DWORD64));
  return HRESULT_FROM_WIN32(res);
}

// String set
HRESULT RegKey::SetValue(const TCHAR * value_name, const TCHAR * value) const {
  return SetStringValue(value_name, value, REG_SZ);
}

// String set helper
// value_name may be NULL.
HRESULT RegKey::SetStringValue(const TCHAR * value_name,
                               const TCHAR * value,
                               DWORD type) const {
  ASSERT1(value);
  ASSERT1(h_key_);
  ASSERT1(type == REG_SZ || type == REG_EXPAND_SZ);
  LONG res = RegSetValueEx(h_key_,
                           value_name,
                           NULL,
                           type,
                           reinterpret_cast<const byte *>(value),
                           (lstrlen(value) + 1) * sizeof(TCHAR));
  return HRESULT_FROM_WIN32(res);
}

// Binary data set
// value may be NULL.
// value_name may be NULL.
HRESULT RegKey::SetValue(const TCHAR * value_name,
                         const byte * value,
                         size_t byte_count) const {
  ASSERT1(h_key_);

  // special case - if 'value' is NULL make sure byte_count is zero
  if (value == NULL) {
    byte_count = 0;
  }

  if (byte_count > DWORD_MAX) {
    return E_INVALIDARG;
  }

  LONG res = RegSetValueEx(h_key_,
                           value_name,
                           NULL,
                           REG_BINARY,
                           value,
                           static_cast<DWORD>(byte_count));
  return HRESULT_FROM_WIN32(res);
}

// Raw data set
// value_name may be NULL.
HRESULT RegKey::SetValue(const TCHAR * value_name,
                         const byte * value,
                         size_t byte_count,
                         DWORD type) const {
  ASSERT1(value);
  ASSERT1(h_key_);

  if (byte_count > DWORD_MAX) {
    return E_INVALIDARG;
  }

  LONG res = RegSetValueEx(h_key_,
                           value_name,
                           NULL,
                           type,
                           value,
                           static_cast<DWORD>(byte_count));
  return HRESULT_FROM_WIN32(res);
}

HRESULT RegKey::RenameValue(const TCHAR* old_value_name,
                            const TCHAR* new_value_name) const {
  ASSERT1(h_key_);
  ASSERT1(new_value_name);
  ASSERT1(old_value_name);

  std::unique_ptr<byte[]> value;
  size_t byte_count = 0;
  DWORD type = 0;

  HRESULT hr = GetValue(old_value_name, &value, &byte_count, &type);
  if (FAILED(hr)) {
    return hr;
  }

  hr = SetValue(new_value_name, value.get(), byte_count, type);
  if (FAILED(hr)) {
    return hr;
  }

  VERIFY_SUCCEEDED(DeleteValue(old_value_name));
  return S_OK;
}

bool RegKey::HasKey(const TCHAR * full_key_name) {
  return HasKeyHelper(full_key_name, KEY_READ);
}

bool RegKey::HasKeyHelper(const TCHAR * full_key_name, DWORD sam_flags) {
  ASSERT1(full_key_name);
  ASSERT1(sam_flags & KEY_READ);

  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  if (info.key != NULL) {
    RegKey key;
    HRESULT hr = key.Open(info.key, key_name.GetString(),
                          ApplyWoWOverride(sam_flags, info.wow_override));
    key.Close();
    return S_OK == hr;
  }
  return false;
}

HRESULT RegKey::CopyValue(const TCHAR * full_from_key_name,
                          const TCHAR * from_value_name,
                          const TCHAR * full_to_key_name,
                          const TCHAR * to_value_name) {
  ASSERT1(full_from_key_name);
  ASSERT1(full_to_key_name);

  RegKey from_reg_key;
  HRESULT hr = from_reg_key.Open(full_from_key_name, KEY_READ);
  if (FAILED(hr)) {
    return hr;
  }

  std::unique_ptr<byte[]> val;
  size_t byte_count = 0;
  DWORD type = 0;
  hr = from_reg_key.GetValue(from_value_name, &val, &byte_count, &type);
  if (FAILED(hr)) {
    return hr;
  }

  RegKey to_reg_key;
  hr = to_reg_key.Open(full_to_key_name, KEY_WRITE);
  if (FAILED(hr)) {
    return hr;
  }

  return to_reg_key.SetValue(to_value_name, val.get(), byte_count, type);
}

// static version of HasValue
bool RegKey::HasValue(const TCHAR * full_key_name, const TCHAR * value_name) {
  ASSERT1(full_key_name);

  bool has_value = false;
  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  if (info.key != NULL) {
    RegKey key;
    if (key.Open(info.key, key_name.GetString(),
                 ApplyWoWOverride(KEY_READ, info.wow_override)) == S_OK) {
      has_value = key.HasValue(value_name);
      key.Close();
    }
  }
  return has_value;
}

HRESULT RegKey::GetValueType(const TCHAR* full_key_name,
                             const TCHAR* value_name,
                             DWORD* value_type) {
  ASSERT1(full_key_name);
  // value_name may be NULL
  ASSERT1(value_type);

  *value_type = REG_NONE;

  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  RegKey key;
  HRESULT hr = key.Open(info.key, key_name,
                        ApplyWoWOverride(KEY_READ, info.wow_override));
  if (FAILED(hr)) {
    return hr;
  }
  return key.GetValueType(value_name, value_type);
}

HRESULT RegKey::DeleteKey(const TCHAR* full_key_name) {
  ASSERT1(full_key_name);

  return DeleteKey(full_key_name, true);
}

HRESULT RegKey::DeleteKey(const TCHAR* full_key_name, bool recursively) {
  ASSERT1(full_key_name);

  // need to open the parent key first
  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  // get the parent key
  CString parent_key(GetParentKeyInfo(&key_name));

  RegKey key;
  HRESULT hr = key.Open(info.key, parent_key,
                        ApplyWoWOverride(KEY_ALL_ACCESS, info.wow_override));

  if (hr == S_OK) {
    hr = recursively ? key.RecurseDeleteSubKey(key_name) :
                       key.DeleteSubKey(key_name);
  } else if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
             hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
    hr = S_FALSE;
  }

  key.Close();
  return hr;
}

HRESULT RegKey::DeleteValue(const TCHAR * full_key_name,
                            const TCHAR * value_name) {
  ASSERT1(value_name);
  ASSERT1(full_key_name);

  HRESULT hr = HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  // get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  if (info.key != NULL) {
    RegKey key;
    hr = key.Open(info.key, key_name.GetString(),
                  ApplyWoWOverride(KEY_ALL_ACCESS, info.wow_override));
    if (hr == S_OK) {
      hr = key.DeleteValue(value_name);
      key.Close();
    }
  }
  return hr;
}

std::tuple<bool, HRESULT> RegKey::DeleteLink(const TCHAR* key_name) {
  ASSERT1(key_name);
  ASSERT1(h_key_);

  RegKey maybe_link;
  HRESULT hr = maybe_link.Open(h_key_,
                               key_name,
                               KEY_QUERY_VALUE | DELETE,
                               REG_OPTION_OPEN_LINK);
  if (FAILED(hr)) {
    return {false, hr};
  }

  DWORD value_type = 0;
  hr = maybe_link.GetValueType(L"SymbolicLinkValue", &value_type);
  if (FAILED(hr) || value_type != REG_LINK) {
    return {false, hr};
  }

  // `::NtDeleteKey` can delete symbolic links opened with
  // `REG_OPTION_OPEN_LINK`.
  return {true, HRESULT_FROM_NT(NtDeleteKey(maybe_link.h_key_))};
}

HRESULT RegKey::RecurseDeleteSubKey(const TCHAR * key_name) {
  ASSERT1(key_name);
  ASSERT1(h_key_);

  const std::tuple<bool, HRESULT> delete_result = DeleteLink(key_name);
  if (std::get<0>(delete_result)) {
    return std::get<1>(delete_result);
  }

  RegKey key;
  HRESULT hr = key.Open(h_key_, key_name,
                        ApplyWoWOverride(KEY_ALL_ACCESS, wow_override_));
  if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
      hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
    hr = S_FALSE;
  }
  if (hr != S_OK) {
    return hr;
  }

  // enumerate all subkeys of this key
  // and recursivelly delete them
  FILETIME time;
  TCHAR key_name_buf[kMaxKeyNameChars];
  DWORD key_name_buf_size = kMaxKeyNameChars;
  while (RegEnumKeyEx(key.h_key_,
                      0,
                      key_name_buf,
                      &key_name_buf_size,
                      NULL,
                      NULL,
                      NULL,
                      &time) == ERROR_SUCCESS) {
    hr = key.RecurseDeleteSubKey(key_name_buf);
    // return if error deleting key
    if (hr != S_OK)
      return hr;
    // restore the buffer size
    key_name_buf_size = kMaxKeyNameChars;
  }
  // close the top key
  key.Close();

  // the key has no more children keys
  // delete the key and all of its values
  return DeleteSubKey(key_name);
}

RegKey::RootKeyInfo RegKey::GetRootKeyInfo(CString* full_key_name) {
  ASSERT1(full_key_name);

  // All accesses go to the 32-bit view of the registry unless overridden.
  RootKeyInfo result = {NULL, k32BitView};

  // get the root HKEY
  int index = String_FindChar(*(full_key_name), '\\');
  CString root_key;

  if (index == -1) {
    root_key = *full_key_name;
    *full_key_name = _T("");
  } else {
    root_key = full_key_name->Left(index);
    *full_key_name =
        full_key_name->Right(full_key_name->GetLength() - index - 1);
  }

  if (!root_key.CompareNoCase(_T("HKLM")) ||
      !root_key.CompareNoCase(_T("HKEY_LOCAL_MACHINE"))) {
    result.key = HKEY_LOCAL_MACHINE;
  } else if (!root_key.CompareNoCase(_T("HKCU")) ||
             !root_key.CompareNoCase(_T("HKEY_CURRENT_USER"))) {
    result.key = HKEY_CURRENT_USER;
  } else if (!root_key.CompareNoCase(_T("HKU")) ||
             !root_key.CompareNoCase(_T("HKEY_USERS"))) {
    result.key = HKEY_USERS;
  } else if (!root_key.CompareNoCase(_T("HKCR")) ||
             !root_key.CompareNoCase(_T("HKEY_CLASSES_ROOT"))) {
    result.key = HKEY_CLASSES_ROOT;
  } else if (!root_key.CompareNoCase(_T("HKLM[64]")) ||
             !root_key.CompareNoCase(_T("HKEY_LOCAL_MACHINE[64]"))) {
    result.key = HKEY_LOCAL_MACHINE;
    result.wow_override = k64BitView;
  }

  return result;
}


// Returns true if this key name is 'safe' for deletion (doesn't specify a
// key root)
bool RegKey::SafeKeyNameForDeletion(const wchar_t *key_name) {
  ASSERT1(key_name);
  CString key(key_name);

  HKEY root_key = GetRootKeyInfo(&key).key;

  if ( !root_key ) {
    key = key_name;
  }
  if ( key.IsEmpty() ) {
    return false;
  }
  bool found_subkey = false, backslash_found = false;
  for (int i = 0 ; i < key.GetLength() ; ++i) {
    if ( key[i] == L'\\' ) {
      backslash_found = true;
    } else if ( backslash_found ) {
      found_subkey = true;
      break;
    }
  }
  return ( root_key == HKEY_USERS ) ? found_subkey : true;
}

CString RegKey::GetParentKeyInfo(CString * key_name) {
  ASSERT1(key_name);

  // get the parent key
  int index = key_name->ReverseFind('\\');
  CString parent_key;
  if (index == -1) {
    parent_key = _T("");
  } else {
    parent_key = key_name->Left(index);
    *key_name = key_name->Right(key_name->GetLength() - index - 1);
  }

  return parent_key;
}

// get the number of values for this key
uint32 RegKey::GetValueCount() {
  ASSERT1(h_key_);
  // number of values for key
  DWORD  num_values = 0;

  LONG res = ::RegQueryInfoKey(h_key_,       // key handle
                               NULL,         // buffer for class name
                               NULL,         // size of class string
                               NULL,         // reserved
                               NULL,         // number of subkeys
                               NULL,         // longest subkey size
                               NULL,         // longest class string
                               &num_values,  // number of values for this key
                               NULL,         // longest value name
                               NULL,         // longest value data
                               NULL,         // security descriptor
                               NULL);        // last write time

  ASSERT1(res == ERROR_SUCCESS);
  return num_values;
}

// Enumerators for the value_names for this key

// Called to get the value name for the given value name index
// Use GetValueCount() to get the total value_name count for this key
// Returns failure if no key at the specified index
// type may be NULL.
HRESULT RegKey::GetValueNameAt(int index, CString *value_name, DWORD *type) {
  ASSERT1(value_name);
  ASSERT1(h_key_);

  LONG res = ERROR_SUCCESS;
  TCHAR value_name_buf[kMaxValueNameChars];
  DWORD value_name_buf_size = kMaxValueNameChars;
  res = ::RegEnumValue(h_key_,
                       index,
                       value_name_buf,
                       &value_name_buf_size,
                       NULL,
                       type,
                       NULL,
                       NULL);

  if (res == ERROR_SUCCESS) {
    value_name->SetString(value_name_buf);
  }

  return HRESULT_FROM_WIN32(res);
}

uint32 RegKey::GetSubkeyCount() {
  ASSERT1(h_key_);

  DWORD num_subkeys = 0;   // number of values for key

  LONG res = ::RegQueryInfoKey(h_key_,        // key handle
                               NULL,          // buffer for class name
                               NULL,          // size of class string
                               NULL,          // reserved
                               &num_subkeys,  // number of subkeys
                               NULL,          // longest subkey size
                               NULL,          // longest class string
                               NULL,          // number of values for this key
                               NULL,          // longest value name
                               NULL,          // longest value data
                               NULL,          // security descriptor
                               NULL);         // last write time

  ASSERT1(res == ERROR_SUCCESS);
  return num_subkeys;
}

HRESULT RegKey::GetSubkeyNameAt(int index, CString * key_name) {
  ASSERT1(key_name);
  ASSERT1(h_key_);

  LONG res = ERROR_SUCCESS;
  TCHAR key_name_buf[kMaxKeyNameChars];
  DWORD key_name_buf_size = kMaxKeyNameChars;

  res = ::RegEnumKeyEx(h_key_,
                       index,
                       key_name_buf,
                       &key_name_buf_size,
                       NULL,
                       NULL,
                       NULL,
                       NULL);

  if (res == ERROR_SUCCESS) {
    key_name->SetString(key_name_buf);
  }

  return HRESULT_FROM_WIN32(res);
}

// Is the key empty: having no sub-keys and values
bool RegKey::IsKeyEmpty(const TCHAR* full_key_name) {
  ASSERT1(full_key_name);

  bool is_empty = true;

  // Get the root HKEY
  CString key_name(full_key_name);
  RootKeyInfo info = GetRootKeyInfo(&key_name);

  // Open the key to check
  if (info.key != NULL) {
    RegKey key;
    HRESULT hr = key.Open(info.key, key_name.GetString(),
                          ApplyWoWOverride(KEY_READ, info.wow_override));
    if (SUCCEEDED(hr)) {
      is_empty = key.GetSubkeyCount() == 0 && key.GetValueCount() == 0;
      key.Close();
    }
  }

  return is_empty;
}

// close this reg key and the event
HRESULT RegKeyWithChangeEvent::Close() {
  reset(change_event_);
  return RegKey::Close();
}

// Called to create/reset the event that gets signaled
// any time the registry key changes
// Note:
//   * reg key should have been opened using KEY_NOTIFY for the sam_desired
//
// See the documentation for RegNotifyChangeKeyValue
// for values for notify_filter.
HRESULT RegKeyWithChangeEvent::SetupEvent(bool watch_subtree,
                                          DWORD notify_filter) {
  // If the event exists, then it should be in the signaled state
  // indicating a registry change took place.  If not, then
  // the caller is setting up the event a second time and this
  // will create a memory leak.
  ASSERT(!valid(change_event_) || HasChangeOccurred(),
         (_T("Event is getting set-up for a second ")
          _T("time without being signaled.")));

  if (!valid(change_event_)) {
    reset(change_event_, ::CreateEvent(NULL, TRUE, FALSE, NULL));
    if (!valid(change_event_)) {
      ASSERT(false, (_T("create event failed")));
      return HRESULT_FROM_WIN32(::GetLastError());
    }
  } else {
    if (!::ResetEvent(get(change_event_))) {
      ASSERT(false, (_T("reset event failed")));
      return HRESULT_FROM_WIN32(::GetLastError());
    }
  }

  LONG res = ::RegNotifyChangeKeyValue(Key(), watch_subtree, notify_filter,
      get(change_event_), TRUE);

  if (res != ERROR_SUCCESS) {
    // You may get this failure if you didn't pass in KEY_NOTIFY
    // as part of the sam_desired flags during Open or Create
    ASSERT(false, (_T("setting up change notification for a reg key failed")));

    // Leave the event around so that it never changes once it has been set-up
    // but in this case it will not get signaled again.
  }

  return HRESULT_FROM_WIN32(res);
}

// Indicates if any changes (that are being monitored have occurred)
bool RegKeyWithChangeEvent::HasChangeOccurred() const {
  return IsHandleSignaled(get(change_event_));
}


RegKeyWatcher::RegKeyWatcher(const TCHAR* reg_key, bool watch_subtree,
                             DWORD notify_filter, bool allow_creation)
    : reg_key_string_(reg_key),
      watch_subtree_(watch_subtree),
      notify_filter_(notify_filter),
      allow_creation_(allow_creation) {
  UTIL_LOG(L3, (_T("[RegKeyWatcher::RegKeyWatcher][%s]"), reg_key));
}

HRESULT RegKeyWatcher::EnsureEventSetup() {
  UTIL_LOG(L3, (_T("[RegKeyWatcher::EnsureEventSetup]")));
  if (!reg_key_with_change_event_.get()) {
    std::unique_ptr<RegKeyWithChangeEvent> local_reg_key(new RegKeyWithChangeEvent);
    if (!local_reg_key.get()) {
      ASSERT(false, (_T("unable to allocate local_reg_key")));
      return E_FAIL;
    }

    if (allow_creation_ && !RegKey::HasKey(reg_key_string_)) {
      RegKey key;
      VERIFY_SUCCEEDED(key.Create(reg_key_string_));
    }

    HRESULT hr = local_reg_key->Open(reg_key_string_, KEY_NOTIFY);
    if (FAILED(hr)) {
      ASSERT(false, (_T("couldn't open %s reg key for notifications. ")
                     _T("Make sure you have pre-created the key!"),
                     reg_key_string_));
      return hr;
    }
    reg_key_with_change_event_.reset(local_reg_key.release());
    reg_key_string_.Empty();
  }

  // if the event is set-up and no changes have occurred,
  // then there is no need to re-setup the event.
  if (reg_key_with_change_event_->change_event() && !HasChangeOccurred()) {
    return S_OK;
  }

  return reg_key_with_change_event_->SetupEvent(watch_subtree_,
                                                notify_filter_);
}

// Get the event that is signaled on registry changes.
HANDLE RegKeyWatcher::change_event() const {
  if (!reg_key_with_change_event_.get()) {
    ASSERT(false, (_T("call RegKeyWatcher::EnsureEventSetup first")));
    return NULL;
  }
  return reg_key_with_change_event_->change_event();
}

}  // namespace omaha

