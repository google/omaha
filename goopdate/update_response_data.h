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
// update_response_data.h: The app/component data for a response including what
// to download, how big it is, hash, etc..

#ifndef OMAHA_GOOPDATE_UPDATE_RESPONSE_DATA_H__
#define OMAHA_GOOPDATE_UPDATE_RESPONSE_DATA_H__

#include <functional>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/browser_utils.h"

namespace omaha {

// key=IndexString, value=InstallDataString.
typedef std::map<CString, CString> InstallDataMap;

// Represents the values that are used by the application to indicate its
// requirement for admin.
enum NeedsAdmin {
  NEEDS_ADMIN_YES = 0,  // The application will install machine-wide.
  NEEDS_ADMIN_NO,       // The application will install per user.
};

// What Omaha should do on successful installation.
enum SuccessfulInstallAction {
  SUCCESS_ACTION_DEFAULT = 0,
  SUCCESS_ACTION_EXIT_SILENTLY,
  SUCCESS_ACTION_EXIT_SILENTLY_ON_LAUNCH_CMD,
};

// Represents the information that is parsed from the response returned by
// the server. The class also represents the information that is parsed
// from the manifest that is downloaded as part of the meta-installer.
// The UpdateResponseData contains this information for one app or a component.
// For an entire product, the UpdateResponse will contain a hierarchy of these,
// one for the main app and for each component.
class UpdateResponseData {
 public:
  UpdateResponseData()
      : size_(0),
        needs_admin_(omaha::NEEDS_ADMIN_NO),
        guid_(GUID_NULL),
        installation_id_(GUID_NULL),
        browser_type_(BROWSER_UNKNOWN),
        terminate_all_browsers_(false),
        success_action_(SUCCESS_ACTION_DEFAULT) {
  }

  // Getters and setters for the private members.
  CString url() const { return url_; }
  void set_url(const CString& url) { url_ = url; }
  int size() const { return size_; }
  void set_size(const int& size) { size_ = size; }
  CString hash() const { return hash_; }
  void set_hash(const CString& hash) { hash_ = hash; }
  NeedsAdmin needs_admin() const { return needs_admin_; }
  void set_needs_admin(const NeedsAdmin& needs_admin) {
    needs_admin_ = needs_admin;
  }
  CString arguments() const { return arguments_; }
  void set_arguments(const CString& arguments) { arguments_ = arguments; }
  GUID guid() const { return guid_; }
  void set_guid(const GUID& guid) { guid_ = guid; }
  CString app_name() const { return app_name_; }
  void set_app_name(const CString& app_name) { app_name_ = app_name; }
  CString language() const { return language_; }
  void set_language(const CString& language) { language_ = language; }
  CString status() const { return status_; }
  void set_status(const CString& status) { status_ = status; }
  GUID installation_id() const { return installation_id_; }
  void set_installation_id(const GUID& installation_id) {
    installation_id_ = installation_id;
  }
  CString ap() const { return ap_; }
  void set_ap(const CString& ap) { ap_ = ap; }

  CString tt_token() const { return tt_token_; }
  void set_tt_token(const CString& tt_token) { tt_token_ = tt_token; }

  CString success_url() const { return success_url_; }
  void set_success_url(const CString& success_url) {
    success_url_ = success_url;
  }

  CString error_url() const { return error_url_; }
  void set_error_url(const CString& error_url) {
    error_url_ = error_url;
  }

  BrowserType browser_type() const { return browser_type_; }
  void set_browser_type(BrowserType type) {
    browser_type_ = type;
  }

  bool terminate_all_browsers() const { return terminate_all_browsers_; }
  void set_terminate_all_browsers(bool terminate_all) {
    terminate_all_browsers_ = terminate_all;
  }

  SuccessfulInstallAction success_action() const { return success_action_; }
  void set_success_action(SuccessfulInstallAction success_action) {
    success_action_ = success_action;
  }

  CString version() const { return version_; }
  void set_version(const CString& version) { version_ = version; }

  CString GetInstallData(const CString& index) const {
    InstallDataMap::const_iterator iter = install_data_.find(index);
    if (iter == install_data_.end()) {
      return CString();
    }
    return iter->second;
  }
  void SetInstallData(const CString& index, const CString& value) {
    ASSERT1(install_data_.find(index) == install_data_.end());
    install_data_[index] = value;
  }

 private:
  CString url_;               // The url for the application installer.
  int size_;                  // The size of the download.
  CString hash_;              // The 160bit SHA1 hash of the downloaded file.
  NeedsAdmin needs_admin_;    // If the application needs admin to install.
  CString arguments_;         // Arguments for application installation.
  GUID guid_;                 // Uniquely represents the application.
  // Remove these 5 members when legacy support is removed.
  CString app_name_;          // Application name for display.
  CString language_;          // The language for this application.
  GUID installation_id_;      // Uniquely represents this install.
  CString ap_;                // ap value to be set in the registry.
  BrowserType browser_type_;  // Browser to launch.

  // The status of the response. There needs to be a protocol established
  // between the server and client to determine what the action should be
  // on downloading the application installer.
  CString status_;

  CString tt_token_;          // TT value to be set in the registry.
  CString success_url_;       // URL to launch the browser on success.
  CString error_url_;         // URL describing error.
  bool terminate_all_browsers_;  // Whether to restart all browsers.
  SuccessfulInstallAction success_action_;  // Action after install success.
  CString version_;           // The version of the application itself.
  InstallDataMap install_data_;  // Written to a file and passed as argument to
                                 // app installer.
};

struct GuidComparer : public std::less<GUID> {
  bool operator()(const GUID& lhs, const GUID& rhs) const {
    CString lhs_guid_str = GuidToString(lhs);
    CString rhs_guid_str = GuidToString(rhs);
    return (_tcsicmp(lhs_guid_str, rhs_guid_str) < 0);
  }
};

// Represent the list of requests and responses that are used to communicate
// with the AU server.
typedef std::map<GUID, UpdateResponseData, GuidComparer> UpdateResponseDatas;

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_UPDATE_RESPONSE_DATA_H__

