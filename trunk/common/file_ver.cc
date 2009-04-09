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

#include "omaha/common/file_ver.h"
#include "omaha/common/debug.h"

namespace omaha {

// TODO(omaha): Write unittest for this class.

FileVer::FileVer() {
  file_ver_data_ = NULL;
  lang_charset_ = 0;
}

FileVer::~FileVer() {
  Close();
}

void FileVer::Close() {
  delete[] file_ver_data_;
  file_ver_data_ = NULL;
  lang_charset_ = 0;
}

BOOL FileVer::Open(const TCHAR* lpszModuleName) {
  ASSERT1(lpszModuleName);

  // Get the version information size and allocate the buffer.
  DWORD handle;
  DWORD ver_info_size =
      ::GetFileVersionInfoSize(const_cast<TCHAR*>(lpszModuleName), &handle);
  if (ver_info_size == 0) {
    return FALSE;
  }

  // Get version information.
  // file_ver_data_ is allocated here and deleted in Close() (or implicitly
  // in the destructor).
  file_ver_data_ = new byte[ver_info_size];
  ASSERT1(file_ver_data_);
  if (!file_ver_data_) {
    return FALSE;
  }

  if (!::GetFileVersionInfo(const_cast<TCHAR*>(lpszModuleName), handle,
                            ver_info_size,
                            reinterpret_cast<void**>(file_ver_data_))) {
    Close();
    return FALSE;
  }

  // Get the first language and character-set identifier.
  UINT query_size = 0;
  DWORD* translation_table = NULL;
  if (!::VerQueryValue(file_ver_data_,
                       _T("\\VarFileInfo\\Translation"),
                       reinterpret_cast<void**>(&translation_table),
                       &query_size) ||
      query_size == 0) {
    Close();
    return FALSE;
  }

  ASSERT1(query_size != 0);
  ASSERT1(translation_table);

  // Create charset.
  lang_charset_ = MAKELONG(HIWORD(translation_table[0]),
                           LOWORD(translation_table[0]));
  return TRUE;
}

CString FileVer::QueryValue(const TCHAR* lpszValueName) const {
  ASSERT1(lpszValueName);

  if (file_ver_data_ == NULL) {
    return (CString)_T("");
  }

  // Query version information value.
  UINT query_size = 0;
  LPVOID query_data = NULL;
  CString str_query_value, str_block_name;
  str_block_name.Format(_T("\\StringFileInfo\\%08lx\\%s"),
                        lang_charset_,
                        lpszValueName);

  if (::VerQueryValue(reinterpret_cast<void**>(file_ver_data_),
                      str_block_name.GetBuffer(0),
                      &query_data,
                      &query_size) &&
      query_size != 0 &&
      query_data) {
    str_query_value = reinterpret_cast<const TCHAR*>(query_data);
  }

  str_block_name.ReleaseBuffer();

  return str_query_value;
}

BOOL FileVer::GetFixedInfo(VS_FIXEDFILEINFO& vsffi) const {   // NOLINT
  if (file_ver_data_ == NULL) {
    return FALSE;
  }

  UINT query_size = 0;
  VS_FIXEDFILEINFO* pVsffi = NULL;
  if (::VerQueryValue(reinterpret_cast<void**>(file_ver_data_),
                      _T("\\"),
                      reinterpret_cast<void**>(&pVsffi),
                      &query_size) &&
      query_size != 0 &&
      pVsffi) {
    vsffi = *pVsffi;
    return TRUE;
  }

  return FALSE;
}

CString FileVer::FormatFixedFileVersion() const {
  CString str_version;
  VS_FIXEDFILEINFO vsffi = {0};

  if (GetFixedInfo(vsffi)) {
    str_version.Format(NOTRANSL(_T("%u.%u.%u.%u")),
                       HIWORD(vsffi.dwFileVersionMS),
                       LOWORD(vsffi.dwFileVersionMS),
                       HIWORD(vsffi.dwFileVersionLS),
                       LOWORD(vsffi.dwFileVersionLS));
  }
  return str_version;
}

CString FileVer::FormatFixedProductVersion() const {
  CString str_version;
  VS_FIXEDFILEINFO vsffi = {0};

  if (GetFixedInfo(vsffi)) {
    str_version.Format(NOTRANSL(_T("%u.%u.%u.%u")),
                       HIWORD(vsffi.dwProductVersionMS),
                       LOWORD(vsffi.dwProductVersionMS),
                       HIWORD(vsffi.dwProductVersionLS),
                       LOWORD(vsffi.dwProductVersionLS));
  }
  return str_version;
}

ULONGLONG FileVer::GetFileVersionAsULONGLONG() const {
  ULONGLONG version = 0;
  VS_FIXEDFILEINFO vsffi = {0};

  if (GetFixedInfo(vsffi)) {
    version = MAKEDLLVERULL(HIWORD(vsffi.dwProductVersionMS),
                            LOWORD(vsffi.dwProductVersionMS),
                            HIWORD(vsffi.dwProductVersionLS),
                            LOWORD(vsffi.dwProductVersionLS));
  }
  return version;
}

}  // namespace omaha

