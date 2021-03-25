// Copyright 2011 Google Inc.
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

#ifndef OMAHA_CLIENT_BUNDLE_CREATOR_H_
#define OMAHA_CLIENT_BUNDLE_CREATOR_H_

#include <windows.h>
#include <atlsafe.h>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "goopdate/omaha3_idl.h"

namespace omaha {

struct CommandLineExtraArgs;

namespace bundle_creator {

// Creates app bundle interface, initializes it with default properties and
// the given install source.
HRESULT Create(bool is_machine,
               const CString& display_language,
               const CString& install_source,
               const CString& session_id,
               bool is_interactive,
               bool send_pings,
               IAppBundle** app_bundle);

// Creates app bundle interface that contains the apps specified in extra_args.
// The apps properties are intialized based on the extra_args and other
// arguments.
HRESULT CreateFromCommandLine(bool is_machine,
                              bool is_eula_accepted,
                              bool is_offline,
                              const CString& offline_dir_name,
                              const CommandLineExtraArgs& extra_args,
                              const CString& install_source,
                              const CString& session_id,
                              bool is_interactive,
                              bool send_pings,
                              IAppBundle** app_bundle);

// Creates app bundle interface by finding apps that need to be force-installed
// according to policy set by a domain administrator and that are not already
// installed.
HRESULT CreateForceInstallBundle(bool is_machine,
                                 const CString& display_language,
                                 const CString& install_source,
                                 const CString& session_id,
                                 bool is_interactive,
                                 bool send_pings,
                                 IAppBundle** app_bundle);

// Creates app bundle interface that contains the given app (app_id).
HRESULT CreateForOnDemand(bool is_machine,
                          const CString& app_id,
                          const CString& install_source,
                          const CString& session_id,
                          HANDLE impersonation_token,
                          HANDLE primary_token,
                          IAppBundle** app_bundle);

}  // namespace bundle_creator

}  // namespace omaha

#endif  // OMAHA_CLIENT_BUNDLE_CREATOR_H_
