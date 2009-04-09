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

#include "omaha/tools/goopdump/data_dumper.h"

#include "omaha/common/reg_key.h"
#include "omaha/tools/goopdump/dump_log.h"

namespace omaha {

DataDumper::DataDumper() {
}

DataDumper::~DataDumper() {
}

void DataDumper::DumpRegValueStr(const DumpLog& dump_log,
                                 const TCHAR* full_key_name,
                                 const TCHAR* value_name) {
  DumpRegValueStrRet(dump_log, full_key_name, value_name, NULL);
}

void DataDumper::DumpRegValueStrRet(const DumpLog& dump_log,
                                    const TCHAR* full_key_name,
                                    const TCHAR* value_name,
                                    CString* str) {
  CString val;
  CString key = full_key_name;
  HRESULT hr = RegKey::GetValue(key, value_name, &val);

  CString value_name_str = value_name ? value_name : _T("default");

  if (SUCCEEDED(hr)) {
    dump_log.WriteLine(_T("%s[%s] = %s"), key, value_name_str, val);
  } else {
    dump_log.WriteLine(_T("%s HAS NO VALUE"), key);
  }

  if (str) {
    *str = val;
  }
}

DumpHeader::DumpHeader(const DumpLog& dump_log, const TCHAR* header_name)
    : dump_log_(dump_log) {
  dump_log_.WriteLine(_T(""));
  dump_log_.WriteLine(_T("--------------------------------------------------"));
  dump_log_.WriteLine(header_name);
  dump_log_.WriteLine(_T("--------------------------------------------------"));
}

DumpHeader::~DumpHeader() {
  dump_log_.WriteLine(_T("--------------------------------------------------"));
  dump_log_.WriteLine(_T(""));
}

void DataDumper::RecursiveDumpRegistryKey(const DumpLog& dump_log,
                                          RegKey* key,
                                          const int indent) {
  ASSERT1(key);
  CString indent_string;
  for (int i = 0; i < indent; ++i) {
    indent_string.Append(_T(" "));
  }
  uint32 value_count = key->GetValueCount();
  for (uint32 idx = 0; idx < value_count; ++idx) {
    CString value_name;
    DWORD value_type = 0;
    key->GetValueNameAt(idx, &value_name, &value_type);
    CString value_name_disp =
        value_name.IsEmpty() ? _T("[default]") : value_name;
    switch (value_type) {
      case REG_SZ: {
        CString value;
        key->GetValue(value_name, &value);
        dump_log.WriteLine(_T("%s%s: (REG_SZ): %s"),
                           indent_string,
                           value_name_disp,
                           value);
        break;
      }
      case REG_DWORD: {
        DWORD value = 0;
        key->GetValue(value_name, &value);
        dump_log.WriteLine(_T("%s%s: (REG_DWORD): %d (0x%x)"),
                           indent_string,
                           value_name_disp,
                           value,
                           value);
        break;
      }
      case REG_BINARY:
        dump_log.WriteLine(_T("%s%s: (REG_BINARY)"),
                           indent_string,
                           value_name_disp);
        break;
      case REG_MULTI_SZ:
        dump_log.WriteLine(_T("%s%s: (REG_MULTI_SZ)"),
                           indent_string,
                           value_name_disp);
        break;
      default:
        dump_log.WriteLine(_T("%s%s: (TYPE: %d)"),
                           indent_string,
                           value_name_disp,
                           value_type);
        break;
    }
  }

  uint32 subkey_count = key->GetSubkeyCount();
  for (uint32 idx = 0; idx < subkey_count; ++idx) {
    CString subkey_name;
    key->GetSubkeyNameAt(idx, &subkey_name);
    RegKey subkey;
    subkey.Open(key->Key(), subkey_name, KEY_READ);
    dump_log.WriteLine(_T("%sSUBKEY: %s"), indent_string, subkey_name);
    RecursiveDumpRegistryKey(dump_log, &subkey, indent+1);
  }
}

void DataDumper::DumpRegistryKeyData(const DumpLog& dump_log,
                                     const CString& key_name) {
  RegKey key_root;
  if (FAILED(key_root.Open(key_name, KEY_READ))) {
    dump_log.WriteLine(_T("Key (%s) could not be opened"), key_name);
    return;
  }
  dump_log.WriteLine(_T("ROOT KEY: %s"), key_name);
  RecursiveDumpRegistryKey(dump_log, &key_root, 1);
}

}  // namespace omaha

