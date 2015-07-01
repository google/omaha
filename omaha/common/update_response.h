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
//
// UpdateRequest allows the caller to deserialize an update request into
// a corresponding object object.

#ifndef OMAHA_COMMON_UPDATE_RESPONSE_H_
#define OMAHA_COMMON_UPDATE_RESPONSE_H_

#include <windows.h>
#include <utility>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/protocol_definition.h"

namespace omaha {

namespace xml {

class UpdateResponse {
 public:
  ~UpdateResponse();

  // Creates an instance of the class. Caller takes ownership.
  static UpdateResponse* Create();

  // Initializes an update response from a xml document in a buffer.
  HRESULT Deserialize(const std::vector<uint8>& buffer);

  // Initializes an update response from a xml document in a file.
  HRESULT DeserializeFromFile(const CString& filename);

  int GetElapsedSecondsSinceDayStart() const;

  int GetElapsedDaysSinceDatum() const;

  const response::Response& response() const { return response_; }

 private:
  friend class XmlParser;
  friend class XmlParserTest;

  // Sets response_ for unit testing.
  friend void SetResponseForUnitTest(UpdateResponse* update_response,
                                     const response::Response& response);

  UpdateResponse();

  response::Response response_;

  DISALLOW_COPY_AND_ASSIGN(UpdateResponse);
};

typedef std::pair<HRESULT, CString> UpdateResponseResult;

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_UPDATE_RESPONSE_H_

