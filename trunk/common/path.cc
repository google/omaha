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

#include "omaha/common/path.h"

#include <atlbase.h>
#include <atlstr.h>
#include <atlpath.h>
#include <map>
#include <vector>
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/shell.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"

namespace omaha {

const TCHAR* const kRegSvr32Cmd1 = _T("regsvr32 ");
const TCHAR* const kRegSvr32Cmd2 = _T("regsvr32.exe ");
const TCHAR* const kRunDll32Cmd1 = _T("rundll32 ");
const TCHAR* const kRunDll32Cmd2 = _T("rundll32.exe ");
const TCHAR* const kMsiExecCmd1  = _T("msiexec ");
const TCHAR* const kMsiExecCmd2  = _T("msiexec.exe ");
const TCHAR* const kDotExe       = _T(".exe");


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

// Get the starting path from the command string
CString GetStartingPathFromString(const CString& s) {
  CString path;
  CString str(s);
  TrimCString(str);

  int len = str.GetLength();
  if (len > 0) {
    if (str[0] == _T('"')) {
      // For something like:  "c:\Program Files\...\" ...
      int idx = String_FindChar(str.GetString() + 1, _T('"'));
      if (idx != -1)
        path.SetString(str.GetString() + 1, idx);
    } else {
      // For something like:  c:\PRGRA~1\... ...
      int idx = String_FindChar(str, _T(' '));
      path.SetString(str, idx == -1 ? len : idx);
    }
  }

  return path;
}

// Get the trailing path from the command string
CString GetTrailingPathFromString(const CString& s) {
  CString path;
  CString str(s);
  TrimCString(str);

  int len = str.GetLength();
  if (len > 0) {
    if (str[len - 1] == _T('"')) {
      // For something like:  regsvr32 /u /s "c:\Program Files\..."
      str.Truncate(len - 1);
      int idx = String_ReverseFindChar(str, _T('"'));
      if (idx != -1)
        path.SetString(str.GetString() + idx + 1, len - idx - 1);
    } else {
      // For something like:  regsvr32 /u /s c:\PRGRA~1\...
      int idx = String_ReverseFindChar(str, _T(' '));
      if (idx != -1)
        path.SetString(str.GetString() + idx + 1, len - idx - 1);
    }
  }

  return path;
}

// Get the file from the command string
HRESULT GetFileFromCommandString(const TCHAR* s, CString* file) {
  ASSERT1(file);

  if (!s || !*s) {
    return E_INVALIDARG;
  }

  CString str(s);
  TrimCString(str);

  // Handle the string starting with quotation mark
  // For example: "C:\Program Files\WinZip\WINZIP32.EXE" /uninstall
  if (str[0] == _T('"')) {
    int idx_quote = str.Find(_T('"'), 1);
    if (idx_quote != -1) {
      file->SetString(str.GetString() + 1, idx_quote - 1);
      return S_OK;
    } else {
      return E_FAIL;
    }
  }

  // Handle the string starting with "regsvr32"
  // For example: regsvr32 /u /s "c:\program files\google\googletoolbar3.dll"
  if (String_StartsWith(str, kRegSvr32Cmd1, true) ||
      String_StartsWith(str, kRegSvr32Cmd2, true)) {
    file->SetString(GetTrailingPathFromString(str));
    return S_OK;
  }

  // Handle the string starting with "rundll32"
  // For example: "rundll32.exe setupapi.dll,InstallHinfSection DefaultUninstall 132 C:\WINDOWS\INF\PCHealth.inf"  // NOLINT
  if (String_StartsWith(str, kRunDll32Cmd1, true) ||
      String_StartsWith(str, kRunDll32Cmd2, true)) {
    int idx_space = str.Find(_T(' '));
    ASSERT1(idx_space != -1);
    int idx_comma = str.Find(_T(','), idx_space + 1);
    if (idx_comma != -1) {
      file->SetString(str.GetString() + idx_space + 1,
                      idx_comma - idx_space - 1);
      TrimCString(*file);
      return S_OK;
    } else {
      return E_FAIL;
    }
  }

  // Handle the string starting with "msiexec"
  // For example: MsiExec.exe /I{25A13826-8E4A-4FBF-AD2B-776447FE9646}
  if (String_StartsWith(str, kMsiExecCmd1, true) ||
      String_StartsWith(str, kMsiExecCmd2, true)) {
    return E_FAIL;
  }

  // Otherwise, try to find the file till reaching ".exe"
  // For example: "C:\Program Files\Google\Google Desktop Search\GoogleDesktopSetup.exe -uninstall"  // NOLINT
  for (int i = 0; i < str.GetLength(); ++i) {
    if (String_StartsWith(str.GetString() + i, kDotExe, true)) {
      file->SetString(str, i + _tcslen(kDotExe));
      return S_OK;
    }
  }

  // As last resort, return the part from the beginning to first space found.
  int idx = str.Find(_T(' '));
  if (idx == -1) {
    file->SetString(str);
  } else {
    file->SetString(str, idx);
  }

  return S_OK;
}

// Expands the string with embedded special folder variables.
// TODO(omaha): This function seems to have a very specific purpose, which
// is not used in our code base. Consider removing it.
HRESULT ExpandStringWithSpecialFolders(CString* str) {
  ASSERT(str, (L""));

#pragma warning(push)
// construction of local static object is not thread-safe
#pragma warning(disable : 4640)
  static std::map<CString, CString> g_special_folders_mapping;
#pragma warning(pop)

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

// Internal helper method for normalizing a path
HRESULT NormalizePathInternal(const TCHAR* path, CString* normalized_path) {
  // We use '|' to separate fields
  CString field;
  int bar_idx = String_FindChar(path, _T('|'));
  if (bar_idx == -1)
    field = path;
  else
    field.SetString(path, bar_idx);

  if (IsRegistryPath(field)) {
    CString key_name, value_name;
    RET_IF_FAILED(RegSplitKeyvalueName(field, &key_name, &value_name));

    CString reg_value;
    RET_IF_FAILED(RegKey::GetValue(key_name, value_name, &reg_value));
    normalized_path->Append(reg_value);
  } else {
    RET_IF_FAILED(ExpandStringWithSpecialFolders(&field));
    normalized_path->Append(field);
  }

  if (bar_idx != -1)
    return NormalizePathInternal(path + bar_idx + 1, normalized_path);
  else
    return S_OK;
}

// Normalize a path
HRESULT NormalizePath(const TCHAR* path, CString* normalized_path) {
  ASSERT1(normalized_path);

  normalized_path->Empty();

  if (path) {
    HRESULT hr = NormalizePathInternal(path, normalized_path);
    if (FAILED(hr)) {
      normalized_path->Empty();
      UTIL_LOG(LE, (_T("[NormalizePath - unable to normalize path][%s][0x%x]"),
                    path, hr));
    }
    return hr;
  } else {
    return S_OK;
  }
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

  int len = ::_tcslen(path);
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

