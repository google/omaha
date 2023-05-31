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
//
// Implementation of the metainstaller logic.
// Untars a tarball and executes the extracted executable.
// If no command line is specified, "/install" is passed to the executable
// along with a .gup file if one is extracted.
// If found, the contents of the signature tag are also passed to the
// executable unmodified.
//
// Since the metainstaller is not linking with any of the Omaha libraries,
// some amount of code is duplicated here.

#include <windows.h>
#include <intsafe.h>
#include <shellapi.h>

#pragma warning(push)
// C4917: a GUID can only be associated with a class, interface or namespace.
// C5038: data member '' will be initialized after data member ''
#pragma warning(disable : 4917 5038)
#include <shlobj.h>
#include <atlstr.h>
#pragma warning(pop)

#include <shlwapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <atlpath.h>
#include <atlsimpcoll.h>
#include <memory>

#pragma warning(push)
// C4310: cast truncates constant value
#pragma warning(disable : 4310)
#include "base/basictypes.h"
#pragma warning(pop)
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/extractor.h"
#pragma warning(push)
// C4244: conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable : 4244)
#pragma warning(pop)

#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/mi_exe_stub/process.h"
#include "omaha/mi_exe_stub/mi.grh"
#include "omaha/mi_exe_stub/tar.h"
#include "omaha/third_party/smartany/scoped_any.h"
extern "C" {
#include "third_party/lzma/files/C/Bcj2.h"
#include "third_party/lzma/files/C/LzmaDec.h"
}

namespace omaha  {

// Resource ID of the goopdate payload inside the meta-installer.
#define IDR_PAYLOAD 102

namespace {

HRESULT HandleError(HRESULT hr);

void SafeCStringAppendCmdLine(CString* cmd_line, LPCTSTR cmd_line_format, ...) {
  _ASSERTE(cmd_line);
  _ASSERTE(cmd_line_format);

  // The maximum command line length for the ::CreateProcess function is 32767
  // characters. Various command line length limits: http://goo.gl/0u9nW.
  const int kMaxCmdLineLength = 32767;

  va_list arg_list;
  va_start(arg_list, cmd_line_format);

  CString append_cmd_line;
  HRESULT hr(::StringCchVPrintf(CStrBuf(append_cmd_line, kMaxCmdLineLength),
                                kMaxCmdLineLength - 1,
                                cmd_line_format,
                                arg_list));
  _ASSERTE(SUCCEEDED(hr));

  cmd_line->Append(append_cmd_line);
  va_end(arg_list);
}

// The function assumes that the extractor has already been opened.
// The buffer must be deleted by the caller. Returns NULL if the file is not
// tagged or the tag could not be read.
char* ReadTag(TagExtractor* extractor) {
  const int kMaxTagLength = 0x10000;  // 64KB

  int tag_buffer_size = 0;
  if (!extractor->ExtractTag(NULL, &tag_buffer_size)) {
    return NULL;
  }
  if (!tag_buffer_size || (tag_buffer_size >= kMaxTagLength)) {
    return NULL;
  }

  std::unique_ptr<char[]> tag_buffer(new char[tag_buffer_size]);
  if (!tag_buffer.get()) {
    return NULL;
  }

  if (!extractor->ExtractTag(tag_buffer.get(), &tag_buffer_size)) {
    _ASSERTE(false);
    return NULL;
  }

  // Do a check of the tag string. The double quote '"'
  // is a special character that should not be included in the tag string.
  for (const char* tag_char = tag_buffer.get(); *tag_char; ++tag_char) {
    if (*tag_char == '"') {
      _ASSERTE(false);
      return NULL;
    }
  }

  return tag_buffer.release();
}

// Extract the tag containing the extra information written by the server.
// The memory returned by the function will have to be freed using delete[]
// operator.
char* ExtractTag(const TCHAR* module_file_name) {
  if (!module_file_name) {
    return NULL;
  }

  TagExtractor extractor;
  if (!extractor.OpenFile(module_file_name)) {
    return NULL;
  }
  char* ret = ReadTag(&extractor);
  extractor.CloseFile();

  return ret;
}

class MetaInstaller {
 public:
  MetaInstaller(HINSTANCE instance, LPCSTR cmd_line)
      : instance_(instance),
        cmd_line_(cmd_line),
        exit_code_(0) {
  }

  ~MetaInstaller() {
    // When a crash happens while running GoogleUpdate and breakpad gets it
    // GooogleUpdate.exe is started with the /report to report the crash.
    // In a crash, the temp directory and the contained files can't be deleted.
    if (exit_code_ != GOOPDATE_E_CRASH) {
      CleanUpTempDirectory();
    }
  }

  int ExtractAndRun() {
    if (CreateUniqueTempDirectory() != 0) {
      return -1;
    }
    scoped_hfile tarball_file(ExtractTarballToTempLocation());
    if (!valid(tarball_file)) {
      return -1;
    }

    // Extract files from the archive and run the first EXE we find in it.
    Tar tar(temp_dir_, get(tarball_file), true);
    tar.SetCallback(TarFileCallback, this);
    if (!tar.ExtractToDir()) {
      return -1;
    }

    if (!CopyMetainstallerToTempLocation()) {
      return -1;
    }

    exit_code_ = ULONG_MAX;
    if (!exe_path_.IsEmpty()) {
      // Build the command line. There are three scenarios we consider:
      // 1. Run by the user, in which case the MI does not receive any
      //    argument on its command line. In this case the command line
      //    to run is: "exe_path" /install [["]manifest["]]
      // 2. Run with command line arguments. The tag, if present, will be
      //    appended to the command line.
      //    The command line is: "exe_path" args <tag>
      //    For example, pass "/silent /install" to the metainstaller to
      //    initiate a silent install using the extra args in the tag.
      //    If a command line does not take a tag or a custom tag is needed,
      //    use an untagged file.
      CString command_line(exe_path_);
      ::PathQuoteSpaces(CStrBuf(command_line, MAX_PATH));

      std::unique_ptr<char[]> tag(GetTag());
      if (cmd_line_.IsEmpty()) {
        // Run-by-user case.
        if (!tag.get()) {
          _ASSERTE(!_T("Must provide arguments with untagged metainstaller."));
          HRESULT hr = GOOPDATE_E_UNTAGGED_METAINSTALLER;
          HandleError(hr);
          return hr;
        }
        SafeCStringAppendCmdLine(&command_line, _T(" /%s %s /%s"),
                                 kCmdLineInstallSource,
                                 kCmdLineInstallSource_TaggedMetainstaller,
                                 kCmdLineInstall);
      } else {
        SafeCStringAppendCmdLine(&command_line, _T(" %s"), cmd_line_);

        CheckAndHandleRecoveryCase(&command_line);
      }

      const bool should_append_tag = !RemoveIgnoreTagSwitch(&command_line);
      if (should_append_tag && tag.get()) {
        SafeCStringAppendCmdLine(&command_line, _T(" \"%s\""),
                                 CString(tag.get()));
      }

      RunAndWait(command_line, &exit_code_);
    }
    // Propagate up the exit code of the program we have run.
    return exit_code_;
  }

 private:
  void CleanUpTempDirectory() {
    // Delete our temp directory and its contents.
    for (int i = 0; i != files_to_delete_.GetSize(); ++i) {
      DeleteFile(files_to_delete_[i]);
    }
    files_to_delete_.RemoveAll();

    ::RemoveDirectory(temp_dir_);
    temp_dir_.Empty();
  }

  // Determines whether this is a silent install.
  bool IsSilentInstall() {
    CString silent_argument;
    SafeCStringAppendCmdLine(&silent_argument, _T("/%s"), kCmdLineSilent);

    return silent_argument == cmd_line_;
  }

  // Determines whether the MI is being invoked for recovery purposes, and,
  // if so, appends the MI's full path to the command line.
  // cmd_line_ must begin with "/recover" in order for the recovery case to be
  // detected.
  void CheckAndHandleRecoveryCase(CString* command_line) {
    _ASSERTE(command_line);

    CString recover_argument;
    SafeCStringAppendCmdLine(&recover_argument, _T("/%s"), kCmdLineRecover);

    if (cmd_line_.Left(recover_argument.GetLength()) == recover_argument) {
      // TODO(omaha3): When we refactor main.lib and can link with it in MI
      // (http://b/5904730), replace this with app_util::GetModulePath().  Do
      // likewise for the other calls to GetModuleFileName() in this module.
      TCHAR current_path[MAX_PATH] = {};
      DWORD len = ::GetModuleFileName(NULL,
                                      current_path,
                                      arraysize(current_path));
      if (len == 0 || len >= arraysize(current_path)) {
        // TODO(omaha3): Can we do some better error handling if this fails?
        _ASSERTE(false);
      } else {
        SafeCStringAppendCmdLine(command_line, _T(" \"%s\""), current_path);
      }
    }
  }

  // Determines if the "/no_mi_tag" switch is at the end of the command line,
  // and if so, removes it.  (We do not recognize this switch if it appears in
  // the start/middle of args.)  Returns true if removed, false if not found.
  // TODO(omaha): Remove this functionality once we refactor ApplyTag to allow
  // removal/truncation of tags.
  bool RemoveIgnoreTagSwitch(CString* command_line) {
    _ASSERTE(command_line);

    CString target;
    SafeCStringAppendCmdLine(&target, _T(" /%s"), kCmdLineNoMetainstallerTag);

    if (command_line->Right(target.GetLength()).CompareNoCase(target) == 0) {
      command_line->Truncate(command_line->GetLength() - target.GetLength());
      return true;
    }

    return false;
  }

  // This is a bit of a hack: we ask the system to create a temporary
  // filename for us, and instead we use that name for a subdirectory name.
  bool CreateTempSubdirectory(const CString& parent_dir) {
    if (!::CreateDirectory(parent_dir, NULL) &&
        ::GetLastError() != ERROR_ALREADY_EXISTS) {
      return false;
    }

    const CString temp_dir = GetTempFilenameAt(parent_dir, _T("GUM"));
    if (temp_dir.IsEmpty()) {
      return false;
    }

    ::DeleteFile(temp_dir);
    if (!::CreateDirectory(temp_dir, NULL)) {
      return false;
    }

    temp_dir_ = temp_dir;
    return true;
  }

  // Create a temp directory under %ProgramFiles%\Google\Temp. To avoid possible
  // issues with anti-malware heuristics, this function returns early and does
  // not try to create a directory if the user is not an admin.
  bool CreateMachineTempDir() {
    const CString google_update_temp_dir(GetSecureSystemTempDir());
    if (google_update_temp_dir.IsEmpty()) {
      return false;
    }

    if (!CreateTempSubdirectory(google_update_temp_dir)) {
      return false;
    }

    temp_root_dir_ = google_update_temp_dir;
    return true;
  }

  // Create a temp directory under %TMP%.
  bool CreateUserTempDir() {
    CString user_tmp_dir;
    // TODO(omaha3): Once we've refactored base into minibase, replace this
    // with a call to app_util::GetTempDir().
    DWORD result = ::GetTempPath(MAX_PATH, CStrBuf(user_tmp_dir, MAX_PATH));
    if (result == 0 || result >= MAX_PATH) {
      return false;
    }

    if (!CreateTempSubdirectory(user_tmp_dir)) {
      return false;
    }

    temp_root_dir_ = user_tmp_dir;
    return true;
  }

  // Creates a temp directory to hold the embedded setup files. First attempts
  // creating the directory under a secure path, and if that fails, creates
  // under the user's %TMP% directory.
  int CreateUniqueTempDirectory() {
    return (::IsUserAnAdmin() ? CreateMachineTempDir()
                              : CreateUserTempDir())
               ? 0
               : -1;
  }

  HANDLE ExtractTarballToTempLocation() {
    HANDLE tarball_file = INVALID_HANDLE_VALUE;
    const CString tarball_filename =
        GetTempFilenameAt(temp_root_dir_, _T("GUT"));
    if (!tarball_filename.IsEmpty()) {
      files_to_delete_.Add(tarball_filename);
      HRSRC res_info = ::FindResource(NULL,
                                      MAKEINTRESOURCE(IDR_PAYLOAD),
                                      _T("B"));
      if (NULL != res_info) {
        HGLOBAL resource = ::LoadResource(NULL, res_info);
        if (NULL != resource) {
          LPVOID resource_pointer = ::LockResource(resource);
          if (NULL != resource_pointer) {
            tarball_file = ::CreateFile(tarball_filename,
                                        GENERIC_READ | GENERIC_WRITE,
                                        0,
                                        NULL,
                                        OPEN_ALWAYS,
                                        0,
                                        NULL);
            if (INVALID_HANDLE_VALUE != tarball_file) {
              LARGE_INTEGER file_position = {};
              if (0 != DecompressBufferToFile(
                      static_cast<const uint8*>(resource_pointer),
                      ::SizeofResource(NULL, res_info),
                      tarball_file) ||
                  !::SetFilePointerEx(tarball_file, file_position, NULL,
                                      FILE_BEGIN)) {
                ::CloseHandle(tarball_file);
                tarball_file = INVALID_HANDLE_VALUE;
              }
            }
          }
        }
      }
    }
    return tarball_file;
  }

  bool CopyMetainstallerToTempLocation() {
    CString module_path;
    DWORD result = ::GetModuleFileName(NULL,
                                       CStrBuf(module_path, MAX_PATH),
                                       MAX_PATH);
    if (!result || result >= MAX_PATH) {
      return false;
    }

    CPath metainstaller_target_path(temp_dir_);
    metainstaller_target_path.Append(kOmahaMetainstallerFileName);

    if (!::CopyFile(module_path, metainstaller_target_path, false)) {
      return false;
    }

    files_to_delete_.Add(metainstaller_target_path);
    return true;
  }

  char* GetTag() const {
    // Get this module file name.
    TCHAR module_file_name[MAX_PATH] = {};
    DWORD len = ::GetModuleFileName(instance_,
                                    module_file_name,
                                    arraysize(module_file_name));
    if (len == 0 || len >= arraysize(module_file_name)) {
      _ASSERTE(false);
      return NULL;
    }

    return ExtractTag(module_file_name);
  }

  static CString GetFilespec(const CString& path) {
    int pos = path.ReverseFind('\\');
    if (pos >= 0) {
      return path.Mid(pos + 1);
    }
    return path;
  }

  void HandleTarFile(const TCHAR* filename) {
    CString new_filename(filename);
    files_to_delete_.Add(new_filename);
    CString filespec(GetFilespec(new_filename));
    filespec.MakeLower();

    if (filespec.GetLength() > 4) {
      CString extension(filespec.Mid(filespec.GetLength() - 4));

      if (extension == _T(".exe")) {
        // We're interested in remembering only the first exe in the tarball.
        if (exe_path_.IsEmpty()) {
          exe_path_ = new_filename;
        }
      }
    }
  }

  static void TarFileCallback(void* context, const TCHAR* filename) {
    MetaInstaller* mi = reinterpret_cast<MetaInstaller*>(context);
    mi->HandleTarFile(filename);
  }

  // TODO(omaha): reimplement the relevant files in the LZMA SDK to optimize
  // for size. We'll have to release the modifications (LZMA SDK is CDDL/CDL),
  // which shouldn't be a problem.
  static void* MyAlloc(void* p, size_t size) {
    UNREFERENCED_PARAMETER(p);
    return new uint8[size];
  }

  static void MyFree(void* p, void* address) {
    UNREFERENCED_PARAMETER(p);
    delete[] address;
  }

  // Decompress the content of the memory buffer into the file
  static int DecompressBufferToFile(const uint8* packed_buffer,
                                    size_t packed_size,
                                    HANDLE file) {
    // need header and len minimally
    if (packed_size < LZMA_PROPS_SIZE + 8) {
      return -1;
    }

    // Note this code won't properly handle decoding large files, since uint32
    // is used in several places to count size.
    ISzAlloc allocators = { &MyAlloc, &MyFree };
    CLzmaDec lzma_state;
    LzmaDec_Construct(&lzma_state);
    LzmaDec_Allocate(&lzma_state, packed_buffer, LZMA_PROPS_SIZE, &allocators);
    LzmaDec_Init(&lzma_state);
    packed_buffer += LZMA_PROPS_SIZE;
    packed_size -= LZMA_PROPS_SIZE;

    // TODO(omaha): make this independent of endianness.
    uint64 unpacked_size_64 = *reinterpret_cast<const uint64*>(packed_buffer);
    size_t unpacked_size = static_cast<size_t>(unpacked_size_64);
    packed_buffer += sizeof(unpacked_size_64);
    packed_size -= sizeof(unpacked_size_64);

    std::unique_ptr<uint8[]> unpacked_buffer(new uint8[unpacked_size]);

    ELzmaStatus status = static_cast<ELzmaStatus>(0);
    SRes result = LzmaDec_DecodeToBuf(
        &lzma_state,
        unpacked_buffer.get(),
        &unpacked_size,
        packed_buffer,
        &packed_size,
        LZMA_FINISH_END,
        &status);
    LzmaDec_Free(&lzma_state, &allocators);
    if (SZ_OK != result) {
      return -1;
    }

#if 0
    // Reverse BCJ coding.
    uint32 x86_conversion_state;
    x86_Convert_Init(x86_conversion_state);
    x86_Convert(unpacked_buffer.get(), unpacked_size, 0, &x86_conversion_state,
                0 /* decoding */);
#else
    // Reverse BCJ2 coding.
    const uint8* p = unpacked_buffer.get();
    uint32 original_size = *reinterpret_cast<const uint32*>(p);
    p += sizeof(uint32);  // NOLINT
    uint32 stream0_size = *reinterpret_cast<const uint32*>(p);
    p += sizeof(uint32);  // NOLINT
    uint32 stream1_size = *reinterpret_cast<const uint32*>(p);
    p += sizeof(uint32);  // NOLINT
    uint32 stream2_size = *reinterpret_cast<const uint32*>(p);
    p += sizeof(uint32);  // NOLINT
    uint32 stream3_size = *reinterpret_cast<const uint32*>(p);
    p += sizeof(uint32);  // NOLINT

    std::unique_ptr<uint8[]> output_buffer(new uint8[original_size]);
    if (SZ_OK != Bcj2_Decode(p,
                             stream0_size,
                             p + stream0_size,
                             stream1_size,
                             p + stream0_size + stream1_size,
                             stream2_size,
                             p + stream0_size + stream1_size + stream2_size,
                             stream3_size,
                             output_buffer.get(), original_size)) {
      return 1;
    }
#endif

    DWORD written;
    if (!::WriteFile(file, output_buffer.get(), original_size, &written,
                     NULL) ||
        written != original_size) {
        return -1;
    }

    return 0;
  }

  HINSTANCE instance_;
  CString cmd_line_;
  CString exe_path_;
  DWORD exit_code_;
  CSimpleArray<CString> files_to_delete_;
  CString temp_dir_;
  CString temp_root_dir_;
};

HRESULT CheckOSRequirements() {
  return SystemInfo::OSWin2KSP4OrLater() ? S_OK :
                                           GOOPDATE_E_RUNNING_INFERIOR_WINDOWS;
}

CString GetCompanyDisplayName() {
  CString company_name;
  company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME);
  _ASSERTE(!company_name.IsEmpty());
  return company_name;
}

CString GetUiTitle() {
  CString title;
  title.FormatMessage(IDS_INSTALLER_DISPLAY_NAME, GetCompanyDisplayName());
  return title;
}

HRESULT HandleError(HRESULT result) {
  _ASSERTE(FAILED(result));
  CString msg_box_text;

  switch (result) {
    case GOOPDATE_E_RUNNING_INFERIOR_WINDOWS:
      msg_box_text.FormatMessage(IDS_RUNNING_INFERIOR_WINDOWS,
                                 GetCompanyDisplayName());
      break;

    case GOOPDATE_E_UNTAGGED_METAINSTALLER:
    default:
      msg_box_text.LoadString(IDS_GENERIC_ERROR);
      _ASSERTE(!msg_box_text.IsEmpty());
      break;
  }

  ::MessageBox(NULL, msg_box_text, GetUiTitle(), MB_OK);
  return result;
}

}  // namespace

}  // namespace omaha

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
  omaha::EnableSecureDllLoading();

  scoped_co_init init_com_apt;
  HRESULT hr(init_com_apt.hresult());
  if (FAILED(hr)) {
    return omaha::HandleError(hr);
  }

  hr = omaha::CheckOSRequirements();
  if (FAILED(hr)) {
    return omaha::HandleError(hr);
  }

  omaha::MetaInstaller mi(hInstance, lpCmdLine);
  int result = mi.ExtractAndRun();
  return result;
}
