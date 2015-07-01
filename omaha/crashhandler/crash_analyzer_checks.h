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

#ifndef OMAHA_CRASHHANDLER_CRASH_ANALYZER_CHECKS_H_
#define OMAHA_CRASHHANDLER_CRASH_ANALYZER_CHECKS_H_

#include "omaha/crashhandler/crash_analyzer.h"

namespace omaha {

// Tests added here must also register with the CrashAnalyzer.

// Checks whether an excessive number of executable pages have been mapped by
// the crashing process. This is typical of attacks that spray executable
// memory in an attempt to overlap a predictable address.
class MaxExecMappings : public CrashAnalyzerCheck {
 public:
  explicit MaxExecMappings(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
 private:
  static const size_t kMaxExecutablePages;
};

// Checks that the stack pointer for each thread points to a location within
// the actual stack allocated for the thread. This will not be the case when a
// stack pivot has been executed to pivot into a ROP chain.
class WildStackPointer : public CrashAnalyzerCheck {
 public:
  explicit WildStackPointer(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
};

// Checks whether the thread stack contain pointers into several low level
// win32 functions that are commonly used within ROP chains to achieve
// arbitrary shellcode execution.
class NtFunctionsOnStack : public CrashAnalyzerCheck {
 public:
  explicit NtFunctionsOnStack(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
};

// Checks whether PE images are mapped within the process which have been
// either unlinked from the modules list or have been loaded with a custom
// loader to avoid the modules list. This is typical of malware.
class PENotInModuleList : public CrashAnalyzerCheck {
 public:
  explicit PENotInModuleList(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
 private:
  bool MatchesPESignature(BYTE* buffer, size_t size) const;
};

// Samples executable mappings within the process to look for sequences
// of bytes which are commonly used in heap sprays. Specifically these
// are patterns which can be used simultaneously as addresses to pivot a
// vtable, vtable entries, and effective no-op instructions.
class ShellcodeSprayPattern : public CrashAnalyzerCheck {
 public:
  explicit ShellcodeSprayPattern(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
 private:
  bool SampleSegmentForRepeatedPatterns(
      BYTE* ptr,
      const std::vector<BYTE> patterns) const;

  static const BYTE kOverlapingInstructions[];
  static const size_t kNumSamples;
  static const size_t kMatchCutoff;
};

// Checks whether the exception which triggered the report was a dereference
// into areas of memory commonly used to bypass ASLR. Namely the addresses of
// thread information blocks or user shared data.
class TiBDereference : public CrashAnalyzerCheck {
 public:
  explicit TiBDereference(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
 private:
  static const DWORD kTiBBottom;
  static const DWORD kTiBTop;
  static const DWORD kSharedUserDataBottom;
  static const DWORD kSharedUserDataTop;
};

class ShellcodeJmpCallPop : public CrashAnalyzerCheck {
 public:
  explicit ShellcodeJmpCallPop(CrashAnalyzer* analyzer);
  virtual CrashAnalysisResult Run();
 private:
  static const size_t kScanOffset;
  static const BYTE kShortJmpIns;
  static const size_t kShortJmpInsSize;
  static const BYTE kLongJmpIns;
  static const size_t kLongJmpInsSize;
  static const size_t kCallInsSize;
  static const BYTE kCallInsPattern;
  static const BYTE kPopInsPattern;
  static const BYTE kPopInsMask;
};

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_ANALYZER_CHECKS_H_
