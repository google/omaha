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
// goopdate_xml_parser for parsing the xml manifest files that are compiled
// into the meta-installer and sent by the auto-update server.

#include "omaha/goopdate/goopdate_xml_parser.h"

#include <stdlib.h>
#include <msxml2.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/error.h"
#include "omaha/common/string.h"
#include "omaha/common/utils.h"
#include "omaha/common/xml_utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/goopdate_metrics.h"
#include "omaha/goopdate/goopdate_utils.h"

namespace omaha {

namespace {

// Loads the specified document and obtains the DOM and Element interfaces.
HRESULT GetDocumentDomAndElement(const CString& file_name,
                                 IXMLDOMDocument** document,
                                 IXMLDOMElement** document_element) {
  CORE_LOG(L3, (_T("[GetDocumentDomAndElement]")));
  ASSERT1(document);
  ASSERT1(document_element);

  HRESULT hr = LoadXMLFromFile(file_name, true, document);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[LoadXMLFromFile failed][0x%x]"), hr));
    return hr;
  }

  hr = (*document)->get_documentElement(document_element);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_documentElement failed][0x%x]"), hr));
    return hr;
  }
  if (!*document_element) {  // Protect against msxml S_FALSE return.
    CORE_LOG(LE, (_T("[*document_element is NULL]")));
    return E_FAIL;
  }

  return S_OK;
}

// Loads the specified document and obtains the DOM interface.
HRESULT GetDocumentElement(const CString& file_name,
                           IXMLDOMElement** document_element) {
  CComPtr<IXMLDOMDocument> document;
  return GetDocumentDomAndElement(file_name, &document, document_element);
}

}  // namespace


const int kGuidLen = 38;
CString GoopdateXmlParser::seed_protocol_version;

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
  }
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
    const TCHAR* const kData = _T("data");

    const TCHAR* const kInstall = _T("install");
    const TCHAR* const kResponse = _T("response");
    const TCHAR* const kNameValue = _T("attr");
  }
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
    const TCHAR* const kErrorUrl = _T("errorurl");
    const TCHAR* const kExtraCode1 = _T("extracode1");
    const TCHAR* const kHash = _T("hash");
    const TCHAR* const kIndex = _T("index");
    const TCHAR* const kInstalledAgeDays = _T("installage");
    const TCHAR* const kIsMachine = _T("ismachine");
    const TCHAR* const kInstallationId = _T("iid");
    const TCHAR* const kInstallSource = _T("installsource");
    const TCHAR* const kLang = _T("lang");
    const TCHAR* const kMachineId = _T("machineid");
    const TCHAR* const kName = _T("name");
    const TCHAR* const kNeedsAdmin = _T("needsadmin");
    const TCHAR* const kParameter = _T("parameter");
    const TCHAR* const kPeriodOverrideSec = _T("periodoverridesec");
    const TCHAR* const kPlatform = _T("platform");
    const TCHAR* const kPreviousVersion = _T("previousversion");
    const TCHAR* const kProtocol = _T("protocol");
    const TCHAR* const kRequestId  = _T("requestid");
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
    const TCHAR* const kTTToken = _T("tttoken");
    const TCHAR* const kUpdateDisabled = _T("updatedisabled");
    const TCHAR* const kUserId = _T("userid");
    const TCHAR* const kVersion = _T("version");
    const TCHAR* const kXmlns = _T("xmlns");
  }

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
    const TCHAR* const kSuccessActionExitSilentlyOnLaunchCmd =
        _T("exitsilentlyonlaunchcmd");

    const TCHAR* const kStatusOk = kResponseStatusOkValue;
    const TCHAR* const kInstallData = _T("install");
  }
}

SuccessfulInstallAction GoopdateXmlParser::ConvertStringToSuccessAction(
    const CString& text) {
  if (text.IsEmpty() ||
      _tcsicmp(text, Xml::Value::kSuccessActionDefault) == 0) {
    return SUCCESS_ACTION_DEFAULT;
  } else if (_tcsicmp(text, Xml::Value::kSuccessActionExitSilently) == 0) {
    return SUCCESS_ACTION_EXIT_SILENTLY;
  } else if (_tcsicmp(text,
                      Xml::Value::kSuccessActionExitSilentlyOnLaunchCmd) == 0) {
    return SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD;
  } else {
    ASSERT(false, (_T("[Unrecognized success action][%s]"), text));
    // Use the default action. This allows Omaha to be forward-compatible with
    // new SuccessActions, meaning older versions will not fail if a config
    // uses a new action.
    return SUCCESS_ACTION_DEFAULT;
  }
}

// Implementation of the GoopdateXmlParser.
HRESULT GoopdateXmlParser::ReadBooleanAttribute(IXMLDOMNode* node,
                                                const TCHAR* attr_name,
                                                bool* value) {
  CORE_LOG(L3, (_T("[ReadBooleanAttribute][%s]"), attr_name));
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadAttribute failed][%s][0x%x]"), attr_name, hr));
    return hr;
  }

  hr = String_StringToBool(static_cast<TCHAR*>(node_value),
                           value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[String_StringToBool failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadIntAttribute(IXMLDOMNode* node,
                                            const TCHAR* attr_name,
                                            int* value) {
  CORE_LOG(L3, (_T("[ReadIntAttribute][%s]"), attr_name));
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadAttribute failed][%s][0x%x]"), attr_name, hr));
    return hr;
  }

  if (!String_StringToDecimalIntChecked(
          static_cast<const TCHAR*>(node_value), value)) {
          return GOOPDATEXML_E_STRTOUINT;
  }
  return S_OK;
}

HRESULT GoopdateXmlParser::ReadGuidAttribute(IXMLDOMNode* node,
                                             const TCHAR* attr_name,
                                             GUID* value) {
  CORE_LOG(L3, (_T("[ReadGuidAttribute][%s]"), attr_name));
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadAttribute failed][%s][0x%x]"), attr_name, hr));
    return hr;
  }

  hr = ::CLSIDFromString(static_cast<TCHAR*>(node_value), value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CLSIDFromString failed][0x%x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadStringAttribute(IXMLDOMNode* node,
                                               const TCHAR* attr_name,
                                               CString* value) {
  CORE_LOG(L3, (_T("[ReadStringAttribute][%s]"), attr_name));
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  CComBSTR node_value;
  HRESULT hr = ReadAttribute(node, attr_name, &node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadAttribute failed][%s][0x%x]"), attr_name, hr));
    return hr;
  }

  // Will extract the underlying string.
  *value = static_cast<TCHAR*>(node_value);

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadAttribute(IXMLDOMNode* node,
                                         const TCHAR* attr_name,
                                         BSTR* value) {
  CORE_LOG(L4, (_T("[ReadAttribute][%s]"), attr_name));
  ASSERT1(node != NULL);
  ASSERT1(attr_name != NULL);
  ASSERT1(value != NULL);

  // First read the attributes.
  CComPtr<IXMLDOMNamedNodeMap> attributes;
  HRESULT hr = node->get_attributes(&attributes);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_attributes failed][0x%x]"), hr));
    return hr;
  }

  if (!attributes) {
    CORE_LOG(LE, (_T("[Msxml S_FALSE return.]")));
    return E_FAIL;  // Protect against msxml S_FALSE return.
  }

  CComPtr<IXMLDOMNode> attribute_node;
  CComVariant node_value;
  CComBSTR temp_attr_name(attr_name);

  // Get the attribute using a named node.
  hr = attributes->getNamedItem(static_cast<BSTR>(temp_attr_name),
                                &attribute_node);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[getNamedItem failed][0x%x]"), hr));
    return hr;
  }

  if (!attribute_node) {
    CORE_LOG(LE, (_T("[Msxml S_FALSE return.]")));
    return E_FAIL;  // Protect against msxml S_FALSE return.
  }

  hr = attribute_node->get_nodeValue(&node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_nodeValue failed][0x%x]"), hr));
    return hr;
  }

  if (node_value.vt == VT_EMPTY) {
    CORE_LOG(LE, (_T("[node_value.vt == VT_EMPTY]")));
    return E_FAIL;
  }

  // Extract the variant into a BSTR.
  node_value.CopyTo(value);

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadStringValue(IXMLDOMNode* node,
                                           CString* value) {
  CORE_LOG(L4, (_T("[ReadStringValue]")));
  ASSERT1(node != NULL);
  ASSERT1(value != NULL);

  CComPtr<IXMLDOMNodeList> child_nodes;
  HRESULT hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_childNodes failed][0x%x]"), hr));
    return hr;
  }
  if (!child_nodes) {
    CORE_LOG(LE, (_T("[Msxml S_FALSE return.]")));
    return E_FAIL;  // Protect against msxml S_FALSE return.
  }

  long count = 0;  // NOLINT
  hr = child_nodes->get_length(&count);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT(count == 1, (_T("count: %u"), count));
  CComPtr<IXMLDOMNode> child_node;
  hr = child_nodes->nextNode(&child_node);
  if (FAILED(hr)) {
    return hr;
  }

  DOMNodeType type = NODE_INVALID;
  hr = child_node->get_nodeType(&type);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_nodeType failed][0x%x]"), hr));
    return hr;
  }

  if (type != NODE_TEXT) {
    CORE_LOG(LE, (_T("[Invalid nodeType][%d]"), type));
    return E_INVALIDARG;
  }

  CComVariant node_value;
  hr = child_node->get_nodeValue(&node_value);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_nodeValue failed][0x%x]"), hr));
    return hr;
  }

  if (node_value.vt != VT_BSTR) {
    CORE_LOG(LE, (_T("[node_value.vt != VT_BSTR][%d]"), node_value.vt));
    return E_INVALIDARG;
  }

  *value = V_BSTR(&node_value);
  return S_OK;
}

HRESULT GoopdateXmlParser::ReadUpdateResponse(IXMLDOMNode* node,
                                              UpdateResponse* response) {
  CORE_LOG(L4, (_T("[ReadUpdateResponse]")));
  ASSERT1(node != NULL);
  ASSERT1(response != NULL);

  UpdateResponseData response_data;

  // Read GUID first since we need the GUID even on errors in order
  // to remove the corresponding request from the jobs list.
  GUID guid = {0};
  HRESULT hr = ReadGuidAttribute(node, Xml::Attribute::kAppId, &guid);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadGuidAttribute failed][0x%x]"), hr));
    return hr;
  }
  response_data.set_guid(guid);

  CString str;
  // Any response status but "ok" for the "app" element stops the "parsing".
  hr = ReadStringAttribute(node, Xml::Attribute::kStatus, &str);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[ReadStringAttribute failed][0x%x]"), hr));
    return hr;
  }
  if (str != Xml::Value::kStatusOk) {
    response_data.set_status(str);
    response->set_update_response_data(response_data);
    return S_OK;
  }

  // Now try and read the children nodes of the response.
  CComPtr<IXMLDOMNodeList> child_nodes;
  // Get all the children of the document.
  hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[get_childNodes failed][0x%x]"), hr));
    return hr;
  }
  if (!child_nodes) {
    CORE_LOG(LE, (_T("[Msxml S_FALSE return.]")));
    return E_FAIL;  // Protect against msxml S_FALSE return.
  }

  // Go over all the children and handle the children. We ignore all the
  // children that we do not understand. Note that we expect the nodes that
  // we want to be present. Although we are not enforcing this for now.
  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName node_name;
    hr = GetXMLFQName(child_node, &node_name);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[GetXMLFQName failed][0x%x]"), hr));
      return hr;
    }

    if (node_name.base == Xml::Element::kUpdateCheck) {
      hr = ReadStringAttribute(child_node, Xml::Attribute::kTTToken, &str);
      if (SUCCEEDED(hr) && !str.IsEmpty()) {
        response_data.set_tt_token(str);
      }

      hr = ReadStringAttribute(child_node, Xml::Attribute::kStatus, &str);
      if (FAILED(hr)) { return hr; }
      response_data.set_status(str);

      if (str == Xml::Value::kStatusOk) {
        int size = 0;
        hr = ReadIntAttribute(child_node, Xml::Attribute::kSize, &size);
        if (FAILED(hr)) { return hr; }
        if (size < 0) {
          return GOOPDATEXML_E_STRTOUINT;
        }
        response_data.set_size(size);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kHash, &str);
        if (FAILED(hr)) { return hr; }
        response_data.set_hash(str);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kCodebase, &str);
        if (FAILED(hr)) { return hr; }
        response_data.set_url(str);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kNeedsAdmin, &str);
        if (FAILED(hr)) { return hr; }
        NeedsAdmin needs_admin;
        hr = goopdate_utils::ConvertStringToNeedsAdmin(str, &needs_admin);
        if (FAILED(hr)) { return hr; }
        response_data.set_needs_admin(needs_admin);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kArguments, &str);
        // arguments is optional
        if (SUCCEEDED(hr)) {
          response_data.set_arguments(str);
        }

        hr = ReadStringAttribute(child_node, Xml::Attribute::kSuccessUrl, &str);
        if (SUCCEEDED(hr)) {
          response_data.set_success_url(str);
        }

        bool terminate_all_browsers = false;
        hr = ReadBooleanAttribute(child_node,
                                  Xml::Attribute::kTerminateAllBrowsers,
                                  &terminate_all_browsers);
        if (SUCCEEDED(hr)) {
          response_data.set_terminate_all_browsers(terminate_all_browsers);
        }

        hr = ReadStringAttribute(child_node,
                                 Xml::Attribute::kSuccessAction,
                                 &str);
        if (SUCCEEDED(hr)) {
          response_data.set_success_action(ConvertStringToSuccessAction(str));
        }

        // If no version exists in the server response, we will still indicate
        // when an update is available. However, we will not provide version
        // information to any JobObserver.
        hr = ReadStringAttribute(child_node,
                                 Xml::Attribute::kVersion,
                                 &str);
        if (SUCCEEDED(hr)) {
          response_data.set_version(str);
        }
      }

      // Always look for the error URL because it is used in errors.
      hr = ReadStringAttribute(child_node, Xml::Attribute::kErrorUrl, &str);
      if (SUCCEEDED(hr)) {
        response_data.set_error_url(str);
      }
    } else if (node_name.base == Xml::Element::kData) {
      hr = ReadStringAttribute(child_node, Xml::Attribute::kStatus, &str);
      if (FAILED(hr)) {
        return hr;
      }

      if (str == Xml::Value::kStatusOk) {
        // <data name="install" index="foo" status="ok">foo bar</data>.
        hr = ReadStringAttribute(child_node, Xml::Attribute::kName, &str);
        if (FAILED(hr)) {
          return hr;
        }
        if (str.CompareNoCase(Xml::Value::kInstallData)) {
          CORE_LOG(LW, (_T("[Skipping unsupported data][%s]"), str));
          continue;
        }

        CString install_data_index;
        hr = ReadStringAttribute(child_node,
                                 Xml::Attribute::kIndex,
                                 &install_data_index);
        if (FAILED(hr)) {
          return hr;
        }

        CString install_data;
        hr = ReadStringValue(child_node, &install_data);
        if (FAILED(hr)) {
          return hr;
        }
        response_data.SetInstallData(install_data_index, install_data);
      }
    } else if (node_name.base == Xml::Element::kPing) {
      // nothing to do here (yet)
    } else if (node_name.base == Xml::Element::kEvent) {
      // nothing to do here (yet)
    } else if (node_name.base == Xml::Element::kComponents) {
      VERIFY1(SUCCEEDED(ReadComponentsResponses(child_node, response)));
    }
    child_node = NULL;
  }

  response->set_update_response_data(response_data);

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadComponentsResponses(IXMLDOMNode* node,
                                                   UpdateResponse* response) {
  CORE_LOG(L4, (_T("[ReadComponentsResponses]")));
  ASSERT1(node);
  ASSERT1(response);

  CComPtr<IXMLDOMNodeList> child_nodes;
  HRESULT hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) { return hr; }
  // If S_FALSE, then there are no children components.
  if (S_FALSE == hr) { return S_OK; }

  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName node_name;
    hr = GetXMLFQName(child_node, &node_name);
    if (FAILED(hr)) { return hr; }

    if (node_name.base == Xml::Element::kComponent) {
      UpdateResponseData component_response_data;
      hr = ReadComponentResponseData(child_node, &component_response_data);
      if (FAILED(hr)) { return hr; }
      response->AddComponentResponseData(component_response_data);
    } else {
      ASSERT1(false);
    }

    child_node = NULL;
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::ReadComponentResponseData(
    IXMLDOMNode* node,
    UpdateResponseData* response_data) {
  CORE_LOG(L4, (_T("[ReadComponentsResponseData]")));
  // TODO(omaha): consolidate component/product parsing. They have several
  // similar attributes.
  // Read GUID first since we need the GUID even on errors in order to remove
  // the corresponding request from the jobs list.
  GUID guid = {0};
  HRESULT hr = ReadGuidAttribute(node, Xml::Attribute::kAppId, &guid);
  if (FAILED(hr)) { return hr; }
  response_data->set_guid(guid);

  // Any response status but "ok" for the "app" element stops the "parsing".
  CString str;
  hr = ReadStringAttribute(node, Xml::Attribute::kStatus, &str);
  if (FAILED(hr)) { return hr; }
  if (str != Xml::Value::kStatusOk) {
    response_data->set_status(str);
    return S_OK;
  }

  // Now try and read the children nodes of the response.
  CComPtr<IXMLDOMNodeList> child_nodes;
  // Get all the children of the document.
  hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) { return hr; }
  if (!child_nodes) { return E_FAIL; }  // Protect against msxml S_FALSE return.

  // Go over all the children and handle the children. We ignore all the
  // children that we do not understand. Note that we expect the nodes that
  // we want to be present. Although we are not enforcing this for now.
  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName node_name;
    hr = GetXMLFQName(child_node, &node_name);
    if (FAILED(hr)) { return hr; }

    if (node_name.base == Xml::Element::kUpdateCheck) {
      hr = ReadStringAttribute(child_node, Xml::Attribute::kStatus, &str);
      if (FAILED(hr)) { return hr; }
      response_data->set_status(str);
      // TODO(omaha):  Check why we have kStatusOk at both levels of the XML
      // (same for product parsing) and consolidate/remove as necessary.
      if (str == Xml::Value::kStatusOk) {
        int size = 0;
        hr = ReadIntAttribute(child_node, Xml::Attribute::kSize, &size);
        if (FAILED(hr)) { return hr; }
        if (size < 0) {
          return GOOPDATEXML_E_STRTOUINT;
        }
        response_data->set_size(size);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kHash, &str);
        if (FAILED(hr)) { return hr; }
        response_data->set_hash(str);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kCodebase, &str);
        if (FAILED(hr)) { return hr; }
        response_data->set_url(str);

        hr = ReadStringAttribute(child_node, Xml::Attribute::kArguments, &str);
        // arguments is optional
        if (SUCCEEDED(hr)) {
          response_data->set_arguments(str);
        }
      }
    } else {
      ASSERT1(false);
    }

    child_node = NULL;
  }

  return S_OK;
}

// This exists only to support handoffs from 1.0.x and 1.1.x metainstallers.
HRESULT GoopdateXmlParser::ReadInstallElement(
    IXMLDOMNode* node,
    UpdateResponseData* response_data) {
  ASSERT1(node);
  ASSERT1(response_data);

  HRESULT hr = ReadRequiredInstallAttributes(node, response_data);
  if (FAILED(hr)) {
    return hr;
  }

  return ReadOptionalInstallAttributes(node, response_data);
}

// This exists only to support handoffs from 1.0.x and 1.1.x metainstallers.
HRESULT GoopdateXmlParser::ReadRequiredInstallAttributes(
    IXMLDOMNode* node,
    UpdateResponseData* response_data) {
  ASSERT1(node);
  ASSERT1(response_data);

  // Read the guid and whether the application needs admin. Both the attributes
  // are required.
  GUID guid_value = {0};
  HRESULT hr = ReadGuidAttribute(node, Xml::Attribute::kAppGuid,
                                 &guid_value);
  if (FAILED(hr)) { return hr; }

  response_data->set_guid(guid_value);

  CString needs_admin_str;
  hr = ReadStringAttribute(node, Xml::Attribute::kNeedsAdmin,
                           &needs_admin_str);
  if (FAILED(hr)) { return hr; }

  NeedsAdmin needs_admin;
  hr = goopdate_utils::ConvertStringToNeedsAdmin(needs_admin_str, &needs_admin);
  if (FAILED(hr)) { return hr; }

  response_data->set_needs_admin(needs_admin);

  // We only read the language and the application name from the seed manifest
  // if the version of the seed manifest is greater than 2.0.
  if (String_StringToDouble(seed_protocol_version) >
      String_StringToDouble(Xml::Value::kVersion2)) {
    ++metric_handoff_legacy_11;

    CString app_name;
    hr = ReadStringAttribute(node, Xml::Attribute::kApplicationName, &app_name);
    if (FAILED(hr)) { return hr; }
    response_data->set_app_name(app_name);

    CString language;
    hr = ReadStringAttribute(node, Xml::Attribute::kLang, &language);
    if (FAILED(hr)) { return hr; }
    response_data->set_language(language);
  } else {
    ++metric_handoff_legacy_10;
  }

  return S_OK;
}

// Since all of these attributes are optional, read failures do not cause this
// method to fail.
// This exists only to support handoffs from 1.0.x and 1.1.x metainstallers.
HRESULT GoopdateXmlParser::ReadOptionalInstallAttributes(
    IXMLDOMNode* node,
    UpdateResponseData* response_data) {
  ASSERT1(node);
  ASSERT1(response_data);

  CString iid;
  HRESULT hr = ReadStringAttribute(node, Xml::Attribute::kInstallationId, &iid);
  if (SUCCEEDED(hr)) {
    response_data->set_installation_id(StringToGuid(iid));
  }

  CString ap;
  hr = ReadStringAttribute(node, Xml::Attribute::kAdditionalParameter, &ap);
  if (SUCCEEDED(hr)) {
    response_data->set_ap(ap);
  }

  CString browser_type;
  hr = ReadStringAttribute(node, Xml::Attribute::kBrowserType, &browser_type);
  if (SUCCEEDED(hr)) {
    BrowserType type = BROWSER_UNKNOWN;
    hr = goopdate_utils::ConvertStringToBrowserType(browser_type, &type);
    if (SUCCEEDED(hr)) {
      response_data->set_browser_type(type);
    }
  }

  return S_OK;
}

// Assumes all higher level validity checks have been done
HRESULT GoopdateXmlParser::ReadUpdateResponses(
    IXMLDOMNode* node,
    UpdateResponses* responses) {
  ASSERT1(node != NULL);
  ASSERT1(responses != NULL);

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
      UpdateResponse response;
      hr = ReadUpdateResponse(child_node, &response);
      if (FAILED(hr)) { return hr; }

      const GUID& response_guid = response.update_response_data().guid();
      ASSERT1(responses->find(response_guid) == responses->end());
      (*responses)[response_guid] = response;
    }
    child_node = NULL;
  }
  return S_OK;
}

// This exists only to support handoffs from 1.0.x and 1.1.x metainstallers.
HRESULT GoopdateXmlParser::ReadSeedInstalls(IXMLDOMNode* node,
                                            UpdateResponses* responses) {
  ASSERT1(node != NULL);
  ASSERT1(responses != NULL);

  CComPtr<IXMLDOMNodeList> child_nodes;
  // Get all the children of the Node.
  HRESULT hr = node->get_childNodes(&child_nodes);
  if (FAILED(hr)) { return hr; }
  if (!child_nodes) { return E_FAIL; }  // Protect against msxml S_FALSE return.

  // Go Over all the children and read each of them. Since seed files are
  // always of a version <= to this code, error if we run into a child we don't
  // recognize.
  hr = child_nodes->reset();
  if (FAILED(hr)) { return hr; }

  CComPtr<IXMLDOMNode> child_node;
  while (child_nodes->nextNode(&child_node) != S_FALSE) {
    XMLFQName child_node_name;
    hr = GetXMLFQName(child_node, &child_node_name);
    if (FAILED(hr)) { return hr; }

    if (child_node_name.base == Xml::Element::kInstall) {
      // We found an install node. We should read that in.
      UpdateResponseData response_data;
      hr = ReadInstallElement(child_node, &response_data);
      if (FAILED(hr)) { return hr; }

      UpdateResponse response(response_data);
      const GUID& response_guid =  response.update_response_data().guid();
      ASSERT1(responses->find(response_guid) == responses->end());
      (*responses)[response_guid] = response;

      child_node = NULL;
    } else {
      child_node = NULL;
      return E_FAIL;
    }
  }
  return S_OK;
}

HRESULT GoopdateXmlParser::GetProtocolVersion(IXMLDOMElement* document_element,
                                              CString* version) {
  ASSERT1(document_element);
  ASSERT1(version);

  CString protocol_version;
  return ReadStringAttribute(document_element,
                             Xml::Attribute::kProtocol,
                             version);
}

HRESULT GoopdateXmlParser::VerifyElementProtocolCompatibility(
    IXMLDOMElement* document_element,
    const CString& expected_version) {
  ASSERT1(document_element);

  CString protocol_version;
  HRESULT hr = GetProtocolVersion(document_element, &protocol_version);
  if (FAILED(hr)) {
    return hr;
  }

  return VerifyProtocolCompatibility(protocol_version, expected_version);
}

// Returns GOOPDATEXML_E_XMLVERSION when the actual_ver is not between
// the start_ver and the end_ver, both inclusive.
HRESULT GoopdateXmlParser::VerifyProtocolRange(const CString& actual_ver,
                                               const CString& start_ver,
                                               const CString& end_ver) {
  const double version = String_StringToDouble(actual_ver);
  const double start = String_StringToDouble(start_ver);
  const double end = String_StringToDouble(end_ver);
  if (version < start || version > end) {
    return GOOPDATEXML_E_XMLVERSION;
  }

  return S_OK;
}

// Verify that the protocol version is one we understand.  We accept
// all version numbers where major version is the same as kExpectedVersion
// which are greater than or equal to kExpectedVersion. In other words,
// we handle future minor version number increases which should be
// compatible.
HRESULT GoopdateXmlParser::VerifyProtocolCompatibility(
    const CString& actual_version,
    const CString& expected_version) {
  if (_tcscmp(actual_version, expected_version) != 0) {
    const double version = String_StringToDouble(actual_version);
    const double expected = String_StringToDouble(expected_version);
    if (expected > version) {
      return GOOPDATEXML_E_XMLVERSION;
    }
    const int version_major = static_cast<int>(version);
    const int expected_major = static_cast<int>(expected);
    if (version_major != expected_major) {
      return GOOPDATEXML_E_XMLVERSION;
    }
  }

  return S_OK;
}

// Currently, this method only validates the name and protocol version.
HRESULT GoopdateXmlParser::ValidateResponseElement(
    IXMLDOMElement* document_element) {
  CComBSTR root_name;
  HRESULT hr = document_element->get_baseName(&root_name);
  if (FAILED(hr)) { return hr; }
  XMLFQName doc_element_name(Xml::Namespace::kResponse, root_name);
  if (doc_element_name.base != Xml::Element::kResponses) {
    return GOOPDATEXML_E_RESPONSESNODE;
  }

  return VerifyElementProtocolCompatibility(document_element,
                                            Xml::Value::kVersion2);
}

HRESULT GoopdateXmlParser::ParseManifestFile(const CString& file_name,
                                             UpdateResponses* responses) {
  if (responses == NULL) {
    return E_INVALIDARG;
  }

  CComPtr<IXMLDOMElement> document_element;
  HRESULT hr = GetDocumentElement(file_name, &document_element);
  if (FAILED(hr)) { return hr; }

  return ParseManifest(document_element, responses);
}

HRESULT GoopdateXmlParser::ParseManifestString(
    const TCHAR* manifest,
    UpdateResponses* responses) {
  ASSERT1(manifest);
  ASSERT1(responses);
  CComPtr<IXMLDOMDocument> document;
  HRESULT hr = LoadXMLFromMemory(manifest, false, &document);
  if (FAILED(hr)) { return hr; }

  CComPtr<IXMLDOMElement> document_element;
  hr = document->get_documentElement(&document_element);
  if (FAILED(hr)) { return hr; }
  if (!document_element) {  // Protect against msxml S_FALSE return.
    return E_FAIL;
  }

  return ParseManifest(document_element, responses);
}

HRESULT GoopdateXmlParser::ParseManifest(IXMLDOMElement* manifest,
                                         UpdateResponses* responses) {
  ASSERT1(manifest);
  ASSERT1(responses);

  CComBSTR uri;
  HRESULT hr = manifest->get_namespaceURI(&uri);
  if (FAILED(hr)) { return hr; }

  if (uri == Xml::Namespace::kResponse) {
    hr = ValidateResponseElement(manifest);
    if (FAILED(hr)) { return hr; }

    hr = ReadUpdateResponses(manifest, responses);
    if (FAILED(hr)) { return hr; }
  } else if (uri == Xml::Namespace::kSeed) {
    // TODO(omaha): We should probably verify the name too.
    hr = GetProtocolVersion(manifest, &seed_protocol_version);
    if (FAILED(hr)) { return hr; }

    hr = VerifyProtocolRange(seed_protocol_version,
                             Xml::Value::kVersion2,
                             Xml::Value::kVersion3);
    if (FAILED(hr)) { return hr; }

    hr = ReadSeedInstalls(manifest, responses);
    if (FAILED(hr)) { return hr; }
  } else {
    return GOOPDATEXML_E_UNEXPECTED_URI;
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::CreateRequestElement(
    const TCHAR* xml_element_name,
    const CString& value,
    IXMLDOMDocument* document,
    IXMLDOMNode** request_element) {

  ASSERT1(xml_element_name != NULL);
  ASSERT1(document != NULL);
  ASSERT1(request_element != NULL);

  // request element names get o: prepended to avoid a size explosion where
  // the namespace attribute gets automatically added to every element by
  // msxml
  CString name;
  name.Format(_T("o:%s"), xml_element_name);

  HRESULT hr = CreateXMLNode(document,
                             NODE_ELEMENT,
                             name,
                             Xml::Namespace::kRequest,
                             value,
                             request_element);
  return hr;
}

HRESULT GoopdateXmlParser::CreateAndAddRequestElement(
    const TCHAR* xml_element_name,
    const CString& value,
    IXMLDOMDocument* document,
    IXMLDOMNode* parent) {

  ASSERT1(xml_element_name != NULL);
  ASSERT1(document != NULL);
  ASSERT1(parent != NULL);

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateRequestElement(xml_element_name,
                                    value,
                                    document,
                                    &element);
  if (FAILED(hr)) { return hr; }

  hr = parent->appendChild(element, NULL);
  if (FAILED(hr)) { return hr; }

  return S_OK;
}

HRESULT GoopdateXmlParser::AddAttributesToNode(
    const CString& namespace_uri,
    const std::vector<XMLNameValuePair>& attributes,
    IXMLDOMNode* element) {
  ASSERT1(element != NULL);
  HRESULT hr;
  for (size_t i = 0; i < attributes.size(); ++i) {
    hr = AddXMLAttributeNode(element,
                             namespace_uri,
                             attributes[i].first,
                             attributes[i].second);
    if (FAILED(hr)) { return hr; }
  }
  return S_OK;
}

HRESULT GoopdateXmlParser::CreateAppRequestElementHelper(
    const AppRequest& app_request,
    IXMLDOMDocument* document,
    IXMLDOMNode** app_element) {
  ASSERT1(app_element);
  ASSERT1(document);

  const AppData& app_data = app_request.request_data().app_data();

  // Create the request node.
  HRESULT hr = CreateRequestElement(Xml::Element::kApp,
                                    _T(""),
                                    document,
                                    app_element);
  if (FAILED(hr)) { return hr; }

  // Add the appid to the app node.
  // appid=""       // The application Id.
  const int guid_len = kGuidLen;
  TCHAR guid_str[guid_len + 1] = { _T('\0') };
  if (StringFromGUID2(app_data.app_guid(), guid_str, guid_len + 1) <= 0) {
    return E_FAIL;
  }
  if (FAILED(hr)) { return hr; }

  // Saves about 600 bytes of code size by creating and destroying the
  // array inside of the statement block.
  {
    // Create the list of attributes and the values that need to be added.
    const XMLNameValuePair elements[] = {
        std::make_pair(Xml::Attribute::kAppId, guid_str),
        std::make_pair(Xml::Attribute::kVersion, app_data.version()),
        std::make_pair(Xml::Attribute::kLang, app_data.language()),
        std::make_pair(Xml::Attribute::kBrandCode, app_data.brand_code()),
        std::make_pair(Xml::Attribute::kClientId, app_data.client_id())
    };

    for (size_t i = 0; i < arraysize(elements); ++i) {
      hr = AddXMLAttributeNode(*app_element,
                               Xml::Namespace::kRequest,
                               elements[i].first,
                               elements[i].second);
      if (FAILED(hr)) { return hr; }
    }
  }

  if (0 != app_data.install_time_diff_sec()) {
    const int kSecondsPerDay = 24 * kSecondsPerHour;
    int installed_full_days = app_data.install_time_diff_sec() / kSecondsPerDay;
    hr = AddXMLAttributeNode(*app_element,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kInstalledAgeDays,
                             itostr(installed_full_days));
    if (FAILED(hr)) { return hr; }
  }

  if (GUID_NULL != app_data.iid()) {
    hr = AddXMLAttributeNode(*app_element,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kInstallationId,
                             GuidToString(app_data.iid()));
    if (FAILED(hr)) { return hr; }
  }

  if (!app_data.install_source().IsEmpty()) {
    hr = AddXMLAttributeNode(*app_element,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kInstallSource,
                             app_data.install_source());
    if (FAILED(hr)) { return hr; }
  }

  // Create and add component elements to the app element.
  if (app_request.num_components() > 0) {
    CComPtr<IXMLDOMNode> components_node;
    hr = CreateRequestElement(Xml::Element::kComponents,
                              _T(""),
                              document,
                              &components_node);
    if (FAILED(hr)) { return hr; }

    hr = (*app_element)->appendChild(components_node, NULL);
    if (FAILED(hr)) { return hr; }

    AppRequestDataVector::const_iterator it;
    for (it = app_request.components_begin();
         it != app_request.components_end();
         ++it) {
      const AppRequestData& component_request_data = *it;
      const AppData& component_app_data = component_request_data.app_data();
      CComPtr<IXMLDOMNode> component_node;
      hr = CreateRequestElement(Xml::Element::kComponent,
                                _T(""),
                                document,
                                &component_node);
      if (FAILED(hr)) { return hr; }

      hr = AddXMLAttributeNode(component_node,
                               Xml::Namespace::kRequest,
                               Xml::Attribute::kAppId,
                               GuidToString(component_app_data.app_guid()));
      if (FAILED(hr)) { return hr; }

      hr = AddXMLAttributeNode(component_node,
                               Xml::Namespace::kRequest,
                               Xml::Attribute::kVersion,
                               component_app_data.version());
      if (FAILED(hr)) { return hr; }

      hr = components_node->appendChild(component_node, NULL);
      if (FAILED(hr)) { return hr; }

      // TODO(omaha):  Create and add event elements to the component element
      // by traversing the ping_events within component_request_data.
      // Probably want to make the PingEvent traversal from below into a
      // separate function.
    }
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::CreateUpdateAppRequestElement(
    const AppRequest& app_request,
    IXMLDOMDocument* document,
    IXMLDOMNode** app_element) {
  HRESULT hr = CreateAppRequestElementHelper(app_request,
                                             document,
                                             app_element);
  if (FAILED(hr)) {
    return hr;
  }

  const AppData& app_data = app_request.request_data().app_data();

  // update check element
  CComPtr<IXMLDOMNode> update_check;
  hr = CreateRequestElement(Xml::Element::kUpdateCheck,
                            _T(""),
                            document,
                            &update_check);
  if (FAILED(hr)) { return hr; }

  if (app_data.is_update_disabled()) {
    hr = AddXMLAttributeNode(update_check,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kUpdateDisabled,
                             Xml::Value::kTrue);
    if (FAILED(hr)) { return hr; }
  }

  // tag is optional
  if (!app_data.ap().IsEmpty()) {
    hr = AddXMLAttributeNode(update_check,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kTag,
                             app_data.ap());
    if (FAILED(hr)) { return hr; }
  }

  // Add the Trusted Tester token under the "updatecheck" element.
  if (!app_data.tt_token().IsEmpty()) {
    hr = AddXMLAttributeNode(update_check,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kTTToken,
                             app_data.tt_token());
    if (FAILED(hr)) { return hr; }
  }

  hr = (*app_element)->appendChild(update_check, NULL);
  if (FAILED(hr)) { return hr; }

  if (!app_data.install_data_index().IsEmpty()) {
    // data element.
    CComPtr<IXMLDOMNode> install_data;
    hr = CreateRequestElement(Xml::Element::kData,
                              _T(""),
                              document,
                              &install_data);
    if (FAILED(hr)) { return hr; }

    hr = AddXMLAttributeNode(install_data,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kName,
                             Xml::Value::kInstallData);
    if (FAILED(hr)) { return hr; }

    hr = AddXMLAttributeNode(install_data,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kIndex,
                             app_data.install_data_index());
    if (FAILED(hr)) { return hr; }

    hr = (*app_element)->appendChild(install_data, NULL);
    if (FAILED(hr)) { return hr; }
  }

  AppData::ActiveStates active = app_data.did_run();
  if (active != AppData::ACTIVE_UNKNOWN) {
    // didrun element. The server calls it "ping" for legacy reasons.
    CComPtr<IXMLDOMNode> ping;
    hr = CreateRequestElement(Xml::Element::kPing,
                              _T(""),
                              document,
                              &ping);
    if (FAILED(hr)) { return hr; }

    const TCHAR* active_str(active == AppData::ACTIVE_RUN ?
                            _T("1") :
                            _T("0"));
    hr = AddXMLAttributeNode(ping,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kActive,
                             active_str);
    if (FAILED(hr)) { return hr; }

    hr = (*app_element)->appendChild(ping, NULL);
    if (FAILED(hr)) { return hr; }
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::CreatePingAppRequestElement(
    const AppRequest& app_request,
    IXMLDOMDocument* document,
    IXMLDOMNode** app_element) {
  HRESULT hr = CreateAppRequestElementHelper(app_request,
                                             document,
                                             app_element);
  if (FAILED(hr)) {
    return hr;
  }

  // create and add Ping elements to the app element. Server calls ping elements
  // "event" elements for legacy reasons.
  std::vector<PingEvent>::const_iterator it;
  for (it = app_request.request_data().ping_events_begin();
       it != app_request.request_data().ping_events_end();
       ++it) {
    const PingEvent& app_event = *it;
    CComPtr<IXMLDOMNode> ev;
    hr = CreateRequestElement(Xml::Element::kEvent,
                              _T(""),
                              document,
                              &ev);
    if (FAILED(hr)) { return hr; }
    CString str;
    str.Format(_T("%d"), app_event.event_type());
    hr = AddXMLAttributeNode(ev,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kEventType,
                             str);
    if (FAILED(hr)) { return hr; }
    str.Format(_T("%d"), app_event.event_result());
    hr = AddXMLAttributeNode(ev,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kEventResult,
                             str);
    if (FAILED(hr)) { return hr; }
    str.Format(_T("%d"), app_event.error_code());
    hr = AddXMLAttributeNode(ev,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kErrorCode,
                             str);
    if (FAILED(hr)) { return hr; }
    str.Format(_T("%d"), app_event.extra_code1());
    hr = AddXMLAttributeNode(ev,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kExtraCode1,
                             str);
    if (FAILED(hr)) { return hr; }
    str = app_event.previous_version();
    if (!str.IsEmpty()) {
      hr = AddXMLAttributeNode(ev,
                               Xml::Namespace::kRequest,
                               Xml::Attribute::kPreviousVersion,
                               str);
      if (FAILED(hr)) { return hr; }
    }

    hr = (*app_element)->appendChild(ev, NULL);
    if (FAILED(hr)) { return hr; }
  }

  return S_OK;
}

HRESULT GoopdateXmlParser::GenerateRequest(const Request& req,
                                           bool is_update_check,
                                           CString* request_buffer) {
  CORE_LOG(L3, (_T("[GenerateRequest]")));
  ASSERT1(request_buffer);
  if (!request_buffer) {
    return E_INVALIDARG;
  }

  // Create the XML document.
  CComPtr<IXMLDOMDocument> document;
  HRESULT hr = CoCreateSafeDOMDocument(&document);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CoCreateSafeDOMDocument failed][0x%x]"), hr));
    return hr;
  }

  // Create the AppRequests Element.
  CComPtr<IXMLDOMNode> update_requests;
  hr = CreateRequestElement(Xml::Element::kRequests,
                            _T(""),
                            document,
                            &update_requests);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateRequestElement failed][0x%x]"), hr));
    return hr;
  }

  // Add the update requests node to the document.
  CComPtr<IXMLDOMElement> update_requests_node;
  hr = update_requests->QueryInterface(&update_requests_node);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[update_requests->QueryInterface][0x%x]"), hr));
    return hr;
  }
  hr = document->putref_documentElement(update_requests_node);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[putref_documentElement failed][0x%x]"), hr));
    return hr;
  }

  // add attributes to the top element:
  // * protocol - protocol version
  // * version - omaha version
  // * machineid - per machine guid
  // * userid - per user guid
  // * testsource - test source
  // * requestid - Unique request id
  hr = AddXMLAttributeNode(update_requests,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kProtocol,
                           Xml::Value::kProtocol);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[AddXMLAttributeNode failed][0x%x]"), hr));
    return hr;
  }
  hr = AddXMLAttributeNode(update_requests,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kVersion,
                           req.version());
  if (FAILED(hr)) { return hr; }
  hr = AddXMLAttributeNode(update_requests,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kIsMachine,
                           req.is_machine() ? _T("1") : _T("0"));
  if (FAILED(hr)) { return hr; }
  hr = AddXMLAttributeNode(update_requests,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kMachineId,
                           req.machine_id());
  if (FAILED(hr)) { return hr; }
  hr = AddXMLAttributeNode(update_requests,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kUserId,
                           req.user_id());
  if (FAILED(hr)) { return hr; }
  if (!req.test_source().IsEmpty()) {
    hr = AddXMLAttributeNode(update_requests,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kTestSource,
                             req.test_source());
    if (FAILED(hr)) { return hr; }
  }
  if (!req.request_id().IsEmpty()) {
    hr = AddXMLAttributeNode(update_requests,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kRequestId,
                             req.request_id());
    if (FAILED(hr)) { return hr; }
  }

  bool is_period_overridden = false;
  const int check_period_sec =
      ConfigManager::Instance()->GetLastCheckPeriodSec(&is_period_overridden);
  if (is_period_overridden) {
    hr = AddXMLAttributeNode(update_requests,
                             Xml::Namespace::kRequest,
                             Xml::Attribute::kPeriodOverrideSec,
                             itostr(check_period_sec));
    ASSERT1(SUCCEEDED(hr));
  }

  // Create os elements and add to the requests element.
  CComPtr<IXMLDOMNode> os_element;
  hr = CreateRequestElement(Xml::Element::kOs, _T(""), document, &os_element);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateRequestElement failed][0x%x]"), hr));
    return hr;
  }
  hr = AddXMLAttributeNode(os_element,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kPlatform,
                           kPlatformWin);
  hr = AddXMLAttributeNode(os_element,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kVersion,
                           req.os_version());
  hr = AddXMLAttributeNode(os_element,
                           Xml::Namespace::kRequest,
                           Xml::Attribute::kServicePack,
                           req.os_service_pack());
  if (FAILED(hr)) { return hr; }
  hr = update_requests->appendChild(os_element, NULL);
  if (FAILED(hr)) { return hr; }

  // Create and add request's to the requests node.
  AppRequestVector::const_iterator i;
  for (i = req.app_requests_begin(); i != req.app_requests_end(); ++i) {
    const AppRequest& app_request = *i;
    // Create each of the request elements and add to the requests element.
    CComPtr<IXMLDOMNode> request_element;
    hr = is_update_check ?
         CreateUpdateAppRequestElement(app_request,
                                       document,
                                       &request_element) :
         CreatePingAppRequestElement(app_request,
                                     document,
                                     &request_element);
    if (FAILED(hr)) { return hr; }

    // Add the update request node to the AppRequests node.
    hr = update_requests->appendChild(request_element, NULL);
    if (FAILED(hr)) { return hr; }
  }

  // Extract the string out of the DOM, and add the initial processing
  // instruction. We cannot use the xml processing instruction as the get_xml
  // method returns utf-16 encoded string which causes the utf-8 marker to be
  // removed. We will convert to utf-8 before the actual save.
  CComBSTR xml_value(Xml::kHeaderText);
  CComBSTR xml_body;
  document->get_xml(&xml_body);
  xml_value += xml_body;

  *request_buffer = static_cast<TCHAR*>(xml_value);

  return S_OK;
}

HRESULT GoopdateXmlParser::LoadXmlFileToMemory(const CString& file_name,
                                               CString* buffer) {
  ASSERT1(buffer);

  CComPtr<IXMLDOMDocument> document;
  HRESULT hr = LoadXMLFromFile(file_name, false, &document);
  if (FAILED(hr)) {
    return hr;
  }

  return SaveXMLToMemory(document, buffer);
}

}  // namespace omaha
