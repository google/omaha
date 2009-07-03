// Copyright 2004-2009 Google Inc.
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

#include "omaha/common/clipboard.h"
#include "base/basictypes.h"
#include "omaha/common/debug.h"

namespace omaha {

// Put the given string on the system clipboard
void SetClipboard(const TCHAR *string_to_set) {
  ASSERT(string_to_set, (L""));

  int len = lstrlen(string_to_set);

  //
  // Note to developer: It is not always possible to step through this code
  //  since the debugger will possibly steal the clipboard.  E.g. OpenClipboard
  //  might succeed and EmptyClipboard might fail with "Thread does not have
  //  clipboard open".
  //

  // Actual clipboard processing
  if (::OpenClipboard(NULL)) {
    BOOL b = ::EmptyClipboard();
    ASSERT(b, (L"EmptyClipboard failed"));

    // Include the terminating null
    len++;

    HANDLE copy_handle = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,
                                       len * sizeof(TCHAR));
    ASSERT(copy_handle, (L""));

    byte* copy_data = reinterpret_cast<byte*>(::GlobalLock(copy_handle));
    memcpy(copy_data, string_to_set, len * sizeof(TCHAR));
    ::GlobalUnlock(copy_handle);

#ifdef _UNICODE
    HANDLE h = ::SetClipboardData(CF_UNICODETEXT, copy_handle);
#else
    HANDLE h = ::SetClipboardData(CF_TEXT, copy_handle);
#endif

    ASSERT(h != NULL, (L"SetClipboardData failed"));
    if (!h) {
      ::GlobalFree(copy_handle);
    }

    VERIFY(::CloseClipboard(), (L""));
  } else {
    ASSERT(false, (L"OpenClipboard failed - %i", ::GetLastError()));
  }
}

}  // namespace omaha

