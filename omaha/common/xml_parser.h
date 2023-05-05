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
// Defines the Goopdate XML parser. The clients of this class are
// UpdateRequest and UpdateResponse.

#ifndef OMAHA_COMMON_XML_PARSER_H_
#define OMAHA_COMMON_XML_PARSER_H_

#include <windows.h>
#include <objbase.h>
#include <msxml2.h>
#include <atlbase.h>
#include <atlstr.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "base/object_factory.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"

namespace omaha {

namespace xml {

class ElementHandler;

// Public static methods instantiate a temporary instance of this class, which
// then parses the specified document. This avoids reusing instances of the
// parser and dealing with stale and dirty data.
class XmlParser {
 public:
  // Parses the update response buffer and fills in the UpdateResponse.
  // The UpdateResponse object is not modified in case of errors and it can
  // be safely reused for subsequent parsing attempts.
  // TODO(omaha): since the xml docs are strings we could use a CString as
  // an input parameter, no reason why this should be a buffer.
  static HRESULT DeserializeResponse(const std::vector<uint8>& buffer,
                                     UpdateResponse* update_response);

  // Generates the update request from the request node.
  static HRESULT SerializeRequest(const UpdateRequest& update_request,
                                  CString* buffer);

 private:
  typedef Factory<ElementHandler, CString> ElementHandlerFactory;

  XmlParser();
  void InitializeElementHandlers();
  void InitializeLegacyElementHandlers();

  // Builds an XML object model corresponding to an xml request.
  HRESULT BuildDom(const request::Request& request);

  // Builds the 'request' element.
  HRESULT BuildRequestElement();

  // Creates the 'hw' element.
  HRESULT BuildHwElement(IXMLDOMNode* parent_node);

  // Creates the 'os' element.
  HRESULT BuildOsElement(IXMLDOMNode* parent_node);

  // Creates the 'app' element. This is usually a sequence of elements.
  HRESULT BuildAppElement(IXMLDOMNode* parent_node);

  // Adds attributes under the 'app' element corresponding to values with a '_'
  // prefix under the ClientState/ClientStateMedium key.
  HRESULT AddAppDefinedAttributes(const request::App& app,
                                  IXMLDOMNode* parent_node);

  // Adds cohort attributes under the 'app' element corresponding to values
  // under the ClientState/{AppID}/Cohort key.
  HRESULT AddCohortAttributes(const request::App& app, IXMLDOMNode* element);

  // Creates the 'updatecheck' element for an application.
  HRESULT BuildUpdateCheckElement(const request::App& app,
                                  IXMLDOMNode* parent_node);

  // Creates Ping aka 'event' elements for an application.
  HRESULT BuildPingRequestElement(const request::App& app,
                                  IXMLDOMNode* parent_node);

  // Creates the 'data' element for an application.
  HRESULT BuildDataElement(const request::App& app,
                           IXMLDOMNode* parent_node);

  // Creates the 'didrun' aka 'active' aka 'ping' element for an application.
  HRESULT BuildDidRunElement(const request::App& app,
                             IXMLDOMNode* parent_node);

  // Serializes the DOM into a string.
  HRESULT GetXml(CString* buffer);

  // Creates an element in the Update2 xml namespace.
  HRESULT CreateElementNode(const TCHAR* name,
                            const TCHAR* value,
                            IXMLDOMNode** element);

  // Starts parsing of the xml document.
  HRESULT Parse();

  // Does a DFS traversal of the dom.
  HRESULT TraverseDOM(IXMLDOMNode* node);

  // Handles a single node during traversal.
  HRESULT VisitElement(IXMLDOMNode* node);

  // The current xml document.
  CComPtr<IXMLDOMDocument> document_;

  // The xml request being serialized. Not owned by this class.
  const request::Request* request_;

  // The xml response being deserialized. Not owned by this class.
  response::Response* response_;

  ElementHandlerFactory element_handler_factory_;

  DISALLOW_COPY_AND_ASSIGN(XmlParser);
};

}  // namespace xml

}  // namespace omaha

#endif  // OMAHA_COMMON_XML_PARSER_H_
