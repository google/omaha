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

#include "omaha/tools/goopdump/goopdump.h"

#include <stdio.h>

#include <vector>

#include "omaha/common/debug.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/tools/goopdump/data_dumper.h"
#include "omaha/tools/goopdump/data_dumper_app_manager.h"
#include "omaha/tools/goopdump/data_dumper_goopdate.h"
#include "omaha/tools/goopdump/data_dumper_network.h"
#include "omaha/tools/goopdump/data_dumper_oneclick.h"
#include "omaha/tools/goopdump/data_dumper_osdata.h"
#include "omaha/tools/goopdump/process_commandline.h"
#include "omaha/tools/goopdump/process_monitor.h"

namespace omaha {

class GoopdateProcessMonitorCallback : public ProcessMonitorCallbackInterface {
 public:
  explicit GoopdateProcessMonitorCallback(const DumpLog& dump_log)
      : dump_log_(dump_log) {
  }
  virtual ~GoopdateProcessMonitorCallback() {}

  virtual void OnProcessAdded(DWORD process_id,
                              const CString& process_pattern) {
    CString cmd_line;
    GetProcessCommandLine(process_id, &cmd_line);
    dump_log_.WriteLine(_T("Process Added.    ProcessId(%d)  Pattern(%s)  ")
                        _T("cmd_line(%s)"),
                        process_id, process_pattern, cmd_line);
  }

  virtual void OnProcessRemoved(DWORD process_id) {
    dump_log_.WriteLine(_T("Process Removed.  ProcessId(%d)"), process_id);
  }

 private:
  const DumpLog& dump_log_;

  DISALLOW_COPY_AND_ASSIGN(GoopdateProcessMonitorCallback);
};


Goopdump::Goopdump() {
}

Goopdump::~Goopdump() {
}

HRESULT Goopdump::Main(const TCHAR* cmd_line, int argc, TCHAR** argv) {
  SetNewHandler();
  dump_log_.EnableConsole(true);
  FileDumpLogHandler file_dumplog_handler;

  ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

  cmd_line_ = cmd_line;

  PrintProgramHeader();
  dump_log_.WriteLine(_T("cmd_line_: %s"), cmd_line_);

  if (FAILED(ParseGoopdumpCmdLine(argc, argv, &args_))) {
    PrintUsage();
    return E_FAIL;
  }

  if (args_.is_write_to_file) {
    file_dumplog_handler.set_filename(args_.log_filename);
    dump_log_.AddLogHandler(&file_dumplog_handler);
  }

  // Dump out any requested data.
  std::vector<DataDumper*> data_dumpers;

  if (args_.is_dump_general) {
    data_dumpers.push_back(new DataDumperOSData());
    data_dumpers.push_back(new DataDumperNetwork());
    data_dumpers.push_back(new DataDumperGoopdate());
  }

  if (args_.is_dump_oneclick) {
    data_dumpers.push_back(new DataDumperOneClick());
  }

  if (args_.is_dump_app_manager) {
    data_dumpers.push_back(new DataDumperAppManager());
  }

  std::vector<DataDumper*>::iterator it = data_dumpers.begin();
  for (; it != data_dumpers.end(); ++it) {
    DataDumper* dumper = *it;

    dumper->Process(dump_log_, args_);
    delete dumper;
  }
  data_dumpers.clear();

  if (args_.is_monitor) {
    // We want to monitor activity from GoogleUpdate.exe.
    // Examples include:
    // * Process start with arguments
    // * Process exit
    // * Others?
    ProcessMonitor process_monitor;
    GoopdateProcessMonitorCallback callback(dump_log_);
    std::vector<CString> patterns;
    CString main_exe_file_name(MAIN_EXE_BASE_NAME _T(".exe"));
    main_exe_file_name.MakeLower();
    patterns.push_back(main_exe_file_name);
    patterns.push_back(CString(_T("notepad.exe")));
    process_monitor.StartWithPatterns(&callback, patterns);
    getchar();
    process_monitor.Stop();
  }

  ::CoUninitialize();

  return S_OK;
}

void Goopdump::PrintProgramHeader() {
  dump_log_.WriteLine(_T(""));
  dump_log_.WriteLine(_T("Goopdump.exe -- Debug Utility for Google Update"));
  dump_log_.WriteLine(_T("(c) Google, Inc."));
  dump_log_.WriteLine(_T(""));
}

void Goopdump::PrintUsage() {
  dump_log_.WriteLine(_T("Usage:"));
  dump_log_.WriteLine(_T(""));
}

void Goopdump::SetNewHandler() {
  VERIFY1(set_new_handler(&Goopdump::OutOfMemoryHandler) == 0);
}

void Goopdump::OutOfMemoryHandler() {
  ::RaiseException(EXCEPTION_ACCESS_VIOLATION,
                   EXCEPTION_NONCONTINUABLE,
                   0,
                   NULL);
}

CString Goopdump::cmd_line() const {
  return cmd_line_;
}

}  // namespace omaha

