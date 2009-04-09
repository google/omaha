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
// application_data.h: Class encapsulates the application registration
// and state information.

#ifndef OMAHA_WORKER_APPLICATION_DATA_H__
#define OMAHA_WORKER_APPLICATION_DATA_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/browser_utils.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/reg_key.h"

namespace omaha {

// Encapsulates all the knowledge about the application. All the code inside
// omaha should use this class to query and update information associated with
// applications.
// This class represents a snapshot of the information in the registry. This
// information could have changed after this snap short has been taken.
// Implementation notes:
// 1. Think about synchronizing access between two workers.
// 2. The code should be able to get the values for params that are present
//    inside client_state. I.e. we should not read from only clients. This is
//    important for language.

class AppData {
 public:
  enum ActiveStates {
    ACTIVE_NOTRUN = 0,
    ACTIVE_RUN,
    ACTIVE_UNKNOWN
  };

  AppData()
      : app_guid_(GUID_NULL),
        parent_app_guid_(GUID_NULL),
        is_machine_app_(false),
        iid_(GUID_NULL),
        is_oem_install_(false),
        is_eula_accepted_(true),  // Safe default.
        browser_type_(BROWSER_UNKNOWN),
        usage_stats_enable_(TRISTATE_NONE),
        did_run_(ACTIVE_UNKNOWN),
        is_uninstalled_(false),
        is_update_disabled_(false) { }

  AppData(const GUID& app_guid, bool is_machine_app)
      : app_guid_(app_guid),
        parent_app_guid_(GUID_NULL),
        is_machine_app_(is_machine_app),
        iid_(GUID_NULL),
        is_oem_install_(false),
        is_eula_accepted_(true),  // Safe default.
        browser_type_(BROWSER_UNKNOWN),
        usage_stats_enable_(TRISTATE_NONE),
        did_run_(ACTIVE_UNKNOWN),
        is_uninstalled_(false),
        is_update_disabled_(false) { }

  GUID app_guid() const { return app_guid_; }
  void set_app_guid(const GUID& guid) { app_guid_ = guid; }

  GUID parent_app_guid() const { return parent_app_guid_; }
  void set_parent_app_guid(const GUID& guid) { parent_app_guid_ = guid; }

  bool is_machine_app() const { return is_machine_app_; }
  void set_is_machine_app(bool is_machine_app) {
    is_machine_app_ = is_machine_app;
  }

  CString version() const { return version_; }
  void set_version(const CString version) { version_ = version; }

  CString previous_version() const { return previous_version_; }
  void set_previous_version(const CString& previous_version) {
    previous_version_ = previous_version;
  }

  CString language() const { return language_; }
  void set_language(const CString& language) { language_ = language; }

  CString ap() const { return ap_; }
  void set_ap(const CString& ap) { ap_ = ap; }

  CString tt_token() const { return tt_token_; }
  void set_tt_token(const CString& tt_token) { tt_token_ = tt_token; }

  GUID iid() const { return iid_; }
  void set_iid(const GUID& iid) { iid_ = iid; }

  CString brand_code() const { return brand_code_; }
  void set_brand_code(const CString& brand_code) { brand_code_ = brand_code; }

  CString client_id() const { return client_id_; }
  void set_client_id(const CString& client_id) { client_id_ = client_id; }

  CString referral_id() const { return referral_id_; }
  void set_referral_id(const CString& referral_id) {
      referral_id_ = referral_id;
  }

  bool is_oem_install() const { return is_oem_install_; }
  void set_is_oem_install(bool is_oem_install) {
    is_oem_install_ = is_oem_install;
  }

  bool is_eula_accepted() const { return is_eula_accepted_; }
  void set_is_eula_accepted(bool is_eula_accepted) {
    is_eula_accepted_ = is_eula_accepted;
  }

  CString display_name() const { return display_name_; }
  void set_display_name(const CString& display_name) {
    display_name_ = display_name;
  }

  BrowserType browser_type() const { return browser_type_; }
  void set_browser_type(BrowserType type) { browser_type_ = type; }

  CString install_source() const { return install_source_; }
  void set_install_source(const CString& install_source) {
    install_source_ = install_source;
  }

  CString encoded_installer_data() const { return encoded_installer_data_; }
  void set_encoded_installer_data(const CString& encoded_installer_data) {
    encoded_installer_data_ = encoded_installer_data;
  }

  CString install_data_index() const { return install_data_index_; }
  void set_install_data_index(const CString& install_data_index) {
    install_data_index_ = install_data_index;
  }

  Tristate usage_stats_enable() const { return usage_stats_enable_; }
  void set_usage_stats_enable(Tristate usage_stats_enable) {
    usage_stats_enable_ = usage_stats_enable;
  }

  ActiveStates did_run() const { return did_run_; }
  void set_did_run(AppData::ActiveStates did_run) {
    did_run_ = did_run;
  }

  bool is_uninstalled() const { return is_uninstalled_; }
  void set_is_uninstalled(bool is_uninstalled) {
    is_uninstalled_ = is_uninstalled;
  }

  bool is_update_disabled() const { return is_update_disabled_; }
  void set_is_update_disabled(bool is_update_disabled) {
    is_update_disabled_ = is_update_disabled;
  }

 private:
  GUID app_guid_;
  GUID parent_app_guid_;
  bool is_machine_app_;

  CString version_;
  CString previous_version_;
  CString language_;

  CString ap_;
  CString tt_token_;
  GUID iid_;
  CString brand_code_;
  CString client_id_;
  CString referral_id_;
  bool is_oem_install_;
  bool is_eula_accepted_;

  CString display_name_;
  BrowserType browser_type_;
  CString install_source_;
  CString encoded_installer_data_;
  CString install_data_index_;
  Tristate usage_stats_enable_;

  ActiveStates did_run_;

  bool is_uninstalled_;
  bool is_update_disabled_;

 private:
  friend class AppDataTest;
  friend class PingTest;
};

typedef std::vector<AppData> AppDataVector;

}  // namespace omaha.

#endif  // OMAHA_WORKER_APPLICATION_DATA_H__
