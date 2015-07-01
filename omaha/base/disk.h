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

#ifndef OMAHA_COMMON_DISK_H__
#define OMAHA_COMMON_DISK_H__

#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

// A constant we use to determine a large drive.
// Although today this isn't really really large,
// it is enough to distinguish small, removable
// drives that are not continually connected to
// a computer, from the drives that are.
// In addition to using this constant, we
// also check if the drive is hot-pluggable.
const uint64 kLargeDriveSize = 0x00000000ffffffff;

// returns true if the device is an external disk
// drive typically is something like: \\?\C:
bool IsDiskExternal(const TCHAR *drive);

//
// Determines if a drive can be unplugged without manually disabling
// the drive first.  By default, USB drives are initialized with
// "surprise removal" enabled, which means they are hot-pluggable.
//
// @param drive  The root of the drive, as returned from GetLogicalDriveStrings
//               e.g. "E:\\".
//
// @returns true if the drive is optimized for quick/surprise removal.
//   If the function returns false, then caching (lazy write) is enabled for
//   the drive, otherwise it is not.
//   If an error occurs during this call, the return value will be 'true' since
//   we always want to treat a drive as hot-pluggable if we're not sure.
//
bool IsHotPluggable(const TCHAR* drive);

//
// @returns true if the specified drive is larger than kLargeDriveSize.
//
// @param drive  The root of the drive, as returned from GetLogicalDriveStrings
//               e.g. "E:\\".
//
bool IsLargeDrive(const TCHAR* drive);

// find the first fixed local disk with at least the space requested
// returns the drive in the drive parameter
// returns E_FAIL if no drive with enough space could be found
HRESULT FindFirstLocalDriveWithEnoughSpace(const uint64 space_required,
                                           CString *drive);

// Get free disk space of a drive containing the specified folder
HRESULT GetFreeDiskSpace(uint32 csidl, uint64* free_disk_space);

// Get free disk space of a drive containing the specified folder
HRESULT GetFreeDiskSpace(const TCHAR* folder, uint64* free_disk_space);

// Has enough free disk space on a drive containing the specified folder
HRESULT HasEnoughFreeDiskSpace(uint32 csidl, uint64 disk_space_needed);

// Has enough free disk space on a drive containing the specified folder
HRESULT HasEnoughFreeDiskSpace(const TCHAR* folder, uint64 disk_space_needed);

// Convert from "\Device\Harddisk0\Partition1\WINNT\System32\ntdll.dll" to
// "C:\WINNT\System32\ntdll.dll"
HRESULT DevicePathToDosPath(const TCHAR* device_path, CString* dos_path);

//
// Disables critical error dialogs on the current thread.
// The system does not display the critical-error-handler message box.
// Instead, the system returns the error to the calling process.
//
class DisableThreadErrorUI {
 public:
  DisableThreadErrorUI() {
    // Set the error mode
    prev_mode_ = SetErrorMode(SEM_FAILCRITICALERRORS);
  }

  ~DisableThreadErrorUI() {
    // Restore the error mode
    SetErrorMode(prev_mode_);
  }

 protected:
  UINT prev_mode_;
};

}  // namespace omaha

#endif  // OMAHA_COMMON_DISK_H__

