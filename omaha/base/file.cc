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
// File handling routines
//
// Possible performance improvement: make a subclass or alternate class
// that reads an entire file into memory and fulfills read/write requests
// in memory (or equivalently, use a memory mapped file). This can greatly
// improve performance if there are files we do a lot of read/write requests on.
//
// We generally are dealing with files that can be very large and want to
// minimize memory usage, so this is not high priority
//
// this has the beginnings of asynchronous access support
//
// Unfortunately, doing asynchronous reads with FILE_FLAG_OVERLAPPED buys us
// nothing because the system cache manager enforces serial requests.
//
// Hence, we need to also use FILE_FLAG_NO_BUFFERING. this has a number of
// constraints:
// - file read/write position must be aligned on multiples of the disk sector
//   size
// - file read/write length must be a multiple of the sector size
// - read/write buffer must be aligned on multiples of the disk sector size
//
// In particular, this means that we cannot write 8 bytes, for example, because
// we have to write an entire sector.
//
// Currently, the implementation only supports enough to do some simple read
// tests
//
// The general idea is code that wants to to a sequence of asynchronous actions
// will look like the following, for an example of reading multiple event
// records asynchronously:
//
// uint32 async_id = File::GetNextAsyncId()
// while (!done) {
//   for (everything_to_do, e.g., for each event to read) {
//     call File::Read to read items needed;
//     returns TR_E_FILE_ASYNC_PENDING if queued; or returns data if done
//     process the item (e.g., event) if desired
//   }
//   call some routine to process pending completions; initiate delayed action
// }
// call some cleanup routine

#include "omaha/base/file.h"

#include <algorithm>
#include <memory>

#include "omaha/base/app_util.h"
#include "omaha/base/const_config.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/system.h"
#include "omaha/base/timer.h"
#include "omaha/base/utils.h"

namespace omaha {

// Constants
const uint32 kZeroSize = 4096;  // Buffer size used for clearing data in a file.

// The moves-pending-reboot is a MULTISZ registry key in the HKLM part of the
// registry.
static const TCHAR* kSessionManagerKey =
    _T("HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager");
static const TCHAR* kPendingFileRenameOps = _T("PendingFileRenameOperations");

File::File()
    : handle_(INVALID_HANDLE_VALUE), read_only_(false), sequence_id_(0) {
}

File::~File() {
  if (handle_ != INVALID_HANDLE_VALUE) {
    VERIFY_SUCCEEDED(Close());
  }
}

// open for reading only if write == false, otherwise both reading and writing
// allow asynchronous operations if async == true. Use this function when you
// need exclusive access to the file.
HRESULT File::Open(const TCHAR* file_name, bool write, bool async) {
  return OpenShareMode(file_name, write, async, 0);
}

// Allows specifying a sharing mode such as FILE_SHARE_READ. Otherwise,
// this is identical to File::Open().
HRESULT File::OpenShareMode(const TCHAR* file_name,
                            bool write,
                            bool async,
                            DWORD share_mode) {
  ASSERT1(file_name && *file_name);
  ASSERT1(handle_ == INVALID_HANDLE_VALUE);
  VERIFY1(!async);

  file_name_ = file_name;

  // there are restrictions on what we can do if using FILE_FLAG_NO_BUFFERING
  // if (!buffer) { flags |= FILE_FLAG_NO_BUFFERING; }
  // FILE_FLAG_WRITE_THROUGH
  // how efficient is NTFS encryption? FILE_ATTRIBUTE_ENCRYPTED
  // FILE_ATTRIBUTE_TEMPORARY
  // FILE_FLAG_RANDOM_ACCESS
  // FILE_FLAG_SEQUENTIAL_SCAN

  handle_ = ::CreateFile(file_name,
                         write ? (FILE_WRITE_DATA       |
                                  FILE_WRITE_ATTRIBUTES |
                                  FILE_READ_DATA) : FILE_READ_DATA,
                         share_mode,
                         NULL,
                         write ? OPEN_ALWAYS : OPEN_EXISTING,
                         FILE_FLAG_RANDOM_ACCESS,
                         NULL);

  if (handle_ == INVALID_HANDLE_VALUE) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
            (_T("[File::OpenShareMode - CreateFile failed][%s][%d][%d][0x%x]"),
             file_name, write, async, hr));
    return hr;
  }

  // This attribute is not supported directly by the CreateFile function.
  if (write &&
      !::SetFileAttributes(file_name, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
            (_T("[File::OpenShareMode - SetFileAttributes failed][0x%x]"), hr));
    return hr;
  }

  read_only_ = !write;
  pos_ = 0;
  return S_OK;
}

HRESULT File::IsReparsePoint(const TCHAR* file_name, bool* is_reparse_point) {
  ASSERT1(file_name && *file_name);
  ASSERT1(is_reparse_point);

  WIN32_FILE_ATTRIBUTE_DATA attrs = {};
  if (!::GetFileAttributesEx(file_name, ::GetFileExInfoStandard, &attrs)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(
        LE,
        (_T("[File::IsReparsePoint][::GetFileAttributesEx failed][%s][%#x]"),
         file_name, hr));
    return hr;
  }

  *is_reparse_point =
      ((attrs.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0);
  return S_OK;
}

HRESULT File::GetWildcards(const TCHAR* dir,
                           const TCHAR* wildcard,
                           std::vector<CString>* matching_paths) {
  ASSERT1(dir && *dir);
  ASSERT1(wildcard && *wildcard);
  ASSERT1(matching_paths);

  matching_paths->clear();

  // Make sure directory name ends with "\"
  CString directory = String_MakeEndWith(dir, _T("\\"), false);

  WIN32_FIND_DATA find_data;
  SetZero(find_data);
  scoped_hfind hfind(::FindFirstFile(directory + wildcard, &find_data));
  if (!hfind) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(L5, (_T("[File::GetWildcards - FindFirstFile failed][0x%x]"), hr));
    return hr;
  }
  do {
    if (find_data.dwFileAttributes == FILE_ATTRIBUTE_NORMAL ||
        !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      CString to_file(directory + find_data.cFileName);
      matching_paths->push_back(to_file);
    }
  } while (::FindNextFile(get(hfind), &find_data));

  HRESULT hr = HRESULTFromLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::GetWildcards - FindNextFile failed][0x%x]"), hr));
    return hr;
  }
  return S_OK;
}

// returns error if cannot remove
// returns success if removed or already removed
HRESULT File::Remove(const TCHAR* file_name) {
  ASSERT1(file_name && *file_name);

  if (!Exists(file_name)) {
    return S_OK;
  }

  if (!::DeleteFile(file_name)) {
    return HRESULTFromLastError();
  }

  return S_OK;
}

HRESULT File::CopyWildcards(const TCHAR* from_dir,
                            const TCHAR* to_dir,
                            const TCHAR* wildcard,
                            bool replace_existing_files) {
  ASSERT1(from_dir && *from_dir);
  ASSERT1(to_dir && *to_dir);
  ASSERT1(wildcard && *wildcard);

  // Make sure dir names end with a "\"
  CString from_directory = String_MakeEndWith(from_dir, _T("\\"), false);
  CString to_directory = String_MakeEndWith(to_dir, _T("\\"), false);

  // Get full path to source files (which is a wildcard)
  CString from_files(from_directory + wildcard);

  // Run over all files that match wildcard
  WIN32_FIND_DATA find_data;
  SetZero(find_data);

  scoped_hfind hfind(::FindFirstFile(from_files, &find_data));
  if (!hfind) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::CopyWildcards - FindFirstFile failed][0x%x]"), hr));
    return hr;
  }
  do {
    // Copy files
    if (find_data.dwFileAttributes == FILE_ATTRIBUTE_NORMAL ||
        !(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      CString from_file(from_directory + find_data.cFileName);
      CString to_file(to_directory + find_data.cFileName);

      if (!replace_existing_files && Exists(to_file)) {
        // Continue, since the caller has explicitly asked us to not replace an
        // existing file
        continue;
      }

      RET_IF_FAILED(Copy(from_file, to_file, replace_existing_files));
    }
  } while (::FindNextFile(get(hfind), &find_data));

  HRESULT hr = HRESULTFromLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::CopyWildcards - FindNextFile failed][0x%x]"), hr));
    return hr;
  }
  return S_OK;
}

HRESULT File::CopyTree(const TCHAR* from_dir,
                       const TCHAR* to_dir,
                       bool replace_existing_files) {
  ASSERT1(from_dir && *from_dir);
  ASSERT1(to_dir && *to_dir);

  UTIL_LOG(L3, (L"[File::CopyTree][from_dir %s][to_dir %s][replace %d]",
                from_dir, to_dir, replace_existing_files));

  // Make sure dir names end with a "\"
  CString from_directory(String_MakeEndWith(from_dir, L"\\", false));
  CString to_directory(String_MakeEndWith(to_dir, L"\\", false));

  RET_IF_FAILED(CreateDir(to_directory, NULL));
  RET_IF_FAILED(CopyWildcards(from_directory,
                              to_directory,
                              L"*.*",
                              replace_existing_files));

  // Run over all directories
  WIN32_FIND_DATA find_data;
  SetZero(find_data);

  CString from_files(from_directory);
  from_files += _T("*.*");

  scoped_hfind hfind(::FindFirstFile(from_files, &find_data));
  if (!hfind) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::CopyTree - FindFirstFile failed][0x%x]"), hr));
    return hr;
  }
  do {
    // Copy files
    if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        String_StrNCmp(find_data.cFileName, L"..", 2, false) &&
        String_StrNCmp(find_data.cFileName, L".", 2, false)) {
      CString from_subdir(from_directory + find_data.cFileName);
      CString to_subdir(to_directory + find_data.cFileName);
      RET_IF_FAILED(CopyTree(from_subdir, to_subdir, replace_existing_files));
    }
  } while (::FindNextFile(get(hfind), &find_data));

  HRESULT hr = HRESULTFromLastError();
  if (hr != HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES)) {
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::CopyTree - FindNextFile failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT File::Copy(const TCHAR* from,
                   const TCHAR* to,
                   bool replace_existing_file) {
  ASSERT1(from && *from);
  ASSERT1(to && *to);

  if (!replace_existing_file && Exists(to)) {
    // Return success, since the caller has explicitly asked us to not replace
    // an existing file
    return S_OK;
  }

  if (!::CopyFile(from, to, !replace_existing_file)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::Copy - CopyFile failed]")
                           _T("[from=%s][to=%s][replace=%u][0x%x]"),
                           from, to, replace_existing_file, hr));
    return hr;
  }

  return S_OK;
}

// TODO(omaha): Combine common code in Move/MoveAfterReboot
HRESULT File::Move(const TCHAR* from,
                   const TCHAR* to,
                   bool replace_existing_file) {
  ASSERT1(from && *from);
  ASSERT1(to && *to);

  DWORD flags = MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH;
  if (replace_existing_file) {
    flags |= MOVEFILE_REPLACE_EXISTING;
  }

  if (!::MoveFileEx(from, to, flags)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR,
             (_T("[File::Move - MoveFileEx failed]")
              _T("[from=%s][to=%s][replace=%u][0x%x]"),
              from, to, replace_existing_file, hr));
    return hr;
  }

  return S_OK;
}

// DeleteAfterReboot tries to delete the files by either moving them to the TEMP
// directory and deleting them on reboot, or if that fails, by trying to delete
// them in-place on reboot
HRESULT File::DeleteAfterReboot(const TCHAR* from) {
  ASSERT1(from && *from);

  if (File::Exists(from)) {
    HRESULT hr = HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    CString from_temp;

    // No point in moving into TEMP if we're already there
    if (!String_StartsWith(from, app_util::GetTempDir(), true)) {
      // Try to move to the TEMP directory first
      CString temp_dir(String_MakeEndWith(app_util::GetTempDir(),
                                          _T("\\"),
                                          false));
      // Of the form "C:\\Windows\\Temp\\FROM.EXE1f4c0b7f"
      SafeCStringFormat(&from_temp, _T("%s%s%x"),
                        temp_dir,
                        GetFileFromPath(from),
                        ::GetTickCount());

      hr = File::Move(from, from_temp, true);
      UTIL_LOG(L2, (_T("[File::DeleteAfterReboot - move %s to %s][0x%x]"),
                    from, from_temp, hr));
    }

    if (SUCCEEDED(hr)) {
      UTIL_LOG(L2, (_T("[File::DeleteAfterReboot - delete %s after reboot]"),
                    from_temp));
      // Move temp file after reboot
      if (FAILED(hr = File::MoveAfterReboot(from_temp, NULL))) {
        UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                               _T("[failed to delete after reboot %s][0x%x]"),
                               from_temp, hr));
      }
    } else  {
      // Move original file after reboot
      if (FAILED(hr = File::MoveAfterReboot(from, NULL))) {
        UTIL_LOG(LEVEL_ERROR, (_T("[DeleteWildcardFiles]")
                               _T("[failed to delete after reboot %s][0x%x]"),
                               from, hr));
      }
    }

    return hr;
  }

  return S_OK;
}


HRESULT File::MoveAfterReboot(const TCHAR* from, const TCHAR* to) {
  ASSERT1(from && *from);

  if (!File::Exists(from)) {
    // File/directory doesn't exist, should this return failure or success?
    // Decision:  Failure.  Because the caller can decide if it is really
    // failure or not in his specific case.
    UTIL_LOG(LEVEL_WARNING, (_T("[File::MoveAfterReboot]")
                             _T("[file doesn't exist][from %s]"), from));
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  DWORD flags = MOVEFILE_DELAY_UNTIL_REBOOT;
  if (!File::IsDirectory(from)) {
    // This flag valid only for files
    flags |= MOVEFILE_REPLACE_EXISTING;
  }

  if (!::MoveFileEx(from, to, flags)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::MoveAfterReboot]")
                           _T("[failed to MoveFileEx from '%s' to '%s'][0x%x]"),
                           from, to, hr));
    return hr;
  }

  return S_OK;
}

// See if we have any moves pending a reboot. Return SUCCESS if we do
// not encounter errors (not finding a move is not an error). We need to
// also check the value of *found_ptr for whether we actually found a move.
// On return, *value_multisz_ptr is the value within
// "PendingFileRenameOperations", but with any moves for in_directory removed
// from it.
// The prefix_match boolean controls whether we do an exact match on
// in_directory, or remove all entries with the in_directory prefix.
// NOTE: If the only values found were our own keys, the whole
// PendingFileRenameOperations MULTISZ needs to be deleted.
// This is signified by a returned *value_size_chars_ptr of 0.
HRESULT File::GetPendingRenamesValueMinusDir(
  const TCHAR* in_directory,
  bool prefix_match,
  std::unique_ptr<TCHAR[]>* value_multisz_ptr,
  size_t* value_size_chars_ptr,
  bool* found_ptr) {
  ASSERT1(in_directory && *in_directory);

  // Convert to references for easier-to-read-code:
  size_t& value_size_chars = *value_size_chars_ptr;
  bool& found = *found_ptr;

  // Initialize [out] parameters
  value_multisz_ptr->reset();
  value_size_chars = 0;
  found = false;

  // Locals mirroring the [out] parameters.
  // We will only set the corresponding [out] parameters when we have something
  // meaningful to return to the caller
  std::unique_ptr<byte[]> value_multisz_bytes;
  size_t value_size_chars_local = 0;

  size_t value_size_bytes = 0;
  // Get the current value of the key
  // If the Key is missing, that's totally acceptable.
  RET_IF_FALSE(
      RegKey::HasValue(kSessionManagerKey, kPendingFileRenameOps) &&
      SUCCEEDED(RegKey::GetValue(kSessionManagerKey,
                                 kPendingFileRenameOps,
                                 &value_multisz_bytes,
                                 &value_size_bytes)),
      S_OK);

  std::unique_ptr<TCHAR[]> value_multisz_local(
    reinterpret_cast<TCHAR*>(value_multisz_bytes.release()));

  ASSERT1(value_multisz_local.get() || value_size_bytes == 0);
  UTIL_LOG(L5, (_T("[File::GetPendingRenamesValueMinusDir]")
                _T("[read multisz %d bytes]"),
                value_size_bytes));
  RET_IF_FALSE(value_size_bytes > 0, S_OK);

  // The size should always be aligned to a TCHAR boundary, otherwise the key
  // is corrupted.
  ASSERT1((value_size_bytes % sizeof(TCHAR)) == 0);
  RET_IF_FALSE((value_size_bytes % sizeof(TCHAR)) == 0,
               HRESULT_FROM_WIN32(ERROR_BADKEY));
  // Valid size, so convert to TCHARs:
  value_size_chars_local = value_size_bytes / sizeof(TCHAR);

  // Buffer must terminate with two nulls
  ASSERT(value_size_chars_local >= 2 &&
         !value_multisz_local[value_size_chars_local - 1] &&
         !value_multisz_local[value_size_chars_local - 2],
         (_T("buffer must terminate with two nulls")));
  RET_IF_FALSE(value_size_chars_local >= 2 &&
               !value_multisz_local[value_size_chars_local - 1] &&
               !value_multisz_local[value_size_chars_local - 2],
               HRESULT_FROM_WIN32(ERROR_BADKEY));
  // Mark the end of the string.
  // multisz_end will point at the character past end of buffer:
  TCHAR* multisz_end = value_multisz_local.get() + value_size_chars_local;

  // We're looking for \??\C:\...  The \??\ was
  // added by the OS to the directory name we specified.
  CString from_dir(_T("\\??\\"));
  from_dir += in_directory;
  DWORD from_dir_len = from_dir.GetLength();

  // A MULTISZ is a list of null terminated strings, terminated by a double
  // null.  We keep two pointers marching along the string in parallel.
  TCHAR* str_read = value_multisz_local.get();
  TCHAR* str_write = str_read;

  while ((str_read < multisz_end) && *str_read) {
    size_t str_len = ::lstrlen(str_read);
  // A FALSE here indicates a corrupt PendingFileRenameOperations
    RET_IF_FALSE((str_read + str_len + 1) < multisz_end,
      HRESULT_FROM_WIN32(ERROR_BADKEY));
    if (0 == String_StrNCmp(str_read,
                            from_dir,
                            from_dir_len + (prefix_match ? 0 : 1),
                            true)) {
      // String matches, we want to remove this string, so advance only the
      // read pointer - past this string and the replacement string.
      UTIL_LOG(L5, (_T("[File::GetPendingRenamesValueMinusDir]")
                    _T("[skips past match '%s']"),
                    str_read));
      str_read += str_len + 1;
      str_read += ::lstrlen(str_read) + 1;
      continue;
    }
    // String doesn't match, we want to keep it.
    if (str_read != str_write) {
      // Here we're not in sync in the buffer, we've got to move two
      // strings down.
      UTIL_LOG(L5, (_T("[File::GetPendingRenamesValueMinusDir]")
                    _T("[copying some other deletion][%s][%s]"),
                    str_read, str_read + ::lstrlen(str_read) + 1));
      ASSERT1(str_write < str_read);
      String_StrNCpy(str_write, str_read, str_len+1);
      str_read += str_len + 1;
      str_write += str_len + 1;
      str_len = ::lstrlen(str_read);
      String_StrNCpy(str_write, str_read, str_len+1);
      str_read += str_len + 1;
      str_write += str_len + 1;
    } else {
      // We're in sync in the buffer, advance both pointers past two strings
      UTIL_LOG(L5, (_T("[File::GetPendingRenamesValueMinusDir]")
                    _T("[skipping past some other deletion][%s][%s]"),
                    str_read, str_read + ::lstrlen(str_read) + 1));
      str_read += str_len + 1;
      str_read += ::lstrlen(str_read) + 1;
      str_write = str_read;
    }
  }

  // A FALSE here indicates a corrupt PendingFileRenameOperations
  RET_IF_FALSE(str_read < multisz_end,
    HRESULT_FROM_WIN32(ERROR_BADKEY));

  if (str_read != str_write) {
    // We found some values
    found = true;

    if (str_write == value_multisz_local.get()) {
      // The only values were our own keys,
      // and the whole PendingFileRenameOperations
      // value needs to be deleted. We do not populate
      // value_size_chars or value_multisz in this case.
      ASSERT1(!value_size_chars);
      ASSERT1(!*value_multisz_ptr);
    } else  {
      // The last string should have a NULL terminator:
      ASSERT1(str_write[-1] == '\0');
      RET_IF_FALSE(str_write[-1] == '\0',
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
      // a REG_MULTI_SZ needs to be terminated with an extra NULL.
      *str_write = '\0';
      ++str_write;

      // Populate value_size_chars and value_multisz in this case.
      value_multisz_ptr->reset(value_multisz_local.release());
      value_size_chars = str_write - value_multisz_ptr->get();
    }
  }

  return S_OK;
}

// Remove any moves pending a reboot from the PendingFileRenameOperations
// in the registry.
// The prefix_match boolean controls whether we do an exact match on
// in_directory, or remove all entries with the in_directory prefix.
HRESULT File::RemoveFromMovesPendingReboot(const TCHAR* in_directory,
                                           bool prefix_match) {
  ASSERT1(in_directory && *in_directory);

  bool found = false;
  // unique_ptr will free the value_multisz buffer on stack unwind:
  std::unique_ptr<TCHAR[]> value_multisz;
  size_t value_size_chars = 0;
  HRESULT hr = GetPendingRenamesValueMinusDir(in_directory,
                                              prefix_match,
                                              &value_multisz,
                                              &value_size_chars,
                                              &found);
  if (SUCCEEDED(hr) && found) {
    if (value_multisz.get() == NULL)  {
      // There's no point in writing an empty value_multisz.
      // Let's delete the PendingFileRenameOperations value
      UTIL_LOG(L5, (_T("[File::RemoveFromMovesPendingReboot]")
                    _T("[deleting PendingFileRenameOperations value]")));
      RET_IF_FAILED(RegKey::DeleteValue(kSessionManagerKey,
                    kPendingFileRenameOps));
    } else  {
      // Let's write the modified value_multisz into the
      // PendingFileRenameOperations value
      UTIL_LOG(L5, (_T("[File::RemoveFromMovesPendingReboot]")
                    _T("[rewriting multisz %d bytes]"),
                    value_size_chars * sizeof(TCHAR)));
      RET_IF_FAILED(RegKey::SetValueMultiSZ(
          kSessionManagerKey,
          kPendingFileRenameOps,
          reinterpret_cast<byte*>(value_multisz.get()),
          value_size_chars * sizeof(TCHAR)));
    }
  }

  // Failure of GetPendingRenamesValueMinusDir() may indicate something
  // seriously wrong with the system. Propogate error.
  return hr;
}

// Did the user try to uninstall a previous install of the same version, and
// we couldn't clean up without a reboot?
// We check if there are any moves pending a reboot from the
// PendingFileRenameOperations in the registry.
// The prefix_match boolean controls whether we do an exact match on
// in_directory, or check all entries with the in_directory prefix.
bool File::AreMovesPendingReboot(const TCHAR* in_directory, bool prefix_match) {
  ASSERT1(in_directory && *in_directory);

  bool found = false;
  // unique_ptr will free the value_multisz buffer on stack unwind:
  std::unique_ptr<TCHAR[]> value_multisz;
  size_t value_size_chars = 0;

  if (SUCCEEDED(GetPendingRenamesValueMinusDir(in_directory,
                                               prefix_match,
                                               &value_multisz,
                                               &value_size_chars,
                                               &found)) && found) {
    return true;
  }

  return false;
}

HRESULT File::GetFileTime(const TCHAR* file_name,
                          FILETIME* created,
                          FILETIME* accessed,
                          FILETIME* modified) {
  ASSERT1(file_name && *file_name);

  bool is_dir = IsDirectory(file_name);
  // To obtain a handle to a directory, call the CreateFile function with
  // the FILE_FLAG_BACKUP_SEMANTICS flag
  scoped_hfile file_handle(
      ::CreateFile(file_name,
                   FILE_READ_DATA,
                   FILE_SHARE_READ,
                   NULL,
                   OPEN_EXISTING,
                   is_dir ? FILE_FLAG_BACKUP_SEMANTICS : NULL,
                   NULL));
  HRESULT hr = S_OK;

  if (!file_handle) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LE, (_T("[File::GetFileTime]")
                  _T("[failed to open file][%s][0x%x]"), file_name, hr));
  } else {
    if (!::GetFileTime(get(file_handle), created, accessed, modified)) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[File::GetFileTime]")
                             _T("[failed to get file time][%s][0x%x]"),
                             file_name, hr));
    }
  }

  return hr;
}

HRESULT File::SetFileTime(const TCHAR* file_name,
                          const FILETIME* created,
                          const FILETIME* accessed,
                          const FILETIME* modified) {
  ASSERT1(file_name && *file_name);

  bool is_dir = IsDirectory(file_name);
  // To obtain a handle to a directory, call the CreateFile function with
  // the FILE_FLAG_BACKUP_SEMANTICS flag
  scoped_hfile file_handle(
      ::CreateFile(file_name,
                   FILE_WRITE_ATTRIBUTES,
                   FILE_SHARE_WRITE,
                   NULL,
                   OPEN_EXISTING,
                   is_dir ? FILE_FLAG_BACKUP_SEMANTICS : NULL,
                   NULL));
  HRESULT hr = S_OK;

  if (!file_handle) {
    hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::GetFileTime]")
                           _T("[failed to open file][%s][0x%x]"),
                           file_name, hr));
  } else {
    BOOL res = ::SetFileTime(get(file_handle), created, accessed, modified);
    if (!res) {
      hr = HRESULTFromLastError();
      UTIL_LOG(LEVEL_ERROR, (_T("[File::SetFileTime]")
                             _T("[failed to set file time][%s][0x%x]"),
                             file_name, hr));
    }
  }

  return hr;
}

HRESULT File::Sync() {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  if (!::FlushFileBuffers(handle_)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::Sync]")
                           _T("[FlushFileBuffers failed][%s][0x%x]"),
                           file_name_, hr));
    return hr;
  }
  return S_OK;
}

HRESULT File::SeekToBegin() {
  return SeekFromBegin(0);
}

HRESULT File::SeekFromBegin(uint32 n) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  if (::SetFilePointer(handle_, n, NULL, FILE_BEGIN) ==
      INVALID_SET_FILE_POINTER) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::SeekFromBegin]")
                           _T("[SetFilePointer failed][%s][0x%x]"),
                           file_name_, hr));
    return hr;
  }
  pos_ = n;
  return S_OK;
}

// read nLen bytes starting at position n
// returns number of bytes read
//
// async operations:
//
// async_id - identifier of a sequence of async operations, 0 for synchronous
//
// if the async operation has not been initiated, we initiate it
// if it is in progress we do nothing
// if it has been completed we return the data
// does not delete async data entry
HRESULT File::ReadAt(const uint32 offset, byte* buf, const uint32 len,
                     const uint32, uint32* bytes_read) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(buf);
  ASSERT1(len);  // reading 0 bytes is not valid (differs from CRT API)

  RET_IF_FAILED(SeekFromBegin(offset));

  DWORD read = 0;
  if (!::ReadFile(handle_, buf, len, &read, NULL)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::ReadAt]")
                           _T("[ReadFile failed][%s][0x%x]"), file_name_, hr));
    return hr;
  }

  if (bytes_read) {
    *bytes_read = read;
  }

  return (read == len) ? S_OK : E_FAIL;
}


// reads up to max_len bytes from the start of the file
// not considered an error if there are less than max_len bytes read
// returns number of bytes read
HRESULT File::ReadFromStartOfFile(const uint32 max_len,
                                  byte* buf,
                                  uint32* bytes_read) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(buf);
  ASSERT1(max_len);

  RET_IF_FAILED(SeekFromBegin(0));

  uint32 file_len = 0;
  RET_IF_FAILED(GetLength(&file_len));

  if (!file_len) {
    if (bytes_read) {
      *bytes_read = 0;
    }
    return S_OK;
  }

  uint32 len = max_len;
  if (len > file_len) {
    len = static_cast<uint32>(file_len);
  }

  return Read(len, buf, bytes_read);
}

// this function handles lines terminated with LF or CRLF
// all CR characters are removed from each line, and LF is assumed
// to be the end of line and is removed
HRESULT File::ReadLineAnsi(uint32 max_len, char* line, uint32* len) {
  ASSERT1(line);
  ASSERT1(max_len);

  char c = 0;
  uint32 len_read = 0;
  uint32 total_len = 0;

  while (SUCCEEDED(Read(1, reinterpret_cast<byte *>(&c), &len_read)) &&
         len_read  &&
         c != '\n') {
    if (total_len < max_len - 1 && c != '\r') {
      line[total_len++] = c;
    }
  }

  ASSERT1(total_len < max_len);
  line[total_len] = '\0';

  if (len) {
    *len = total_len;
  }

  return (len_read || total_len) ? S_OK : E_FAIL;
}

// used by ReadFromStartOfFile and ReadLineAnsi; not reading all requested bytes
// is not considered fatal
HRESULT File::Read(const uint32 len, byte* buf, uint32* bytes_read) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(buf);
  ASSERT1(len);

  DWORD read = 0;
  if (!::ReadFile(handle_, buf, len, &read, NULL)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::ReadAt]")
                           _T("[ReadFile failed][%s][0x%x]"),
                           file_name_, hr));
    return hr;
  }

  if (bytes_read) {
    *bytes_read = read;
  }

  return S_OK;
}


// returns number of bytes written
HRESULT File::WriteAt(const uint32 offset,
                      const byte* buf,
                      const uint32 len,
                      uint32,
                      uint32* bytes_written) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(!read_only_);
  ASSERT1(buf);
  ASSERT1(len);

  RET_IF_FAILED(SeekFromBegin(offset));

  return Write(buf, len, bytes_written);
}


// write buffer n times
HRESULT File::WriteN(const byte* buf,
                     const uint32 len,
                     const uint32 n,
                     uint32* bytes_written) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(!read_only_);
  ASSERT1(buf);
  ASSERT1(len);
  ASSERT1(n);

  HRESULT hr = S_OK;

  uint32 total_wrote = 0;

  byte* temp_buf = const_cast<byte*>(buf);

  std::unique_ptr<byte[]> encrypt_buf;

  uint32 to_go = n;
  while (to_go) {
    uint32 wrote = 0;
    hr = Write(temp_buf, len, &wrote);
    if (FAILED(hr)) {
      if (bytes_written) {
        *bytes_written = total_wrote;
      }
      return hr;
    }

    total_wrote += wrote;
    to_go--;
  }

  if (bytes_written) {
    *bytes_written = total_wrote;
  }
  return hr;
}

// returns number of bytes written
HRESULT File::Write(const byte* buf, const uint32 len, uint32* bytes_written) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(!read_only_);
  ASSERT1(buf);
  ASSERT1(len);  // writing 0 bytes is not valid (differs from CRT API)

  byte* b = const_cast<byte*>(buf);

  std::unique_ptr<byte[]> encrypt_buf;

  DWORD wrote = 0;
  if (!::WriteFile(handle_, b, len, &wrote, NULL)) {
    HRESULT hr = HRESULTFromLastError();
    UTIL_LOG(LEVEL_ERROR, (_T("[File::Write]")
                           _T("[WriteFile failed][%s][0x%x]"),
                           file_name_, hr));
    return hr;
  }

  if (bytes_written) {
    *bytes_written = wrote;
  }
  pos_ += wrote;

  return (wrote == len) ? S_OK : E_FAIL;
}

HRESULT File::ClearAt(const uint32 offset,
                      const uint32 len,
                      uint32* bytes_written) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(!read_only_);
  ASSERT1(len);

  byte zero[kZeroSize] = {0};
  uint32 to_go = len;
  uint32 written = 0;
  uint32 pos = offset;

  while (to_go) {
    uint32 wrote = 0;
    uint32 write_len = std::min(to_go, kZeroSize);
    RET_IF_FAILED(WriteAt(pos, zero, write_len, 0, &wrote));

    if (wrote != write_len) {
      return E_FAIL;
    }
    pos += wrote;
    written += wrote;
    to_go -= write_len;
  }

  if (bytes_written) {
    *bytes_written = written;
  }
  return S_OK;
}

// returns true on failure
// zeros new data if zero_data == true
HRESULT File::SetLength(const uint32 n, bool zero_data) {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  ASSERT1(!read_only_);
  ASSERT1(n <= kMaxFileSize);

  HRESULT hr = S_OK;

  uint32 len = 0;
  VERIFY_SUCCEEDED(GetLength(&len));

  if (len == n) {
    return S_OK;
  }

  // according to the documentation, the
  // new space will not be initialized
  if (n > len) {
    if (zero_data) {
      uint32 bytes_written = 0;
      RET_IF_FAILED(ClearAt(len, n - len, &bytes_written));
      if (bytes_written != n - len) {
        return E_FAIL;
      }
    } else {
      byte zero = 0;
      uint32 bytes_written = 0;
      RET_IF_FAILED(WriteAt(n - 1, &zero, 1, 0, &bytes_written));
      if (bytes_written != 1) {
        return E_FAIL;
      }
    }
  } else {
    SeekFromBegin(n);
    SetEndOfFile(handle_);
  }

  ASSERT1(SUCCEEDED(GetLength(&len)) && len == n);

  return S_OK;
}

HRESULT File::ExtendInBlocks(const uint32 block_size, uint32 size_needed,
                             uint32* new_size, bool clear_new_space) {
  ASSERT1(new_size);

  *new_size = size_needed;

  if (*new_size % block_size) {
    *new_size += block_size - (*new_size % block_size);
  }

  // is zero_data needed? may reduce fragmentation by causing the block to
  // be written
  return SetLength(*new_size, clear_new_space);
}

// returns S_OK on success
HRESULT File::GetLength(uint32* length) {
  ASSERT1(length);
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  DWORD len = GetFileSize(handle_, NULL);
  if (len == INVALID_FILE_SIZE) {
    ASSERT(false, (_T("cannot get file length")));
    return E_FAIL;
  }
  *length = len;
  return S_OK;
}

HRESULT File::Touch() {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  FILETIME file_time;
  SetZero(file_time);

  ::GetSystemTimeAsFileTime(&file_time);

  if (!::SetFileTime(handle_, NULL, NULL, &file_time)) {
    return HRESULTFromLastError();
  }
  return S_OK;
}

HRESULT File::Close() {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  HRESULT hr = S_OK;
  if (!::CloseHandle(handle_)) {
    hr = HRESULTFromLastError();
  }

  handle_ = INVALID_HANDLE_VALUE;

  return hr;
}

// this is just for consistency with other classes; does not do anything
HRESULT File::Reload(uint32* number_errors) {
  ASSERT1(number_errors);
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  *number_errors = 0;
  return S_OK;
}

// this is just for consistency with other classes; does not do anything
HRESULT File::Verify(uint32* number_errors) {
  ASSERT1(number_errors);
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  *number_errors = 0;
  return S_OK;
}

// this is just for consistency with other classes; does not do anything
HRESULT File::Dump() {
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);
  return S_OK;
}

// for consistency with other classes
HRESULT File::GetSizeOnDisk(uint64* size_on_disk) {
  ASSERT1(size_on_disk);
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  uint32 len = 0;
  RET_IF_FAILED(GetLength(&len));

  *size_on_disk = len;
  return S_OK;
}

// for consistency with other classes
HRESULT File::GetReloadDiskSpaceNeeded(uint64* bytes_needed) {
  ASSERT1(bytes_needed);
  ASSERT1(handle_ != INVALID_HANDLE_VALUE);

  uint32 len = 0;
  RET_IF_FAILED(GetLength(&len));

  *bytes_needed = len;
  return S_OK;
}

// Get the file size
HRESULT File::GetFileSizeUnopen(const TCHAR* filename, uint32* out_size) {
  ASSERT1(filename);
  ASSERT1(out_size);

  WIN32_FILE_ATTRIBUTE_DATA data;
  SetZero(data);

  if (!::GetFileAttributesEx(filename, ::GetFileExInfoStandard, &data)) {
    return HRESULTFromLastError();
  }

  *out_size = data.nFileSizeLow;

  return S_OK;
}

// Get the last time with a file was written to, and the size
HRESULT File::GetLastWriteTimeAndSize(const TCHAR* file_path,
                                      SYSTEMTIME* out_time,
                                      unsigned int* out_size) {
  ASSERT1(file_path);

  WIN32_FIND_DATA wfd;
  SetZero(wfd);

  HANDLE find = ::FindFirstFile(file_path, &wfd);
  if (find == INVALID_HANDLE_VALUE) {
    return HRESULTFromLastError();
  }

  ::FindClose(find);

  if (out_size) {
    *out_size = wfd.nFileSizeLow;
  }

  if (out_time) {
    // If created time is newer than write time, then use that instead
    // [it tends to be more relevant when copying files around]
    FILETIME* latest_time = NULL;
    if (::CompareFileTime(&wfd.ftCreationTime, &wfd.ftLastWriteTime) > 0) {
      latest_time = &wfd.ftCreationTime;
    } else {
      latest_time = &wfd.ftLastWriteTime;
    }

    if (!::FileTimeToSystemTime(latest_time, out_time)) {
      return HRESULTFromLastError();
    }
  }

  return S_OK;
}

bool File::AreFilesIdentical(const TCHAR* filename1, const TCHAR* filename2) {
  UTIL_LOG(L4, (_T("[File::AreFilesIdentical][%s][%s]"), filename1, filename2));

  uint32 file_size1 = 0;
  HRESULT hr = File::GetFileSizeUnopen(filename1, &file_size1);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetFileSizeUnopen failed file_size1][0x%x]"), hr));
    return false;
  }

  uint32 file_size2 = 0;
  hr = File::GetFileSizeUnopen(filename2, &file_size2);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[GetFileSizeUnopen failed file_size2][0x%x]"), hr));
    return false;
  }

  if (file_size1 != file_size2) {
    UTIL_LOG(L3, (_T("[file_size1 != file_size2][%d][%d]"),
                  file_size1, file_size2));
    return false;
  }

  File file1;
  hr = file1.OpenShareMode(filename1, false, false, FILE_SHARE_READ);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[file1.OpenShareMode failed][0x%x]"), hr));
    return false;
  }

  File file2;
  hr = file2.OpenShareMode(filename2, false, false, FILE_SHARE_READ);
  if (FAILED(hr)) {
    UTIL_LOG(LE, (_T("[file2.OpenShareMode failed][0x%x]"), hr));
    return false;
  }

  static const uint32 kBufferSize = 0x10000;
  std::vector<uint8> buffer1(kBufferSize);
  std::vector<uint8> buffer2(kBufferSize);
  uint32 bytes_left = file_size1;

  while (bytes_left > 0) {
    uint32 bytes_to_read = std::min(bytes_left, kBufferSize);
    uint32 bytes_read1 = 0;
    uint32 bytes_read2 = 0;

    hr = file1.Read(bytes_to_read, &buffer1.front(), &bytes_read1);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[file1.Read failed][%d][%d][0x%x]"),
                    bytes_left, bytes_to_read, hr));
      return false;
    }

    hr = file2.Read(bytes_to_read, &buffer2.front(), &bytes_read2);
    if (FAILED(hr)) {
      UTIL_LOG(LE, (_T("[file2.Read failed][%d][%d][0x%x]"),
                    bytes_left, bytes_to_read, hr));
      return false;
    }

    if (bytes_to_read != bytes_read1 || bytes_to_read != bytes_read2) {
      UTIL_LOG(LE,
          (_T("[bytes_to_read != bytes_read1 || bytes_to_read != bytes_read2]")
          _T("[%d][%d][%d]"), bytes_to_read, bytes_read1, bytes_read2));
      return false;
    }

    if (memcmp(&buffer1.front(), &buffer2.front(), bytes_read1) != 0) {
      UTIL_LOG(L3, (_T("[memcmp failed][%d][%d]"), bytes_left, bytes_read1));
      return false;
    }

    if (bytes_left < bytes_to_read) {
      UTIL_LOG(LE, (_T("[bytes_left < bytes_to_read][%d][%d]"),
                    bytes_left, bytes_to_read));
      return false;
    }

    bytes_left -= bytes_to_read;
  }

  return true;
}

FileLock::FileLock() {
}

FileLock::~FileLock() {
  Unlock();
}

HRESULT FileLock::Lock(const TCHAR* file) {
  std::vector<CString> files;
  files.push_back(file);
  return Lock(files);
}

HRESULT FileLock::Lock(const std::vector<CString>& files) {
  ASSERT1(!files.empty());

  // Try to lock all files
  size_t curr_size = handles_.size();
  for (size_t i = 0; i < files.size(); ++i) {
    scoped_hfile handle(::CreateFile(files[i],
                                     GENERIC_READ,
                                     FILE_SHARE_READ,
                                     NULL,
                                     OPEN_EXISTING,
                                     FILE_ATTRIBUTE_NORMAL,
                                     NULL));
    if (!handle) {
      UTIL_LOG(LEVEL_ERROR,
               (_T("[FileLock::Lock - failed to lock file][%s][0x%x]"),
                files[i], HRESULTFromLastError()));
      break;
    }
    handles_.push_back(release(handle));
  }

  // Cleanup if we fail to lock all the files
  if (curr_size +  files.size() < handles_.size()) {
    for (size_t i = handles_.size() - 1; i >= curr_size; --i) {
      VERIFY(::CloseHandle(handles_[i]), (_T("")));
      handles_.pop_back();
    }
    return E_FAIL;
  }

  return S_OK;
}

HRESULT FileLock::Unlock() {
  for (size_t i = 0; i < handles_.size(); ++i) {
    VERIFY(::CloseHandle(handles_[i]), (_T("")));
  }
  handles_.clear();
  return S_OK;
}


// path_name: the directory to watch
// watch_subtree: watch all subdirectory changes  or
//                only immediate child values
// notify_filter: See the documentation for FindFirstChangeNotification
FileWatcher::FileWatcher(const TCHAR* path_name, bool watch_subtree,
                         DWORD notify_filter)
    : path_name_(path_name),
      watch_subtree_(watch_subtree),
      notify_filter_(notify_filter) {
  ASSERT1(path_name && *path_name);
  UTIL_LOG(L3, (_T("[FileWatcher::FileWatcher][%s]"), path_name));
}

// Get the event that is signaled on store changes.
HANDLE FileWatcher::change_event() const {
  ASSERT(valid(change_event_), (_T("call FileWatcher::SetupEvent first")));
  return get(change_event_);
}


// Called to create/reset the event that gets signaled
// any time the store changes.  Access the created
// event using change_event().
HRESULT FileWatcher::EnsureEventSetup() {
  UTIL_LOG(L3, (_T("[FileWatcher::EnsureEventSetup]")));
  if (!valid(change_event_)) {
    reset(change_event_, ::FindFirstChangeNotification(path_name_,
                                                       watch_subtree_,
                                                       notify_filter_));
    if (!valid(change_event_)) {
      ASSERT(false, (_T("unable to get file change notification")));
      return E_FAIL;
    }
    // path name was only needed to set-up the event and now that is done....
    path_name_.Empty();
    return S_OK;
  }

  // if the event is set-up and no changes have occurred,
  // then there is no need to re-setup the event.
  if (valid(change_event_) && !HasChangeOccurred()) {
    return NOERROR;
  }

  return ::FindNextChangeNotification(get(change_event_)) ? S_OK : E_FAIL;
}

}  // namespace omaha

