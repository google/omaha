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

#include "omaha/tools/goopdump/data_dumper_app_manager.h"

#include <vector>

#include "omaha/common/reg_key.h"
#include "omaha/common/utils.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/application_manager.h"
#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

namespace omaha {

namespace {

CString ActiveStateToString(omaha::AppData::ActiveStates state) {
  CString str = _T("UNDEFINED");
  switch (state) {
    case omaha::AppData::ACTIVE_NOTRUN:
      str = _T("NOT RUN");
      break;
    case omaha::AppData::ACTIVE_RUN:
      str = _T("RUN");
      break;
    case omaha::AppData::ACTIVE_UNKNOWN:
      str = _T("UNKNOWN");
      break;
    default:
      ASSERT1(false);
      break;
  }
  return str;
}

CString BoolToString(bool val) {
  return val ? _T("TRUE") : _T("FALSE");
}

// TODO(omaha): This should use display_name if available. Only Omaha needs to
// be hard-coded. Maybe write its name during Setup instead.
CString GuidToFriendlyAppName(const GUID& guid) {
  struct MapGuidToName {
    const TCHAR* guid;
    const TCHAR* name;
  };

  // IMPORTANT: Only put released products in this list since this tool will go
  // to customers.
  MapGuidToName guid_to_name[] = {
    {_T("{283EAF47-8817-4c2b-A801-AD1FADFB7BAA}"), _T("Gears")},
    {_T("{430FD4D0-B729-4F61-AA34-91526481799D}"), _T("Google Update")},
    {_T("{8A69D345-D564-463C-AFF1-A69D9E530F96}"), _T("Chrome")},
  };

  CString str = _T("unknown");

  for (int i = 0; i < arraysize(guid_to_name); ++i) {
    if (::IsEqualGUID(guid, StringToGuid(guid_to_name[i].guid))) {
      str = guid_to_name[i].name;
      break;
    }
  }

  return str;
}

}  // namespace

DataDumperAppManager::DataDumperAppManager() {
}

DataDumperAppManager::~DataDumperAppManager() {
}

HRESULT DataDumperAppManager::Process(const DumpLog& dump_log,
                                      const GoopdumpCmdLineArgs& args) {
  UNREFERENCED_PARAMETER(args);

  DumpHeader header(dump_log, _T("AppManager Data"));

  if (args.is_machine) {
    dump_log.WriteLine(_T("--- MACHINE APPMANAGER DATA ---"));
    AppManager app_manager(true);
    DumpAppManagerData(dump_log, app_manager);
    DumpRawRegistryData(dump_log, true);
  }

  if (args.is_user) {
    dump_log.WriteLine(_T("--- USER APPMANAGER DATA ---"));
    AppManager app_manager(false);
    DumpAppManagerData(dump_log, app_manager);
    DumpRawRegistryData(dump_log, false);
  }

  return S_OK;
}

void DataDumperAppManager::DumpAppManagerData(const DumpLog& dump_log,
                                              const AppManager& app_manager) {
  ProductDataVector products;
  HRESULT hr = app_manager.GetRegisteredProducts(&products);
  if (FAILED(hr)) {
    dump_log.WriteLine(_T("Failed GetRegisteredProducts() hr=0x%x"), hr);
    return;
  }

  for (size_t i = 0; i < products.size(); ++i) {
    const ProductData& product_data = products[i];
    const AppData& data = product_data.app_data();

    dump_log.WriteLine(_T("---------- APP ----------"));
    dump_log.WriteLine(_T("app name:\t%s"),
                       GuidToFriendlyAppName(data.app_guid()));
    dump_log.WriteLine(_T("guid:\t\t%s"), GuidToString(data.app_guid()));
    // parent_app_guid is not displayed.
    dump_log.WriteLine(_T("is_machine_app:\t%s"),
                       BoolToString(data.is_machine_app()));
    dump_log.WriteLine(_T("version:\t%s"), data.version());
    dump_log.WriteLine(_T("prev_version:\t%s"), data.previous_version());
    dump_log.WriteLine(_T("language:\t%s"), data.language());
    dump_log.WriteLine(_T("ap:\t\t%s"), data.ap());
    dump_log.WriteLine(_T("ttt:\t\t%s"), data.tt_token());
    dump_log.WriteLine(_T("iid:\t\t%s"), GuidToString(data.iid()));
    dump_log.WriteLine(_T("brand:\t\t%s"), data.brand_code());
    dump_log.WriteLine(_T("client:\t\t%s"), data.client_id());
    dump_log.WriteLine(_T("referral:\t\t%s"), data.referral_id());
    dump_log.WriteLine(_T("install_time_diff_sec:\t%u"),
                       data.install_time_diff_sec());
    dump_log.WriteLine(_T("is_oem_install:\t%s"),
                       BoolToString(data.is_oem_install()));
    dump_log.WriteLine(_T("is_eula_accepted:\t%s"),
                       BoolToString(data.is_eula_accepted()));
    // TODO(omaha): Use display_name above and note its use on this line.
    dump_log.WriteLine(_T("browser_type:\t\t%u"), data.browser_type());

    // The following are not saved and thus should always have default values.
    dump_log.WriteLine(_T("install_source:\t\t%s"), data.install_source());
    dump_log.WriteLine(_T("encoded_installer_data:\t\t%s"),
                       data.encoded_installer_data());
    dump_log.WriteLine(_T("install_data_index:\t\t%s"),
                       data.install_data_index());

    dump_log.WriteLine(_T("usage_stats_enable:\t\t%u"),
                       data.usage_stats_enable());
    dump_log.WriteLine(_T("did_run:\t%s"),
                       ActiveStateToString(data.did_run()));

    // The following are not saved and thus should always have default values.
    dump_log.WriteLine(_T("is_uninstalled:\t%s"),
                       BoolToString(data.is_uninstalled()));
    dump_log.WriteLine(_T("is_update_disabled:\t%s"),
                       BoolToString(data.is_update_disabled()));
    dump_log.WriteLine(_T(""));
  }
}

void DataDumperAppManager::DumpRawRegistryData(const DumpLog& dump_log,
                                               bool is_machine) {
  dump_log.WriteLine(_T("--- RAW REGISTRY DATA ---"));
  CString key_name = ConfigManager::Instance()->registry_clients(is_machine);
  DumpRegistryKeyData(dump_log, key_name);
}

}  // namespace omaha

