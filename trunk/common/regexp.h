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
// Implementors: There is only one function to implement -- DoMatchImpl
// See below

#ifndef OMAHA_COMMON_REGEXP_H__
#define OMAHA_COMMON_REGEXP_H__

#include <atlstr.h>

namespace omaha {

// Interface for regular expression matching.  Also corresponds to a
// pre-compiled regular expression.  An "RE" object is safe for
// concurrent use by multiple threads.
class RE {
 public:

  // Matches "text" against "pattern".  If pointer arguments are
  // supplied, copies matched sub-patterns into them. Use braces
  // "{", "}" within the regexp to indicate a pattern to be copied.
  //
  // Returns true iff all of the following conditions are satisfied:
  //   a. some substring of "text" matches "pattern"
  //   b. The number of matched sub-patterns is >= number of supplied pointers
  static bool PartialMatch(const TCHAR* text, const RE& re, // 3..16 args
    CString * a0 = NULL,
    CString * a1 = NULL,
    CString * a2 = NULL,
    CString * a3 = NULL,
    CString * a4 = NULL,
    CString * a5 = NULL,
    CString * a6 = NULL,
    CString * a7 = NULL,
    CString * a8 = NULL,
    CString * a9 = NULL,
    CString * a10 = NULL,
    CString * a11 = NULL,
    CString * a12 = NULL,
    CString * a13 = NULL,
    CString * a14 = NULL,
    CString * a15 = NULL);

  // Like PartialMatch(), except the "input" is advanced past the matched
  // text.  Note: "input" is modified iff this routine returns true.
  // For example, "FindAndConsume(s, "{\\w+}", &word)" finds the next
  // word in "s" and stores it in "word".
  static bool FindAndConsume(const TCHAR** input, const RE& re,
    CString * a0 = NULL,
    CString * a1 = NULL,
    CString * a2 = NULL,
    CString * a3 = NULL,
    CString * a4 = NULL,
    CString * a5 = NULL,
    CString * a6 = NULL,
    CString * a7 = NULL,
    CString * a8 = NULL,
    CString * a9 = NULL,
    CString * a10 = NULL,
    CString * a11 = NULL,
    CString * a12 = NULL,
    CString * a13 = NULL,
    CString * a14 = NULL,
    CString * a15 = NULL);

 protected:

  // The behavior of this function is subject to how it's used
  // in PartialMatch() and FindAndConsume() above. See the header
  // description of those functions to understand how an implementation
  // should behave.
  // text is the text we're looking in
  // args is where matches should be outputted
  // n is the number of CStrings in args
  // match_end is a pointer to the position in text that
  // we ended matching on
  // returns true if data was found, false otherwise
  // Example:Suppose text = "google 1\nYahoo! 2\n ..." and the regexp
  // is something like "{\w+} \d". If args has two CStrings (n=2),
  // then args[0] = "google", arg[1] = "1" and match_end will point to the \n
  // before "Yahoo!"
  virtual bool DoMatchImpl(const TCHAR *text,
    CString * args[],
    int n,
    const TCHAR ** match_end) const = 0;
};

}  // namespace omaha

#endif  // OMAHA_COMMON_REGEXP_H__
