// Copyright 2006-2009 Google Inc.
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


#ifndef OMAHA_MI_EXE_STUB_TAR_H_
#define OMAHA_MI_EXE_STUB_TAR_H_

#include <windows.h>
#include <tchar.h>
#include <atlsimpcoll.h>
#include <atlstr.h>

namespace omaha {

static const int kNameSize = 100;

typedef struct {
  char name[kNameSize];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[kNameSize];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];  // make it exactly 512 bytes
} USTARHeader;

// Supports untarring of files from a tar-format archive. Pretty minimal;
// doesn't work with everything in the USTAR format.
class Tar {
 public:
  Tar(const CString& target_dir, HANDLE file_handle, bool delete_when_done);
  ~Tar();

  typedef void (*TarFileCallback)(void* context, const TCHAR* filename);

  void SetCallback(TarFileCallback callback, void* callback_context) {
    callback_ = callback;
    callback_context_ = callback_context;
  }

  // Extracts all files in archive to directory specified in constructor.
  // Directory must exist. Returns true if successful.
  bool ExtractToDir();

 private:
  CString target_directory_name_;
  HANDLE file_handle_;
  bool delete_when_done_;
  CSimpleArray<CString> files_to_delete_;
  TarFileCallback callback_;
  void* callback_context_;

  bool ExtractOneFile(bool* done);
};

}  // namespace omaha

#endif  // OMAHA_MI_EXE_STUB_TAR_H_
