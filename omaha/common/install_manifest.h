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

// Defines the installation manifest, which is specific to each application and
// version. This is a quasi-static data structure returned by the server in
// the update response.

#ifndef OMAHA_COMMON_INSTALL_MANIFEST_H_
#define OMAHA_COMMON_INSTALL_MANIFEST_H_

#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/const_goopdate.h"

namespace omaha {

namespace xml {

struct InstallPackage {
  InstallPackage() : is_required(false), size(0) {}

  CString name;
  CString version;
  bool is_required;
  int size;
  CString hash_sha1;  // base64 encoded.
  CString hash_sha256;  // hex-digit encoded.
};

struct InstallAction {
  InstallAction()
      : install_event(static_cast<InstallEvent>(0)),
        needs_admin(NEEDS_ADMIN_NO),
        terminate_all_browsers(false),
        success_action(SUCCESS_ACTION_DEFAULT) {}

  enum InstallEvent { kPreInstall = 1, kInstall, kUpdate, kPostInstall };

  InstallEvent install_event;

  // Whether the action should be run as admin. This may differ from whether the
  // app is_machine.
  // TODO(omaha3): This value is defined in the protocol but not implemented:
  //  * It is not yet set by the XML parser.
  //  * This should probably be renamed run_elevated since it is more likely to
  //    be used to run de-elevated from an elevated Omaha instance.
  //  * What should machine Omaha do if there is no logged in user and the
  //    action cannot be de-elevated?
  //  * This should default to the bundle's is_machine.
  //  * Assert if !is_machine and needs_admin == NEEDS_ADMIN_YES.
  NeedsAdmin needs_admin;

  // TODO(omaha3): Need some more thinking here. On one hand, overloading this
  // is tempting. On the other hand, it may be hard to read.
  // This could also be SuccessfulInstallActions such as "launch_browser", or
  // "terminate_all_browsers". The program_params in the case of
  // "launch_browser" would be the URL to navigate to.
  CString program_to_run;
  CString program_arguments;

  CString success_url;       // URL to launch the browser on success.
  bool terminate_all_browsers;
  SuccessfulInstallAction success_action;  // Action after install success.
};

// TODO(omaha3): Should all these really be public members?
struct InstallManifest {
  CString name;       // TBD.
  CString version;
  std::vector<InstallPackage> packages;
  // TODO(omaha3): Maybe this should be a map or array of kPostInstall + 1 ptrs.
  std::vector<InstallAction> install_actions;
};

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_INSTALL_MANIFEST_H_
