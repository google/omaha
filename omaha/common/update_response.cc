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

#include "omaha/common/update_response.h"
#include "omaha/base/utils.h"
#include "omaha/common/xml_parser.h"

namespace omaha {

namespace xml {

UpdateResponse::UpdateResponse() {
}

UpdateResponse::~UpdateResponse() {
}

UpdateResponse* UpdateResponse::Create() {
  return new UpdateResponse;
}

HRESULT UpdateResponse::Deserialize(const std::vector<uint8>& buffer) {
  return XmlParser::DeserializeResponse(buffer, this);
}

HRESULT UpdateResponse::DeserializeFromFile(const CString& filename) {
  std::vector<uint8> buffer;
  HRESULT hr = ReadEntireFile(filename, 0, &buffer);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(!buffer.empty());
  return Deserialize(buffer);
}

int UpdateResponse::GetElapsedSecondsSinceDayStart() const {
  return response_.day_start.elapsed_seconds;
}

int UpdateResponse::GetElapsedDaysSinceDatum() const {
  return response_.day_start.elapsed_days;
}

// Sets update_response's response_ member to response. Used by unit tests to
// set the response without needing to craft corresponding XML. UpdateResponse
// friends this function, allowing it to access the private member.
void SetResponseForUnitTest(UpdateResponse* update_response,
                            const response::Response& response) {
  ASSERT1(update_response);
  update_response->response_ = response;
}

}  // namespace xml

}  // namespace omaha
