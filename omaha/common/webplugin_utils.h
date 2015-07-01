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

#ifndef OMAHA_COMMON_WEBPLUGIN_UTILS_H__
#define OMAHA_COMMON_WEBPLUGIN_UTILS_H__

#include <windows.h>
#include <atlpath.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/common/command_line.h"
#include "omaha/common/config_manager.h"

namespace omaha {

namespace webplugin_utils {

// This function escapes all unsafe characters, and then unescapes a selective
// white-list of characters: '=', '&', '{', '}', '%'. For a properly formatted
// extra args string, extra_args_out will be exactly equal to extra_args_in.
HRESULT SanitizeExtraArgs(const CString& extra_args_in,
                          CString* extra_args_out);

// This function builds a sanitized /pi command line.
HRESULT BuildWebPluginCommandLine(const CString& url_domain,
                                  const CString& extra_args,
                                  CString* final_cmd_line_args);

// Parses the arguments, extracts the language parameter, and verifies that we
// support the requested language.
HRESULT IsLanguageSupported(const CString& webplugin_args);

// Copies required Goopdate files to a temp location before installing.
HRESULT CopyGoopdateToTempDir(const CPath& current_goopdate_path,
                              CPath* goopdate_temp_path);

// Launches google_update.exe based on parameters sent with /webplugin.
HRESULT DoOneClickInstall(const CommandLineArgs& args);

// Creates request string for the webplugin URL check webservice call.
HRESULT BuildOneClickRequestString(const CommandLineArgs& args,
                                   CString* request_str);

// Builds up the command line arguments to re-launch google_update.exe
// when called with /pi.
HRESULT BuildOneClickWorkerArgs(const CommandLineArgs& args, CString* args_out);

}  // namespace webplugin_utils

}  // namespace omaha

#endif  // OMAHA_COMMON_WEBPLUGIN_UTILS_H__
