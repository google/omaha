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


#include "omaha/tools/omahacompatibility/httpserver/xml_parser.h"
#include <msxml2.h>
#include "omaha/common/string.h"
#include "omaha/common/xml_utils.h"
#include "omaha/goopdate/goopdate_xml_parser.h"

namespace omaha {

// Constant strings to form the server responses.
const TCHAR* const kResponseXmlHeader = _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");  // NOLINT
const TCHAR* const kResponseXmlGupdateHeader = _T("<gupdate xmlns=\"http://www.google.com/update2/response\" protocol=\"2.0\">"); // NOLINT
const TCHAR* const kResponseAppEvent = _T("<app appid=\"%s\" status=\"ok\"><event status=\"ok\"/></app>"); // NOLINT
const TCHAR* const kResponseAppNoUpdate = _T("<app appid=\"%s\" status=\"ok\"><event status=\"no-update\"/></app>"); // NOLINT
const TCHAR* const kResponseAppUpdate = _T("<app appid=\"%s\" status=\"ok\"><updatecheck codebase=\"%s\" hash=\"%s\" needsadmin=\"%s\" size=\"%d\" status=\"ok\"/></app>"); // NOLINT
const TCHAR* const kResponseGupdateEndTag = _T("</gupdate>");

// Constants for creating the xml request.
namespace Xml {
  const TCHAR* const kHeaderText =
    _T("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
  const TCHAR* const kProcessingText =
    _T("version=\"1.0\" encoding=\"UTF-8\"");

  namespace Namespace {
    const TCHAR* const kRequest = _T("http://www.google.com/update2/request");
    const TCHAR* const kResponse = _T("http://www.google.com/update2/response");
    const TCHAR* const kSeed = _T("http://www.google.com/update2/install");
  }  // Namespace.

  namespace Element {
    const TCHAR* const kXml = _T("xml");
    const TCHAR* const kRequests = _T("gupdate");
    const TCHAR* const kOmahaVersion = _T("updaterversion");
    const TCHAR* const kOs = _T("os");
    const TCHAR* const kApp = _T("app");
    const TCHAR* const kUpdateCheck = _T("updatecheck");
    const TCHAR* const kPing = _T("ping");
    const TCHAR* const kEvent = _T("event");
    const TCHAR* const kComponents = _T("components");
    const TCHAR* const kComponent = _T("component");

    const TCHAR* const kResponses = _T("gupdate");
  }  // namespace Element.

  namespace Attribute {
    const TCHAR* const kActive = _T("active");
    const TCHAR* const kAdditionalParameter = _T("ap");
    const TCHAR* const kAppGuid = _T("appguid");
    const TCHAR* const kApplicationName = _T("appname");
    const TCHAR* const kAppId = _T("appid");
    const TCHAR* const kArguments = _T("arguments");
    const TCHAR* const kBrandCode = _T("brand");
    const TCHAR* const kBrowserType = _T("browser");
    const TCHAR* const kClientId = _T("client");
    const TCHAR* const kCodebase = _T("codebase");
    const TCHAR* const kCountry = _T("country");
    const TCHAR* const kErrorCode = _T("errorcode");
    const TCHAR* const kEventResult = _T("eventresult");
    const TCHAR* const kEventType = _T("eventtype");
    const TCHAR* const kExtraCode1 = _T("extracode1");
    const TCHAR* const kHash = _T("hash");
    const TCHAR* const kIsMachine = _T("ismachine");
    const TCHAR* const kInstallationId = _T("iid");
    const TCHAR* const kInstallSource = _T("installsource");
    const TCHAR* const kLang = _T("lang");
    const TCHAR* const kMachineId = _T("machineid");
    const TCHAR* const kNeedsAdmin = _T("needsadmin");
    const TCHAR* const kParameter = _T("parameter");
    const TCHAR* const kPlatform = _T("platform");
    const TCHAR* const kPreviousVersion = _T("previousversion");
    const TCHAR* const kProtocol = _T("protocol");
    const TCHAR* const kServicePack = _T("sp");
    const TCHAR* const kSessionId = _T("sessionid");
    const TCHAR* const kSignature = _T("signature");
    const TCHAR* const kSize = _T("size");
    const TCHAR* const kStatus = _T("status");
    const TCHAR* const kSuccessAction = _T("onsuccess");
    const TCHAR* const kSuccessUrl = _T("successurl");
    const TCHAR* const kTag = _T("tag");
    const TCHAR* const kTestSource = _T("testsource");
    const TCHAR* const kTerminateAllBrowsers = _T("terminateallbrowsers");
    const TCHAR* const kUserId = _T("userid");
    const TCHAR* const kVersion = _T("version");
    const TCHAR* const kXmlns = _T("xmlns");
    const TCHAR* const kTTToken = _T("tttoken");
  }  // namespace Attribute.

  namespace Value {
    const TCHAR* const kRequestType = _T("UpdateRequest");
    const TCHAR* const kProtocol = _T("2.0");
    const TCHAR* const kVersion2 = _T("2.0");
    const TCHAR* const kVersion3 = _T("3.0");
    const TCHAR* const kTrue = _T("true");
    const TCHAR* const kFalse = _T("false");
    const TCHAR* const kStatusError = _T("error");
    const TCHAR* const kSuccessActionDefault = _T("default");
    const TCHAR* const kSuccessActionExitSilently = _T("exitsilently");
    const TCHAR* const kWinPlatform = _T("win");

    const TCHAR* const kStatusOk = kResponseStatusOkValue;
  }  //  namespace value.
}  // namespace xml.

HRESULT ReadAttribute(IXMLDOMNode* node,
                      const TCHAR* attr_name,
                      BSTR* value) {
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  // First read the attributes.
  CComPtr<IXMLDOMNamedNodeMap> attributes;
  HRESULT hr = node->get_attributes(&attributes);
  if (FAILED(hr)) { return hr; }
  if (!attributes) { return E_FAIL; }  // Protect against msxml S_FALSE return.

  CComPtr<IXMLDOMNode> attribute_node;
  CComVariant node_value;
  CComBSTR temp_attr_name(attr_name);

  // Get the attribute using a named node.
  hr = attributes->getNamedItem(static_cast<BSTR>(temp_attr_name),
                                &attribute_node);
  if (FAILED(hr)) { return hr; }
  if (!attribute_node) { return E_FAIL; }  // Protect against msxml S_FALSE
                                           // return.

  hr = attribute_node->get_nodeValue(&node_value);
  if (FAILED(hr)) { return hr; }
  if (node_value.vt == VT_EMPTY) { return E_FAIL; }

  // Extract the variant into a BSTR.
  node_value.CopyTo(value);

  return S_OK;
}

HRESULT ReadIntAttribute(IXMLDOMNode* node,
                         const TCHAR* attr_name,
                         int* value) {
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) { return hr; }
  if (!String_StringToDecimalIntChecked(
          static_cast<const TCHAR*>(node_value), value)) {
          return E_FAIL;
  }
  return S_OK;
}

HRESULT ReadGuidAttribute(IXMLDOMNode* node,
                          const TCHAR* attr_name,
                          GUID* value) {
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) { return hr; }
  hr = ::CLSIDFromString(static_cast<TCHAR*>(node_value), value);
  if (FAILED(hr)) { return hr; }

  return S_OK;
}

HRESULT ReadStringAttribute(IXMLDOMNode* node,
                            const TCHAR* attr_name,
                            CString* value) {
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) { return hr; }

  // Will extract the underlying string.
  *value = static_cast<TCHAR*>(node_value);

  return S_OK;
}

HRESULT ParseAppUpdateCheckNode(IXMLDOMNode* node, AppData* request) {
  ASSERT1(node);
  ASSERT1(request);

  // Read the tag value.
  CString str;
  ReadStringAttribute(node, Xml::Attribute::kTag, &str);
  request->set_ap(str);

  return S_OK;
}

HRESULT ParseAppPingNode(IXMLDOMNode* node, AppRequestData* request) {
  ASSERT1(node);
  ASSERT1(request);

  // Read the event type.
  int event_type = 0;
  HRESULT hr = ReadIntAttribute(node, Xml::Attribute::kEventType, &event_type);
  if (FAILED(hr)) { return hr; }

  // Read the event result.
  int event_result = 0;
  hr = ReadIntAttribute(node, Xml::Attribute::kEventResult, &event_result);
  if (FAILED(hr)) { return hr; }

  // Read the errorcode.
  int error_code = 0;
  hr = ReadIntAttribute(node, Xml::Attribute::kErrorCode, &error_code);
  if (FAILED(hr)) { return hr; }

  // Read the extracode1.
  int extra_code = 0;
  hr = ReadIntAttribute(node, Xml::Attribute::kExtraCode1, &extra_code);
  if (FAILED(hr)) { return hr; }

  PingEvent ping_event(static_cast<PingEvent::Types>(event_type),
                       static_cast<PingEvent::Results>(event_result),
                       error_code,
                       extra_code,
                       CString());
  request->AddPingEvent(ping_event);

  return S_OK;
}

HRESULT ParseAppNode(IXMLDOMNode* node, AppRequestDataVector* request) {
  ASSERT1(node);
  ASSERT1(request);

  AppRequest app_request;
  AppData app_data;
  AppRequestData app_request_data;

  // Read the app guid.
  GUID guid = {0};
  HRESULT hr = ReadGuidAttribute(node, Xml::Attribute::kAppId, &guid);
  if (FAILED(hr)) { return hr; }
  app_data.set_app_guid(guid);

  // Read the app version.
  CString str;
  hr = ReadStringAttribute(node, Xml::Attribute::kVersion, &str);
  if (FAILED(hr)) { return hr; }
  app_data.set_version(str);

  // Read the app language.
  hr = ReadStringAttribute(node, Xml::Attribute::kLang, &str);
  if (FAILED(hr)) { return hr; }
  app_data.set_language(str);

  CComPtr<IXMLDOMNodeList> child_nodes;
  // Get all the children of the Node.
  hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) { return hr; }
  if (!child_nodes) { return E_FAIL; }  // Protect against msxml S_FALSE return.

  // Go Over all the children and read each of them. we will ignore ones that
  // we dont understand.
  hr = child_nodes->reset();
  if (FAILED(hr)) { return hr; }

  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName child_node_name;
    hr = GetXMLFQName(child_node, &child_node_name);
    if (FAILED(hr)) { return hr; }

    if (child_node_name.base == Xml::Element::kUpdateCheck) {
      // Read in the update check request.
      hr = ParseAppUpdateCheckNode(child_node, &app_data);
      if (FAILED(hr)) { return hr; }
    } else if (child_node_name.base == Xml::Element::kEvent) {
      // Read in the ping request.
      hr = ParseAppPingNode(child_node, &app_request_data);
    }

    child_node = NULL;
  }

  app_request_data.set_app_data(app_data);
  request->push_back(app_request_data);
  return S_OK;
}

HRESULT ParseGupdateNode(IXMLDOMNode* node, AppRequestDataVector* request) {
  ASSERT1(node);
  ASSERT1(request);

  CComPtr<IXMLDOMNodeList> child_nodes;
  // Get all the children of the Node.
  HRESULT hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) { return hr; }
  if (!child_nodes) { return E_FAIL; }  // Protect against msxml S_FALSE return.

  // Go Over all the children and read each of them. we will ignore ones that
  // we dont understand.
  hr = child_nodes->reset();
  if (FAILED(hr)) { return hr; }

  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName child_node_name;
    hr = GetXMLFQName(child_node, &child_node_name);
    if (FAILED(hr)) { return hr; }

    if (child_node_name.base == Xml::Element::kApp) {
      // we got a response we should read that in.
      hr = ParseAppNode(child_node, request);
      if (FAILED(hr)) { return hr; }
    }
    child_node = NULL;
  }
  return S_OK;
}

HRESULT ParseUpdateCheck(const CString& request_str,
                         AppRequestDataVector* request) {
  ASSERT1(request);

  CComPtr<IXMLDOMDocument> document;
  HRESULT hr = LoadXMLFromMemory(request_str, false, &document);
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IXMLDOMElement> document_element;
  hr = document->get_documentElement(&document_element);
  if (FAILED(hr)) {
    return hr;
  }
  if (!document_element) {  // Protect against msxml S_FALSE return.
    return E_FAIL;
  }

  return ParseGupdateNode(document_element, request);
}

HRESULT BuildUpdateResponse(const ServerResponses& responses,
                            CString* response) {
  ASSERT1(response);

  CString response_str(kResponseXmlHeader);
  response_str.Append(kResponseXmlGupdateHeader);

  for (size_t i = 0; i < responses.size(); ++i) {
    CString guid = responses[i].guid;

    if (responses[i].is_ping) {
      // If the response is to a ping event, then we just add the ok to the
      // response. The reason this works is because we either have a ping
      // request or a update check, never both. If this changes, then we need
      // to change this code.
      response_str.AppendFormat(kResponseAppEvent, guid);
    } else if (responses[i].is_update_response) {
      // respond with the value of the updates.
      UpdateResponseData data = responses[i].response_data;
      response_str.AppendFormat(kResponseAppUpdate,
                                guid,
                                data.url(),
                                data.hash(),
                                data.needs_admin() ? _T("true") : _T("false"),
                                data.size());
    } else {
      // respond with a no-update.
      response_str.AppendFormat(kResponseAppNoUpdate, guid);
    }
  }

  response_str.Append(kResponseGupdateEndTag);
  *response = response_str;
  return S_OK;
}

}  // namespace omaha
