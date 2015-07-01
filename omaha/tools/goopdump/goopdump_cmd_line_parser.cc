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


#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

#include "omaha/common/debug.h"
#include "omaha/goopdate/command_line_parser.h"

namespace omaha {

HRESULT ParseGoopdumpCmdLine(int argc,
                             TCHAR** argv,
                             GoopdumpCmdLineArgs* args) {
  ASSERT1(argc >= 1);
  ASSERT1(argv);

  UNREFERENCED_PARAMETER(argc);
  UNREFERENCED_PARAMETER(argv);

  CommandLineParser parser;
  HRESULT hr = parser.ParseFromArgv(argc, argv);
  if (FAILED(hr)) {
    return hr;
  }

  std::vector<CString> valid_params;
  valid_params.push_back(_T("dumpapps"));
  valid_params.push_back(_T("oneclick"));
  valid_params.push_back(_T("monitor"));
  valid_params.push_back(_T("file"));

  for (int i = 0; i < parser.GetSwitchCount(); ++i) {
    CString switch_name;
    parser.GetSwitchNameAtIndex(i, &switch_name);
    bool found = false;
    for (size_t i = 0; i < valid_params.size(); ++i) {
      const CString& valid_param = valid_params[i];
      if (valid_param.Compare(switch_name) == 0) {
        found = true;
      }
    }
    if (!found) {
      return E_INVALIDARG;
    }
  }

  if (parser.HasSwitch(_T("file"))) {
    int arg_count = 0;
    parser.GetSwitchArgumentCount(_T("file"), &arg_count);
    if (arg_count != 1) {
      return E_INVALIDARG;
    }
    args->is_write_to_file = true;
    parser.GetSwitchArgumentValue(_T("file"), 0, &(args->log_filename));
  }

  if (parser.GetSwitchCount() == 0 ||
      (parser.HasSwitch(_T("file")) && parser.GetSwitchCount() == 1)) {
    // If you don't pass anything, give them everything except monitoring.
    args->is_dump_general = true;
    args->is_dump_app_manager = true;
    args->is_dump_oneclick = true;
    args->is_machine = true;
    args->is_user = true;
  }

  if (parser.HasSwitch(_T("dumpapps"))) {
    args->is_dump_general = true;
    args->is_dump_app_manager = true;
    args->is_machine = true;
    args->is_user = true;
  }

  if (parser.HasSwitch(_T("oneclick"))) {
    args->is_dump_general = true;
    args->is_dump_oneclick = true;
    args->is_machine = true;
    args->is_user = true;
  }

  if (parser.HasSwitch(_T("monitor"))) {
    args->is_monitor = true;
  }

  return S_OK;
}

}  // namespace omaha

