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

#ifndef OMAHA_COMMON_PATH_H__
#define OMAHA_COMMON_PATH_H__

#include <atlstr.h>
#include <vector>

namespace omaha {

// Get the starting path from the command string
CString GetStartingPathFromString(const CString& s);

// Get the trailing path from the command string
CString GetTrailingPathFromString(const CString& s);

// Get the file from the command string
HRESULT GetFileFromCommandString(const TCHAR* s, CString* file);

// Expands the string with embedded special folder variables
HRESULT ExpandStringWithSpecialFolders(CString* str);

// Normalize a path
HRESULT NormalizePath(const TCHAR* path, CString* normalized_path);

// Concatenate two paths together
CString ConcatenatePath(const CString& path1, const CString& path2);

// Get the file out of the file path
CString GetFileFromPath(const CString& path);

// Get the directory from the path
CString GetDirectoryFromPath(const CString& path);

// Remove the extension from the path.
CString GetPathRemoveExtension(const CString& path);

// Returns true iff path is an absolute path (starts with a drive name)
bool IsAbsolutePath(const TCHAR* path);

// Makes sure the path is enclosed with double quotation marks.
void EnclosePath(CString* path);

// Used to enclose paths that are typically used with LocalServer32 entries.
// Unenclosed LocalServer32 entries with spaces are not recommended because
// the LocalServer32 entry is a command line, not just an EXE path.
// DLL paths should not be enclosed, because InProcServer32 entries are just
// a DLL path and not a command line.
CString EnclosePathIfExe(const CString& module_path);

// remove any double quotation masks from an enclosed path
void UnenclosePath(CString* path);

// Converts the short path name to long name.
HRESULT ShortPathToLongPath(const CString& short_path, CString* long_path);

// Returns a list of files that match the criteria.
HRESULT FindFiles(const CString& dir,
                  const CString& pattern,
                  std::vector<CString>* files);

HRESULT FindFilesEx(const CString& dir,
                    const CString& pattern,
                    std::vector<CString>* files);

HRESULT FindFileRecursive(const CString& dir,
                          const CString& pattern,
                          std::vector<CString>* files);

}  // namespace omaha

#endif  // OMAHA_COMMON_PATH_H__
