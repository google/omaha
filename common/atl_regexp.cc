// Copyright 2005-2009 Google Inc.
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

#include "omaha/common/atl_regexp.h"

namespace omaha {

const int kMaxArgs  = 16;

AtlRE::AtlRE(const TCHAR* pattern, bool case_sensitive) {
  ASSERT(pattern, (L""));
  REParseError status = re_.Parse(pattern, case_sensitive);
  ASSERT(status == REPARSE_ERROR_OK, (L""));
}

AtlRE::~AtlRE() {
}

bool AtlRE::DoMatchImpl(const TCHAR* text,
                        CString* args[],
                        int n,
                        const TCHAR** match_end) const {
  // text may be NULL.
  ASSERT(args, (L""));

  if (!text) {
    return false;
  }

  AtlMatchContext matches;
  BOOL b = re_.Match(text, &matches, match_end);
  if (!b || matches.m_uNumGroups < static_cast<uint32>(n)) {
    return false;
  }

  // Oddly enough, the Match call will make match_end
  // point off the end of the string if the result is at the
  // end of the string. We check this and handle it.
  if (match_end) {
    if ((*match_end - text) >= lstrlen(text)) {
      *match_end = NULL;
    }
  }

  const TCHAR* start = 0;
  const TCHAR* end = 0;
  for (int i = 0; i < n; ++i) {
    matches.GetMatch(i, &start, &end);
    ptrdiff_t len = end - start;
    ASSERT(args[i], (L""));
    // len+1 for the NULL character that's placed by lstrlen
    VERIFY1(lstrcpyn(args[i]->GetBufferSetLength(len), start, len + 1) != NULL);
  }
  return true;
}

}  // namespace omaha
