// Copyright 2004-2009 Google Inc.
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
// Disk functions

#include "omaha/base/disk.h"

#include <algorithm>

#include "omaha/base/commontypes.h"
#include "omaha/base/const_config.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/shell.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/utils.h"

namespace omaha {

HRESULT DevicePathToDosPath(const TCHAR* device_path, CString* dos_path) {
  ASSERT1(device_path);
  ASSERT1(dos_path);

  dos_path->Empty();

  TCHAR drive_strings[MAX_PATH] = _T("");
  if (!::GetLogicalDriveStrings(arraysize(drive_strings), drive_strings)) {
    UTIL_LOG(L4, (_T("[DevicePathToDosPath-GetLogicalDriveStrings fail][0x%x]"),
                  HRESULTFromLastError()));
    return HRESULTFromLastError();
  }

  // Drive strings are stored as a set of null terminated strings, with an
  // extra null after the last string. Each drive string is of the form "C:\".
  // We convert it to the form "C:", which is the format expected by
  // ::QueryDosDevice().
  TCHAR drive_colon[3] = _T(" :");
  for (const TCHAR* next_drive_letter = drive_strings;
       *next_drive_letter;
       next_drive_letter += _tcslen(next_drive_letter) + 1) {
    // Dos device of the form "C:".
    *drive_colon = *next_drive_letter;
    TCHAR device_name[MAX_PATH] = _T("");
    if (!::QueryDosDevice(drive_colon, device_name, arraysize(device_name))) {
      UTIL_LOG(LEVEL_ERROR, (_T("[QueryDosDevice failed][0x%x]"),
                             HRESULTFromLastError()));
      continue;
    }

    size_t name_length = _tcslen(device_name);
    if (_tcsnicmp(device_path, device_name, name_length) == 0) {
      // Construct DOS path.
      SafeCStringFormat(dos_path, _T("%s%s"),
                        drive_colon, device_path + name_length);
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE);
}

}  // namespace omaha
