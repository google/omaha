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

#include "omaha/tools/goopdump/data_dumper_oneclick.h"

#include "omaha/common/reg_key.h"
#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

namespace omaha {

DataDumperOneClick::DataDumperOneClick() {
}

DataDumperOneClick::~DataDumperOneClick() {
}

void DataDumperOneClick::DumpOneClickDataForVersion(const DumpLog& dump_log,
                                                    int plugin_version) {

  dump_log.WriteLine(_T("Trying Plugin Version: %d"), plugin_version);

  CString oneclick_name;
  oneclick_name.Format(_T("Google.OneClickCtrl.%d"), plugin_version);

  CString reg_str;
  reg_str.Format(_T("HKCR\\%s"), oneclick_name);
  DumpRegValueStr(dump_log, reg_str, NULL);

  CString clsid;
  reg_str.Append(_T("\\CLSID"));
  DumpRegValueStrRet(dump_log, reg_str, NULL, &clsid);

  reg_str.Format(_T("HKCR\\MIME\\DataBase\\Content Type\\application/%s"),
                 oneclick_name);
  DumpRegValueStr(dump_log, reg_str, _T("CLSID"));

  CString key;
  key.Format(_T("HKCR\\CLSID\\%s"), clsid);

  if (!clsid.IsEmpty()) {
    DumpRegistryKeyData(dump_log, key);
  }

  CString typelib;
  CString key2 = key;
  key2.Append(_T("\\TypeLib"));
  if (SUCCEEDED(RegKey::GetValue(key2, NULL, &typelib))) {
    if (!typelib.IsEmpty()) {
      key.Format(_T("HKCR\\Typelib\\%s\\1.0\\0\\win32"), typelib);
      DumpRegistryKeyData(dump_log, key);
    }
  }

  dump_log.WriteLine(_T(""));
}

HRESULT DataDumperOneClick::Process(const DumpLog& dump_log,
                                    const GoopdumpCmdLineArgs& args) {
  UNREFERENCED_PARAMETER(args);

  DumpHeader header(dump_log, _T("OneClick Data"));

  CString activex_version_str(ACTIVEX_VERSION_ANSI);
  int activex_version = _ttoi(activex_version_str);

  for (int i = 1; i <= activex_version; ++i) {
    DumpOneClickDataForVersion(dump_log, i);
  }

  return S_OK;
}

}  // namespace omaha

