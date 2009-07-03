// Copyright 2008-2009 Google Inc.
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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMPATIBILITY_TEST_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMPATIBILITY_TEST_H_

#include <Windows.h>
#include <tchar.h>
#include "base/scoped_ptr.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/apply_tag.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/file.h"
#include "omaha/common/logging.h"
#include "omaha/common/path.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/tools/omahacompatibility/console_writer.h"
#include "omaha/tools/omahacompatibility/HttpServer/http_server.h"
#include "omaha/tools/omahacompatibility/HttpServer/update_check_handler.h"
#include "omaha/tools/omahacompatibility/HttpServer/download_handler.h"

namespace omaha {

// This is the main class the starts the HttpServer and stamps GoogleUpdate
// appropriately and runs the installer.
class CompatibilityTest {
 public:
  CompatibilityTest() : thread_id_(0) {}
  int Main(bool test_omaha);
  void set_config_file(const CString& config_file) {
    config_file_ = config_file;
  }
  void set_googleupdate_setup_path(const CString& path) {
    googleupdate_setup_path_ = path;
  }
  HRESULT RunHttpServerInternal();

 private:
  static DWORD WINAPI StartStartHttpServerInternal(void* omaha);

  // Returns true of omaha is installer for user or machine.
  bool IsOmahaInstalled();

  // Starts running the HttpServer.
  HRESULT StartHttpServer();

  // Overrides the url, pingurl, aucheckperiod, overinstall values.
  static HRESULT SetupRegistry(bool needs_admin);

  // Restores the values of the registry.
  static HRESULT RestoreRegistry();

  // Builds a unique path to run the installer from.
  HRESULT BuildTaggedGoogleUpdatePath();

  // Stamps googleupdate with the correct values and runs the it.
  HRESULT StartGoogleUpdate();

  // Signals omaha to update the application.
  HRESULT StartApplicationUpdate();

  scoped_thread thread_;
  DWORD thread_id_;
  CString config_file_;
  CString googleupdate_setup_path_;
  ConfigResponses config_responses_;
  CString tagged_google_update_path_;
  scoped_ptr<ConsoleWriter> console_writer_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMPATIBILITY_TEST_H_
