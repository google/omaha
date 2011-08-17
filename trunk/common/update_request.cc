// Copyright 2009-2010 Google Inc.
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

#include "omaha/common/update_request.h"
#include "base/scoped_ptr.h"
#include "omaha/base/debug.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/system_info.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/xml_parser.h"

namespace omaha {

namespace xml {

UpdateRequest::UpdateRequest() {
}

UpdateRequest::~UpdateRequest() {
}

// TODO(omaha): handle errors.
UpdateRequest* UpdateRequest::Create(bool is_machine,
                                     const CString& session_id,
                                     const CString& install_source,
                                     const CString& origin_url) {
  scoped_ptr<UpdateRequest> update_request(new UpdateRequest);

  request::Request& request = update_request->request_;

  request.is_machine = is_machine;
  request.protocol_version = _T("3.0");

  request.uid = goopdate_utils::GetUserIdLazyInit(is_machine);

  request.omaha_version = GetVersionString();
  request.install_source = install_source;
  request.origin_url = origin_url;
  request.test_source = ConfigManager::Instance()->GetTestSource();

  GUID req_id = GUID_NULL;
  VERIFY1(SUCCEEDED(::CoCreateGuid(&req_id)));
  request.request_id = GuidToString(req_id);

  request.session_id = session_id;

  request.os.platform = kPlatformWin;
  VERIFY1(SUCCEEDED(goopdate_utils::GetOSInfo(&request.os.version,
                                              &request.os.service_pack)));
  request.os.arch = xml::ConvertProcessorArchitectureToString(
      SystemInfo::GetProcessorArchitecture());

  bool is_period_overridden = false;
  const int check_period_sec =
      ConfigManager::Instance()->GetLastCheckPeriodSec(&is_period_overridden);
  if (is_period_overridden) {
    request.check_period_sec = check_period_sec;
  }

  return update_request.release();
}

void UpdateRequest::AddApp(const request::App& app) {
  request_.apps.push_back(app);
}

bool UpdateRequest::has_tt_token() const {
  for (size_t i = 0; i != request_.apps.size(); ++i) {
    const request::App& app(request_.apps[i]);
    if (!app.update_check.tt_token.IsEmpty()) {
      return true;
    }
  }
  return false;
}

HRESULT UpdateRequest::Serialize(CString* buffer) const {
  ASSERT1(buffer);
  return XmlParser::SerializeRequest(*this, buffer);
}

bool UpdateRequest::IsEmpty() const {
  return request_.apps.empty();
}

}  // namespace xml

}  // namespace omaha
