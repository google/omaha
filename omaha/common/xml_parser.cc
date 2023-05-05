// Copyright 2007-2010 Google Inc.
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

#include "omaha/common/xml_parser.h"
#include <memory>
#include <stdlib.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/constants.h"
#include "omaha/base/error.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"
#include "omaha/base/xml_utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_group_policy.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/update_request.h"
#include "omaha/common/update_response.h"
#include "omaha/common/xml_const.h"

namespace omaha {

namespace xml {

namespace {

// Helper structure similar with an std::pair but without a constructor.
// Instance of it can be stored in arrays.
template <typename Type1, typename Type2>
struct Tuple {
  Type1 first;
  Type2 second;
};

// Converts a string to the SuccessfulInstallAction enum.
HRESULT ConvertStringToSuccessfulInstallAction(
    const CString& str,
    SuccessfulInstallAction* successful_install_action) {
  ASSERT1(successful_install_action);

  const Tuple<const TCHAR*, SuccessfulInstallAction> tuples[] = {
    { xml::value::kSuccessActionDefault,
      SUCCESS_ACTION_DEFAULT },

    { xml::value::kSuccessActionExitSilently,
      SUCCESS_ACTION_EXIT_SILENTLY },

    { xml::value::kSuccessActionExitSilentlyOnLaunchCmd,
      SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD },
  };

  if (str.IsEmpty()) {
    *successful_install_action = SUCCESS_ACTION_DEFAULT;
    return S_OK;
  }

  for (size_t i = 0; i != arraysize(tuples); ++i) {
    if (str.CompareNoCase(tuples[i].first) == 0) {
      *successful_install_action = tuples[i].second;
      return S_OK;
    }
  }

  // Using the default action allows Omaha to be forward-compatible with
  // new SuccessActions, meaning older versions will not fail if a config
  // uses a new action.
  ASSERT(false, (_T("[Unrecognized success action][%s]"), str));
  *successful_install_action = SUCCESS_ACTION_DEFAULT;
  return S_OK;
}

HRESULT ConvertStringToInstallEvent(
    const CString& str,
    InstallAction::InstallEvent* install_event) {
  ASSERT1(install_event);

  const Tuple<const TCHAR*, InstallAction::InstallEvent> tuples[] = {
    {xml::value::kPreinstall,  InstallAction::kPreInstall},
    {xml::value::kInstall,     InstallAction::kInstall},
    {xml::value::kUpdate,      InstallAction::kUpdate},
    {xml::value::kPostinstall, InstallAction::kPostInstall},
  };

  for (size_t i = 0; i != arraysize(tuples); ++i) {
    if (str.CompareNoCase(tuples[i].first) == 0) {
      *install_event = tuples[i].second;
      return S_OK;
    }
  }

  return E_INVALIDARG;
}

// Returns S_OK if each child element of the node is any of the elements
// provided as an argument. This is useful to detect if the element contains
// only known children.
// TODO(omaha): implement.
HRESULT AreChildrenAnyOf(IXMLDOMNode* node,
                         const std::vector<const TCHAR*>& element_names) {
  UNREFERENCED_PARAMETER(node);
  UNREFERENCED_PARAMETER(element_names);

  return S_OK;
}

// TODO(omaha): implement.
HRESULT HasNoChildren(IXMLDOMNode* node) {
  UNREFERENCED_PARAMETER(node);
  return S_OK;
}

// Verify that the protocol version is understood. We accept
// all version numbers where major version is the same as kExpectedVersion
// which are greater than or equal to kExpectedVersion. In other words,
// we handle future minor version number increases which should be
// compatible.
HRESULT VerifyProtocolCompatibility(const CString& actual_version,
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

}  // namespace

// The ElementHandler classes should also be in an anonymous namespace but
// the base class cannot be because it is used in the header file.

// Defines the base class of a hierarchy that deals with validating and
// parsing of a single node element in the dom. The implementation uses
// the template method design pattern.
class ElementHandler {
 public:
  ElementHandler() {}
  virtual ~ElementHandler() {}

  HRESULT Handle(IXMLDOMNode* node, response::Response* response) {
    ASSERT1(node);
    ASSERT1(response);

    HRESULT hr = Validate(node);
    if (FAILED(hr)) {
      return hr;
    }

    hr = Parse(node, response);
    if (FAILED(hr)) {
      return hr;
    }

    return S_OK;
  }

 private:
  // Validates a node and returns S_OK in case of success.
  virtual HRESULT Validate(IXMLDOMNode* node) {
    UNREFERENCED_PARAMETER(node);
    return S_OK;
  }

  // Parses the node and stores its values in the response.
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    UNREFERENCED_PARAMETER(node);
    UNREFERENCED_PARAMETER(response);
    return S_OK;
  }

  DISALLOW_COPY_AND_ASSIGN(ElementHandler);
};


// Parses 'response'.
class ResponseElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new ResponseElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kProtocol,
                                     &response->protocol);
    if (FAILED(hr)) {
      return hr;
    }
    hr = VerifyProtocolCompatibility(response->protocol, xml::value::kVersion3);
    if (FAILED(hr)) {
      return hr;
    }

    return S_OK;
  }
};

// Parses 'app'.
class AppElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new AppElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::App app;

    HRESULT hr = ReadStringAttribute(node, xml::attribute::kAppId, &app.appid);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadStringAttribute(node, xml::attribute::kStatus, &app.status);
    if (FAILED(hr)) {
      return hr;
    }

    // At present, the server may omit the following optional attributes if the
    // contents would be empty strings.  Guard these reads appropriately.
    // TODO(omaha3): If we adapt the server to send an empty string for these
    // attributes, we can remove these checks.
    if (HasAttribute(node, xml::attribute::kExperiments)) {
      hr = ReadStringAttribute(node,
                               xml::attribute::kExperiments,
                               &app.experiments);
      if (FAILED(hr)) {
        return hr;
      }
    }

    hr = ReadCohortAttributes(node, &app);
    if (FAILED(hr)) {
      return hr;
    }

    response->apps.push_back(app);
    return S_OK;
  }

  HRESULT ReadCohortAttributes(IXMLDOMNode* node, response::App* app) {
    ASSERT1(node);
    ASSERT1(app);

    if (HasAttribute(node, xml::attribute::kCohort)) {
      HRESULT hr = ReadStringAttribute(node,
                                       xml::attribute::kCohort,
                                       &app->cohort);
      if (FAILED(hr)) {
        return hr;
      }
    }

    if (HasAttribute(node, xml::attribute::kCohortHint)) {
      HRESULT hr = ReadStringAttribute(node,
                                       xml::attribute::kCohortHint,
                                       &app->cohort_hint);
      if (FAILED(hr)) {
        return hr;
      }
    }

    if (HasAttribute(node, xml::attribute::kCohortName)) {
      HRESULT hr = ReadStringAttribute(node,
                                       xml::attribute::kCohortName,
                                       &app->cohort_name);
      if (FAILED(hr)) {
        return hr;
      }
    }

    return S_OK;
  }
};


// Parses 'updatecheck'.
class UpdateCheckElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new UpdateCheckElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::UpdateCheck& update_check = response->apps.back().update_check;

    ReadStringAttribute(node,
                        xml::attribute::kTTToken,
                        &update_check.tt_token);

    ReadStringAttribute(node,
                        xml::attribute::kErrorUrl,
                        &update_check.error_url);

    return ReadStringAttribute(node,
                               xml::attribute::kStatus,
                               &update_check.status);
  }
};


// Parses 'urls'.
class UrlsElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new UrlsElementHandler; }
};


// Parses 'url'.
class UrlElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new UrlElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    CString url;
    HRESULT hr = ReadStringAttribute(node, xml::attribute::kCodebase, &url);
    if (FAILED(hr)) {
      return hr;
    }

    response::UpdateCheck& update_check = response->apps.back().update_check;
    update_check.urls.push_back(url);

    return S_OK;
  }
};


// Parses 'manifest'.
class ManifestElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new ManifestElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    InstallManifest& install_manifest =
        response->apps.back().update_check.install_manifest;
    ReadStringAttribute(node,
                        xml::attribute::kVersion,
                        &install_manifest.version);
    return S_OK;
  }
};


// Parses 'packages'.
class PackagesElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new PackagesElementHandler; }
};



// Parses 'package'.
class PackageElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new PackageElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    InstallPackage install_package;

    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kName,
                                     &install_package.name);
    if (FAILED(hr)) {
      return hr;
    }

    install_package.is_required = true;
    hr = ReadBooleanAttribute(node,
                              xml::attribute::kRequired,
                              &install_package.is_required);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadIntAttribute(node, xml::attribute::kSize, &install_package.size);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadStringAttribute(node,
                             xml::attribute::kHashSha256,
                             &install_package.hash_sha256);
    HRESULT hr2 = ReadStringAttribute(node,
                                      xml::attribute::kHash,
                                      &install_package.hash_sha1);
    if (FAILED(hr) && FAILED(hr2)) {
      return hr;
    }

    InstallManifest& install_manifest =
        response->apps.back().update_check.install_manifest;
    install_manifest.packages.push_back(install_package);

    return S_OK;
  }
};

// Parses 'actions'.
class ActionsElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new ActionsElementHandler; }
};


// Parses 'action'.
class ActionElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new ActionElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    InstallAction install_action;

    CString event;
    HRESULT hr = ReadStringAttribute(node, xml::attribute::kEvent, &event);
    if (FAILED(hr)) {
      return hr;
    }
    hr = ConvertStringToInstallEvent(event, &install_action.install_event);
    if (FAILED(hr)) {
      return hr;
    }

    ReadStringAttribute(node,
                        xml::attribute::kRun,
                        &install_action.program_to_run);
    ReadStringAttribute(node,
                        xml::attribute::kArguments,
                        &install_action.program_arguments);

    ReadStringAttribute(node,
                        xml::attribute::kSuccessUrl,
                        &install_action.success_url);

    ReadBooleanAttribute(node,
                         xml::attribute::kTerminateAllBrowsers,
                         &install_action.terminate_all_browsers);

    CString success_action;
    ReadStringAttribute(node,
                        xml::attribute::kSuccessAction,
                        &success_action);
    ConvertStringToSuccessfulInstallAction(success_action,
                                           &install_action.success_action);

    InstallManifest& install_manifest =
        response->apps.back().update_check.install_manifest;
    install_manifest.install_actions.push_back(install_action);

    return S_OK;
  }
};


// Parses 'data'.
class DataElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new DataElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response->apps.back().data.push_back(response::Data());
    response::Data& data = response->apps.back().data.back();

    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kStatus,
                                     &data.status);
    if (FAILED(hr)) {
      return hr;
    }

    CString& data_name = data.name;
    hr = ReadStringAttribute(node, xml::attribute::kName, &data_name);
    if (FAILED(hr)) {
      return hr;
    }

    if (data_name == xml::value::kInstallData) {
      hr = ReadStringAttribute(node,
                               xml::attribute::kIndex,
                               &data.install_data_index);
      if (FAILED(hr)) {
        return hr;
      }
      if (data.status == xml::response::kStatusOkValue) {
        return ReadStringValue(node, &data.install_data);
      }
      return S_OK;
    } else if (data_name == xml::value::kUntrusted) {
      return S_OK;
    }

    ASSERT(false, (data_name));
    return E_UNEXPECTED;
  }
};

// Parses 'ping'.
class PingElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new PingElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::Ping& ping = response->apps.back().ping;
    ReadStringAttribute(node, xml::attribute::kStatus, &ping.status);
    ASSERT1(ping.status == xml::response::kStatusOkValue);
    return S_OK;
  }
};

// Parses 'event'.
class EventElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new EventElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::Event event;
    ReadStringAttribute(node, xml::attribute::kStatus, &event.status);
    ASSERT1(event.status == xml::response::kStatusOkValue);
    response::App& app = response->apps.back();
    app.events.push_back(event);
    return S_OK;
  }
};

// Parses 'daystart'.
class DayStartElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new DayStartElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    ReadIntAttribute(node,
                     xml::attribute::kElapsedSeconds,
                     &response->day_start.elapsed_seconds);

    HRESULT hr = ReadIntAttribute(node,
                                  xml::attribute::kElapsedDays,
                                  &response->day_start.elapsed_days);
    if (FAILED(hr)) {
      CORE_LOG(LW, (_T("[DayStartElementHandler][hr=%#x]"), hr));
      return hr;
    }
    ASSERT1(response->day_start.elapsed_days >= kMinDaysSinceDatum);
    ASSERT1(response->day_start.elapsed_days <= kMaxDaysSinceDatum);
    return S_OK;
  }
};

// Parses 'systemrequirements'.
class SystemRequirementsElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() {
    return new SystemRequirementsElementHandler;
  }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::SystemRequirements& sys_req = response->sys_req;

    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kPlatform,
                                     &sys_req.platform);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ReadStringAttribute(node,
                             xml::attribute::kArch,
                             &sys_req.arch);
    if (FAILED(hr)) {
      return hr;
    }

    return ReadStringAttribute(node,
                               xml::attribute::kMinOSVersion,
                               &sys_req.min_os_version);
  }
};

namespace v2 {

namespace element {

const TCHAR* const kGUpdate = _T("gupdate");

}  // namespace element

namespace attributev2 {

// Some v2 offline manifests were incorrectly authored as 'Version' with a
// capital 'V'.
const TCHAR* const kVersionProperCased = _T("Version");

}  // namespace attributev2

namespace value {

const TCHAR* const kVersion2 = _T("2.0");

}  // namespace value

// Parses Omaha v2 'gupdate'.
class GUpdateElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new GUpdateElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kProtocol,
                                     &response->protocol);
    if (FAILED(hr)) {
      return hr;
    }
    return VerifyProtocolCompatibility(response->protocol,
                                       value::kVersion2);
  }
};

// Parses Omaha v2 'updatecheck'.
class UpdateCheckElementHandler : public ElementHandler {
 public:
  static ElementHandler* Create() { return new UpdateCheckElementHandler; }

 private:
  virtual HRESULT Parse(IXMLDOMNode* node, response::Response* response) {
    response::UpdateCheck& update_check = response->apps.back().update_check;

    HRESULT hr = ReadStringAttribute(node,
                                     xml::attribute::kStatus,
                                     &update_check.status);
    if (FAILED(hr)) {
      return hr;
    }

    if (update_check.status.CompareNoCase(xml::response::kStatusOkValue)) {
      return S_OK;
    }

    InstallManifest& install_manifest = update_check.install_manifest;
    if (FAILED(ReadStringAttribute(node,
                                   xml::attribute::kVersion,
                                   &install_manifest.version))) {
      ReadStringAttribute(node,
                          v2::attributev2::kVersionProperCased,
                          &install_manifest.version);
    }

    CString url;
    hr = ReadStringAttribute(node, xml::attribute::kCodebase, &url);
    if (FAILED(hr)) {
      return hr;
    }

    int start_file_name_idx = url.ReverseFind(_T('/'));
    if (start_file_name_idx <= 0) {
      return GOOPDATEDOWNLOAD_E_INVALID_PATH;
    }
    CString base_url = url.Left(start_file_name_idx + 1);
    update_check.urls.push_back(base_url);

    CString package_name = url.Right(url.GetLength() - start_file_name_idx - 1);
    if (package_name.IsEmpty()) {
      return GOOPDATEDOWNLOAD_E_FILE_NAME_EMPTY;
    }

    InstallPackage install_package;
    install_package.name = package_name;
    install_package.is_required = true;
    hr = ReadIntAttribute(node, xml::attribute::kSize, &install_package.size);
    if (FAILED(hr)) {
      return hr;
    }
    hr = ReadStringAttribute(node,
                             xml::attribute::kHash,
                             &install_package.hash_sha1);
    if (FAILED(hr)) {
      return hr;
    }

    install_manifest.packages.push_back(install_package);

    InstallAction install_action;
    install_action.install_event = InstallAction::kInstall;
    install_action.program_to_run = package_name;
    ReadStringAttribute(node,
                        xml::attribute::kArguments,
                        &install_action.program_arguments);

    install_manifest.install_actions.push_back(install_action);

    InstallAction post_install_action;
    if (SUCCEEDED(ParsePostInstallActions(node, &post_install_action))) {
      install_manifest.install_actions.push_back(post_install_action);
    }

    return S_OK;
  }

  HRESULT ParsePostInstallActions(IXMLDOMNode* node,
                                  InstallAction* post_install_action) {
    InstallAction install_action;
    CString success_action;
    if (FAILED(ReadStringAttribute(node,
                                   xml::attribute::kSuccessAction,
                                   &success_action)) &&
        FAILED(ReadStringAttribute(node,
                                   xml::attribute::kSuccessUrl,
                                   &install_action.success_url)) &&
        FAILED(ReadBooleanAttribute(node,
                                    xml::attribute::kTerminateAllBrowsers,
                                    &install_action.terminate_all_browsers))) {
      return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    }

    install_action.install_event = InstallAction::kPostInstall;
    ReadStringAttribute(node, xml::attribute::kSuccessAction, &success_action);
    ConvertStringToSuccessfulInstallAction(success_action,
                                           &install_action.success_action);
    ReadStringAttribute(node,
                        xml::attribute::kSuccessUrl,
                        &install_action.success_url);
    ReadBooleanAttribute(node,
                         xml::attribute::kTerminateAllBrowsers,
                         &install_action.terminate_all_browsers);
    *post_install_action = install_action;
    return S_OK;
  }
};

}  // namespace v2

XmlParser::XmlParser() {}

void XmlParser::InitializeElementHandlers() {
  const Tuple<const TCHAR*, ElementHandler* (*)()> tuples[] = {
    {xml::element::kAction, &ActionElementHandler::Create},
    {xml::element::kActions, &ActionsElementHandler::Create},
    {xml::element::kApp, &AppElementHandler::Create},
    {xml::element::kData, &DataElementHandler::Create},
    {xml::element::kDayStart, &DayStartElementHandler::Create},
    {xml::element::kSystemRequirements,
        &SystemRequirementsElementHandler::Create},
    {xml::element::kEvent, &EventElementHandler::Create},
    {xml::element::kManifest, &ManifestElementHandler::Create},
    {xml::element::kPackage, &PackageElementHandler::Create},
    {xml::element::kPackages, &PackagesElementHandler::Create},
    {xml::element::kPing, &PingElementHandler::Create},
    {xml::element::kResponse, &ResponseElementHandler::Create},
    {xml::element::kUpdateCheck, &UpdateCheckElementHandler::Create},
    {xml::element::kUrl, &UrlElementHandler::Create},
    {xml::element::kUrls, &UrlsElementHandler::Create},
  };

  for (size_t i = 0; i != arraysize(tuples); ++i) {
    VERIFY1(element_handler_factory_.Register(tuples[i].first,
                                              tuples[i].second));
  }
}

// The AppElementHandler and the DataElementHandler are shared, because the
// format is identical between Omaha v2 and Omaha v3. We should make copies of
// the shared classes if these elements diverge between v2 and v3. The worst
// case scenario is that we break v2 compatibility if we do not do a
// copy-on-write, which might be acceptable.
void XmlParser::InitializeLegacyElementHandlers() {
  const Tuple<const TCHAR*, ElementHandler* (*)()> tuples[] = {
    {xml::element::kApp, &AppElementHandler::Create},
    {xml::element::kData, &DataElementHandler::Create},
    {v2::element::kGUpdate, &v2::GUpdateElementHandler::Create},
    {xml::element::kUpdateCheck, &v2::UpdateCheckElementHandler::Create},
  };

  for (size_t i = 0; i != arraysize(tuples); ++i) {
    VERIFY1(element_handler_factory_.Register(tuples[i].first,
                                              tuples[i].second));
  }
}

HRESULT XmlParser::SerializeRequest(const UpdateRequest& update_request,
                                    CString* buffer) {
  ASSERT1(buffer);

  XmlParser xml_parser;

  HRESULT hr = xml_parser.BuildDom(update_request.request());
  if (FAILED(hr)) {
    return hr;
  }

  hr = xml_parser.GetXml(buffer);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT XmlParser::BuildDom(const request::Request& request) {
  CORE_LOG(L3, (_T("[XmlParser::BuildDom]")));
  HRESULT hr = CoCreateSafeDOMDocument(&document_);
  if (FAILED(hr)) {
    return hr;
  }

  request_ = &request;

  hr = BuildRequestElement();
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Extract the string out of the DOM, and add the initial processing
// instruction. The xml property of the document converts to the document from
// its original encoding to Unicode. As a result, the xml directive and the
// desired encoding must be explicitely set here.
HRESULT XmlParser::GetXml(CString* buffer) {
  CORE_LOG(L3, (_T("[XmlParser::GetXml]")));

  ASSERT1(buffer);

  CComBSTR xml_body;
  HRESULT hr = document_->get_xml(&xml_body);
  if (FAILED(hr)) {
    return hr;
  }

  *buffer = kXmlDirective;
  *buffer += xml_body;

  // The xml string contains a CR LF pair at the end.
  buffer->TrimRight(_T("\r\n"));

  return S_OK;
}


HRESULT XmlParser::BuildRequestElement() {
  CORE_LOG(L3, (_T("[XmlParser::BuildRequestElement]")));

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateElementNode(xml::element::kRequest, _T(""), &element);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(request_);

  // Add attributes to the top element:
  // * protocol - protocol version
  // * version - Omaha (goopdate.dll) version
  // * shell_version - Omaha shell (GoogleUpdate.exe) version
  // * ismachine - is machine Omaha
  // * installsource - install source
  // * originurl - origin url, primarily set by Update3Web plugins
  // * testsource - test source
  // * requestid - unique request ID
  // * periodoverridesec - override value for update check frequency
  // * dedup - the algorithm used to dedup users
  // * dlpref - the GPO settings for download url preference

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kProtocol,
                           request_->protocol_version);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kUpdater,
                           xml::value::kUpdater);
  if (FAILED(hr)) {
    return hr;
  }


  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kUpdaterVersion,
                           request_->omaha_version);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kShellVersion,
                           request_->omaha_shell_version);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kIsMachine,
                           request_->is_machine ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSessionId,
                           request_->session_id);
  if (FAILED(hr)) {
    return hr;
  }

  if (!request_->uid.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kUserId,
                             request_->uid);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!request_->install_source.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kInstallSource,
                             request_->install_source);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!request_->origin_url.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kOriginURL,
                             request_->origin_url);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!request_->test_source.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kTestSource,
                             request_->test_source);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!request_->request_id.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kRequestId,
                             request_->request_id);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (request_->check_period_sec != -1) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kPeriodOverrideSec,
                             itostr(request_->check_period_sec));
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kDedup,
                           xml::value::kClientRegulated);
  if (FAILED(hr)) {
    return hr;
  }

  if (request_->dlpref == kDownloadPreferenceCacheable) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kDlPref,
                             xml::value::kCacheable);
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kDomainJoined,
                           request_->domain_joined ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = BuildHwElement(element);
  if (FAILED(hr)) {
    return hr;
  }

  hr = BuildOsElement(element);
  if (FAILED(hr)) {
    return hr;
  }

  // Add the app element sequence to the request.
  hr = BuildAppElement(element);
  if (FAILED(hr)) {
    return hr;
  }

  // Add the request node to the document root.
  CComPtr<IXMLDOMElement> element_node;
  hr = element->QueryInterface(&element_node);
  if (FAILED(hr)) {
    return hr;
  }

  hr = document_->putref_documentElement(element_node);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT XmlParser::BuildHwElement(IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildHwElement]")));

  ASSERT1(parent_node);
  ASSERT1(request_);

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateElementNode(xml::element::kHw, _T(""), &element);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kPhysMemory,
                           itostr(request_->hw.physmemory));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSse,
                           request_->hw.has_sse ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSse2,
                           request_->hw.has_sse2 ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSse3,
                           request_->hw.has_sse3 ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSsse3,
                           request_->hw.has_ssse3 ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSse41,
                           request_->hw.has_sse41 ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kSse42,
                           request_->hw.has_sse42 ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kAvx,
                           request_->hw.has_avx ? _T("1") : _T("0"));
  if (FAILED(hr)) {
    return hr;
  }

  hr = parent_node->appendChild(element, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT XmlParser::BuildOsElement(IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildOsElement]")));

  ASSERT1(parent_node);
  ASSERT1(request_);

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateElementNode(xml::element::kOs, _T(""), &element);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kPlatform,
                           request_->os.platform);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kVersion,
                           request_->os.version);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kServicePack,
                           request_->os.service_pack);
  if (FAILED(hr)) {
    return hr;
  }

  hr = AddXMLAttributeNode(element,
                           kXmlNamespace,
                           xml::attribute::kArch,
                           request_->os.arch);
  if (FAILED(hr)) {
    return hr;
  }

  hr = parent_node->appendChild(element, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Create and add request's to the requests node.
HRESULT XmlParser::BuildAppElement(IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildAppElement]")));

  ASSERT1(parent_node);
  ASSERT1(request_);

  for (size_t i = 0; i < request_->apps.size(); ++i) {
    const request::App& app = request_->apps[i];

    CComPtr<IXMLDOMNode> element;
    HRESULT hr = CreateElementNode(xml::element::kApp, _T(""), &element);
    if (FAILED(hr)) {
      return hr;
    }

    ASSERT1(IsGuid(app.app_id));
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kAppId,
                             app.app_id);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kVersion,
                             app.version);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kNextVersion,
                             app.next_version);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddAppDefinedAttributes(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    if (!app.ap.IsEmpty()) {
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kAdditionalParameters,
                               app.ap);
      if (FAILED(hr)) {
        return hr;
      }
    }

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kLang,
                             app.lang);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kBrandCode,
                             app.brand_code);
    if (FAILED(hr)) {
      return hr;
    }

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kClientId,
                             app.client_id);
    if (FAILED(hr)) {
      return hr;
    }

    // TODO(omaha3): Determine whether or not the server is able to accept an
    // empty string here.  If so, remove this IsEmpty() check, and always emit.
    if (!app.experiments.IsEmpty()) {
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kExperiments,
                               app.experiments);
      if (FAILED(hr)) {
        return hr;
      }
    }

    // 0 seconds indicates unknown install time. A new install uses -1 days.
    if (app.install_time_diff_sec) {
      const int installed_full_days =
          static_cast<int>(app.install_time_diff_sec) / kSecondsPerDay;
      ASSERT1(installed_full_days >= 0 || installed_full_days == -1);
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kInstalledAgeDays,
                               itostr(installed_full_days));
      if (FAILED(hr)) {
        return hr;
      }
    }

    // Three possible categories for value of DayOfInstall:
    //   -1: it's a new installation.
    //   0: unknown day of install. Probably it's a legacy app. Skip sending.
    //   Positive number: will be adjusted to the first day of the install week.
    //       (install cohort is weekly based).
    if (app.day_of_install != 0) {
      ASSERT1(app.day_of_install >= kMinDaysSinceDatum ||
              app.day_of_install == -1);
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kInstallDate,
                               itostr(app.day_of_install));
      if (FAILED(hr)) {
        return hr;
      }
    }

    if (!app.iid.IsEmpty() && app.iid != GuidToString(GUID_NULL)) {
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kInstallationId,
                               app.iid);
      if (FAILED(hr)) {
        return hr;
      }
    }

    hr = AddCohortAttributes(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = BuildUpdateCheckElement(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = BuildPingRequestElement(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = BuildDataElement(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = BuildDidRunElement(app, element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = parent_node->appendChild(element, NULL);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT XmlParser::AddAppDefinedAttributes(const request::App& app,
                                           IXMLDOMNode* element) {
  CORE_LOG(L3, (_T("[XmlParser::AddAppDefinedAttributes]")));

  if (app.app_defined_attributes.empty()) {
    return S_OK;
  }

  for (size_t i = 0; i < app.app_defined_attributes.size(); ++i) {
    const CString& name(app.app_defined_attributes[i].first);
    const CString& value(app.app_defined_attributes[i].second);

    ASSERT1(String_StartsWith(name, xml::attribute::kAppDefinedPrefix, false));

    HRESULT hr = AddXMLAttributeNode(element, kXmlNamespace, name, value);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT XmlParser::AddCohortAttributes(const request::App& app,
                                       IXMLDOMNode* element) {
  CORE_LOG(L3, (_T("[XmlParser::AddCohortAttributes]")));

  if (!app.cohort.IsEmpty()) {
    HRESULT hr = AddXMLAttributeNode(element,
                                     kXmlNamespace,
                                     xml::attribute::kCohort,
                                     app.cohort);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app.cohort_hint.IsEmpty()) {
    HRESULT hr = AddXMLAttributeNode(element,
                                     kXmlNamespace,
                                     xml::attribute::kCohortHint,
                                     app.cohort_hint);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app.cohort_name.IsEmpty()) {
    HRESULT hr = AddXMLAttributeNode(element,
                                     kXmlNamespace,
                                     xml::attribute::kCohortName,
                                     app.cohort_name);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT XmlParser::BuildUpdateCheckElement(const request::App& app,
                                           IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildUpdateCheckElement]")));
  ASSERT1(parent_node);

  // Create a DOM element only if the update check member is valid.
  if (!app.update_check.is_valid) {
    return S_OK;
  }

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateElementNode(xml::element::kUpdateCheck, _T(""), &element);
  if (FAILED(hr)) {
    return hr;
  }

  if (app.update_check.is_update_disabled) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kUpdateDisabled,
                             xml::value::kTrue);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app.update_check.tt_token.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kTTToken,
                             app.update_check.tt_token);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (app.update_check.is_rollback_allowed) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kRollbackAllowed,
                             xml::value::kTrue);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app.update_check.target_version_prefix.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kTargetVersionPrefix,
                             app.update_check.target_version_prefix);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (!app.update_check.target_channel.IsEmpty()) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kTargetChannel,
                             app.update_check.target_channel);
    if (FAILED(hr)) {
      return hr;
    }
  }

  hr = parent_node->appendChild(element, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

// Ping elements are called "event" elements for legacy reasons.
HRESULT XmlParser::BuildPingRequestElement(const request::App& app,
                                           IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildPingRequestElement]")));
  ASSERT1(parent_node);

  // Create a DOM element only if there is are ping_events.
  if (app.ping_events.empty()) {
    return S_OK;
  }

  PingEventVector::const_iterator it;
  for (it = app.ping_events.begin(); it != app.ping_events.end(); ++it) {
    const PingEventPtr ping_event = *it;
    CComPtr<IXMLDOMNode> element;
    HRESULT hr = CreateElementNode(xml::element::kEvent, _T(""), &element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = ping_event->ToXml(element);
    if (FAILED(hr)) {
      return hr;
    }

    hr = parent_node->appendChild(element, NULL);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT XmlParser::BuildDataElement(const request::App& app,
                                    IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildDataElement]")));
  ASSERT1(parent_node);

  // Create a DOM element only if there is a data object.
  if (app.data.empty()) {
    return S_OK;
  }

  for (size_t i = 0; i != app.data.size(); ++i) {
    CComPtr<IXMLDOMNode> element;
    HRESULT hr = CreateElementNode(xml::element::kData, _T(""), &element);
    if (FAILED(hr)) {
      return hr;
    }

    const xml::request::Data& data = app.data[i];

    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kName,
                             data.name);
    if (FAILED(hr)) {
      return hr;
    }

    const CString& install_data_index = data.install_data_index;
    const CString& untrusted_data     = data.untrusted_data;

    ASSERT1(install_data_index.IsEmpty() != untrusted_data.IsEmpty());

    using xml::value::kInstall;
    using xml::value::kUntrusted;

    if (data.name == kInstall && !install_data_index.IsEmpty()) {
      hr = AddXMLAttributeNode(element,
                               kXmlNamespace,
                               xml::attribute::kIndex,
                               data.install_data_index);
    } else if (data.name == kUntrusted && !untrusted_data.IsEmpty()) {
      hr = element->put_text(CComBSTR(untrusted_data));
    } else {
      ASSERT1(false);
      hr = E_UNEXPECTED;
    }

    if (FAILED(hr)) {
      return hr;
    }

    hr = parent_node->appendChild(element, NULL);
    if (FAILED(hr)) {
      return hr;
    }
  }

  return S_OK;
}

HRESULT XmlParser::BuildDidRunElement(const request::App& app,
                                      IXMLDOMNode* parent_node) {
  CORE_LOG(L3, (_T("[XmlParser::BuildDidRunElement]")));
  ASSERT1(parent_node);

  const bool was_active = app.ping.active == ACTIVE_RUN;
  const bool need_active = app.ping.active != ACTIVE_UNKNOWN;
  const bool has_sent_a_today = app.ping.days_since_last_active_ping == 0;
  const bool need_a = was_active && !has_sent_a_today;
  const bool need_r = app.ping.days_since_last_roll_call != 0;
  const bool need_ad = was_active && app.ping.day_of_last_activity != 0;
  const bool need_rd = app.ping.day_of_last_roll_call != 0;
  const bool has_freshness = !app.ping.ping_freshness.IsEmpty();

  // Create a DOM element only if the didrun object has actual state.
  if (!need_active && !need_a && !need_r && !need_ad && !need_rd &&
      !has_freshness) {
    return S_OK;
  }

  ASSERT1(app.update_check.is_valid);

  CComPtr<IXMLDOMNode> element;
  HRESULT hr = CreateElementNode(xml::element::kPing, _T(""), &element);
  if (FAILED(hr)) {
    return hr;
  }

  // TODO(omaha): Remove "active" attribute after transition.
  if (need_active) {
    const TCHAR* active_str(was_active ? _T("1") : _T("0"));
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kActive,
                             active_str);
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (need_a) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kDaysSinceLastActivePing,
                             itostr(app.ping.days_since_last_active_ping));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (need_r) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kDaysSinceLastRollCall,
                             itostr(app.ping.days_since_last_roll_call));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (need_ad) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kDayOfLastActivity,
                             itostr(app.ping.day_of_last_activity));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (need_rd) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kDayOfLastRollCall,
                             itostr(app.ping.day_of_last_roll_call));
    if (FAILED(hr)) {
      return hr;
    }
  }

  if (has_freshness) {
    hr = AddXMLAttributeNode(element,
                             kXmlNamespace,
                             xml::attribute::kPingFreshness,
                             app.ping.ping_freshness);
    if (FAILED(hr)) {
      return hr;
    }
  }


  hr = parent_node->appendChild(element, NULL);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

HRESULT XmlParser::CreateElementNode(const TCHAR* name,
                                     const TCHAR* value,
                                     IXMLDOMNode** element) {
  ASSERT1(name);
  ASSERT1(value);
  ASSERT1(element);

  // When there is a namespace, the element names get o: prepended to avoid a
  // size explosion where the namespace uri gets automatically added to every
  // element by msxml.
  CString namespace_qualified_name;
  SafeCStringFormat(&namespace_qualified_name,
                    kXmlNamespace ? _T("o:%s") : _T("%s"),
                    name);
  ASSERT1(document_);
  HRESULT hr = CreateXMLNode(document_,
                             NODE_ELEMENT,
                             namespace_qualified_name,
                             kXmlNamespace,
                             value,
                             element);
  return hr;
}


HRESULT XmlParser::DeserializeResponse(const std::vector<uint8>& buffer,
                                       UpdateResponse* update_response) {
  ASSERT1(update_response);

  XmlParser xml_parser;
  HRESULT hr = LoadXMLFromRawData(buffer, false, &xml_parser.document_);
  if (FAILED(hr)) {
    return hr;
  }

  response::Response response;
  xml_parser.response_ = &response;

  hr = xml_parser.Parse();
  if (FAILED(hr)) {
    return hr;
  }

  update_response->response_ = response;
  return S_OK;
}

HRESULT XmlParser::Parse() {
  CORE_LOG(L3, (_T("[XmlParser::Parse]")));
  ASSERT1(response_);

  CComPtr<IXMLDOMElement> root_node;
  HRESULT hr = document_->get_documentElement(&root_node);
  if (FAILED(hr)) {
    return hr;
  }
  if (!root_node) {
    return GOOPDATEXML_E_PARSE_ERROR;
  }

  CComBSTR root_name;
  hr = root_node->get_baseName(&root_name);
  if (FAILED(hr)) {
    return hr;
  }

  if (root_name == xml::element::kResponse) {
    InitializeElementHandlers();
    return TraverseDOM(root_node);
  }

  if (root_name == v2::element::kGUpdate) {
    InitializeLegacyElementHandlers();
    return TraverseDOM(root_node);
  }

  return GOOPDATEXML_E_RESPONSENODE;
}

HRESULT XmlParser::TraverseDOM(IXMLDOMNode* node) {
  CORE_LOG(L5, (_T("[XmlParser::TraverseDOM]")));
  ASSERT1(node);

  HRESULT hr = VisitElement(node);
  if (FAILED(hr)) {
    return hr;
  }

  CComPtr<IXMLDOMNodeList> children_list;
  hr = node->get_childNodes(&children_list);
  if (FAILED(hr)) {
    return hr;
  }

  long num_children = 0;   // NOLINT
  hr = children_list->get_length(&num_children);
  if (FAILED(hr)) {
    return hr;
  }

  for (int i = 0; i < num_children; ++i) {
    CComPtr<IXMLDOMNode> child_node;
    hr = children_list->get_item(i, &child_node);
    if (FAILED(hr)) {
      return hr;
    }

    DOMNodeType type = NODE_INVALID;
    hr = child_node->get_nodeType(&type);
    if (FAILED(hr)) {
      return hr;
    }

    ASSERT1(type == NODE_TEXT || type == NODE_ELEMENT || type == NODE_COMMENT);

    if (type == NODE_ELEMENT) {
      hr = TraverseDOM(child_node);
      if (FAILED(hr)) {
        return hr;
      }
    }
  }

  return S_OK;
}

HRESULT XmlParser::VisitElement(IXMLDOMNode* node) {
  XMLFQName node_name;
  HRESULT hr = GetXMLFQName(node, &node_name);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetXMLFQName failed][0x%x]"), hr));
    return hr;
  }

  CORE_LOG(L4, (_T("[element name][%s:%s]"), node_name.uri, node_name.base));

  // Ignore elements not understood.
  std::unique_ptr<ElementHandler> element_handler;
  element_handler.reset(element_handler_factory_.CreateObject(node_name.base));
  if (element_handler.get()) {
    return element_handler->Handle(node, response_);
  } else {
    CORE_LOG(LW, (_T("[VisitElement: don't know how to handle %s:%s]"),
                  node_name.uri, node_name.base));
  }
  return S_OK;
}

}  // namespace xml

}  // namespace omaha
