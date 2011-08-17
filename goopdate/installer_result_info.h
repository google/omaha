// Copyright 2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_INSTALLER_RESULT_INFO_H_
#define OMAHA_GOOPDATE_INSTALLER_RESULT_INFO_H_

#include <atlstr.h>
#include "goopdate/omaha3_idl.h"
#include "omaha/common/const_goopdate.h"

namespace omaha {

// TODO(omaha3): perhaps rename "text" to "message".
struct InstallerResultInfo {
  InstallerResultInfo()
      : type(INSTALLER_RESULT_UNKNOWN),
        code(0),
        extra_code1(0),
        post_install_action(POST_INSTALL_ACTION_DEFAULT) {}

  InstallerResultType type;
  DWORD code;
  DWORD extra_code1;
  CString text;
  CString post_install_launch_command_line;
  CString post_install_url;
  PostInstallAction post_install_action;
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_INSTALLER_RESULT_INFO_H_
