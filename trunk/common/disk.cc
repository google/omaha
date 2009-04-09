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

#include "omaha/common/disk.h"

#include <winioctl.h>
#include "omaha/common/const_config.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/localization.h"
#include "omaha/common/logging.h"
#include "omaha/common/shell.h"
#include "omaha/common/string.h"
#include "omaha/common/synchronized.h"
#include "omaha/common/system.h"
#include "omaha/common/timer.h"
#include "omaha/common/utils.h"

namespace omaha {

#define kNetdiskVendorId "netdisk"

// see also: http://support.microsoft.com/default.aspx?scid=kb;EN-US;Q264203

#if _MSC_VER < 1400
// Not defined in the headers we have; from MSDN:
#define IOCTL_STORAGE_QUERY_PROPERTY \
          CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _STORAGE_DEVICE_DESCRIPTOR {
  ULONG  Version;
  ULONG  Size;
  UCHAR  DeviceType;
  UCHAR  DeviceTypeModifier;
  BOOLEAN  RemovableMedia;
  BOOLEAN  CommandQueueing;
  ULONG  VendorIdOffset;
  ULONG  ProductIdOffset;
  ULONG  ProductRevisionOffset;
  ULONG  SerialNumberOffset;
  STORAGE_BUS_TYPE  BusType;
  ULONG  RawPropertiesLength;
  UCHAR  RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;

typedef enum _STORAGE_QUERY_TYPE {
  PropertyStandardQuery = 0,
  PropertyExistsQuery,
  PropertyMaskQuery,
  PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

typedef enum _STORAGE_PROPERTY_ID {
  StorageDeviceProperty = 0,
  StorageAdapterProperty,
  StorageDeviceIdProperty
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef struct _STORAGE_PROPERTY_QUERY {
  STORAGE_PROPERTY_ID  PropertyId;
  STORAGE_QUERY_TYPE  QueryType;
  UCHAR  AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;

// -------
#endif

#define kIoctlBufferSize 1024

#define kMaxDrivesCached 3
#define kMaxDriveLen 20
static TCHAR g_cache_drive[kMaxDrivesCached][kMaxDriveLen+1];
static bool g_cache_external[kMaxDrivesCached];
static int g_cache_pos;
static LLock g_cache_lock;

bool IsDiskExternal(const TCHAR *drive) {
  ASSERT(drive, (L""));
  ASSERT(lstrlen(drive) < kMaxDriveLen, (L""));

  DisableThreadErrorUI disable_error_dialog_box;

  {
    __mutexScope(g_cache_lock);
    for (int i = 0; i < kMaxDrivesCached; i++)
      if (!lstrcmp(drive, g_cache_drive[i])) {
        UTIL_LOG(L1, (L"cached disk ext %s %d", drive, g_cache_external[i]));
        return g_cache_external[i];
      }
  }

#ifdef _DEBUG
  Timer timer(true);
#endif

  byte buffer[kIoctlBufferSize+1];

  bool external = false;
  HANDLE device = CreateFile(drive,
                             GENERIC_READ,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             NULL,
                             NULL);
  if (device == INVALID_HANDLE_VALUE) {
      UTIL_LOG(L1, (L"disk external could not open drive %s", drive));
      goto done;
  }
  STORAGE_DEVICE_DESCRIPTOR *device_desc;
  STORAGE_PROPERTY_QUERY query;
  DWORD out_bytes;
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;
  *(query.AdditionalParameters) = 0;

  device_desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buffer);
  // should not be needed, but just to be safer
  ZeroMemory(buffer, kIoctlBufferSize);

  BOOL ok = ::DeviceIoControl(device,
                              IOCTL_STORAGE_QUERY_PROPERTY,
                              &query,
                              sizeof(STORAGE_PROPERTY_QUERY),
                              buffer,
                              kIoctlBufferSize,
                              &out_bytes,
                              (LPOVERLAPPED)NULL);

  if (ok &&
      device_desc->VendorIdOffset &&
      stristr(reinterpret_cast<char*>(buffer + device_desc->VendorIdOffset),
              kNetdiskVendorId)) {
    external = true;
    UTIL_LOG(L1, (L"ximeta netdisk %s", drive));
  }

  if (ok &&
      (device_desc->BusType == BusTypeUsb ||
       device_desc->BusType == BusType1394)) {
    external = true;
  }
  if (!ok) {
    UTIL_LOG(L1, (L"disk external ioctl failed %s", drive));
  }
  CloseHandle(device);
  done:
  UTIL_LOG(L1, (L"disk external %s %d time %s",
      drive, external, String_DoubleToString(timer.GetMilliseconds(), 3)));

  {
    __mutexScope(g_cache_lock);
    lstrcpyn(g_cache_drive[g_cache_pos], drive, kMaxDriveLen+1);
    g_cache_external[g_cache_pos] = external;
    if (++g_cache_pos >= kMaxDrivesCached) g_cache_pos = 0;
  }

  return external;
}

// find the first fixed local disk with at least the space requested
// confirms that we can create a directory on the drive
// returns the drive in the drive parameter
// returns E_FAIL if no drive with enough space could be found
HRESULT FindFirstLocalDriveWithEnoughSpace(const uint64 space_required,
                                           CString *drive) {
  ASSERT1(drive);

  DisableThreadErrorUI disable_error_dialog_box;

  const int kMaxNumDrives = 26;
  static const size_t kBufLen = (STR_SIZE("c:\\\0") * kMaxNumDrives) + 1;

  // obtain the fixed system drives
  TCHAR buf[kBufLen];
  DWORD str_len = ::GetLogicalDriveStrings(kBufLen, buf);
  if (str_len > 0 && str_len < kBufLen) {
    for (TCHAR* ptr = buf; *ptr != L'\0'; ptr += (lstrlen(ptr) + 1)) {
      UINT drive_type = GetDriveType(ptr);
      if (drive_type == DRIVE_FIXED) {
        CString test_drive(ptr);
        if (!IsDiskExternal(CString(L"\\\\?\\") + test_drive.Left(2))) {
          uint64 free_disk_space = 0;
          HRESULT hr = GetFreeDiskSpace(test_drive, &free_disk_space);

          if (SUCCEEDED(hr) && space_required <= free_disk_space) {
            CString temp_dir;
            // confirm that we can create a directory on this drive
            bool found = false;
            while (!found) {
              temp_dir = test_drive +
                         NOTRANSL(L"test") +
                         itostr(static_cast<uint32>(::GetTickCount()));
              if (!File::Exists (temp_dir)) found = true;
            }

            if (SUCCEEDED(CreateDir(temp_dir, NULL))) {
              VERIFY1(SUCCEEDED(DeleteDirectory(temp_dir)));
              *drive = test_drive;
              UTIL_LOG(L1, (L"drive %s enough space %d", test_drive.GetString(),
                            free_disk_space));
              return S_OK;
            }
          }
        }
      }
    }
  }

  return E_FAIL;
}

// Get free disk space of a drive containing the specified folder
HRESULT GetFreeDiskSpace(uint32 csidl, uint64* free_disk_space) {
  ASSERT1(free_disk_space);

  CString path;
  RET_IF_FAILED(Shell::GetSpecialFolder(csidl, false, &path));

  return GetFreeDiskSpace(path, free_disk_space);
}

// Get free disk space of a drive containing the specified folder
HRESULT GetFreeDiskSpace(const TCHAR* folder, uint64* free_disk_space) {
  ASSERT1(folder && *folder);
  ASSERT1(free_disk_space);

  DisableThreadErrorUI disable_error_dialog_box;

  CString drive(folder);

  // (Stupid API used by System::GetDiskStatistics will work with any folder -
  // as long as it EXISTS.  Since the data storage folder might not exist yet
  // (e.g., on a clean install) we'll just truncate it down to a drive letter.)
  drive = drive.Left(3);  // "X:\"
  ASSERT1(String_EndsWith(drive, _T(":\\"), false));

  // Get the free disk space available to this user on this drive
  uint64 free_bytes_current_user = 0LL;
  uint64 total_bytes_current_user = 0LL;
  uint64 free_bytes_all_users = 0LL;
  RET_IF_FAILED(System::GetDiskStatistics(drive,
                                          &free_bytes_current_user,
                                          &total_bytes_current_user,
                                          &free_bytes_all_users));

  *free_disk_space = std::min(free_bytes_current_user, free_bytes_all_users);

  return S_OK;
}

// Has enough free disk space on a drive containing the specified folder
HRESULT HasEnoughFreeDiskSpace(uint32 csidl, uint64 disk_space_needed) {
  uint64 free_disk_space = 0;
  if (SUCCEEDED(GetFreeDiskSpace(csidl, &free_disk_space))) {
    return (disk_space_needed <= free_disk_space) ?
           S_OK : CI_E_NOT_ENOUGH_DISK_SPACE;
  }
  return S_OK;
}

// Has enough free disk space on a drive containing the specified folder
HRESULT HasEnoughFreeDiskSpace(const TCHAR* folder, uint64 disk_space_needed) {
  uint64 free_disk_space = 0;
  if (SUCCEEDED(GetFreeDiskSpace(folder, &free_disk_space))) {
    return (disk_space_needed <= free_disk_space) ?
           S_OK : CI_E_NOT_ENOUGH_DISK_SPACE;
  }
  return S_OK;
}

bool IsHotPluggable(const TCHAR* drive) {
  ASSERT(drive, (L""));

  // Disable potential error dialogs during this check
  DisableThreadErrorUI disable_error_dialog_box;

  //
  // We set the default return value to true so that
  // we treat the disk as hot-pluggable in case we
  // don't know.
  //
  bool ret = true;

  if (drive && lstrlen(drive) >= 2) {
    CString volume_path(_T("\\\\.\\"));
    // We don't want the trailing backslash.
    volume_path.Append(drive, 2);

    CHandle volume(CreateFile(volume_path, GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              NULL, OPEN_EXISTING, 0, NULL));

    if (volume != INVALID_HANDLE_VALUE) {
      STORAGE_HOTPLUG_INFO shi = {0};
      shi.Size = sizeof(shi);
      DWORD bytes_returned = 0;
      if (::DeviceIoControl(volume, IOCTL_STORAGE_GET_HOTPLUG_INFO,  NULL, 0,
                            &shi, sizeof(STORAGE_HOTPLUG_INFO), &bytes_returned,
                            NULL)) {
          ret = (shi.DeviceHotplug != false);
        }
    }
  } else {
    ASSERT(false, (L"Invalid path"));
  }

  return ret;
}

bool IsLargeDrive(const TCHAR* drive) {
  ASSERT1(drive && *drive);

  DisableThreadErrorUI disable_error_dialog_box;

  ULARGE_INTEGER caller_free_bytes = {0};
  ULARGE_INTEGER total_bytes = {0};
  ULARGE_INTEGER total_free_bytes = {0};

  if (!::GetDiskFreeSpaceEx(drive,
                            &caller_free_bytes,
                            &total_bytes,
                            &total_free_bytes)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[IsLargeDrive - failed to GetDiskFreeSpaceEx][0x%x]"), hr));
    return false;
  }

  return (total_bytes.QuadPart > kLargeDriveSize);
}

HRESULT DevicePathToDosPath(const TCHAR* device_path, CString* dos_path) {
  ASSERT1(device_path);
  ASSERT1(dos_path);
  UTIL_LOG(L4, (_T("[DevicePathToDosPath][device_path=%s]"), device_path));

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

    UTIL_LOG(L4, (_T("[DevicePathToDosPath found drive]")
                  _T("[logical drive %s][device name %s]"),
                  drive_colon, device_name));

    size_t name_length = _tcslen(device_name);
    if (_tcsnicmp(device_path, device_name, name_length) == 0) {
      // Construct DOS path.
      dos_path->Format(_T("%s%s"), drive_colon, device_path + name_length);
      UTIL_LOG(L4, (_T("[DevicePathToDosPath][dos_path=%s]"), *dos_path));
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_INVALID_DRIVE);
}

}  // namespace omaha

