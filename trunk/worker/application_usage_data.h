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
// application_usage_data.h : Includes methods to deal with application
// usage data. Currently it only deals with the did_run key.
// The class provides methods to process the application data, before and
// after the update check. In case of the did_run key we read the key
// pre-update check and clear it post-update check.

#ifndef OMAHA_GOOPDATE_APPLICATION_USAGE_DATA_H__
#define OMAHA_GOOPDATE_APPLICATION_USAGE_DATA_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

class ApplicationUsageData {
 public:
  ApplicationUsageData(bool is_machine, bool check_low_integrity);
  ~ApplicationUsageData();

  // Reads the did run values for the application indentified by the app_guid.
  HRESULT ReadDidRun(const CString& app_guid);

  // Clears and performs the post processing after an update ckeck for the
  // did run key.
  HRESULT ResetDidRun(const CString& app_guid);

  bool exists() const { return exists_; }
  bool did_run() const { return did_run_; }

 private:
  // Processes the did run value for the machine goopdate.
  HRESULT ProcessMachineDidRun(const CString& app_guid);

  // Processes the did run value for the user goopdate.
  HRESULT ProcessUserDidRun(const CString& app_guid);

  // Calls the pre or the post update check methods based on the
  // is_pre_update_check_ value.
  HRESULT ProcessDidRun(const CString& app_guid);

  // Pre or post process the key that is passed in.
  HRESULT ProcessKey(const CString& key_name);

  // Reads the did run value and populates did_run_ and exists_.
  HRESULT ProcessPreUpdateCheck(const CString& key_name);

  // Clears the did_run value.
  HRESULT ProcessPostUpdateCheck(const CString& key_name);

  // Reads and updates the did_run key for the machine. This is a backward
  // compatibility requirement, since applications have not been updated to
  // write to HKCU yet.
  HRESULT ProcessBackWardCompatKey(const CString& key_name);

  bool exists_;                // Whether the did_run value exists.
  bool did_run_;               // The value of did_run.
  bool is_machine_;            // Whether this is a machine instance.
  bool is_pre_update_check_;   // Internal state of pre or post update.
  bool check_low_integrity_;   // Whether to check the low integrity registry
                               // location.

  DISALLOW_EVIL_CONSTRUCTORS(ApplicationUsageData);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_APPLICATION_USAGE_DATA_H__
