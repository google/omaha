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

#include "omaha/crashhandler/crash_analyzer_checks.h"

#include <winnt.h>
#include <cstdlib>
#include <ctime>

#include "omaha/base/safe_format.h"
#include "omaha/base/utils.h"

namespace omaha {

// The cutoff for executable mappings being reported is 400MB.
const size_t MaxExecMappings::kMaxExecutablePages = 0x19000;

MaxExecMappings::MaxExecMappings(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult MaxExecMappings::Run() {
  if (analyzer_.exec_pages() > kMaxExecutablePages) {
    return ANALYSIS_EXCESSIVE_EXEC_MEM;
  }
  return ANALYSIS_NORMAL;
}

WildStackPointer::WildStackPointer(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult WildStackPointer::Run() {
  const ThreadMap contexts = analyzer_.thread_contexts();
  for (ThreadMap::const_iterator thread_it = contexts.begin();
       thread_it != contexts.end();
       ++thread_it) {
    BYTE* tib_address = (*thread_it).first;
    BYTE* base_address = analyzer_.FindContainingMemorySegment(tib_address);
    BYTE* actual_stack = analyzer_.GetThreadStack(tib_address);
    if (!base_address || !actual_stack) {
      continue;
    }
    BYTE* buffer = 0;
    size_t size = 0;
    if (!analyzer_.ReadMemorySegment(base_address, &buffer, &size)) {
      continue;
    }
#ifdef _WIN64
    NT_TIB64* tib =
        reinterpret_cast<NT_TIB64*>(buffer +
            (tib_address - base_address));
#else
    NT_TIB32* tib =
        reinterpret_cast<NT_TIB32*>(buffer +
            (tib_address - base_address));
#endif
    if (reinterpret_cast<UINT_PTR>(actual_stack) < tib->StackLimit ||
        reinterpret_cast<UINT_PTR>(actual_stack) > tib->StackBase) {
      CStringA context;
      SafeCStringAFormat(
          &context, "The stack pointer for a thread points outside the TiB "
                    "stack segment.\n"
                    "TiB Address: %x\n"
                    "Actual stack pointer: %x\n"
                    "TiB stack base: %x\n"
                    "TiB stack limit: %x\n",
                    tib_address,
                    reinterpret_cast<size_t>(actual_stack),
                    tib->StackBase,
                    tib->StackLimit);
      analyzer_.AddCommentToUserStreams(context);
      return ANALYSIS_WILD_STACK_PTR;
    }
  }
  return ANALYSIS_NORMAL;
}

NtFunctionsOnStack::NtFunctionsOnStack(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult NtFunctionsOnStack::Run() {
  // Because the crash handler process is running on the same system
  // as the process which crashed we can assume that they are mapped
  // in the same location in both processes.
  HMODULE ntdll = LoadSystemLibrary(_T("ntdll.dll"));
  HMODULE kernel32 = LoadSystemLibrary(_T("kernel32.dll"));
  std::vector<BYTE*> functions;
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(kernel32, "HeapCreate")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(ntdll, "RtlCreateHeap")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(ntdll, "ZwProtectVirtualMemory")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(ntdll, "ZwAllocateVirtualMemory")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(kernel32, "SetProcessDEPPolicy")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(ntdll, "NtSetInformationProcess")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(kernel32, "WriteProcessMemory")));
  functions.push_back(reinterpret_cast<BYTE*>(
      ::GetProcAddress(ntdll, "ZwWriteVirtualMemory")));

  const ThreadMap contexts = analyzer_.thread_contexts();
  for (ThreadMap::const_iterator i = contexts.begin();
       i != contexts.end();
       ++i) {
    BYTE* stack_segment = analyzer_.FindContainingMemorySegment(
        analyzer_.GetThreadStack((*i).first));
    if (!stack_segment) {
      continue;
    }
    for (size_t p = 0; p != functions.size(); ++p) {
      if (analyzer_.ScanSegmentForPointer(stack_segment, functions[p])) {
        const BYTE* func_ptr = functions[p];
        CStringA context;
        SafeCStringAFormat(
            &context, "The stack for a thread contains pointers commonly used "
                      "for ROP chains.\n"
                      "Stack segment: %x\n"
                      "Suspicious pointer: %x\n",
                      reinterpret_cast<size_t>(stack_segment),
                      reinterpret_cast<size_t>(func_ptr));
        analyzer_.AddCommentToUserStreams(context);
        return ANALYSIS_NT_FUNC_STACK;
      }
    }
  }
  return ANALYSIS_NORMAL;
}

PENotInModuleList::PENotInModuleList(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult PENotInModuleList::Run() {
  SYSTEM_INFO system_info = {0};
  ::GetSystemInfo(&system_info);
  const size_t page_size = system_info.dwPageSize;
  const MemoryMap map = analyzer_.memory_regions();
  const ModuleMap modules = analyzer_.modules();
  for (MemoryMap::const_iterator it = map.begin();
       it != map.end();
       ++it) {
    if ((*it).second.Protect != PAGE_EXECUTE &&
        (*it).second.Protect != PAGE_EXECUTE_READ &&
        (*it).second.Protect != PAGE_EXECUTE_READWRITE &&
        (*it).second.Protect != PAGE_EXECUTE_WRITECOPY) {
      continue;
    }
    BYTE* exec_segment = (*it).first;
    BYTE* possible_header = 0;
    if (map.find(exec_segment - page_size) !=
        map.end()) {
      possible_header = exec_segment - page_size;
    }
    if (modules.find(exec_segment) != modules.end() ||
        (possible_header && modules.find(possible_header) !=
            modules.end())) {
      continue;
    }
    BYTE* exec_buffer = 0;
    size_t exec_size = 0;
    if (analyzer_.ReadMemorySegment(exec_segment,
                                    &exec_buffer,
                                    &exec_size)) {
      if (MatchesPESignature(exec_buffer, exec_size)) {
        return ANALYSIS_BAD_IMAGE_MAPPING;
      }
    }
    if (possible_header) {
      BYTE* header_buffer;
      size_t header_size = 0;
      if (!analyzer_.ReadMemorySegment(possible_header,
                                       &header_buffer,
                                       &header_size)) {
        continue;
      } else if (MatchesPESignature(header_buffer, header_size)) {
        CStringA context;
        SafeCStringAFormat(
            &context, "The process has a PE mapped which is not in the "
                      "modules list.\n"
                      "Segment: %x\n",
                      reinterpret_cast<size_t>(possible_header));
        analyzer_.AddCommentToUserStreams(context);
        CStringA segment_data(reinterpret_cast<char *>(header_buffer),
                              static_cast<int>(header_size));
        analyzer_.AddCommentToUserStreams(segment_data);
        return ANALYSIS_BAD_IMAGE_MAPPING;
      }
    }
  }
  return ANALYSIS_NORMAL;
}

bool PENotInModuleList::MatchesPESignature(BYTE* buffer, size_t size) const {
  if (size < sizeof(IMAGE_DOS_HEADER) ||
      buffer[0] != 'M' ||
      buffer[1] != 'Z') {
    return false;
  }
  IMAGE_DOS_HEADER* dos_header = reinterpret_cast<IMAGE_DOS_HEADER*>(buffer);
  if (size < dos_header->e_lfanew + sizeof(IMAGE_NT_HEADERS) ||
      buffer[dos_header->e_lfanew] != 'P' ||
      buffer[dos_header->e_lfanew + 1] != 'E' ||
      buffer[dos_header->e_lfanew + 2] != 0 ||
      buffer[dos_header->e_lfanew + 3] != 0) {
    return false;
  }
#ifndef _WIN64
  // For 32 bit binaries running on 64 bit platforms we need to account for
  // the wow64 thunks.
  IMAGE_NT_HEADERS* nt_header =
      reinterpret_cast<IMAGE_NT_HEADERS*>(buffer + dos_header->e_lfanew);
  if (nt_header->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
    return false;
  }
#endif
  return true;
}

// These patterns are the various bytes that, when repeated, can be used both
// as valid usermode addresses and effective no-op instructions.
const BYTE ShellcodeSprayPattern::kOverlapingInstructions[] = {
    0x04, 0x0C, 0x0D, 0x14, 0x15, 0x1C, 0x1D, 0x24, 0x25, 0x27, 0x2C, 0x2D,
    0x2F, 0x34, 0x35, 0x37, 0x3C, 0x3D, 0x3F, 0x40, 0x41, 0x42, 0x43, 0x44,
    0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F };
const size_t ShellcodeSprayPattern::kNumSamples = 20;
const size_t ShellcodeSprayPattern::kMatchCutoff = 50;

ShellcodeSprayPattern::ShellcodeSprayPattern(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult ShellcodeSprayPattern::Run() {
  const std::vector<BYTE> patterns(kOverlapingInstructions,
      kOverlapingInstructions + sizeof(kOverlapingInstructions));
  SYSTEM_INFO system_info = {0};
  ::GetSystemInfo(&system_info);
  const size_t page_size = system_info.dwPageSize;
  const MemoryMap map = analyzer_.memory_regions();
  const ModuleMap modules = analyzer_.modules();
  for (MemoryMap::const_iterator it = map.begin();
       it != map.end();
       ++it) {
    BYTE* base_address = (*it).first;
    if (((*it).second.Protect != PAGE_EXECUTE &&
         (*it).second.Protect != PAGE_EXECUTE_READ &&
         (*it).second.Protect != PAGE_EXECUTE_READWRITE &&
         (*it).second.Protect != PAGE_EXECUTE_WRITECOPY) ||
        modules.find(base_address) != modules.end()) {
      continue;
    }
    MemoryMap::const_iterator header_it = it;
    if (header_it != map.begin()) {
      --header_it;
      const size_t difference = base_address - (*header_it).first;
      if (difference <= page_size * 4 &&
          modules.find((*header_it).first) !=
              modules.end()) {
        // The mapping directly before the executable region (less than 4 pages
        // previous) is the PE header for a known module so we do not need to
        // scan this segment.
        continue;
      }
    }
    if (SampleSegmentForRepeatedPatterns(base_address, patterns)) {
      CStringA context;
      SafeCStringAFormat(
          &context, "The process has an executable mapping which contains "
                    "a overlapping instruction shellode spray pattern.\n"
                    "Segment: %x\n",
                    reinterpret_cast<size_t>(base_address));
      analyzer_.AddCommentToUserStreams(context);
      BYTE* buffer;
      size_t size = 0;
      if (analyzer_.ReadMemorySegment(base_address, &buffer, &size)) {
        CStringA segment_data(reinterpret_cast<char *>(buffer),
                              static_cast<int>(size));
        analyzer_.AddCommentToUserStreams(segment_data);
      }

      return ANALYSIS_FOUND_SHELLCODE;
    }
  }
  return ANALYSIS_NORMAL;
}

bool ShellcodeSprayPattern::SampleSegmentForRepeatedPatterns(
    BYTE* ptr,
    const std::vector<BYTE> patterns) const {
  BYTE* buffer;
  size_t size = 0;
  if (!analyzer_.ReadMemorySegment(ptr, &buffer, &size)) {
    return false;
  }
  srand(static_cast<unsigned int>(time(NULL)));
  // Rather than scan all executable memory for these patterns we sample random
  // offsets into the buffer for the patterns we are looking for. When we find
  // one we scan forward and backward and count the number of adjacent dwords
  // that match the pattern. We only trigger when we find kMatchCutoff
  // consecutive dwords matching the same pattern.
  for (size_t i = 0; i != kNumSamples; ++i) {
    const size_t offset = rand() % (size - 3);  // NOLINT
    const DWORD cmp = *(reinterpret_cast<DWORD*>(&buffer[offset]));
    for (size_t p = 0; p != patterns.size(); ++p) {
      size_t matches = 0;
      const DWORD pattern = patterns[p] << 24 |
                            patterns[p] << 16 |
                            patterns[p] << 8 |
                            patterns[p];
      if (cmp != pattern) {
        continue;
      }
      ++matches;
      for (size_t forward_off = offset + sizeof(DWORD);
           forward_off < size - sizeof(DWORD) && matches < kMatchCutoff;
           forward_off += sizeof(DWORD)) {
        const DWORD forward_cmp =
            *(reinterpret_cast<DWORD*>(&buffer[forward_off]));
        if (forward_cmp != pattern) {
          break;
        }
        ++matches;
      }
      for (size_t backward_off =
              (offset >= sizeof(DWORD)) ? offset - sizeof(DWORD) : offset;
           backward_off != 0 && matches < kMatchCutoff;
           backward_off -=
               (backward_off > sizeof(DWORD)) ? sizeof(DWORD) : backward_off) {
        const DWORD backward_cmp =
            *(reinterpret_cast<DWORD*>(&buffer[backward_off]));
        if (backward_cmp != pattern) {
          break;
        }
        ++matches;
      }
      if (matches >= kMatchCutoff) {
        return true;
      }
      break;
    }
  }
  return false;
}

const DWORD TiBDereference::kTiBBottom = 0x7ef00000;
const DWORD TiBDereference::kTiBTop = 0x7effffff;
const DWORD TiBDereference::kSharedUserDataBottom = 0x7ffe0000;
const DWORD TiBDereference::kSharedUserDataTop = 0x7fff0000;

TiBDereference::TiBDereference(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult TiBDereference::Run() {
  EXCEPTION_RECORD record = {0};
  if (!analyzer_.ReadExceptionRecord(&record)) {
    return ANALYSIS_ERROR;
  }
  if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
    return ANALYSIS_NORMAL;
  }
  if (record.NumberParameters == 2 &&
      ((record.ExceptionInformation[1] >= kTiBBottom &&
          record.ExceptionInformation[1] <= kTiBTop) ||
      (record.ExceptionInformation[1] >= kSharedUserDataBottom &&
          record.ExceptionInformation[1] <= kSharedUserDataTop))) {
    CStringA context;
    SafeCStringAFormat(
        &context, "An access violation occurred dereferencing an address "
                  "commonly used to defeat ASLR.\n"
                  "Crashing address: %x\n",
                  record.ExceptionInformation[1]);
    analyzer_.AddCommentToUserStreams(context);
    return ANALYSIS_FAILED_TIB_DEREF;
  }
  return ANALYSIS_NORMAL;
}

const size_t ShellcodeJmpCallPop::kScanOffset = 0x1000;
const BYTE ShellcodeJmpCallPop::kShortJmpIns = 0xEB;
const size_t ShellcodeJmpCallPop::kShortJmpInsSize = 2;
const BYTE ShellcodeJmpCallPop::kLongJmpIns = 0xE9;
const size_t ShellcodeJmpCallPop::kLongJmpInsSize = 5;
const size_t ShellcodeJmpCallPop::kCallInsSize = 5;
const BYTE ShellcodeJmpCallPop::kCallInsPattern = 0xE8;
const BYTE ShellcodeJmpCallPop::kPopInsPattern = 0x58;
const BYTE ShellcodeJmpCallPop::kPopInsMask = 0xF8;

ShellcodeJmpCallPop::ShellcodeJmpCallPop(CrashAnalyzer* analyzer)
    : CrashAnalyzerCheck(analyzer) {}

CrashAnalysisResult ShellcodeJmpCallPop::Run() {
  EXCEPTION_RECORD record = {0};
  if (!analyzer_.ReadExceptionRecord(&record)) {
    return ANALYSIS_ERROR;
  }
  BYTE* segment_base = analyzer_.FindContainingMemorySegment(
      reinterpret_cast<BYTE*>(record.ExceptionAddress));
  if (!segment_base) {
    return ANALYSIS_ERROR;
  }
  const size_t offset =
      reinterpret_cast<BYTE*>(record.ExceptionAddress) - segment_base;

  const ModuleMap modules = analyzer_.modules();
  if (modules.find(segment_base) != modules.end()) {
    return ANALYSIS_NORMAL;
  }

  size_t size = 0;
  BYTE* segment = 0;
  if (!analyzer_.ReadMemorySegment(segment_base, &segment, &size)) {
    return ANALYSIS_ERROR;
  }
  for (BYTE* ptr = (kScanOffset > offset) ?
           segment :
           ((segment + offset) - kScanOffset);
       (kScanOffset > ((size - kLongJmpInsSize) - offset)) ?
           (ptr < ((segment + size) - kLongJmpInsSize)) :
           (ptr < (segment + offset + kScanOffset));
       ++ptr) {
    int dest_offset = 0;
    if (*ptr == kShortJmpIns) {
      dest_offset = ptr[1] + kShortJmpInsSize;
    } else if (*ptr == kLongJmpIns) {
      dest_offset = (ptr[1] |
                     ptr[2] << 8 |
                     ptr[3] << 16 |
                     ptr[4] << 24) + kLongJmpInsSize;
    } else {
      continue;
    }
    if (dest_offset > static_cast<int>(kScanOffset) ||
        dest_offset < -static_cast<int>(kScanOffset) ||
        ptr + dest_offset > (segment + size) - kCallInsSize ||
        ptr + dest_offset < segment) {
      continue;
    }

    BYTE* call_ptr = ptr + dest_offset;
    if (*call_ptr != kCallInsPattern)
      continue;

    dest_offset = (call_ptr[1] |
                   call_ptr[2] << 8 |
                   call_ptr[3] << 16 |
                   call_ptr[4] << 24) + kCallInsSize;
    if (dest_offset > static_cast<int>(kScanOffset) ||
        dest_offset < -static_cast<int>(kScanOffset) ||
        call_ptr + dest_offset > segment + size ||
        call_ptr + dest_offset < segment) {
      continue;
    }

    if ((call_ptr[dest_offset] & kPopInsMask) == kPopInsPattern) {
      CStringA context;
      SafeCStringAFormat(
          &context, "The process crashed near a code sequence containing a "
                    "Jump->Call->Pop sequence commonly used by shellcode to "
                    "find the address of the instruction pointer.\n"
                    "Crashing address: %x\n"
                    "Crashing segment base: %x\n"
                    "Offset of JMP->CALL->POP: %x\n",
                    record.ExceptionAddress,
                    reinterpret_cast<size_t>(segment_base),
                    offset);
      analyzer_.AddCommentToUserStreams(context);
      CStringA segment_data(reinterpret_cast<char *>(segment),
                            static_cast<int>(size));
      analyzer_.AddCommentToUserStreams(segment_data);

      return ANALYSIS_FOUND_SHELLCODE;
    }
  }
  return ANALYSIS_NORMAL;
}

}  // namespace omaha
