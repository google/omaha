// Copyright 2007-2009 Google Inc.
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
// Path utility functions.

#include "omaha/base/path.h"

#include <atlbase.h>
#include <atlstr.h>
#include <atlpath.h>
#include <map>
#include <vector>
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/shell.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"

namespace omaha {
namespace detail {

typedef bool (*Filter)(const WIN32_FIND_DATA&);

bool IsFile(const WIN32_FIND_DATA& find_data) {
  return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool IsDirectory(const WIN32_FIND_DATA& find_data) {
  return (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool AllFiles(const WIN32_FIND_DATA&) {
  return true;
}

HRESULT FindFilesEx(const CString& dir,
                    const CString& pattern,
                    std::vector<CString>* files,
                    Filter func) {
  ASSERT1(files);

  files->clear();
  if (!File::Exists(dir)) {
    return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
  }

  CString files_to_find = ConcatenatePath(dir, pattern);
  WIN32_FIND_DATA find_data = {0};
  scoped_hfind hfind(::FindFirstFile(files_to_find, &find_data));
  if (!hfind) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L5, (_T("[File::GetWildcards - FindFirstFile failed][0x%x]"), hr));
    return hr;
  }

  do {
    if (func(find_data)) {
      files->push_back(find_data.cFileName);
    }
  } while (::FindNextFile(get(hfind), &find_data));

  return S_OK;
}

}  // namespace detail.

// Expands the string with embedded special folder variables.
// TODO(omaha): This function seems to have a very specific purpose, which
// is not used in our code base. Consider removing it.
HRESULT ExpandStringWithSpecialFolders(CString* str) {
  ASSERT(str, (L""));

  static std::map<CString, CString> g_special_folders_mapping;

  if (g_special_folders_mapping.size() == 0) {
    RET_IF_FAILED(
        Shell::GetSpecialFolderKeywordsMapping(&g_special_folders_mapping));
  }

  CString expanded_str;
  RET_IF_FAILED(
      ExpandEnvLikeStrings(*str, g_special_folders_mapping, &expanded_str));

  str->SetString(expanded_str);

  return S_OK;
}

CString ConcatenatePath(const CString& path1, const CString& path2) {
  CString ret(path1);

  // Append the file path using the PathAppend.
  VERIFY1(::PathAppend(CStrBuf(ret, MAX_PATH), path2));

  return ret;
}

// Get the filename from the path
// "C:\TEST\sample.txt" returns "sample.txt"
CString GetFileFromPath(const CString& path) {
  CPath path1(path);
  path1.StripPath();
  return static_cast<CString>(path1);
}

// Get the directory from the path
// "C:\TEST\sample.txt" returns "C:\TEST"
// Get the directory out of the file path
CString GetDirectoryFromPath(const CString& path) {
  CPath path1(path);
  path1.RemoveFileSpec();
  return static_cast<CString>(path1);
}

// Remove the extension from the path.
// "C:\TEST\sample.txt" returns "C:\TEST\sample"
CString GetPathRemoveExtension(const CString& path) {
  CPath path1(path);
  path1.RemoveExtension();
  return static_cast<CString>(path1);
}

// Basically, an absolute path starts with X:\ or \\ (a UNC name)
bool IsAbsolutePath(const TCHAR* path) {
  ASSERT1(path);

  const size_t len = ::_tcslen(path);
  if (len < 3)
    return false;
  if (*path == _T('"'))
    path++;
  if (String_StartsWith(path+1, _T(":\\"), false))
    return true;
  if (String_StartsWith(path, _T("\\\\"), false))
    return true;
  return false;
}

bool IsPathInFolder(const TCHAR* path, const TCHAR* folder) {
  ASSERT1(path);
  ASSERT1(folder);
  CString folder_path(folder);
  folder_path.TrimRight(_T('\\'));
  folder_path.MakeLower();

  const CPath common_path = CPath(path).CommonPrefix(folder_path);
  CString common_prefix = static_cast<CString>(common_path);
  common_prefix.MakeLower();
  return folder_path == common_prefix;
}

void EnclosePath(CString* path) {
  ASSERT1(path);

  if (path->IsEmpty()) {
    return;
  }

  bool starts_with_quote = (_T('"') == path->GetAt(0));
  bool ends_with_quote = (_T('"') == path->GetAt(path->GetLength() - 1));
  ASSERT(starts_with_quote == ends_with_quote, (_T("%s"), path->GetString()));
  bool is_enclosed = starts_with_quote && ends_with_quote;
  if (is_enclosed) {
    return;
  }

  path->Insert(0, _T('"'));
  path->AppendChar(_T('"'));
}

CString EnclosePathIfExe(const CString& module_path) {
  if (!String_EndsWith(module_path, _T(".exe"), true)) {
    return module_path;
  }

  CString enclosed_path(module_path);
  EnclosePath(&enclosed_path);
  return enclosed_path;
}

// remove any double quotation masks from an enclosed path
void UnenclosePath(CString* path) {
  ASSERT1(path);

  if (path->GetLength() > 1 && path->GetAt(0) == _T('"')) {
    bool right_quote_exists = (path->GetAt(path->GetLength() - 1) == _T('"'));
    ASSERT(right_quote_exists,
           (_T("[UnenclosePath - double quote mismatches]")));
    if (right_quote_exists) {
      // Remove the double quotation masks
      path->Delete(0);
      path->Truncate(path->GetLength() - 1);
    }
  }
}

HRESULT ShortPathToLongPath(const CString& short_path, CString* long_path) {
  ASSERT1(long_path);

  TCHAR long_name[MAX_PATH] = {0};
  if (!::GetLongPathName(short_path, long_name, MAX_PATH)) {
    return HRESULTFromLastError();
  }

  *long_path = long_name;
  return S_OK;
}

HRESULT FindFilesEx(const CString& dir,
                    const CString& pattern,
                    std::vector<CString>* files) {
  return detail::FindFilesEx(dir, pattern, files, &detail::IsFile);
}

HRESULT FindFiles(const CString& dir,
                  const CString& pattern,
                  std::vector<CString>* files) {
  return detail::FindFilesEx(dir, pattern, files, &detail::AllFiles);
}

HRESULT FindSubDirectories(const CString& dir,
                           const CString& pattern,
                           std::vector<CString>* files) {
  return detail::FindFilesEx(dir, pattern, files, &detail::IsDirectory);
}

HRESULT FindFileRecursive(const CString& dir,
                          const CString& pattern,
                          std::vector<CString>* files) {
  ASSERT1(files);

  std::vector<CString> temp_files;
  HRESULT hr = FindFilesEx(dir, pattern, &temp_files);
  if (hr != HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) && FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i < temp_files.size(); ++i) {
    files->push_back(ConcatenatePath(dir, temp_files[i]));
  }

  std::vector<CString> sub_dirs;
  hr = FindSubDirectories(dir, _T("*"), &sub_dirs);
  if (FAILED(hr)) {
    return hr;
  }

  for (size_t i = 0; i < sub_dirs.size(); ++i) {
    const CString& sub_dir = sub_dirs[i];
    if (sub_dir == _T(".") || sub_dir == _T("..")) {
      continue;
    }

    CString path = ConcatenatePath(dir, sub_dir);
    hr = FindFileRecursive(path, pattern, files);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

}  // namespace omaha

