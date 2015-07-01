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

#include "omaha/crashhandler/crash_analyzer.h"

#include <windows.h>
#include <atlstr.h>
#include <algorithm>

#include "omaha/base/scoped_ptr_address.h"
#include "omaha/base/utils.h"
#include "omaha/crashhandler/crash_analyzer_checks.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"

namespace omaha {

GPA_WRAP(psapi.dll,
         GetMappedFileNameA,
         (HANDLE process, LPVOID address, LPCSTR filename, DWORD size),
         (process, address, filename, size),
         WINAPI,
         DWORD,
         0);

void CrashAnalyzer::RegisterCrashAnalysisChecks() {
  checks_.push_back(new PENotInModuleList(this));
  checks_.push_back(new ShellcodeSprayPattern(this));
  checks_.push_back(new TiBDereference(this));
  checks_.push_back(new ShellcodeJmpCallPop(this));
  // The wild stack pointer check should always be performed at the end as it
  // is used as a signal for certain crashes and we miss out on the additional
  // context other checks could provide if we do this early.
  checks_.push_back(new WildStackPointer(this));
  // This check is likely to cause many false positives so it makes sense to do
  // it last.
  checks_.push_back(new MaxExecMappings(this));
}

CrashAnalyzer::CrashAnalyzer(const google_breakpad::ClientInfo& client_info)
    : exec_pages_(0),
      client_info_(client_info),
      user_stream_count_(0) {
  RegisterCrashAnalysisChecks();
  pid_ = ::GetProcessId(client_info_.process_handle());
}

bool CrashAnalyzer::Init() {
  HANDLE process = AttachDebugger();
  if (process == INVALID_HANDLE_VALUE) {
    return false;
  }

  InitializeMappings(process);
  return InitializeDebuggerAndSuspendProcess();
}

CrashAnalyzer::~CrashAnalyzer() {
  if (debug_break_event_.get()) {
    ::ContinueDebugEvent(debug_break_event_->dwProcessId,
                         debug_break_event_->dwThreadId,
                         DBG_CONTINUE);
    ::DebugActiveProcessStop(pid_);
  }
  for (size_t i = 0; i != checks_.size(); ++i) {
    delete checks_[i];
  }
  for (MemoryCache::iterator it = memory_cache_.begin();
       it != memory_cache_.end();
       ++it) {
    delete[] (*it).second;
  }
  for (ModuleMap::iterator it = modules_.begin();
       it != modules_.end();
       ++it) {
    ModuleInfo module_info = (*it).second;
    ::CloseHandle(module_info.handle);
  }
  for (size_t i = 0; i < user_streams_.size(); ++i) {
    char* buffer = reinterpret_cast<char*>(user_streams_[i].Buffer);
    delete[] buffer;
  }
}

bool CrashAnalyzer::ReadDwordAtAddress(BYTE* ptr, DWORD* buffer) const {
  if (!FindContainingMemorySegment(ptr) ||
      !::ReadProcessMemory(client_info_.process_handle(),
                           ptr,
                           buffer,
                           4,
                           NULL)) {
    return false;
  }
  return true;
}

bool CrashAnalyzer::ReadExceptionContext(CONTEXT* context) const {
  EXCEPTION_POINTERS* epp = NULL;
  EXCEPTION_POINTERS exception_pointers = {0};
  memset(context, 0, sizeof(*context));

  if (!::ReadProcessMemory(client_info_.process_handle(),
                           reinterpret_cast<LPCVOID>(client_info_.ex_info()),
                           reinterpret_cast<LPVOID>(&epp),
                           sizeof(epp),
                           NULL) ||
      epp == NULL) {
    return false;
  }
  if (!::ReadProcessMemory(client_info_.process_handle(),
                           reinterpret_cast<LPCVOID>(epp),
                           reinterpret_cast<LPVOID>(&exception_pointers),
                           sizeof(exception_pointers),
                           NULL) ||
      exception_pointers.ContextRecord == NULL) {
    return false;
  }
  if (!::ReadProcessMemory(
      client_info_.process_handle(),
      reinterpret_cast<LPCVOID>(exception_pointers.ContextRecord),
      reinterpret_cast<LPVOID>(context),
      sizeof(*context),
      NULL)) {
    return false;
  }
  return true;
}

bool CrashAnalyzer::ReadExceptionRecord(
    EXCEPTION_RECORD* exception_record) const {
  EXCEPTION_POINTERS* epp = NULL;
  EXCEPTION_POINTERS exception_pointers = {0};
  CONTEXT context = {0};
  memset(exception_record, 0, sizeof(exception_pointers));

  if (!::ReadProcessMemory(client_info_.process_handle(),
                           reinterpret_cast<LPCVOID>(client_info_.ex_info()),
                           reinterpret_cast<LPVOID>(&epp),
                           sizeof(epp),
                           NULL) ||
      epp == NULL) {
    return false;
  }
  if (!::ReadProcessMemory(client_info_.process_handle(),
                           reinterpret_cast<LPCVOID>(epp),
                           reinterpret_cast<LPVOID>(&exception_pointers),
                           sizeof(exception_pointers),
                           NULL) ||
      exception_pointers.ExceptionRecord == NULL) {
    return false;
  }
  if (!::ReadProcessMemory(
      client_info_.process_handle(),
      reinterpret_cast<LPCVOID>(exception_pointers.ExceptionRecord),
      reinterpret_cast<LPVOID>(exception_record),
      sizeof(*exception_record),
      NULL)) {
    return false;
  }
  return true;
}

BYTE* CrashAnalyzer::GetCachedSegment(BYTE* ptr) const {
  MemoryCache::const_iterator it = memory_cache_.find(ptr);
  if (it == memory_cache_.end()) {
    return NULL;
  }
  return (*it).second;
}

// Caller is responsible for deleting the buffer that this creates.
bool CrashAnalyzer::ReadMemorySegment(BYTE* ptr,
                                      BYTE** buffer,
                                      size_t* size) {
  MemoryMap::const_iterator it = memory_regions_.find(ptr);
  if (it == memory_regions_.end()) {
    return false;
  }
  const size_t region_size = (*it).second.RegionSize;
  *buffer = GetCachedSegment(ptr);
  if (*buffer) {
    *size = region_size;
    return true;
  }
  scoped_array<BYTE> region_buffer(new BYTE[region_size]);
  memset(region_buffer.get(), 0, region_size);
  if (!::ReadProcessMemory(client_info_.process_handle(),
                           ptr,
                           region_buffer.get(),
                           region_size,
                           NULL)) {
    return false;
  }
  *buffer = region_buffer.get();
  *size = region_size;
  memory_cache_[ptr] = region_buffer.release();
  return true;
}

BYTE* CrashAnalyzer::FindContainingMemorySegment(BYTE* ptr) const {
  SYSTEM_INFO system_info = {0};
  ::GetSystemInfo(&system_info);
  UINT_PTR page_size = system_info.dwPageSize;
  for (UINT_PTR guess = reinterpret_cast<UINT_PTR>(ptr) &
           ~(page_size - 1);
       guess != 0;
       guess -= system_info.dwPageSize) {
    BYTE* guess_addr = reinterpret_cast<BYTE*>(guess);
    MemoryMap::const_iterator mem_it = memory_regions_.find(guess_addr);
    if (mem_it == memory_regions_.end()) {
      continue;
    }
    const MEMORY_BASIC_INFORMATION mbi = (*mem_it).second;
    if (guess_addr + mbi.RegionSize > ptr) {
      return guess_addr;
    }
    // The address is outside of the closest mapping and therefore bad.
    break;
  }
  // We either reached 0x0 without finding a mapped segment or the address
  // is not mapped.
  return 0;
}

BYTE* CrashAnalyzer::GetThreadStack(BYTE* ptr) const {
  ThreadMap::const_iterator thread_it = thread_contexts_.find(ptr);
  if (thread_it == thread_contexts_.end()) {
    return 0;
  }
  const ThreadInfo thread = (*thread_it).second;
#ifdef _WIN64
  UINT_PTR stack_address = thread.context.Rsp;
#else
  UINT_PTR stack_address = thread.context.Esp;
#endif
  return reinterpret_cast<BYTE*>(stack_address);
}

size_t CrashAnalyzer::ScanSegmentForPointer(BYTE* ptr, BYTE* pattern) {
  BYTE* buffer = 0;
  size_t size = 0;
  if (!ReadMemorySegment(ptr, &buffer, &size)) {
    return 0;
  }
  size_t matches = 0;
  for (size_t i = 0; i != size; ++i) {
    BYTE** cmp = reinterpret_cast<BYTE**>(&buffer[i]);
    if (*cmp == pattern) {
      ++matches;
    }
  }
  return matches;
}

void CrashAnalyzer::AddCommentToUserStreams(const CStringA& text) {
  MINIDUMP_USER_STREAM user_stream = {0};
  user_stream.Type = CommentStreamA;
  user_stream.BufferSize = static_cast<ULONG>(text.GetLength());
  user_stream.Buffer = new char[user_stream.BufferSize];
  memcpy(user_stream.Buffer, text.GetString(), user_stream.BufferSize);
  user_streams_.push_back(user_stream);
}

size_t CrashAnalyzer::GetUserStreamInfo(MINIDUMP_USER_STREAM* user_stream_array,
                                        const size_t user_stream_array_size) {
  if (!user_stream_array) {
    return 0;
  }

  const size_t num_streams =
      std::min(user_streams_.size(), user_stream_array_size);
  for (size_t i = 0; i < num_streams; ++i) {
    memcpy(&user_stream_array[i], &user_streams_[i],
        sizeof(MINIDUMP_USER_STREAM));
  }

  return num_streams;
}

// Timeout for attaching the debugger is 2 seconds.
DWORD kDebugAttachTimeoutMs = 2000;

HANDLE CrashAnalyzer::AttachDebugger() {
  if (!::DebugActiveProcess(pid_)) {
    return INVALID_HANDLE_VALUE;
  }

  HANDLE process = 0;
  DEBUG_EVENT debug_event = {0};
  DWORD baseline_tick_count = ::GetTickCount();
  while (!process) {
    if (!::WaitForDebugEvent(&debug_event, 0)) {
      // We check if the timeout has elapsed only when the debugee has stopped
      // queueing debug events to avoid erroring out when a process has many
      // modules and threads.
      ::Sleep(50);
      if (TimeHasElapsed(baseline_tick_count, kDebugAttachTimeoutMs)) {
        ::DebugActiveProcessStop(pid_);
        return INVALID_HANDLE_VALUE;
      }
      continue;
    }
    if (debug_event.dwDebugEventCode != CREATE_PROCESS_DEBUG_EVENT) {
      ::DebugActiveProcessStop(pid_);
      return INVALID_HANDLE_VALUE;
    }
    process = debug_event.u.CreateProcessInfo.hProcess;
    if (!process) {
      ::DebugActiveProcessStop(pid_);
      return INVALID_HANDLE_VALUE;
    }
    ThreadInfo thread_info = {0};
    thread_info.context.ContextFlags = CONTEXT_INTEGER |
                                       CONTEXT_CONTROL |
                                       CONTEXT_SEGMENTS;
    thread_info.handle = debug_event.u.CreateProcessInfo.hThread;
    if (::GetThreadContext(debug_event.u.CreateProcessInfo.hThread,
                           &thread_info.context)) {
      thread_contexts_[reinterpret_cast<BYTE*>(
          debug_event.u.CreateProcessInfo.lpThreadLocalBase)] = thread_info;
    }
    ModuleInfo module_info = {0};
    MEMORY_BASIC_INFORMATION mbi = {0};
    if (::VirtualQueryEx(process,
                         module_info.address,
                         &mbi,
                         sizeof(mbi)) == sizeof(mbi)) {
      module_info.address = debug_event.u.CreateProcessInfo.lpBaseOfImage;
      module_info.size = mbi.RegionSize;
      module_info.handle = debug_event.u.CreateProcessInfo.hFile;
      module_info.base_image = true;
      modules_[reinterpret_cast<BYTE*>(module_info.address)] = module_info;
    }
    ::ContinueDebugEvent(debug_event.dwProcessId,
                         debug_event.dwThreadId,
                         DBG_CONTINUE);
  }
  return process;
}

void CrashAnalyzer::InitializeMappings(HANDLE process) {
  SYSTEM_INFO system_info = {0};
  ::GetSystemInfo(&system_info);
  MEMORY_BASIC_INFORMATION mbi = {0};
  BYTE* base_address = NULL;
  exec_pages_ = 0;
  while (::VirtualQueryEx(process, base_address, &mbi,
                          sizeof(mbi)) == sizeof(mbi)) {
    if (mbi.State == MEM_COMMIT) {
      memory_regions_[reinterpret_cast<BYTE*>(mbi.BaseAddress)] = mbi;
      if (mbi.Protect &
          (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE))
        exec_pages_ += mbi.RegionSize / system_info.dwPageSize;
    }
    base_address = static_cast<BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
  }
  return;
}

bool CrashAnalyzer::InitializeDebuggerAndSuspendProcess() {
  DWORD baseline_tick_count = ::GetTickCount();
  DEBUG_EVENT debug_event = {0};
  bool init_done = false;
  while (!init_done) {
    if (!::WaitForDebugEvent(&debug_event, 0)) {
      ::Sleep(50);
      if (TimeHasElapsed(baseline_tick_count, kDebugAttachTimeoutMs)) {
        ::DebugActiveProcessStop(pid_);
        return false;
      }
      continue;
    }
    MemoryMap::iterator it;
    ModuleInfo module_info = {0};
    ThreadInfo thread_info = {0};

    switch (debug_event.dwDebugEventCode) {
      case EXCEPTION_DEBUG_EVENT:
        init_done = true;
        break;
      case LOAD_DLL_DEBUG_EVENT:
        it = memory_regions_.find(
            reinterpret_cast<BYTE*>(debug_event.u.LoadDll.lpBaseOfDll));
        if (it == memory_regions_.end()) {
          break;
        }
        module_info.address = debug_event.u.LoadDll.lpBaseOfDll;
        module_info.size = it->second.RegionSize;
        module_info.handle = debug_event.u.LoadDll.hFile;
        module_info.base_image = false;
        modules_[reinterpret_cast<BYTE*>(module_info.address)] = module_info;
        break;
      case CREATE_THREAD_DEBUG_EVENT:
        thread_info.context.ContextFlags = CONTEXT_INTEGER |
                                           CONTEXT_CONTROL |
                                           CONTEXT_SEGMENTS;
        thread_info.handle = debug_event.u.CreateThread.hThread;
        if (::GetThreadContext(debug_event.u.CreateThread.hThread,
                               &thread_info.context)) {
          thread_contexts_[reinterpret_cast<BYTE*>(
              debug_event.u.CreateThread.lpThreadLocalBase)] =
                  thread_info;
        }
        break;
      default:
        break;
    }
    // We keep the process suspended while performing the analysis.
    if (!init_done) {
      ::ContinueDebugEvent(debug_event.dwProcessId,
                           debug_event.dwThreadId,
                           DBG_CONTINUE);
    }
  }
  debug_break_event_.reset(new DEBUG_EVENT);
  memcpy(debug_break_event_.get(), &debug_event, sizeof(*debug_break_event_));
  return true;
}

CrashAnalysisResult CrashAnalyzer::Analyze() {
  CrashAnalysisResult ret = ANALYSIS_NORMAL;
  for (size_t i = 0; i != checks_.size(); ++i) {
    ret = checks_[i]->Run();
    if (ret != ANALYSIS_NORMAL && ret != ANALYSIS_ERROR) {
      break;
    }
    ret = ANALYSIS_NORMAL;
  }

  return ret;
}

}  // namespace omaha
