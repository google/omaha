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

#ifndef OMAHA_GOOPDATE_WEBPLUGIN_UTILS_H__
#define OMAHA_GOOPDATE_WEBPLUGIN_UTILS_H__

#include <windows.h>
#include <atlpath.h>
#include <atlstr.h>
#include "base/basictypes.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/config_manager.h"

namespace omaha {

namespace webplugin_utils {

// Calls google.com backend to verify the site that the web plugin was
// called from and verifies command line parameters.
HRESULT VerifyParamsViaWebService(CString webplugin_check_url,
                                  const CommandLineArgs& args);

// Verifies that the resource DLL we're going to load within googleupdate.exe is
// available and loadable.
HRESULT VerifyResourceLanguage(const CString& webplugin_args);

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

// Parses the response from the OneClick check web service.
// Returns S_OK if we get a status="pass" and url_domain_requested equals the
// domain in the response_xml.
// Returns GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED if status="fail" or
// url_domain_requested doesn't match the domain in the response xml.
// Returns E_INVALIDARG on parsing errors.
HRESULT ProcessOneClickResponseXml(const CString& url_domain_requested,
                                   CString response_xml);

}  // namespace webplugin_utils

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_WEBPLUGIN_UTILS_H__
