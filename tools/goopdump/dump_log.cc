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

#include "omaha/tools/goopdump/dump_log.h"

#include <vector>

#include "omaha/common/debug.h"
#include "omaha/common/file.h"

namespace omaha {

DumpLogHandler::DumpLogHandler() {
}

DumpLogHandler::~DumpLogHandler() {
}


ConsoleDumpLogHandler::ConsoleDumpLogHandler() {
}

ConsoleDumpLogHandler::~ConsoleDumpLogHandler() {
}

void ConsoleDumpLogHandler::WriteLine(const TCHAR* line) {
  _tprintf(_T("%s"), line);
}


DebugDumpLogHandler::DebugDumpLogHandler() {
}

DebugDumpLogHandler::~DebugDumpLogHandler() {
}

void DebugDumpLogHandler::WriteLine(const TCHAR* line) {
  ::OutputDebugString(line);
}


FileDumpLogHandler::FileDumpLogHandler() {
}

FileDumpLogHandler::~FileDumpLogHandler() {
}

void FileDumpLogHandler::set_filename(const TCHAR* filename) {
  filename_ = filename;
  if (File::Exists(filename_)) {
    File::Remove(filename_);

    // Write the UNICODE file marker at the beginning.
    char buf[2] = {0xff, 0xfe};
    WriteBufToFile(buf, 2);
  }
}

void FileDumpLogHandler::WriteLine(const TCHAR* line) {
  if (filename_.IsEmpty()) {
    return;
  }

  WriteBufToFile(line, _tcslen(line) * sizeof(TCHAR));
}

void FileDumpLogHandler::WriteBufToFile(const void* buf,
                                        DWORD num_bytes_to_write) {
  HANDLE h = ::CreateFile(filename_,
                          GENERIC_WRITE,
                          FILE_SHARE_READ,
                          NULL,
                          OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL);
  if (h == INVALID_HANDLE_VALUE) {
    return;
  }

  ::SetFilePointer(h, NULL, NULL, FILE_END);
  DWORD bytes_written = 0;
  ::WriteFile(h, buf, num_bytes_to_write, &bytes_written, NULL);
  ::CloseHandle(h);
}


DumpLog::DumpLog() {
}

DumpLog::~DumpLog() {
}

void DumpLog::EnableConsole(bool enable) {
  if (enable) {
    AddLogHandler(&console_handler_);
  } else {
    RemoveLogHandler(&console_handler_);
  }
}

void DumpLog::EnableDebug(bool enable) {
  if (enable) {
    AddLogHandler(&debug_handler_);
  } else {
    RemoveLogHandler(&debug_handler_);
  }
}

void DumpLog::AddLogHandler(DumpLogHandler* log_handler) {
  ASSERT1(log_handler);
  std::vector<DumpLogHandler*>::iterator it = log_handlers_.begin();
  for (; it != log_handlers_.end(); ++it) {
    DumpLogHandler* handler = *it;
    if (handler == log_handler) {
      return;
    }
  }

  log_handlers_.push_back(log_handler);
}

void DumpLog::RemoveLogHandler(DumpLogHandler* log_handler) {
  ASSERT1(log_handler);
  std::vector<DumpLogHandler*>::iterator it = log_handlers_.begin();
  for (; it != log_handlers_.end(); ++it) {
    DumpLogHandler* handler = *it;
    if (handler == log_handler) {
      log_handlers_.erase(it);
      return;
    }
  }
}

void DumpLog::WriteLine(const TCHAR* format, ...) const {
  va_list arg_list;
  va_start(arg_list, format);

  CString line;
  line.FormatV(format, arg_list);
  line.Append(_T("\r\n"));

  std::vector<DumpLogHandler*>::const_iterator it = log_handlers_.begin();
  for (; it != log_handlers_.end(); ++it) {
    DumpLogHandler* handler = *it;
    handler->WriteLine(line);
  }
}

}  // namespace omaha

