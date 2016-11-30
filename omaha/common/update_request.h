// Copyright 2009 Google Inc.
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
// UpdateRequest allows the caller to build an update request object and
// serialize it as a string.

#ifndef OMAHA_COMMON_UPDATE_REQUEST_H_
#define OMAHA_COMMON_UPDATE_REQUEST_H_

#include <windows.h>
#include "base/basictypes.h"
#include "omaha/common/protocol_definition.h"

namespace omaha {

namespace xml {

class UpdateRequest {
 public:
  ~UpdateRequest();

  // Creates an instance of the class. Caller takes ownership.
  static UpdateRequest* Create(bool is_machine,
                               const CString& session_id,
                               const CString& install_source,
                               const CString& origin_url,
                               const CString& request_id);
  static UpdateRequest* Create(bool is_machine,
                               const CString& session_id,
                               const CString& install_source,
                               const CString& origin_url);

  // Adds an 'app' element to the request.
  void AddApp(const request::App& app);

  // Returns true if the requests does not contain applications.
  bool IsEmpty() const;

  // Serializes the request into a buffer.
  HRESULT Serialize(CString* buffer) const;

  // Returns true if one of the applications in the request carries a
  // trusted tester token.
  bool has_tt_token() const;

  CString app_ids() const;

  void set_omaha_shell_version(const CString& shell_version_string) {
    request_.omaha_shell_version = shell_version_string;
  }
  const request::Request& request() const { return request_; }

 private:
  friend class XmlParserTest;

  UpdateRequest();

  request::Request request_;

  DISALLOW_COPY_AND_ASSIGN(UpdateRequest);
};

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_UPDATE_REQUEST_H_
