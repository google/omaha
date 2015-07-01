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
// Simple tool to read the stamped tag inside a binary.

#include <Windows.h>
#include <stdio.h>
#include "base/scoped_ptr.h"
#include "omaha/common/file.h"
#include "omaha/common/extractor.h"

int _tmain(int argc, TCHAR* argv[]) {
  if (argc != 2) {
    _tprintf(_T("Incorrect number of arguments!\n"));
    _tprintf(_T("Usage: ReadTag <tagged_file>\n"));
    return -1;
  }

  const TCHAR* file = argv[1];
  if (!omaha::File::Exists(file)) {
    _tprintf(_T("File \"%s\" not found"), file);
    return -1;
  }

  omaha::TagExtractor ext;
  if (!ext.OpenFile(file)) {
    _tprintf(_T("Could not open file \"%s\""), file);
    return -1;
  }

  int len = 0;
  if (!ext.ExtractTag(NULL, &len)) {
    _tprintf(_T("Extract tag failed."));
    return -1;
  }

  scoped_array<char> buffer(new char[len]);
  if (!ext.ExtractTag(buffer.get(), &len)) {
    _tprintf(_T("Extract tag failed."));
    return -1;
  }

  printf("Tag = '%s'", buffer);
  return 0;
}
