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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_CONFIG_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_CONFIG_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>

namespace omaha {

// This struct is used to hold the responses to give to a particular
// application. It holds only the very basic information that is needed
// to accomplish this. The real ascii protocol buffer used by the server
// contains a lot more fields.
struct ConfigResponse {
  ConfigResponse(GUID g, const CString& v, const CString& u,
                 const CString& h, int s, bool b)
      : guid(g),
        version(v),
        url(u),
        hash(h),
        size(s),
        needs_admin(b) {}

  // The information to match the request against.
  ConfigResponse() {}
  CString app_name;
  GUID guid;
  CString version;
  CString language;

  // Response values.
  CString local_file_name;
  CString url;
  CString hash;
  int size;
  bool needs_admin;
};

typedef std::vector<ConfigResponse> ConfigResponses;


// Reads a config file that contains the specification
// of the update responses of the server.
// For an example of this file see example_config.txt.
HRESULT ReadConfigFile(const CString& file_name,
                       const CString& download_url_prefix,
                       ConfigResponses* config_response);

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_COMMON_CONFIG_H_
