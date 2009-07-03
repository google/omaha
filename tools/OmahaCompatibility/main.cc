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
//
// Invoke with:
// omaha_comtibility_test.exe <path>\example_config.txt
//                            <path>\GoogleUpdateSetup.exe
// or if you only want to run the httpserver that reads the config and responds
// appropriately use
// OmahaCompatibility.exe <path>\example_config.txt
//

#include <Windows.h>
#include <tchar.h>
#include "omaha/common/vistautil.h"
#include "omaha/tools/omahacompatibility/compatibility_test.h"

void PrintUsage() {
  wprintf(_T("Incorrect arguments\n")
          _T("Usage:\n")
          _T("OmahaCompatibility <Full path to config file>")
          _T("                   <Full path to googleupdatesetup.exe>\n")
          _T("\nor:\n")
          _T("OmahaCompatibility <Full path to config file>")
          _T("Note: The debug version of googleupdatesetup is needed.\n")
          _T("You need to be admin on the machine to run this program,\n")
          _T("although omaha itself does not need admin to run."));
}

// Set the appropriate update dev registry keys. Url, AuCheckPeriodMs
// Start the http server.
// Launch googleupdate.
int _tmain(int argc, TCHAR* argv[]) {
  if (argc < 2 || argc > 3) {
    PrintUsage();
    return -1;
  }

  // TODO(omaha): Remove this requirement. This is needed for now, since we
  // need permissions to write the updatedev key to override the url that omaha
  // talks to.
  if (!omaha::vista_util::IsUserAdmin()) {
    PrintUsage();
    return -1;
  }

  omaha::CompatibilityTest compat_test;
  compat_test.set_config_file(argv[1]);

  bool test_omaha = false;
  if (argc == 3) {
    compat_test.set_googleupdate_setup_path(argv[2]);
    test_omaha = true;
  }
  return compat_test.Main(test_omaha);
}

