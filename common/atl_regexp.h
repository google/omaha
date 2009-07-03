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
// ATL Regular expression class that implements the RE interface.
// See MSDN help for example patterns (lookup help on CAtlRegExp)
// NOTE: This class adds about 8k to release builds.
// TODO(omaha): add a unit test, showing examples, testing functionality
// and perf.

#ifndef OMAHA_COMMON_ATL_REGEXP_H__
#define OMAHA_COMMON_ATL_REGEXP_H__

#include <atlstr.h>
#include <atlrx.h>
#include "base/basictypes.h"
#include "omaha/common/regexp.h"
#include "omaha/common/string.h"

namespace omaha {

// This class is essentially a copy of CAtlRECharTraitsWide from <atlrx.h>, but
// I've replaced CRT functions that are not in our minicrt.
//
// TODO(omaha): do we need this?
class CAtlRECharTraitsWideNoCrt
{
public:
  typedef WCHAR RECHARTYPE;

  // ATL80 addition.
  static size_t GetBitFieldForRangeArrayIndex(const RECHARTYPE *sz) throw()
  {
#ifndef ATL_NO_CHECK_BIT_FIELD
    ATLASSERT(UseBitFieldForRange());
#endif
    return static_cast<size_t>(*sz);
  }

  static RECHARTYPE *Next(const RECHARTYPE *sz) throw()
  {
    return (RECHARTYPE *) (sz+1);
  }

  static int Strncmp(const RECHARTYPE *szLeft,
                     const RECHARTYPE *szRight, size_t nCount) throw()
  {
    return String_StrNCmp(szLeft, szRight, nCount,false);
  }

  static int Strnicmp(const RECHARTYPE *szLeft,
                      const RECHARTYPE *szRight, size_t nCount) throw()
  {
    return String_StrNCmp(szLeft, szRight, nCount,true);
  }

  static RECHARTYPE *Strlwr(RECHARTYPE *sz) throw()
  {
    return String_FastToLower(sz);
  }

  // In ATL 80 Strlwr must be passed a buffer size for security reasons.
  // TODO(omaha): Implement the function to consider the nSize param.
  static RECHARTYPE *Strlwr(RECHARTYPE *sz, int) throw()
  {
    return Strlwr(sz);
  }

  static long Strtol(const RECHARTYPE *sz,
                     RECHARTYPE **szEnd, int nBase) throw()
  {
    return Wcstol(sz, szEnd, nBase);
  }

  static int Isdigit(RECHARTYPE ch) throw()
  {
    return String_IsDigit(ch) ? 1 : 0;
  }

  static const RECHARTYPE** GetAbbrevs()
  {
    static const RECHARTYPE *s_szAbbrevs[] =
    {
      L"a([a-zA-Z0-9])",  // alpha numeric
        L"b([ \\t])",    // white space (blank)
        L"c([a-zA-Z])",  // alpha
        L"d([0-9])",    // digit
        L"h([0-9a-fA-F])",  // hex digit
        L"n(\r|(\r?\n))",  // newline
        L"q(\"[^\"]*\")|(\'[^\']*\')",  // quoted string
        L"w([a-zA-Z]+)",  // simple word
        L"z([0-9]+)",    // integer
        NULL
    };

    return s_szAbbrevs;
  }

  static BOOL UseBitFieldForRange() throw()
  {
    return FALSE;
  }

  static int ByteLen(const RECHARTYPE *sz) throw()
  {
    return int(lstrlen(sz)*sizeof(WCHAR));
  }
};

typedef CAtlRegExp<CAtlRECharTraitsWideNoCrt> AtlRegExp;
typedef CAtlREMatchContext<CAtlRECharTraitsWideNoCrt> AtlMatchContext;

// implements the RE class using the ATL Regular Expressions class
class AtlRE : public RE {
 public:

  AtlRE(const TCHAR* pattern, bool case_sensitive = true);
  virtual ~AtlRE();

 protected:
  // See regexp.h for an explanation.
  virtual bool DoMatchImpl(const TCHAR* text,
                           CString* args[],
                           int n,
                           const TCHAR** match_end) const;

 private:
  mutable AtlRegExp re_;
  DISALLOW_EVIL_CONSTRUCTORS(AtlRE);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_ATL_REGEXP_H__
