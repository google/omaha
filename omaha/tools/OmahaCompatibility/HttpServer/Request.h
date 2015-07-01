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

#ifndef OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_REQUEST_H_
#define OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_REQUEST_H_

#include <http.h>
#include <winhttp.h>

namespace omaha {

// Represents the HttpRequest received by the server.
class HttpRequest {
 public:
  HTTP_VERB http_verb() const { return verb_; }
  void set_http_verb(HTTP_VERB verb) { verb_ = verb; }

  CString path() const { return path_; }
  void set_path(const CString& path) { path_ = path; }

  CString content() const { return content_; }
  void set_content(const CString& content) { content_ = content; }

  HTTP_REQUEST http_request() const { return http_request_; }
  void set_http_request(const HTTP_REQUEST& request) {
    http_request_ = request;
  }

  CString query_str() const { return query_str_; }
  void set_query_str(const CString& query_str) {
    query_str_ = query_str;
  }

 private:
  HTTP_REQUEST http_request_;
  HTTP_VERB verb_;
  CString path_;
  CString content_;
  CString query_str_;
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_OMAHACOMPATIBILITY_HTTPSERVER_REQUEST_H_
