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

#include "omaha/common/registry_store.h"
#include <vector>
#include "omaha/common/debug.h"
#include "omaha/common/reg_key.h"

namespace omaha {

bool RegistryStore::Open(const TCHAR* key_path) {
  key_path_ = key_path;
  return true;
}

bool RegistryStore::Close() {
  key_path_.Empty();
  return true;
}

bool RegistryStore::Clear() {
  if (RegKey::HasKey(key_path_)) {
    return SUCCEEDED(RegKey::DeleteKey(key_path_, false));
  } else {
    return true;
  }
}

bool RegistryStore::Read(const TCHAR* name, std::vector<byte>* data) const {
  ASSERT1(name);
  ASSERT1(data);

  byte* sdata = NULL;
  DWORD sdata_size = 0;
  HRESULT hr = RegKey::GetValue(key_path_, name, &sdata, &sdata_size);
  if (FAILED(hr) || !sdata || !sdata_size)
    return false;

  data->resize(sdata_size);
  memcpy(&data->front(), sdata, sdata_size);

  delete[] sdata;

  return true;
}

bool RegistryStore::Write(const TCHAR* name, byte* data, int data_size) {
  ASSERT1(name);
  ASSERT1(data);
  ASSERT1(data_size);

  return SUCCEEDED(RegKey::SetValue(key_path_, name, data, data_size));
}

bool RegistryStore::Exists(const TCHAR* name) {
  ASSERT1(name);

  return RegKey::HasValue(key_path_, name);
}

bool RegistryStore::Remove(const TCHAR* name) {
  ASSERT1(name);

  return SUCCEEDED(RegKey::DeleteValue(key_path_, name));
}

bool RegistryStore::GetValueCount(uint32* value_count) {
  ASSERT1(value_count);

  CString key_name(key_path_);
  HKEY h_key = RegKey::GetRootKeyInfo(&key_name);

  RegKey reg_key;
  if (FAILED(reg_key.Open(h_key, key_name.GetString(), KEY_READ)))
    return false;

  *value_count = reg_key.GetValueCount();

  reg_key.Close();

  return true;
}

bool RegistryStore::GetValueNameAt(int index, CString* value_name) {
  ASSERT1(index >= 0);
  ASSERT1(value_name);

  CString key_name(key_path_);
  HKEY h_key = RegKey::GetRootKeyInfo(&key_name);

  RegKey reg_key;
  if (FAILED(reg_key.Open(h_key, key_name.GetString(), KEY_READ)))
    return false;

  HRESULT hr = reg_key.GetValueNameAt(index, value_name, NULL);

  reg_key.Close();

  return SUCCEEDED(hr);
}

}  // namespace omaha

