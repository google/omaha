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

#pragma warning(push)
// C4548: expression before comma has no effect
#pragma warning(disable : 4548)
#include <windows.h>
#include <atlstr.h>
#include <atlsimpcoll.h>
#pragma warning(pop)
#include <msxml2.h>
#include <shellapi.h>
#include <algorithm>

#include "base/scoped_ptr.h"
#include "omaha/common/constants.h"
#include "omaha/common/const_cmd_line.h"
#include "omaha/common/error.h"
#include "omaha/common/extractor.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/system_info.h"
#include "omaha/mi_exe_stub/process.h"
#include "omaha/mi_exe_stub/mi.grh"
#include "omaha/mi_exe_stub/tar.h"
extern "C" {
#include "third_party/lzma/LzmaStateDecode.h"
}

namespace omaha  {

// Resource ID of the goopdate payload inside the meta-installer.
#define IDR_PAYLOAD 102

namespace {

HRESULT HandleError(HRESULT hr);

// The function assumes that the extractor has already been opened.
// The buffer must be deleted by the caller.
char* ReadTag(TagExtractor* extractor) {
  const int kMaxTagLength = 0x10000;  // 64KB

  int tag_buffer_size = 0;
  if (!extractor->ExtractTag(NULL, &tag_buffer_size)) {
    return NULL;
  }
  if (!tag_buffer_size || (tag_buffer_size >= kMaxTagLength)) {
    return NULL;
  }

  scoped_array<char> tag_buffer(new char[tag_buffer_size]);
  if (!tag_buffer.get()) {
    return NULL;
  }

  if (!extractor->ExtractTag(tag_buffer.get(), &tag_buffer_size)) {
    _ASSERTE(false);
    return NULL;
  }

  // Do a sanity check of the tag string. The double quote '"'
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

  CString ConvertByteArrayToWideCharArray(const char* input, size_t len) {
    _ASSERTE(input != NULL);
    CString out_str;
    TCHAR* out = out_str.GetBufferSetLength(len);
    for (size_t i = 0; i < len; ++i) {
      out[i] = static_cast<TCHAR>(input[i]);
    }
    return out_str;
  }

  int ExtractAndRun() {
    if (CreateUniqueTempDirectory() != 0) {
      return -1;
    }
    CString tarball_filename(ExtractTarballToTempLocation());
    if (tarball_filename.IsEmpty()) {
      return -1;
    }
    scoped_hfile tarball_file(::CreateFile(tarball_filename,
                                           GENERIC_READ,
                                           FILE_SHARE_READ,
                                           NULL,
                                           OPEN_EXISTING,
                                           0,
                                           NULL));
    if (!tarball_file) return -1;

    // Extract files from the archive and run the first EXE we find in it.
    Tar tar(temp_dir_, get(tarball_file), true);
    tar.SetCallback(TarFileCallback, this);
    if (!tar.ExtractToDir()) {
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

      scoped_array<char> tag(GetTag());
      CString wide_tag;
      if (tag.get()) {
        wide_tag = ConvertByteArrayToWideCharArray(tag.get(),
                                                           strlen(tag.get()));
      }

      if (cmd_line_.IsEmpty()) {
        // Run-by-user case.
        if (wide_tag.IsEmpty()) {
          _ASSERTE(!"Must provide arguments with an untagged metainstaller.");
          HRESULT hr = GOOPDATE_E_UNTAGGED_METAINSTALLER;
          HandleError(hr);
          return hr;
        }
        command_line.AppendFormat(" /%s", kCmdLineInstall);
      } else {
        command_line.AppendFormat(" %s", cmd_line_);

        CheckAndHandleRecoveryCase(&command_line);
      }

      if (!wide_tag.IsEmpty()) {
        command_line.AppendFormat(" \"%s\"", wide_tag);
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
    silent_argument.Format("/%s", kCmdLineSilent);

    return silent_argument == cmd_line_;
  }

  // Determines whether the MI is being invoked for recovery purposes, and,
  // if so, appends the MI's full path to the command line.
  // cmd_line_ must begin with "/recover" in order for the recovery case to be
  // detected.
  void CheckAndHandleRecoveryCase(CString* command_line) {
    _ASSERTE(command_line);

    CString recover_argument;
    recover_argument.Format("/%s", kCmdLineRecover);

    if (cmd_line_.Left(recover_argument.GetLength()) == recover_argument) {
      TCHAR current_path[MAX_PATH] = {0};
      if (::GetModuleFileName(NULL, current_path, MAX_PATH - 1)) {
        command_line->AppendFormat(" \"%s\"", current_path);
      }
    }
  }

  // Create a temp directory to hold the embedded setup files.
  // This is a bit of a hack: we ask the system to create a temporary
  // filename for us, and instead we use that name for a subdirectory name.
  int CreateUniqueTempDirectory() {
    ::GetTempPath(MAX_PATH, CStrBuf(temp_root_dir_, MAX_PATH));
    if (::CreateDirectory(temp_root_dir_, NULL) != 0 ||
        ::GetLastError() == ERROR_ALREADY_EXISTS) {
      if (!::GetTempFileName(temp_root_dir_,
                             _T("GUM"),
                             0,  // form a unique filename
                             CStrBuf(temp_dir_, MAX_PATH))) {
        return -1;
      }
      // GetTempFileName() actually creates the temp file, so delete it.
      ::DeleteFile(temp_dir_);
      ::CreateDirectory(temp_dir_, NULL);
    } else {
      return -1;
    }
    return 0;
  }

  CString ExtractTarballToTempLocation() {
    CString tarball_filename;
    if (::GetTempFileName(temp_root_dir_,
                          _T("GUT"),
                          0,  // form a unique filename
                          CStrBuf(tarball_filename, MAX_PATH))) {
      files_to_delete_.Add(tarball_filename);
      HRSRC res_info = ::FindResource(NULL,
                                      MAKEINTRESOURCE(IDR_PAYLOAD),
                                      _T("B"));
      if (NULL != res_info) {
        HGLOBAL resource = ::LoadResource(NULL, res_info);
        if (NULL != resource) {
          LPVOID resource_pointer = ::LockResource(resource);
          if (NULL != resource_pointer) {
            scoped_hfile tarball_file(::CreateFile(tarball_filename,
                                                   GENERIC_READ | GENERIC_WRITE,
                                                   0,
                                                   NULL,
                                                   OPEN_ALWAYS,
                                                   0,
                                                   NULL));
            if (valid(tarball_file)) {
              int error = DecompressBufferToFile(
                              resource_pointer,
                              ::SizeofResource(NULL, res_info),
                              get(tarball_file));
              if (error == 0) {
                return tarball_filename;
              }
            }
          }
        }
      }
    }
    return CString();
  }

  char* GetTag() const {
    // Get this module file name.
    TCHAR module_file_name[MAX_PATH] = {0};
    if (!::GetModuleFileName(instance_, module_file_name, MAX_PATH)) {
      _ASSERTE(false);
      return NULL;
    }

    return ExtractTag(module_file_name);
  }

  static CString GetFilespec(const CString &path) {
    int pos = path.ReverseFind('\\');
    if (pos != -1) {
      return path.Mid(pos + 1);
    }
    return path;
  }

  void HandleTarFile(const char *filename) {
    CString new_filename(filename);
    files_to_delete_.Add(new_filename);
    CString filespec(GetFilespec(new_filename));
    filespec.MakeLower();

    if (filespec.GetLength() > 4) {
      CString extension(filespec.Mid(filespec.GetLength() - 4));

      if (extension == ".exe") {
        // We're interested in remembering only the first exe in the tarball.
        if (exe_path_.IsEmpty()) {
          exe_path_ = new_filename;
        }
      }
    }
  }

  static void TarFileCallback(void *context, const char *filename) {
    MetaInstaller *mi = reinterpret_cast<MetaInstaller *>(context);
    mi->HandleTarFile(filename);
  }

  // Decompress the content of the memory buffer into the file
  // This code stolen from jeanluc in //googleclient/bar
  static int DecompressBufferToFile(const void *buf,
                                    size_t buf_len,
                                    HANDLE file) {
    // need header and len minimally
    if (buf_len < LZMA_PROPERTIES_SIZE + 8) {
      return -1;
    }

    CLzmaDecoderState decoder = {};
    const unsigned char *pos = reinterpret_cast<const unsigned char*>(buf);

    // get properties
    int res_info = LzmaDecodeProperties(&decoder.Properties, pos,
      LZMA_PROPERTIES_SIZE);
    if (LZMA_RESULT_OK != res_info) {
      return -1;
    }

    // advance buffer past header
    pos += LZMA_PROPERTIES_SIZE;
    buf_len -= LZMA_PROPERTIES_SIZE;

    // get the length
    ULONGLONG size;
    memcpy(&size, pos, sizeof(size));

    pos += sizeof(size);
    buf_len -= sizeof(size);

    // allocate the dictionary buffer
    CAutoVectorPtr<CProb> probs;
    if (!probs.Allocate(LzmaGetNumProbs(&decoder.Properties))) {
      return -1;
    }

    CAutoVectorPtr<unsigned char> dict;
    if (!dict.Allocate(decoder.Properties.DictionarySize)) {
      return -1;
    }

    // and initialize the decoder
    decoder.Dictionary = dict.m_p;
    decoder.Probs = probs.m_p;

    LzmaDecoderInit(&decoder);

    while (0 != size || 0 != buf_len) {
      SizeT in_consumed = 0;
      SizeT out_produced = 0;
      unsigned char chunk[8192];

      // extract a chunk - note that the decompresser barfs on us if we
      // extract too much data from it, so make sure to bound out_len
      // to the amount of data left.
      SizeT out_size = std::min(static_cast<SizeT>(size), sizeof(chunk));
      res_info = LzmaDecode(&decoder, pos, buf_len, &in_consumed, chunk,
        out_size, &out_produced, buf_len == 0);
      if (LZMA_RESULT_OK != res_info) {
        return -1;
      }

      pos += in_consumed;
      buf_len -= in_consumed;
      size -= out_produced;

      DWORD written;
      if (!::WriteFile(file, chunk, out_produced, &written, NULL)) {
        return -1;
      }

      if (written != out_produced) {
        return -1;
      }
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

HRESULT HandleError(HRESULT hr) {
  CString msg_box_title;
  CString msg_box_text;

  msg_box_title.LoadString(IDS_GENERIC_INSTALLER_DISPLAY_NAME);
  switch (hr) {
    case GOOPDATE_E_RUNNING_INFERIOR_WINDOWS:
      msg_box_text.LoadString(IDS_RUNNING_INFERIOR_WINDOWS);
      break;

    case GOOPDATE_E_UNTAGGED_METAINSTALLER:
    default:
      msg_box_text.LoadString(IDS_GENERIC_ERROR);
      break;
  }

  ::MessageBox(NULL, msg_box_text, msg_box_title, MB_OK);
  return hr;
}

}  // namespace

}  // namespace omaha

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
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

