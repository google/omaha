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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_RESPONSE_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_RESPONSE_H_

#include <http.h>
#include <winhttp.h>
#include "omaha/tools/omahacompatibility/httpserver/request.h"

namespace omaha {

// This class contains the result of the processing.
// For update responses it contains the response to the post in the
// string and for downloads it contains the filename and the size.
// TODO(omaha): Remove the size and filename from here and make this,
// class derivable. This will allow the http_server to not deal with
// the specifics of either a download or update check response.
class HttpResponse {
 public:
  explicit HttpResponse(const HttpRequest& request) : request_(request) {}
  ~HttpResponse() {}

  CString response_str() const { return response_str_; }
  void set_response_str(const CString& response_str) {
    response_str_  = response_str;
  }
  const HttpRequest& request() const { return request_; }

  int size() const { return size_; }
  void set_size(int size) { size_ = size; }

  CString file_name() const { return file_name_; }
  void set_file_name(const CString& filename) { file_name_ = filename; }

 private:
  HttpRequest request_;
  CString response_str_;

  // This is used for the download responses.
  int size_;
  CString file_name_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_RESPONSE_H_
