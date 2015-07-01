// Copyright 2011 Google Inc.
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

#include "omaha/common/ping_event.h"
#include "omaha/base/string.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/xml_const.h"

namespace omaha {

HRESULT PingEvent::ToXml(IXMLDOMNode* parent_node) const {
  HRESULT hr = AddXMLAttributeNode(parent_node,
                                   xml::kXmlNamespace,
                                   xml::attribute::kEventType,
                                   itostr(event_type_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kEventResult,
                           itostr(event_result_));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(parent_node,
                           xml::kXmlNamespace,
                           xml::attribute::kErrorCode,
                           itostr(error_code_));
  if (FAILED(hr)) {
    return hr;
  }

  return AddXMLAttributeNode(parent_node,
                             xml::kXmlNamespace,
                             xml::attribute::kExtraCode1,
                             itostr(extra_code1_));
}

CString PingEvent::ToString() const {
  CString ping_str;
  ping_str.Format(_T("%s=%s, %s=%s, %s=%s, %s=%s"),
      xml::attribute::kEventType, itostr(event_type_),
      xml::attribute::kEventResult, itostr(event_result_),
      xml::attribute::kErrorCode, itostr(error_code_),
      xml::attribute::kExtraCode1, itostr(extra_code1_));

  return ping_str;
}

}  // namespace omaha

