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

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_DUMP_LOG_H_
#define OMAHA_TOOLS_SRC_GOOPDUMP_DUMP_LOG_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"

// TODO(omaha):  Can we use the other Omaha logging classes for this?

namespace omaha {

class DumpLogHandler {
 public:
  DumpLogHandler();
  virtual ~DumpLogHandler();

  virtual void WriteLine(const TCHAR* line) = 0;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DumpLogHandler);
};

class ConsoleDumpLogHandler : public DumpLogHandler {
 public:
  ConsoleDumpLogHandler();
  virtual ~ConsoleDumpLogHandler();

  virtual void WriteLine(const TCHAR* line);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ConsoleDumpLogHandler);
};

class DebugDumpLogHandler : public DumpLogHandler {
 public:
  DebugDumpLogHandler();
  virtual ~DebugDumpLogHandler();

  virtual void WriteLine(const TCHAR* line);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(DebugDumpLogHandler);
};

class FileDumpLogHandler : public DumpLogHandler {
 public:
  FileDumpLogHandler();
  virtual ~FileDumpLogHandler();

  void set_filename(const TCHAR* filename);
  virtual void WriteLine(const TCHAR* line);

 private:
  void WriteBufToFile(const void* buf, DWORD num_bytes_to_write);

  CString filename_;
  DISALLOW_EVIL_CONSTRUCTORS(FileDumpLogHandler);
};


class DumpLog {
 public:
  DumpLog();
  ~DumpLog();

  // Enables output to the console.
  void EnableConsole(bool enable);

  // Enables output to OutputDebugString().
  void EnableDebug(bool enable);

  // Adds a log handler to pipe content to.
  // Not needed for console or debug, those are done separately.
  // The class does not assume ownership of the pointer and the pointer must
  // live until this class is destroyed.
  void AddLogHandler(DumpLogHandler* log_handler);

  // Removes the log handler.  Removal is done based on pointer equality.
  void RemoveLogHandler(DumpLogHandler* log_handler);

  // Writes a line to each of the connected log handlers.
  void WriteLine(const TCHAR* format, ...) const;

 private:
  std::vector<DumpLogHandler*> log_handlers_;

  ConsoleDumpLogHandler console_handler_;
  DebugDumpLogHandler debug_handler_;

  DISALLOW_EVIL_CONSTRUCTORS(DumpLog);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_DUMP_LOG_H_

