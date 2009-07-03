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
// Declares the Goopdate XML parser. This XML schema is used in the following:
// 1. Creating update pings.
// 2. Parsing update responses.
// 3. Parsing the Install manifest.

#ifndef OMAHA_GOOPDATE_GOOPDATE_XML_PARSER_H__
#define OMAHA_GOOPDATE_GOOPDATE_XML_PARSER_H__

#include <map>
#include <vector>
#include "base/basictypes.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/update_response.h"

namespace omaha {

const int kInvalidId = -1;

// name and value of an element or attribute, used in internal loops.
typedef std::pair<CString, CString> XMLNameValuePair;

// Reader/writer for the Update request. The class is a simple wrapper on
// top of XML DOM. The class performs validation of the response schema,
// without calling into the xml schema validation of DOM.
class GoopdateXmlParser {
 public:
  // Parses the manifest file.
  static HRESULT ParseManifestFile(const CString& file_name,
                                   UpdateResponses* responses);

  // Parses the manifest string.
  static HRESULT ParseManifestString(const TCHAR* manifest,
                                     UpdateResponses* responses);

  // Generates the update request from the request node.
  static HRESULT GenerateRequest(const Request& request,
                                 bool is_update_check,
                                 CString* request_string);

  // Loads an XML file into memory.
  static HRESULT LoadXmlFileToMemory(const CString& file_name,
                                     CString* xml_string);

 private:
  // Reads the protocol version of the xml file.
  static HRESULT GetProtocolVersion(IXMLDOMElement* document_element,
                                    CString* version);

  // Reads the app elements into individual UpdateResponse elements
  // and adds them to responses.
  static HRESULT ReadUpdateResponses(IXMLDOMNode* gupdate,
                                     UpdateResponses* responses);

  // Reads the UpdateResponse element.
  static HRESULT ReadUpdateResponse(IXMLDOMNode* app,
                                    UpdateResponse* response);

  // Reads the components section of a response.
  static HRESULT ReadComponentsResponses(IXMLDOMNode* node,
                                         UpdateResponse* response);

  // Reads an individual component of a response.
  static HRESULT ReadComponentResponseData(IXMLDOMNode* node,
                                           UpdateResponseData* response_data);

  // Reads the install elements into individual UpdateResponse elements
  // and adds them to responses.
  static HRESULT ReadSeedInstalls(IXMLDOMNode* gupdate,
                                  UpdateResponses* responses);

  // Reads the InstallsElement.
  static HRESULT ReadInstallElement(IXMLDOMNode* node,
                                    UpdateResponseData* response_data);

  // Reads the attributes that must be in an install node.
  static HRESULT ReadRequiredInstallAttributes(
      IXMLDOMNode* node,
      UpdateResponseData* response_data);

  // Reads the attributes that are optional in an install node.
  static HRESULT ReadOptionalInstallAttributes(
      IXMLDOMNode* node,
      UpdateResponseData* response_data);

  // Reads the string value, either TEXT or CDATA, within the given node.
  static HRESULT ReadStringValue(IXMLDOMNode* node, CString* value);

  // Reads an attribute, given the node and the name of the attribute.
  static HRESULT ReadAttribute(IXMLDOMNode* node, const TCHAR* attr_name,
                               BSTR* value);

  // Reads and parses an attribute that contains a boolean value.
  static HRESULT ReadBooleanAttribute(IXMLDOMNode* node,
                                      const TCHAR* attr_name,
                                      bool* value);

  // Reads an attribute that contains a guid.
  static HRESULT ReadGuidAttribute(IXMLDOMNode* node,
                                   const TCHAR* attr_name,
                                   GUID* value);

  // Reads a string attribute given the node and the attribute name.
  static HRESULT ReadStringAttribute(IXMLDOMNode* node,
                                     const TCHAR* attr_name,
                                     CString* value);

  // Reads an int attribute given the node and the attribute name.
  static HRESULT ReadIntAttribute(IXMLDOMNode* node,
                                  const TCHAR* attr_name,
                                  int* value);

  // creates an element in the request namespace
  static HRESULT CreateRequestElement(const TCHAR* xml_element_name,
                                      const CString& value,
                                      IXMLDOMDocument* document,
                                      IXMLDOMNode** request_element);

  // Creates an element in the request namespace and adds it to parent
  static HRESULT CreateAndAddRequestElement(const TCHAR* xml_element_name,
                                            const CString& value,
                                            IXMLDOMDocument* document,
                                            IXMLDOMNode* parent);

  // Converts a string to the SuccessfulInstallAction enum.
  static SuccessfulInstallAction ConvertStringToSuccessAction(
      const CString& text);

  // Adds the attributes to the node.
  static HRESULT AddAttributesToNode(
      const CString& namespace_uri,
      const std::vector<XMLNameValuePair>& attributes,
      IXMLDOMNode* element);

  // Verifies that document element's version is compatible.
  static HRESULT VerifyElementProtocolCompatibility(
      IXMLDOMElement* document_element,
      const CString& expected_version);

  // Verifies that the versions are compatible.
  static HRESULT VerifyProtocolCompatibility(const CString& actual_version,
                                             const CString& expected_version);

  // Verifies that the response node name and version are valid.
  static HRESULT ValidateResponseElement(IXMLDOMElement* document_element);

  // Parses the manifest.
  static HRESULT ParseManifest(IXMLDOMElement* manifest,
                               UpdateResponses* responses);

  // Verify that the protocol version is between the versions.
  static HRESULT VerifyProtocolRange(const CString& actual_ver,
                                     const CString& start_ver,
                                     const CString& end_ver);

 private:
  // Helper for CreateUpdateAppRequestElement and CreatePingAppRequestElement.
  static HRESULT CreateAppRequestElementHelper(const AppRequest& app_request,
                                               IXMLDOMDocument* document,
                                               IXMLDOMNode** app_element);

  // Creates the main app element, including the update element, and all its
  // children.
  static HRESULT CreateUpdateAppRequestElement(const AppRequest& app_request,
                                               IXMLDOMDocument* document,
                                               IXMLDOMNode** app_element);

  // Creates the main app element, including the ping element, and all its
  // children.
  static HRESULT CreatePingAppRequestElement(const AppRequest& app_request,
                                             IXMLDOMDocument* document,
                                             IXMLDOMNode** app_element);

  static CString seed_protocol_version;

  friend class GoopdateXmlParserTest;

  DISALLOW_IMPLICIT_CONSTRUCTORS(GoopdateXmlParser);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_GOOPDATE_XML_PARSER_H__
