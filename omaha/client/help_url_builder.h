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

#ifndef OMAHA_CLIENT_HELP_URL_BUILDER_H_
#define OMAHA_CLIENT_HELP_URL_BUILDER_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "omaha/base/debug.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

class HelpUrlBuilder {
 public:
  struct AppResult {
    AppResult(CString app_guid,
              HRESULT install_error_code,
              int install_extra_code) :
        guid(app_guid),
        error_code(install_error_code),
        extra_code(install_extra_code) {}

    CString guid;
    HRESULT error_code;
    int extra_code;
  };

  HelpUrlBuilder(bool is_machine,
                 const CString& language,
                 const GUID& iid,
                 const CString& brand) :
    is_machine_(is_machine),
    language_(language),
    iid_(iid),
    brand_(brand) {}

  HRESULT BuildUrl(const std::vector<AppResult>& app_results,
                   CString* help_url) const;

 private:
  // Creates the query string based on the passed service_url and arguments.
  // service_url must end in a '?' or '&'.
  HRESULT BuildHttpGetString(
      const CString& service_url,
      const std::vector<AppResult>& app_results,
      const CString& goopdate_version,
      const CString& source_id,
      CString* get_request) const;

 private:
  const bool is_machine_;
  const CString language_;
  const GUID iid_;
  const CString brand_;

  friend class HelpUrlBuilderTest;
  DISALLOW_COPY_AND_ASSIGN(HelpUrlBuilder);
};

}  // namespace omaha

#endif  // OMAHA_CLIENT_HELP_URL_BUILDER_H_
