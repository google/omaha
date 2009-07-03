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
// The main file for a simple tool to apply a tag to a signed file.
#include <Windows.h>
#include <TCHAR.h>
#include "omaha/common/apply_tag.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/utils.h"

using omaha::CreateDir;
using omaha::ConcatenatePath;
using omaha::File;
using omaha::GetCurrentDir;
using omaha::GetDirectoryFromPath;
using omaha::GetFileFromPath;

int _tmain(int argc, TCHAR* argv[]) {
  if (argc != 4 && argc != 5) {
    _tprintf(_T("Incorrect number of arguments!\n"));
    _tprintf(_T("Usage: ApplyTag <signed_file> <outputfile> <tag> [append]\n"));
    return -1;
  }

  const TCHAR* file = argv[1];
  if (!File::Exists(file)) {
    _tprintf(_T("File \"%s\" not found!\n"), file);
    return -1;
  }

  bool append = false;
  if (argc == 5 && _tcsicmp(argv[4], _T("append")) == 0) {
    append = true;
  }

  CString dir = GetDirectoryFromPath(argv[2]);
  CString path = ConcatenatePath(GetCurrentDir(), dir);
  ASSERT1(!path.IsEmpty());
  if (!File::Exists(path)) {
    HRESULT hr = CreateDir(path, NULL);
    if (FAILED(hr)) {
      _tprintf(_T("Could not create dir %s\n"), path);
      return hr;
    }
  }

  CString file_name = GetFileFromPath(argv[2]);
  CString out_path = ConcatenatePath(path, file_name);
  ASSERT1(!out_path.IsEmpty());
  ASSERT1(File::Exists(path));
  omaha::ApplyTag tag;
  HRESULT hr = tag.Init(argv[1],
                        CT2CA(argv[3]),
                        lstrlenA(CT2CA(argv[3])),
                        out_path,
                        append);
  if (hr == E_INVALIDARG) {
    _tprintf(_T("The tag_string %s contains invalid characters."), argv[3]);
    _tprintf(_T("  We accept the following ATL RegEx '[-%{}/\a&=._]*'\n"));
    return hr;
  }

  if (FAILED(hr)) {
    _tprintf(_T("Tag.Init Failed hr = %x\n"), hr);
    return hr;
  }

  hr = tag.EmbedTagString();
  if (hr == APPLYTAG_E_ALREADY_TAGGED) {
    _tprintf(_T("The binary %s is already tagged."), argv[1]);
    _tprintf(_T(" In order to append the tag string, use the append flag.\n"));
    _tprintf(_T("Usage: ApplyTag <signed_file> <outputfile> <tag> [append]\n"));
  }

  return 0;
}
