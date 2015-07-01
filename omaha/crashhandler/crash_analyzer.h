// Copyright 2007-2013 Google Inc.
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

#ifndef OMAHA_CRASHHANDLER_CRASH_ANALYZER_H_
#define OMAHA_CRASHHANDLER_CRASH_ANALYZER_H_

#include <windows.h>
#include <atlstr.h>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/scoped_ptr_address.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

namespace omaha {

class CrashAnalyzerCheck;
class CrashAnalyzer;

enum CrashAnalysisResult {
  // The analysis ran successfully and there were no red flags.
  ANALYSIS_NORMAL,
  // The analysis failed.
  ANALYSIS_ERROR,
  // The process has a suspicious number of executable memory mappings.
  // This is likely the result of a spray.
  ANALYSIS_EXCESSIVE_EXEC_MEM,
  // Crash occured near an instruction sequence that modifies the stack
  // pointer.
  ANALYSIS_STACK_PIVOT,
  // The crashing instruction sequence looks like a bad dereference of
  // the vtable or member of it.
  ANALYSIS_VTABLE_DEREF,
  // A JMP, CALL, POP instruction sequence was found near a thread's
  // program counter.
  ANALYSIS_INS_PTR_READ,
  // A Pointer to a low level function commonly used in exploitation was
  // found on a thread's stack.
  ANALYSIS_NT_FUNC_STACK,
  // A thread's stack pointer is outside of a valid stack segment.
  ANALYSIS_WILD_STACK_PTR,
  // An image mapping was found that is not within the modules list.
  ANALYSIS_BAD_IMAGE_MAPPING,
  // An access violation occurred in ranges commonly used for TIBs or user
  // shared data. Probably an attempt to defeat ASLR.
  ANALYSIS_FAILED_TIB_DEREF,
  // An instruction sequence that looks like shellcode was found in an
  // executable mapping.
  ANALYSIS_FOUND_SHELLCODE
};

static CString CrashAnalysisResultToString(CrashAnalysisResult type) {
  switch (type) {
    case ANALYSIS_NORMAL:
      return CString(_T("Normal"));
    case ANALYSIS_ERROR:
      return CString(_T("Error occurred during analysis"));
    case ANALYSIS_EXCESSIVE_EXEC_MEM:
      return CString(_T("Excessive executable mappings found"));
    case ANALYSIS_STACK_PIVOT:
      return CString(_T("Crashing instruction modifies the stack pointer"));
    case ANALYSIS_VTABLE_DEREF:
      return
          CString(_T("Crashing instructions are dereferences into a vtable"));
    case ANALYSIS_INS_PTR_READ:
      return CString(_T("Crash occurred near JMP, CALL, POP sequence"));
    case ANALYSIS_NT_FUNC_STACK:
      return CString(_T("Stack contained pointer to internal function"));
    case ANALYSIS_WILD_STACK_PTR:
      return CString(_T("Stack pointer outside stack segment"));
    case ANALYSIS_BAD_IMAGE_MAPPING:
      return CString(_T("Image mapping found which is not in module list"));
    case ANALYSIS_FAILED_TIB_DEREF:
      return CString(_T("Crash attempting to dereference common TIB address"));
    case ANALYSIS_FOUND_SHELLCODE:
      return CString(_T("Executable mapping contained probable shellcode"));
  }
  return CString(_T("Unknown type"));
}

typedef struct {
  CONTEXT context;
  HANDLE handle;
} ThreadInfo;

typedef struct {
  PVOID address;
  size_t size;
  HANDLE handle;
  bool base_image;
} ModuleInfo;

// Memory map for the process keyed by base address.
typedef std::map<BYTE*, MEMORY_BASIC_INFORMATION> MemoryMap;

// Map of process' loaded modules keyed by base address.
typedef std::map<BYTE*, ModuleInfo> ModuleMap;

// Map of process threads keyed by TIB address
typedef std::map<BYTE*, ThreadInfo> ThreadMap;

// Map for caching memory segments read from the debugee.
typedef std::map<BYTE*, BYTE*> MemoryCache;

class CrashAnalyzer {
 public:
  explicit CrashAnalyzer(const google_breakpad::ClientInfo& client_info);
  ~CrashAnalyzer();

  bool Init();
  CrashAnalysisResult Analyze();

  bool ReadDwordAtAddress(BYTE* ptr, DWORD* buffer) const;
  bool ReadMemorySegment(BYTE* ptr, BYTE** buffer, size_t* size);
  BYTE* FindContainingMemorySegment(BYTE* ptr) const;
  BYTE* GetThreadStack(BYTE* ptr) const;
  size_t ScanSegmentForPointer(BYTE* ptr, BYTE* pattern);
  bool ReadExceptionContext(CONTEXT* context) const;
  bool ReadExceptionRecord(EXCEPTION_RECORD* exception_record) const;

  void AddCommentToUserStreams(const CStringA& text);
  // Returns the number of streams written to the provided array.
  size_t GetUserStreamInfo(MINIDUMP_USER_STREAM* user_stream_array,
                           size_t user_stream_array_size);

  size_t exec_pages() const { return exec_pages_; }
  MemoryMap memory_regions() const { return memory_regions_; }
  ModuleMap modules() const { return modules_; }
  ThreadMap thread_contexts() const { return thread_contexts_; }
  const google_breakpad::ClientInfo& client_info() const {
    return client_info_;
  }

 private:
  void RegisterCrashAnalysisChecks();
  HANDLE AttachDebugger();
  void InitializeMappings(HANDLE process);
  bool InitializeDebuggerAndSuspendProcess();
  BYTE* GetCachedSegment(BYTE* ptr) const;

  scoped_ptr<DEBUG_EVENT> debug_break_event_;
  DWORD pid_;

  size_t exec_pages_;
  MemoryMap memory_regions_;
  ModuleMap modules_;
  ThreadMap thread_contexts_;
  const google_breakpad::ClientInfo& client_info_;
  std::vector<CrashAnalyzerCheck*> checks_;
  MemoryCache memory_cache_;
  std::vector<MINIDUMP_USER_STREAM> user_streams_;
  size_t user_stream_count_;

  DISALLOW_COPY_AND_ASSIGN(CrashAnalyzer);
};

class CrashAnalyzerCheck {
 public:
  virtual ~CrashAnalyzerCheck() {}
  virtual CrashAnalysisResult Run() = 0;

 protected:
  explicit CrashAnalyzerCheck(CrashAnalyzer* analyzer)
    : analyzer_(*analyzer) {}

  // This class does not take ownership of the analyzer.
  CrashAnalyzer& analyzer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashAnalyzerCheck);
};

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_ANALYZER_H_
