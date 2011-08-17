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
// encryption is not currently active
//
#ifndef OMAHA_BASE_FILE_H_
#define OMAHA_BASE_FILE_H_

#include <windows.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/store_watcher.h"

namespace omaha {

class File {
 public:

    File();
    ~File();

    HRESULT Open(const TCHAR* file_name, bool write, bool async);
    HRESULT OpenShareMode(const TCHAR* file_name,
                          bool write,
                          bool async,
                          DWORD share_mode);

    HRESULT Close();

    static bool Exists(const TCHAR* file_name);
    static bool IsDirectory(const TCHAR *file_name);
    static HRESULT GetWildcards(const TCHAR* dir, const TCHAR* wildcard,
                                std::vector<CString>* matching_paths);
    // returns S_OK on successful removal or if not existing
    static HRESULT Remove(const TCHAR* file_name);
    // CopyWildcards doesn't work recursively
    static HRESULT CopyWildcards(const TCHAR* from_dir, const TCHAR* to_dir,
                                 const TCHAR* wildcard,
                                 bool replace_existing_files);
    static HRESULT CopyTree(const TCHAR* from_dir, const TCHAR* to_dir,
                            bool replace_existing_files);
                                                    // to_dir need not exist
    static HRESULT Copy(const TCHAR* from, const TCHAR* to,
                        bool replace_existing_file);
    static HRESULT Move(const TCHAR* from, const TCHAR* to,
                        bool replace_existing_file);
    // DeleteAfterReboot tries to delete the files by either moving them to
    // the TEMP directory and deleting them on reboot, or if that fails, by
    // trying to delete them in-place on reboot
    static HRESULT DeleteAfterReboot(const TCHAR* from);
    static HRESULT MoveAfterReboot(const TCHAR* from, const TCHAR* to);
    // Remove any moves pending a reboot from the PendingFileRenameOperations
    // in the registry.
    // The prefix_match boolean controls whether we do an exact match on
    // in_directory, or remove all entries with the in_directory prefix.
    static HRESULT RemoveFromMovesPendingReboot(const TCHAR* in_directory,
                                                bool prefix_match);
    // Did the user try to uninstall a previous install of the same version,
    // and we couldn't clean up without a reboot?
    // We check if there are any moves pending a reboot from the
    // PendingFileRenameOperations in the registry.
    // The prefix_match boolean controls whether we do an exact match on
    // in_directory, or check all entries with the in_directory prefix.
    static bool AreMovesPendingReboot(const TCHAR* in_directory,
                                      bool prefix_match);

    // The GetFileTime function retrieves the date and time that a file was
    // created, last accessed, and last modified. The parameters 'created',
    // 'accessed', 'modified' can be null if the caller does not require that
    // information. All times are utc
    // (http://support.microsoft.com/default.aspx?scid=kb;%5BLN%5D;158588)
    // To compare FILETIME values, use CompareFileTime API.
    static HRESULT GetFileTime(const TCHAR* file_name, FILETIME* created,
                               FILETIME* accessed, FILETIME* modified);

    // Sets the file time
    static HRESULT SetFileTime(const TCHAR* file_name,
                               const FILETIME* created,
                               const FILETIME* accessed,
                               const FILETIME* modified);

    // sync flushes any pending writes to disk
    HRESULT Sync();
    // static HRESULT SyncAllFiles();

    HRESULT SeekToBegin();
    HRESULT SeekFromBegin(uint32 n);

    HRESULT ReadFromStartOfFile(const uint32 max_len, byte *buf,
                                uint32 *bytes_read);
    HRESULT ReadLineAnsi(uint32 max_len, char *line, uint32 *len);

    // read len bytes, reading 0 bytes is invalid
    HRESULT Read(const uint32 len, byte *buf, uint32 *bytes_read);
    // read len bytes starting at position n, reading 0 bytes is invalid
    HRESULT ReadAt(const uint32 offset, byte *buf, const uint32 len,
                    const uint32 async_id, uint32 *bytes_read);

    // write len bytes, writing 0 bytes is invalid
    HRESULT Write(const byte *buf, const uint32 len, uint32 *bytes_written);
    // write len bytes, writing 0 bytes is invalid
    HRESULT WriteAt(const uint32 offset, const byte *buf, const uint32 len,
                     const uint32 async_id, uint32 *bytes_written);

    // write buffer n times
    HRESULT WriteN(const byte *buf, const uint32 len, const uint32 n,
                    uint32 *bytes_written);

    // zeros section of file
    HRESULT ClearAt(const uint32 offset, const uint32 len,
                     uint32 *bytes_written);

    // set length of file
    // if new length is greater than current length, new data is undefined
    // unless zero_data == true in which case the new data is zeroed.
    HRESULT SetLength(const uint32 n, bool zero_data);
    HRESULT ExtendInBlocks(const uint32 block_size, uint32 size_needed,
                            uint32 *new_size, bool clear_new_space);
    HRESULT GetLength(uint32 *len);

    // Sets the last write time to the current time
    HRESULT Touch();

    // all the data storage classes contain these functions
    // we implemenent them here for consistency
    // e.g., so we can do object->GetSizeOnDisk independent of the object type
    HRESULT GetSizeOnDisk(uint64 *size_on_disk);
    HRESULT GetReloadDiskSpaceNeeded(uint64 *bytes_needed);
    HRESULT Reload(uint32 *number_errors);
    HRESULT Verify(uint32 *number_errors);
    HRESULT Dump();

    // Gets the size of a file, without opening it [the regular GetFileSize
    // requires a file handle, which conflicts if the file is already opened
    // and locked]
    static HRESULT GetFileSizeUnopen(const TCHAR * filename,
                                     uint32 * out_size);

    // Optimized function that gets the last write time and size
    static HRESULT GetLastWriteTimeAndSize(const TCHAR* file_path,
                                           SYSTEMTIME* out_time,
                                           unsigned int* out_size);

    // Returns true if the two files are binary-identical.
    static bool AreFilesIdentical(const TCHAR* filename1,
                                  const TCHAR* filename2);

 private:
    // See if we have any moves pending a reboot. Return SUCCESS if we do
    // not encounter errors (not finding a move is not an error). We need to
    // also check the value of *found_ptr for whether we actually found a move.
    // On return, *value_multisz_ptr is the value within
    // "PendingFileRenameOperations", but with any moves for in_directory
    // removed from it.
    // The prefix_match boolean controls whether we do an exact match on
    // in_directory, or remove all entries with the in_directory prefix.
    // NOTE: If the only values found were our own keys, the whole
    // PendingFileRenameOperations MULTISZ needs to be deleted. This is
    // signified by a returned *value_size_chars_ptr of 0.
    static HRESULT GetPendingRenamesValueMinusDir(const TCHAR* in_directory,
      bool prefix_match, TCHAR** value_multisz_ptr, DWORD* value_size_chars_ptr,
      bool* found_ptr);

    HANDLE handle_;
    CString file_name_;
    bool read_only_;
    bool sync_write_done_;
    uint32 pos_;
    uint32 encryption_seed_;
    uint32 sequence_id_;
    enum EncryptionTypes encryption_;

    static const int kMaxFileSize = kint32max;

    DISALLOW_EVIL_CONSTRUCTORS(File);
};

// File lock
class FileLock {
 public:
  // Default constructor
  FileLock();

  // Destructor
  ~FileLock();

  // Lock a single file
  HRESULT Lock(const TCHAR* file);

  // Lock multiple files (atomic)
  HRESULT Lock(const std::vector<CString>& files);

  // Unlock all
  HRESULT Unlock();

 private:
  std::vector<HANDLE> handles_;

  DISALLOW_EVIL_CONSTRUCTORS(FileLock);
};


// Does the common things necessary for watching
// changes in a directory.  If there are file change or other watchers,
// there could be a common interface for the three methods to decouple
// the code that is doing the watching from the code that owns the store.
class FileWatcher : public StoreWatcher {
 public:
  // path_name: the directory to watch
  // watch_subtree: watch all subdirectory changes  or
  //                only immediate child values
  // notify_filter: See the documentation for FindFirstChangeNotification
  FileWatcher(const TCHAR* path_name, bool watch_subtree, DWORD notify_filter);

  // Called to create/reset the event that gets signaled
  // any time the store changes.  Access the created
  // event using change_event().
  virtual HRESULT EnsureEventSetup();

  // Get the event that is signaled on store changes.
  virtual HANDLE change_event() const;

 private:
  scoped_hfind_change_notification change_event_;
  CString path_name_;
  bool watch_subtree_;
  DWORD notify_filter_;

  DISALLOW_EVIL_CONSTRUCTORS(FileWatcher);
};

}  // namespace omaha

#endif  // OMAHA_BASE_FILE_H_

