// Copyright 2003-2009 Google Inc.
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
//
// Debug unittest

#include <stdexcept>

#include "omaha/common/debug.h"
#include "omaha/common/test.h"
#include "omaha/common/time.h"

namespace omaha {

// test what happens when we hit an exception
int SEHExceptionTest(int level, int reserved) {
  const uint32 kflags = MB_SETFOREGROUND  |
                        MB_TOPMOST        |
                        MB_ICONWARNING    |
                        MB_OKCANCEL;
  if (::MessageBox(NULL,
                   L"do exception?",
                   L"exception test",
                   kflags) == IDOK) {
    int *a1 = static_cast<int *>(1);
    *a1 = 2;

    // if that does not work try:
    // simulate a divide by zero
    int a = 10;
    int b2 = a;
    b2 /= 2;
    b2 -= 5;
    // int c = a/b2;
    int c = 0;
    TCHAR *s = 0;
    s++;
    *s = 0;

    RaiseException(EXCEPTION_BREAKPOINT, 0, 0, NULL);
  }

  return 0;
}

int SEHCatchExceptionTest(int level, int reserved) {
  __try {
    SEHExceptionTest(level, reserved);
  } __except (SehSendMinidump(GetExceptionCode(),
                              GetExceptionInformation(),
                              kMinsTo100ns)) {
  }

  return 0;
}

#pragma warning(push)
#pragma warning(disable:4702)
// test what happens when we do a C++ exception
int CppExceptionTest(int level, int reserved) {
  _TRY_BEGIN
#if 0
    _THROW(std::logic_error, "throwing a fake logic_error");
#else
//    std::logic_error e("throwing a fake logic_error");
  //    e._Raise();
  std::runtime_error(std::string("throwing a fake logic_error"))._Raise();
#endif
  _CATCH(std::logic_error e)
    ASSERT(false, (L"caught exception"));
  _CATCH_END
  return 0;
}
#pragma warning(pop)

// test what happens when we do a REPORT
int ReportTest(int level, int reserved) {
  REPORT(false, R_ERROR, (L"test REPORT"), 592854117);
  return 0;
}

// test what happens when we hit an ASSERT
int AssertTest(int level, int reserved) {
  ASSERT(false, (L"test ASSERT"));
  return 0;
}

// test what happens when we hit an ABORT
int AbortTest(int level, int reserved) {
  ABORT((L"test ABORT"));
  ASSERT(false, (L"returned from ABORT"));
  return 0;
}

}  // namespace omaha

