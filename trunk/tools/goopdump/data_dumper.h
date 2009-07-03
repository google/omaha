// Copyright 2009 Google Inc.
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

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_H__
#define OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class RegKey;

class DumpLog;
struct GoopdumpCmdLineArgs;

class DataDumper {
 public:
  DataDumper();
  virtual ~DataDumper();

  virtual HRESULT Process(const DumpLog& dump_log,
                          const GoopdumpCmdLineArgs& args) = 0;

 protected:
  void DumpRegValueStr(const DumpLog& dump_log,
                       const TCHAR* full_key_name,
                       const TCHAR* value_name);

  void DumpRegValueStrRet(const DumpLog& dump_log,
                          const TCHAR* full_key_name,
                          const TCHAR* value_name,
                          CString* str);

  void DumpRegistryKeyData(const DumpLog& dump_log,
                           const CString& key_name);

 private:
  void RecursiveDumpRegistryKey(const DumpLog& dump_log,
                                RegKey* key,
                                const int indent);

  DISALLOW_EVIL_CONSTRUCTORS(DataDumper);
};

class DumpHeader {
 public:
  DumpHeader(const DumpLog& dump_log, const TCHAR* header_name);
  ~DumpHeader();

 private:
  const DumpLog& dump_log_;
  DISALLOW_EVIL_CONSTRUCTORS(DumpHeader);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_H__

