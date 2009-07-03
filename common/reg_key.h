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
//
// reg_key.h
//
// Registry configuration wrappers class
//
// Offers static functions for convenient
// fast access for individual values
//
// Also provides a wrapper class for efficient
// batch operations on values of a given registry key.

#ifndef OMAHA_COMMON_REG_KEY_H_
#define OMAHA_COMMON_REG_KEY_H_

#include <windows.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/static_assert.h"
#include "omaha/common/store_watcher.h"

namespace omaha {

// maximum sizes registry key and value names
#define kMaxKeyNameChars   (255 + 1)
#define kMaxValueNameChars (16383 + 1)

class RegKey {
 public:
  RegKey();
  virtual ~RegKey();

  // create a reg key
  HRESULT Create(HKEY hKeyParent, const TCHAR * key_name,
        TCHAR * reg_class = REG_NONE, DWORD options = REG_OPTION_NON_VOLATILE,
        REGSAM sam_desired = KEY_ALL_ACCESS,
        LPSECURITY_ATTRIBUTES lp_sec_attr = NULL,
        LPDWORD lp_disposition = NULL);

  // create a reg key, given the full key name, including the HKEY root
  // (say for example, "HKLM\\Software")
  HRESULT Create(const TCHAR * full_key_name,
        TCHAR * reg_class = REG_NONE, DWORD options = REG_OPTION_NON_VOLATILE,
        REGSAM sam_desired = KEY_ALL_ACCESS,
        LPSECURITY_ATTRIBUTES lp_sec_attr = NULL,
        LPDWORD lp_disposition = NULL);

  // static helper function that create a set of reg keys,
  // given an array of full key names including the HKEY root
  // (say for example, "HKLM\\Software")
  static HRESULT CreateKeys(const TCHAR* keys_to_create[],
                            DWORD number_of_keys,
                            TCHAR* reg_class = REG_NONE,
                            DWORD options = REG_OPTION_NON_VOLATILE,
                            LPSECURITY_ATTRIBUTES lp_sec_attr = NULL);

  // Static method to create a single key.
  static HRESULT CreateKey(const TCHAR * full_key_name,
                           TCHAR * reg_class = REG_NONE,
                           DWORD options = REG_OPTION_NON_VOLATILE,
                           LPSECURITY_ATTRIBUTES lp_sec_attr = NULL);

  // open an existing reg key
  HRESULT Open(HKEY hKeyParent,
               const TCHAR * key_name,
               REGSAM sam_desired = KEY_ALL_ACCESS);

  // open an existing reg key, given the full key name, including the HKEY root
  // (say for example, "HKLM\\Software")
  HRESULT Open(const TCHAR * full_key_name,
               REGSAM sam_desired = KEY_ALL_ACCESS);

  // close this reg key
  virtual HRESULT Close();

  // check if the key has a specified value
  bool HasValue(const TCHAR * value_name);

  // get the number of values for this key
  uint32 GetValueCount();

  // Called to get the value name for the given value name index
  // Use GetValueCount() to get the total value_name count for this key
  // Returns failure if no key at the specified index
  // If you modify the key while enumerating, the indexes will be out of order.
  // Since the index order is not guaranteed, you need to reset your counting
  // loop.
  // type refers to REG_DWORD, REG_QWORD, etc..
  // 'type' can be NULL if not interested in the value type
  HRESULT GetValueNameAt(int index, CString * value_name, DWORD *type);

  // check if the current key has the specified subkey
  bool HasSubkey(const TCHAR * key_name) const;

  // get the number of subkeys for this key
  uint32 GetSubkeyCount();

  // Called to get the key name for the given key index
  // Use GetSubkeyCount() to get the total count for this key
  // Returns failure if no key at the specified index
  // If you modify the key while enumerating, the indexes will be out of order.
  // Since the index order is not guaranteed, you need to reset your counting
  // loop.
  HRESULT GetSubkeyNameAt(int index, CString * key_name);

  // SETTERS

  // set an int32 value - use when reading multiple values from a key
  HRESULT SetValue(const TCHAR * value_name, DWORD value) const;

  // set an int64 value
  HRESULT SetValue(const TCHAR * value_name, DWORD64 value) const;

  // set a string value
  HRESULT SetValue(const TCHAR * value_name, const TCHAR * value) const;

  // set binary data
  HRESULT SetValue(const TCHAR * value_name,
                   const byte * value,
                   DWORD byte_count) const;

  // set raw data, including type
  HRESULT SetValue(const TCHAR * value_name,
                   const byte * value,
                   DWORD byte_count,
                   DWORD type) const;

  // GETTERS

  // get an int32 value
  HRESULT GetValue(const TCHAR * value_name, DWORD * value) const;

  // get an int64 value
  //
  // Note: if you are using time64 you should
  // likely use GetLimitedTimeValue (util.h) instead of this method.
  HRESULT GetValue(const TCHAR * value_name, DWORD64 * value) const;

  // get a string value - the caller must free the return buffer
  HRESULT GetValue(const TCHAR * value_name, TCHAR * * value) const;

  // get a CString value
  HRESULT GetValue(const TCHAR* value_name, OUT CString* value) const;

  // get a vector<CString> value from REG_MULTI_SZ type
  HRESULT GetValue(const TCHAR * value_name,
                   std::vector<CString> * value) const;

  // get binary data - the caller must free the return buffer
  HRESULT GetValue(const TCHAR * value_name,
                   byte * * value,
                   DWORD * byte_count) const;

  // get raw data, including type - the caller must free the return buffer
  HRESULT GetValue(const TCHAR * value_name,
                   byte * * value,
                   DWORD * byte_count,
                   DWORD *type) const;

  // RENAMERS

  // Rename a named value.
  HRESULT RenameValue(const TCHAR * old_value_name,
                      const TCHAR * new_value_name) const;

  // STATIC VERSIONS

  // flush
  static HRESULT FlushKey(const TCHAR * full_key_name);

  // Check if a key exists.
  static bool HasKey(const TCHAR * full_key_name);

  // Check if a key exists in the native (i.e. non-redirected) registry.
  static bool HasNativeKey(const TCHAR * full_key_name);

  // check if the key has a specified value
  static bool HasValue(const TCHAR * full_key_name, const TCHAR * value_name);

  // SETTERS

  // STATIC int32 set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          DWORD value);

  // STATIC int64 set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          DWORD64 value);

  // STATIC float set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          float value);

  // STATIC double set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          double value);

  // STATIC string set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          const TCHAR * value);

  // STATIC binary data set
  static HRESULT SetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          const byte * value,
                          DWORD byte_count);

  // STATIC array of strings set
  static HRESULT SetValueMultiSZ(const TCHAR * full_key_name,
                                 const TCHAR * value_name,
                                 const byte * value,
                                 DWORD byte_count);

  // STATIC expandable string set
  static HRESULT SetValueExpandSZ(const TCHAR * full_key_name,
                                  const TCHAR * value_name,
                                  const TCHAR * value);

  // GETTERS

  // STATIC int32 get
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          DWORD * value);

  // STATIC int64 get
  //
  // Note: if you are using time64 you should
  // likely use GetLimitedTimeValue (util.h) instead of this method.
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          DWORD64 * value);

  // STATIC float get
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          float * value);

  // STATIC double get
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          double * value);

  // STATIC string get (STR and CString versions) - the caller must free
  // the return buffer
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          TCHAR * * value);

  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          CString * value);

  // STATIC REG_MULTI_SZ get
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          std::vector<CString> * value);

  // STATIC get binary data - the caller must free the return buffer
  static HRESULT GetValue(const TCHAR * full_key_name,
                          const TCHAR * value_name,
                          byte * * value,
                          DWORD * byte_count);

  // Try reg keys successively if there is a failure in getting a value.
  //
  // Typically used when there is a user value and a default value if the
  // user has none.
  template<typename T>
  static HRESULT GetValue(const TCHAR * full_key_names[],
                          int key_names_length,
                          const TCHAR * value_name,
                          T* value);

  // RENAMERS

  // Rename a named value.
  static HRESULT RenameValue(const TCHAR * full_key_name,
                             const TCHAR * old_value_name,
                             const TCHAR * new_value_name);

  // COPIERS

  // The full_to_key must exist for CopyValue to succeed.
  static HRESULT CopyValue(const TCHAR * full_from_key_name,
                           const TCHAR * from_value_name,
                           const TCHAR * full_to_key_name,
                           const TCHAR * to_value_name);

  static HRESULT CopyValue(const TCHAR * full_from_key_name,
                           const TCHAR * full_to_key_name,
                           const TCHAR * value_name);

  // Get type of a registry value
  static HRESULT GetValueType(const TCHAR* full_key_name,
                              const TCHAR* value_name,
                              DWORD* value_type);

  // delete a subkey of the current key (with no subkeys)
  HRESULT DeleteSubKey(const TCHAR * key_name);

  // recursively delete a sub key of the current key (and all its subkeys)
  HRESULT RecurseDeleteSubKey(const TCHAR * key_name);

  // STATIC version of delete key - handles nested keys also
  // delete a key and all its sub-keys recursively
  // Returns S_FALSE if key didn't exist, S_OK if deletion was successful,
  // and failure otherwise.
  static HRESULT DeleteKey(const TCHAR* full_key_name);

  // STATIC version of delete key
  // delete a key recursively or non-recursively
  // Returns S_FALSE if key didn't exist, S_OK if deletion was successful,
  // and failure otherwise.
  static HRESULT DeleteKey(const TCHAR* full_key_name, bool recursive);

  // delete the specified value
  HRESULT DeleteValue(const TCHAR * value_name) const;

  // STATIC version of delete value
  // Returns S_FALSE if key didn't exist, S_OK if deletion was successful,
  // and failure otherwise.
  static HRESULT DeleteValue(const TCHAR * full_key_name,
                             const TCHAR * value_name);

  // Peek inside (use a RegKey as a smart wrapper around a registry handle)
  HKEY Key() { return h_key_; }

  // Used to help test the private functionality
  friend class RegKeyTestClass;

  // helper function to get the HKEY and the root key from a string
  // representation modifies the argument in place and returns the key name
  // e.g. HKLM\\Software\\Google\... returns HKLM, "Software\\Google\..."
  // Necessary for the static versions that use the full name of the reg key
  static HKEY GetRootKeyInfo(CString * full_key_name);

  // Returns true if this key name is 'safe' for deletion (doesn't specify
  // a key root)
  static bool SafeKeyNameForDeletion(const wchar_t *key_name);

  // save the key and all of its subkeys and values to a file
  static HRESULT Save(const TCHAR* full_key_name, const TCHAR* file_name);

  // restore the key and all of its subkeys and values which are saved into
  // a file
  static HRESULT Restore(const TCHAR* full_key_name, const TCHAR* file_name);

  // Is the key empty: having no sub-keys and values
  static bool IsKeyEmpty(const TCHAR* full_key_name);

 private:

  // Helper function to check if a key exists, using the sam flags specified.
  // Note: KEY_READ must be included in sam_flags.
  static bool HasKeyHelper(const TCHAR * full_key_name, DWORD sam_flags);

  // helper function to get the parent key name and the subkey from a string
  // representation modifies the argument in place and returns the key name
  // e.g. Software\\Google\\Foo_Bar returns "Software\\Google", "Foo_Bar"
  // Necessary for the static versions that use the full name of the reg key
  static CString GetParentKeyInfo(CString * key_name);

  // helper function to get any value from the registry
  // used when the size of the data is unknown
  HRESULT GetValueHelper(const TCHAR * value_name,
                         DWORD * type,
                         byte * * value,
                         DWORD * byte_count) const;

  // common SET Helper for the static case
  static HRESULT SetValueStaticHelper(const TCHAR * full_key_name,
                                      const TCHAR * value_name,
                                      DWORD type,
                                      LPVOID value,
                                      DWORD byte_count = 0);

  // common GET Helper for the static case
  static HRESULT GetValueStaticHelper(const TCHAR * full_key_name,
                                      const TCHAR * value_name,
                                      DWORD type,
                                      LPVOID value,
                                      DWORD * byte_count = NULL);

  // convert REG_MULTI_SZ bytes to string array
  static HRESULT MultiSZBytesToStringArray(const byte * buffer,
                                           DWORD byte_count,
                                           std::vector<CString> * value);

  // set a string or expandable string value
  HRESULT SetStringValue(const TCHAR * value_name,
                         const TCHAR * value,
                         DWORD type) const;

  // the HKEY for the current key
  HKEY h_key_;

  DISALLOW_EVIL_CONSTRUCTORS(RegKey);
};

// Provides all the functionality of RegKey plus
// an event to watch for changes to the registry key.
class RegKeyWithChangeEvent : public RegKey {
 public:
  RegKeyWithChangeEvent() {}
  // close this reg key and the event
  virtual HRESULT Close();

  // Called to create/reset the event that gets signaled
  // any time the registry key changes.  Access the created
  // event using change_event().
  //
  // See the documentation for RegNotifyChangeKeyValue
  // for values for notify_filter.
  HRESULT SetupEvent(bool watch_subtree, DWORD notify_filter);

  // Indicates if any changes (that are being monitored) have occured
  bool HasChangeOccurred() const;

  // Get the event that is signaled on registry changes.
  // Note:
  //   * This event will remain constant until Close() is called.
  //   * One should call SetupEvent to set-up the event.
  //   * The event is only signaled on the next change and remains signaled.
  //     Do not call ::ResetEvent().  Call SetupEvent() to reset
  //     the event and wait for more changes.
  HANDLE change_event() const {
    return get(change_event_);
  }

 private:
  scoped_handle change_event_;
  DISALLOW_EVIL_CONSTRUCTORS(RegKeyWithChangeEvent);
};

// Does the common things necessary for watching
// registry key changes.  If there are file change or other watchers,
// there could be a common interface for the three methods to decouple
// the code that is doing the watching from the code that owns the store.
class RegKeyWatcher : public StoreWatcher {
 public:
  // reg_key: the full string for the reg key
  // watch_subtree: watch all subkey changes  or
  //                only immediate child values
  // notify_filter: See the documentation for RegNotifyChangeKeyValue
  // allow_creation: Should the key be created if it doesn't exist?
  RegKeyWatcher(const TCHAR* reg_key, bool watch_subtree,
                DWORD notify_filter, bool allow_creation);
  virtual ~RegKeyWatcher() {}

  // Called to create/reset the event that gets signaled
  // any time the registry key changes.  Access the created
  // event using change_event().
  virtual HRESULT EnsureEventSetup();

  // Get the event that is signaled on registry changes.
  virtual HANDLE change_event() const;

 private:
  // Used to do the SetupEvent method
  scoped_ptr<RegKeyWithChangeEvent> reg_key_with_change_event_;

  CString reg_key_string_;
  bool watch_subtree_;
  bool allow_creation_;
  DWORD notify_filter_;
  DISALLOW_EVIL_CONSTRUCTORS(RegKeyWatcher);
};


inline RegKey::RegKey() { h_key_ = NULL; }

inline RegKey::~RegKey() { Close(); }

inline bool RegKey::HasValue(const TCHAR* value_name) {
  return (ERROR_SUCCESS == ::RegQueryValueEx(h_key_,
                                             value_name,
                                             NULL, NULL,
                                             NULL,
                                             NULL));
}

// SETTERS static versions
inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                DWORD value) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name, value_name, REG_DWORD, &value);
}

inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                DWORD64 value) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name, value_name, REG_QWORD, &value);
}

inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                float value) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name,
                              value_name,
                              REG_BINARY,
                              &value,
                              sizeof(value));
}

inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                double value) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name,
                              value_name,
                              REG_BINARY,
                              &value,
                              sizeof(value));
}

inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                const TCHAR* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return SetValueStaticHelper(full_key_name,
                              value_name,
                              REG_SZ,
                              const_cast<TCHAR*>(value));
}

inline HRESULT RegKey::SetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                const byte* value,
                                DWORD byte_count) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name, value_name, REG_BINARY,
                              const_cast<byte*>(value), byte_count);
}

inline HRESULT RegKey::SetValueMultiSZ(const TCHAR* full_key_name,
                                       const TCHAR* value_name,
                                       const byte* value,
                                       DWORD byte_count) {
  ASSERT1(full_key_name);

  return SetValueStaticHelper(full_key_name, value_name, REG_MULTI_SZ,
                              const_cast<byte*>(value), byte_count);
}

inline HRESULT RegKey::SetValueExpandSZ(const TCHAR* full_key_name,
                                        const TCHAR* value_name,
                                        const TCHAR* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return SetValueStaticHelper(full_key_name,
                              value_name,
                              REG_EXPAND_SZ,
                              const_cast<TCHAR*>(value));
}

// GETTERS static versions
inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                DWORD* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return GetValueStaticHelper(full_key_name, value_name, REG_DWORD, value);
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                DWORD64* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return GetValueStaticHelper(full_key_name, value_name, REG_QWORD, value);
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                float* value) {
  ASSERT1(value);
  ASSERT1(value_name);
  ASSERT1(full_key_name);

  DWORD byte_count = 0;
  byte* buffer = NULL;
  HRESULT hr = GetValueStaticHelper(full_key_name,
                                    value_name,
                                    REG_BINARY,
                                    &buffer,
                                    &byte_count);
  scoped_array<byte> free_buffer(buffer);

  if (SUCCEEDED(hr)) {
    if (byte_count == sizeof(*value)) {
      ::CopyMemory(value, buffer, sizeof(*value));
    } else {
      UTIL_LOG(LEVEL_ERROR, (_T("[RegKey::GetValue]")
                             _T("[size mismatches for float value][%s\\%s]"),
                             full_key_name, value_name));
      return HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
    }
  }

  return hr;
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                double* value) {
  ASSERT1(value);
  ASSERT1(value_name);
  ASSERT1(full_key_name);

  DWORD byte_count = 0;
  byte* buffer = NULL;
  HRESULT hr = GetValueStaticHelper(full_key_name,
                                    value_name,
                                    REG_BINARY,
                                    &buffer,
                                    &byte_count);
  scoped_array<byte> free_buffer(buffer);

  if (SUCCEEDED(hr)) {
    if (byte_count == sizeof(*value)) {
      ::CopyMemory(value, buffer, sizeof(*value));
    } else {
      UTIL_LOG(LEVEL_ERROR, (_T("[RegKey::GetValue]")
                             _T("[size mismatches for double value][%s\\%s]"),
                             full_key_name, value_name));
      return HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
    }
  }

  return hr;
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                TCHAR** value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return GetValueStaticHelper(full_key_name, value_name, REG_SZ, value);
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                CString* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  TCHAR* buffer = NULL;
  HRESULT hr = RegKey::GetValue(full_key_name, value_name, &buffer);
  value->SetString(buffer);
  delete [] buffer;
  return hr;
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                std::vector<CString>* value) {
  ASSERT1(full_key_name);
  ASSERT1(value);

  return GetValueStaticHelper(full_key_name, value_name, REG_MULTI_SZ, value);
}

inline HRESULT RegKey::GetValue(const TCHAR* full_key_name,
                                const TCHAR* value_name,
                                byte** value,
                                DWORD* byte_count) {
  ASSERT1(full_key_name);
  ASSERT1(value);
  ASSERT1(byte_count);

  return GetValueStaticHelper(full_key_name,
                              value_name,
                              REG_BINARY,
                              value,
                              byte_count);
}

template<typename T>
HRESULT RegKey::GetValue(const TCHAR* full_key_names[],
                         int key_names_length,
                         const TCHAR* value_name,
                         T* value) {
  HRESULT hr = S_OK;
  for (int i = 0; i < key_names_length; ++i) {
    hr = GetValue(full_key_names[i], value_name, value);
    if (SUCCEEDED(hr)) {
      return hr;
    }
  }
  return hr;
}

// Rename a named value.
inline HRESULT RegKey::RenameValue(const TCHAR * full_key_name,
                                   const TCHAR * old_value_name,
                                   const TCHAR * new_value_name) {
  ASSERT1(full_key_name);

  RegKey reg_key;
  HRESULT hr = reg_key.Open(full_key_name);
  if (FAILED(hr)) {
    return hr;
  }

  return reg_key.RenameValue(old_value_name, new_value_name);
}

inline HRESULT RegKey::CopyValue(const TCHAR * full_from_key_name,
                                 const TCHAR * full_to_key_name,
                                 const TCHAR * value_name) {
  return CopyValue(full_from_key_name,
                   value_name,
                   full_to_key_name,
                   value_name);
}

// DELETE
inline HRESULT RegKey::DeleteSubKey(const TCHAR* key_name) {
  ASSERT1(key_name);
  ASSERT1(h_key_);

  LONG res = ::RegDeleteKey(h_key_, key_name);
  HRESULT hr = HRESULT_FROM_WIN32(res);
  if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
      hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
    hr = S_FALSE;
  }
  return hr;
}

inline HRESULT RegKey::DeleteValue(const TCHAR* value_name) const {
  ASSERT1(value_name);
  ASSERT1(h_key_);

  LONG res = ::RegDeleteValue(h_key_, value_name);
  HRESULT hr = HRESULT_FROM_WIN32(res);
  if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) ||
      hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)) {
    hr = S_FALSE;
  }
  return hr;
}

}  // namespace omaha

#endif  // OMAHA_COMMON_REG_KEY_H_

