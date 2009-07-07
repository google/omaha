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

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_GOOPDATE_H_
#define OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_GOOPDATE_H_

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

#include "omaha/tools/goopdump/data_dumper.h"

namespace omaha {

class DataDumperGoopdate : public DataDumper {
 public:
  DataDumperGoopdate();
  virtual ~DataDumperGoopdate();

  virtual HRESULT Process(const DumpLog& dump_log,
                          const GoopdumpCmdLineArgs& args);

 private:
  void DumpGoogleUpdateIniFile(const DumpLog& dump_log);
  void DumpHostsFile(const DumpLog& dump_log);
  void DumpUpdateDevKeys(const DumpLog& dump_log);
  void DumpLogFile(const DumpLog& dump_log);
  void DumpEventLog(const DumpLog& dump_log);
  void DumpGoogleUpdateProcessInfo(const DumpLog& dump_log);
  void DumpDirContents(const DumpLog& dump_log, bool is_machine);
  void DumpServiceInfo(const DumpLog& dump_log);
  void DumpRunKeys(const DumpLog& dump_log);
  void DumpScheduledTaskInfo(const DumpLog& dump_log, bool is_machine);

  HRESULT GetRegisteredVersion(bool is_machine, CString* version);
  HRESULT GetDllDir(bool is_machine, CString* dll_path);

  void DumpFileContents(const DumpLog& dump_log,
                        const CString& file_path,
                        int num_tail_lines);


  DISALLOW_EVIL_CONSTRUCTORS(DataDumperGoopdate);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_GOOPDATE_H_

