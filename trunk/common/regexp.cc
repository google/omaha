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

#include "regexp.h"
#include "common/debug.h"

namespace omaha {

#define kMaxArgs 16

bool RE::PartialMatch(const TCHAR* text, const RE& re,  // 3..16 args
                      CString * a0,
                      CString * a1,
                      CString * a2,
                      CString * a3,
                      CString * a4,
                      CString * a5,
                      CString * a6,
                      CString * a7,
                      CString * a8,
                      CString * a9,
                      CString * a10,
                      CString * a11,
                      CString * a12,
                      CString * a13,
                      CString * a14,
                      CString * a15)
{
  ASSERT(text, (L""));
  // a0 may be NULL
  // a1 may be NULL
  // a2 may be NULL
  // a3 may be NULL
  // a4 may be NULL
  // a5 may be NULL
  // a6 may be NULL
  // a7 may be NULL
  // a8 may be NULL
  // a9 may be NULL
  // a10 may be NULL
  // a11 may be NULL
  // a12 may be NULL
  // a13 may be NULL
  // a14 may be NULL
  // a15 may be NULL

  CString * args[kMaxArgs];
  int n = 0;
  if (a0 == NULL) goto done; args[n++] = a0;
  if (a1 == NULL) goto done; args[n++] = a1;
  if (a2 == NULL) goto done; args[n++] = a2;
  if (a3 == NULL) goto done; args[n++] = a3;
  if (a4 == NULL) goto done; args[n++] = a4;
  if (a5 == NULL) goto done; args[n++] = a5;
  if (a6 == NULL) goto done; args[n++] = a6;
  if (a7 == NULL) goto done; args[n++] = a7;
  if (a8 == NULL) goto done; args[n++] = a8;
  if (a9 == NULL) goto done; args[n++] = a9;
  if (a10 == NULL) goto done; args[n++] = a10;
  if (a11 == NULL) goto done; args[n++] = a11;
  if (a12 == NULL) goto done; args[n++] = a12;
  if (a13 == NULL) goto done; args[n++] = a13;
  if (a14 == NULL) goto done; args[n++] = a14;
  if (a15 == NULL) goto done; args[n++] = a15;

done:
  return re.DoMatchImpl(text,args,n,NULL);
}

// Like PartialMatch(), except the "input" is advanced past the matched
// text.  Note: "input" is modified iff this routine returns true.
// For example, "FindAndConsume(s, "(\\w+)", &word)" finds the next
// word in "s" and stores it in "word".
bool RE::FindAndConsume(const TCHAR **input, const RE& re,
                        CString * a0,
                        CString * a1,
                        CString * a2,
                        CString * a3,
                        CString * a4,
                        CString * a5,
                        CString * a6,
                        CString * a7,
                        CString * a8,
                        CString * a9,
                        CString * a10,
                        CString * a11,
                        CString * a12,
                        CString * a13,
                        CString * a14,
                        CString * a15)
{
  ASSERT(input, (L""));
  // a0 may be NULL
  // a1 may be NULL
  // a2 may be NULL
  // a3 may be NULL
  // a4 may be NULL
  // a5 may be NULL
  // a6 may be NULL
  // a7 may be NULL
  // a8 may be NULL
  // a9 may be NULL
  // a10 may be NULL
  // a11 may be NULL
  // a12 may be NULL
  // a13 may be NULL
  // a14 may be NULL
  // a15 may be NULL

  CString * args[kMaxArgs];
  int n = 0;
  if (a0 == NULL) goto done; args[n++] = a0;
  if (a1 == NULL) goto done; args[n++] = a1;
  if (a2 == NULL) goto done; args[n++] = a2;
  if (a3 == NULL) goto done; args[n++] = a3;
  if (a4 == NULL) goto done; args[n++] = a4;
  if (a5 == NULL) goto done; args[n++] = a5;
  if (a6 == NULL) goto done; args[n++] = a6;
  if (a7 == NULL) goto done; args[n++] = a7;
  if (a8 == NULL) goto done; args[n++] = a8;
  if (a9 == NULL) goto done; args[n++] = a9;
  if (a10 == NULL) goto done; args[n++] = a10;
  if (a11 == NULL) goto done; args[n++] = a11;
  if (a12 == NULL) goto done; args[n++] = a12;
  if (a13 == NULL) goto done; args[n++] = a13;
  if (a14 == NULL) goto done; args[n++] = a14;
  if (a15 == NULL) goto done; args[n++] = a15;

done:
  return re.DoMatchImpl(*input,args,n,input);
}

}  // namespace omaha

