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

#include <cmath>

#include "base/cpu.h"
#include "omaha/base/debug.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/system.h"
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
                                     const CString& origin_url,
                                     const CString& request_id) {
  const ConfigManager* cm = ConfigManager::Instance();

  std::unique_ptr<UpdateRequest> update_request(new UpdateRequest);

  request::Request& request = update_request->request_;

  request.is_machine = is_machine;
  request.protocol_version = _T("3.0");

  request.uid = goopdate_utils::GetUserIdLazyInit(is_machine);

  request.omaha_version = GetVersionString();
  request.omaha_shell_version = GetShellVersionString();
  request.install_source = install_source;
  request.origin_url = origin_url;
  request.test_source = cm->GetTestSource();

  request.session_id = session_id;
  request.request_id = request_id;

  bool is_period_overridden = false;
  const int check_period_sec = cm->GetLastCheckPeriodSec(&is_period_overridden);
  if (is_period_overridden) {
    request.check_period_sec = check_period_sec;
  }

  request.dlpref = cm->GetDownloadPreferenceGroupPolicy(NULL);

  request.domain_joined = IsEnterpriseManaged();

  // Hardware platform attributes.
  //
  // The amount of memory available to the operating system can be less than
  // the amount of memory physically installed in the computer. The difference
  // is relatively small and this value is a good approximation of what the
  // computer BIOS has reported.
  uint64 physmemory(0);
  if (SUCCEEDED(System::GetGlobalMemoryStatistics(NULL,
                                                  NULL,
                                                  &physmemory,
                                                  NULL,
                                                  NULL,
                                                  NULL,
                                                  NULL))) {
    // Converts the amount of physical memory to the nearest GB.
    const size_t kOneGigaByte = 1024 * 1024 * 1024;
    request.hw.physmemory = static_cast<uint32>(std::floor(
        0.5 + static_cast<double>(physmemory) / kOneGigaByte));
  }

  const CPU cpu;
  request.hw.has_sse   = cpu.has_sse();
  request.hw.has_sse2  = cpu.has_sse2();
  request.hw.has_sse3  = cpu.has_sse3();
  request.hw.has_ssse3 = cpu.has_ssse3();
  request.hw.has_sse41 = cpu.has_sse41();
  request.hw.has_sse42 = cpu.has_sse42();
  request.hw.has_avx   = cpu.has_avx();

  // Software platform attributes.
  request.os.platform = kPlatformWin;
  VERIFY_SUCCEEDED(goopdate_utils::GetOSInfo(&request.os.version,
                                              &request.os.service_pack));
  request.os.arch = SystemInfo::GetArchitecture();

  return update_request.release();
}

UpdateRequest* UpdateRequest::Create(bool is_machine,
                                     const CString& session_id,
                                     const CString& install_source,
                                     const CString& origin_url) {
  CString request_id;
  VERIFY_SUCCEEDED(GetGuid(&request_id));
  return Create(is_machine, session_id, install_source, origin_url, request_id);
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

CString UpdateRequest::app_ids() const {
  CString app_ids_string;
  for (size_t i = 0; i != request_.apps.size(); ++i) {
    if (i > 0) {
      app_ids_string += ',';
    }

    app_ids_string += request_.apps[i].app_id;
  }

  return app_ids_string;
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
