// Copyright 2007-2009 Google Inc.
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
// goopdate server request

#ifndef OMAHA_GOOPDATE_REQUEST_H__
#define OMAHA_GOOPDATE_REQUEST_H__

#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/utils.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/app_request.h"
#include "omaha/worker/ping_event.h"

namespace omaha {

class PingMock;

class Request {
 public:
  explicit Request(bool is_machine);
  ~Request();

  bool is_machine() const { return is_machine_; }
  void set_is_machine(bool is_machine) { is_machine_ = is_machine; }
  CString machine_id() const { return machine_id_; }
  void set_machine_id(const CString& machine_id) { machine_id_ = machine_id; }
  CString user_id() const { return user_id_; }
  void set_user_id(const CString& user_id) { user_id_ = user_id; }
  CString version() const { return version_; }
  void set_version(const CString& version) { version_ = version; }
  CString os_version() const { return os_version_; }
  void set_os_version(const CString& os_version) { os_version_ = os_version; }
  CString os_service_pack() const { return os_service_pack_; }
  void set_os_service_pack(const CString& os_service_pack) {
    os_service_pack_ = os_service_pack;
  }
  CString test_source() const { return test_source_; }
  void set_test_source(const CString& test_source) {
    test_source_ = test_source;
  }
  CString request_id() const { return request_id_; }
  void set_request_id(const CString& request_id) {
    request_id_ = request_id;
  }

  size_t get_request_count() const { return app_requests_.size(); }
  AppRequestVector::const_iterator app_requests_begin() const {
    return app_requests_.begin();
  }
  AppRequestVector::const_iterator app_requests_end() const {
    return app_requests_.end();
  }

  // Request will hold onto and manage/release the app_request instance.
  void AddAppRequest(const AppRequest& app_request);

 private:

  bool is_machine_;
  CString machine_id_;
  CString user_id_;
  CString version_;
  CString os_version_;
  CString os_service_pack_;

  // Identifies the source of the request as a test/production prober system.
  CString test_source_;

  // Unique identifier for this request, used to associate the same request
  // received multiple times on the server.
  CString request_id_;
  AppRequestVector app_requests_;

  friend class PingMock;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Request);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_REQUEST_H__
