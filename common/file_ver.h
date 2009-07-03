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

#ifndef OMAHA_COMMON_FILE_VER_H_
#define OMAHA_COMMON_FILE_VER_H_

#include <windows.h>
#include <tchar.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class FileVer {
 public:
  FileVer();
  ~FileVer();

  // opens the version info for the specified file
  BOOL Open(const TCHAR* lpszModuleName);

  // Cleanup
  void Close();

  // Query for a given vlaue
  CString QueryValue(const TCHAR* lpszValueName) const;

  // Shortcuts for common values
  CString GetFileDescription() const {return QueryValue(_T("FileDescription"));}
  CString GetFileVersion() const     {return QueryValue(_T("FileVersion"));    }
  CString GetCompanyName() const     {return QueryValue(_T("CompanyName"));    }
  CString GetProductName() const     {return QueryValue(_T("ProductName"));    }
  CString GetProductVersion() const  {return QueryValue(_T("ProductVersion")); }

  // gets the FIXEDFILEINFO datastructure
  BOOL GetFixedInfo(VS_FIXEDFILEINFO& vsffi) const;   // NOLINT

  // returns a formated string representing the file and product versions
  // e.g. 2.4.124.34
  CString FormatFixedFileVersion() const;
  CString FormatFixedProductVersion() const;

  // Returns a ULONGLONG containing the version in DLL version format.
  ULONGLONG GetFileVersionAsULONGLONG() const;

  // gets the language ID
  LCID GetLanguageID() const         { return HIWORD(lang_charset_); }

 private:
  // versioning data returned by GetFileVersionInfo
  byte*  file_ver_data_;

  // language charset
  DWORD   lang_charset_;

  DISALLOW_EVIL_CONSTRUCTORS(FileVer);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_FILE_VER_H_
