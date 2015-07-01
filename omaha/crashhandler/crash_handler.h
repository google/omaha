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

#ifndef OMAHA_CRASHHANDLER_CRASH_HANDLER_H_
#define OMAHA_CRASHHANDLER_CRASH_HANDLER_H_

#include <windows.h>
#include <atlbase.h>
#include <atlsecurity.h>
#include <atlstr.h>
#include <rpc.h>
#include <map>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/scoped_any.h"
#include "omaha/base/shutdown_callback.h"
#include "omaha/base/string.h"
#include "omaha/common/exception_handler.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/crash_generation/crash_generation_server.h"
#include "third_party/breakpad/src/client/windows/crash_generation/minidump_generator.h"

namespace omaha {

class Reactor;
class ShutdownHandler;

class CrashHandler : public ShutdownCallback {
 public:
  CrashHandler();
  virtual ~CrashHandler();

  // Executes the instance entry point with given parameters.
  HRESULT Main(bool is_system_process);

 private:
  enum CrashHandlerResult {
    CRASH_HANDLER_SUCCESS,
    CRASH_HANDLER_ERROR
  };

  static CString GetCrashHandlerInstanceName();

  // The instance is run as a dedicated process for a particular crash to
  // generate its minidump and upload to server.
  HRESULT RunAsCrashHandlerWorker();

  // The instance is run as a singleton to distribute crash requests system
  // wide.
  HRESULT RunAsCrashHandler();

  // Creates a child process with equivalent privilege as the crashed process
// owner to generate and upload minidump.
  HRESULT CreateCrashHandlerProcess(
      HANDLE crash_processed_event,  // Event to signal to trigger upload.
      HANDLE mini_dump_handle,       // Handle for writing the minidump.
      HANDLE full_dump_handle,       // Handle for writing the full dump.
      HANDLE custom_info_handle,     // Handle for writing the custom info.
      const google_breakpad::ClientInfo& client_info,  // Crash client info.
      PROCESS_INFORMATION* pi);      // Created subprocess info.

  // Starts the Breakpad server.  (Runs in the thread pool.)
  HRESULT StartServer();

  // Stops the Breakpad server.
  void StopServer();

  // Launches an uploader process when a client crashes.  Invoked by the
  // BreakpadClientCrashed() callback below.
  HRESULT StartCrashUploader(const CString& crash_filename,
                             const CString& custom_info_filename);

  // Breakpad interface: Callback invoked when clients connect.
  static void BreakpadClientConnected(
      void* context,
      const google_breakpad::ClientInfo* client_info);

  // Breakpad interface: Callback invoked when clients request a dump.
  static void BreakpadClientCrashed(
      void* context,
      const google_breakpad::ClientInfo* client_info,
      const std::wstring* dump_path);

  // Breakpad interface: Callback invoked when clients disconnect.
  static void BreakpadClientDisconnected(
      void* context,
      const google_breakpad::ClientInfo* client_info);

  // Creates a new sandboxed process to handle an incoming crash.
  CrashHandlerResult CreateCrashHandlingProcessAndWait(
      const google_breakpad::ClientInfo& client_info,
      HANDLE mini_dump_handle,
      HANDLE full_dump_handle,
      HANDLE custom_info_handle);

  // Run periodically to clean up old crashes that were marked for deferred
  // upload.
  void CleanStaleCrashes();

  // Breakpad interface: Callback invoked when an upload is requested.
  static void BreakpadClientUpload(void* context, const DWORD crash_id);

  // Pumps messages until the ShutdownHandler invokes Shutdown() below.
  HRESULT RunUntilShutdown();

  // ShutdownCallback interface.  Signals the CrashHandler to stop handling
  // events and exit by posting a WM_QUIT to its message pump.
  virtual HRESULT Shutdown();

  // Function pointer type for UuidCreate, which is looked up dynamically.
  typedef RPC_STATUS (RPC_ENTRY* UuidCreateType)(UUID* Uuid);

  // Generates a unique path for the dump file under the crash_dir_.
  CString GenerateDumpFilePath();

  bool is_system_;
  CString crash_dir_;
  scoped_ptr<google_breakpad::CrashGenerationServer> crash_server_;

  DWORD main_thread_id_;
  scoped_ptr<Reactor> reactor_;
  scoped_ptr<ShutdownHandler> shutdown_handler_;

  std::map<DWORD, CString> saved_crashes_;

  scoped_ptr<OmahaExceptionHandler> exception_handler_;

  DISALLOW_COPY_AND_ASSIGN(CrashHandler);
};

}  // namespace omaha

#endif  // OMAHA_CRASHHANDLER_CRASH_HANDLER_H_

