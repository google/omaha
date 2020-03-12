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
//
// TODO(omaha): for reliability sake, the code that sets up the exception
// handler should be very minimalist and not call so much outside of this
// module. One idea is to split the crash module in two: one minimalist part
// responsible for setting up the exception handler and one that is uploading
// the crash.

#include "omaha/goopdate/crash.h"

#include <windows.h>
#include <shlwapi.h>
#include <atlbase.h>
#include <atlstr.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/app_util.h"
#include "omaha/base/const_addresses.h"
#include "omaha/base/const_object_names.h"
#include "omaha/base/constants.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/omaha_version.h"
#include "omaha/base/path.h"
#include "omaha/base/reg_key.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/scope_guard.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/base/vistautil.h"
#include "omaha/common/command_line_builder.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/crash_utils.h"
#include "omaha/common/event_logger.h"
#include "omaha/common/goopdate_utils.h"
#include "omaha/common/lang.h"
#include "omaha/common/stats_uploader.h"
#include "omaha/goopdate/goopdate_metrics.h"
#include "omaha/third_party/smartany/scoped_any.h"
#include "third_party/breakpad/src/client/windows/crash_generation/client_info.h"
#include "third_party/breakpad/src/client/windows/sender/crash_report_sender.h"

using google_breakpad::CrashReportSender;
using google_breakpad::ReportResult;

namespace omaha {

const TCHAR* const CrashReporter::kDefaultProductName =
    SHORT_COMPANY_NAME _T(" Error Reporting");

CrashReporter::CrashReporter()
  : is_machine_(false),
    crash_report_url_(kUrlCrashReport),
    max_reports_per_day_(INT_MAX) {
}

HRESULT CrashReporter::Initialize(bool is_machine) {
  is_machine_ = is_machine;

  HRESULT hr = crash_utils::InitializeCrashDir(is_machine, &crash_dir_);
  if (FAILED(hr)) {
    return hr;
  }
  ASSERT1(!crash_dir_.IsEmpty());
  CORE_LOG(L2, (_T("[crash dir %s]"), crash_dir_));

  // The checkpoint file maintains state information for the crash report
  // client, such as the number of reports per day successfully sent.
  checkpoint_file_ = ConcatenatePath(crash_dir_, _T("checkpoint"));
  if (checkpoint_file_.IsEmpty()) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  return S_OK;
}


HRESULT CrashReporter::Report(const CString& crash_filename,
                              const CString& custom_info_filename) {
  ASSERT1(!crash_dir_.IsEmpty());

  ConfigManager* cm = ConfigManager::Instance();
  const bool can_upload_omaha_crashes =
      cm->CanCollectStats(is_machine_) && cm->CanUseNetwork(is_machine_);
  bool can_upload = false;

  ParameterMap parameters;

  const bool is_out_of_process = !custom_info_filename.IsEmpty();
  if (is_out_of_process) {
    ++metric_oop_crashes_total;
    HRESULT hr = ReadCustomInfoFile(custom_info_filename, &parameters);
    if (FAILED(hr)) {
      OPT_LOG(L2, (_T("[ReadParamsFromCustomInfoFile failed][%#08x]"), hr));
    }

    // OOP crashes for other products are always uploaded.
    const CString product_name = ReadMapProductName(parameters);
    can_upload = (product_name != kCrashOmahaProductName) ?
                  true : can_upload_omaha_crashes;
  } else {
    // Some of the Omaha crashes are handled in-process, for instance, when
    // the crash handlers are not available or the OOP registration failed for
    // any reason.
    ++metric_crashes_total;
    BuildParametersFromGoopdate(&parameters);
    can_upload = can_upload_omaha_crashes;
  }

  if (cm->AlwaysAllowCrashUploads()) {
    can_upload = true;
  }

  // All received crashes are logged in the Windows event log for applications,
  // unless the logging is disabled by the administrator.
  const CString product_name = GetProductNameForEventLogging(parameters);

  CString event_text;
  SafeCStringFormat(&event_text,
      _T("%s has encountered a fatal error.\r\n")
      _T("ver=%s;lang=%s;guid=%s;is_machine=%d;oop=%d;upload=%d;minidump=%s"),
      product_name,
      ReadMapValue(parameters, _T("ver")),
      ReadMapValue(parameters, _T("lang")),
      ReadMapValue(parameters, _T("guid")),
      is_machine_,
      is_out_of_process,
      can_upload,
      crash_filename);
  VERIFY_SUCCEEDED(WriteToWindowsEventLog(EVENTLOG_ERROR_TYPE,
                                           kCrashReportEventId,
                                           product_name,
                                           event_text));
  // Upload the crash.
  CString report_id;
  HRESULT hr = DoSendCrashReport(can_upload,
                                 is_out_of_process,
                                 crash_filename,
                                 parameters,
                                 &report_id);

  // Delete the minidump, and the custom info file if it exists.  Then, clean
  // any stale crashes.
  ::DeleteFile(crash_filename);
  if (is_out_of_process) {
    ::DeleteFile(custom_info_filename);
  }
  CleanStaleCrashes();

  return hr;
}

HRESULT CrashReporter::ReadCustomInfoFile(const CString& custom_info_filename,
                                          ParameterMap* parameters) {
  ASSERT1(!custom_info_filename.IsEmpty());
  ASSERT1(parameters);
  parameters->clear();

  std::map<CString, CString> parameters_temp;
  HRESULT hr = goopdate_utils::ReadNameValuePairsFromFile(
      custom_info_filename,
      kCustomClientInfoGroup,
      &parameters_temp);
  if (FAILED(hr)) {
    return hr;
  }

  std::map<CString, CString>::const_iterator iter;
  for (iter = parameters_temp.begin(); iter != parameters_temp.end(); ++iter) {
    (*parameters)[iter->first.GetString()] = iter->second.GetString();
  }

  return S_OK;
}

void CrashReporter::BuildParametersFromGoopdate(ParameterMap* parameters) {
  ASSERT1(parameters);
  parameters->clear();

  // In the case of a dump that was created by the in-process Breakpad handler,
  // we don't get a custom info file.  We'd really like to fix this; in the
  // meantime, it's a reasonable guess that the version of Omaha that crashed
  // is the same version that's running now to report the crash.  So, fill out
  // the parameter map using the data from the currently running Omaha.

  (*parameters)[_T("prod")]   = kCrashOmahaProductName;
  (*parameters)[_T("ver")]    = crash_utils::GetCrashVersionString();
  (*parameters)[_T("guid")] = goopdate_utils::GetUserIdLazyInit(is_machine_);
  (*parameters)[_T("lang")]   = lang::GetDefaultLanguage(is_machine_);
}

// Backs up the crash and uploads it if allowed to.
HRESULT CrashReporter::DoSendCrashReport(bool can_upload,
                                         bool is_out_of_process,
                                         const CString& crash_filename,
                                         const ParameterMap& parameters,
                                         CString* report_id) {
  ASSERT1(!crash_filename.IsEmpty());
  ASSERT1(report_id);
  report_id->Empty();

  if (!File::Exists(crash_filename)) {
    return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
  }

  CString product_name = ReadMapProductName(parameters);
  VERIFY_SUCCEEDED(SaveLastCrash(crash_filename, product_name));

  HRESULT hr = S_OK;
  if (can_upload) {
    hr = UploadCrash(is_out_of_process, crash_filename, parameters, report_id);
  } else {
    CORE_LOG(L2, (_T("[crash uploads are not allowed]")));
  }

  return hr;
}

HRESULT CrashReporter::UploadCrash(bool is_out_of_process,
                                   const CString& crash_filename,
                                   const ParameterMap& parameters,
                                   CString* report_id) {
  ASSERT1(report_id);
  report_id->Empty();

  // Calling this avoids crashes in WinINet. See: http://b/1258692
  EnsureRasmanLoaded();

  ASSERT1(!crash_dir_.IsEmpty());
  ASSERT1(!checkpoint_file_.IsEmpty());

  // Do best effort to send the crash. If it can't communicate with the backend,
  // it retries a few times over a few hours time interval.
  HRESULT hr = S_OK;
  for (int i = 0; i != kCrashReportAttempts; ++i) {
    std::wstring report_code;
    CrashReportSender sender(checkpoint_file_.GetString());
    sender.set_max_reports_per_day(max_reports_per_day_);
    OPT_LOG(L2, (_T("[Uploading crash report]")
                 _T("[%s][%s]"), crash_report_url_, crash_filename));
    ASSERT1(!crash_report_url_.IsEmpty());
    std::map<std::wstring, std::wstring> crash_files;
    crash_files[L"upload_file_minidump"] = crash_filename.GetString();
    ReportResult res = sender.SendCrashReport(crash_report_url_.GetString(),
                                              parameters,
                                              crash_files,
                                              &report_code);
    switch (res) {
      case google_breakpad::RESULT_SUCCEEDED:
        report_id->SetString(report_code.c_str());
        hr = S_OK;
        break;

      case google_breakpad::RESULT_FAILED:
        OPT_LOG(L2, (_T("[Crash report failed but it will retry sending]")));
        ::Sleep(kCrashReportResendPeriodMs);
        hr = E_FAIL;
        break;

      case google_breakpad::RESULT_REJECTED:
        hr = GOOPDATE_E_CRASH_REJECTED;
        break;

      case google_breakpad::RESULT_THROTTLED:
        hr = GOOPDATE_E_CRASH_THROTTLED;
        break;

      default:
        hr = E_FAIL;
        break;
    }

    // Continue the retry loop only when it could not contact the server.
    if (res != google_breakpad::RESULT_FAILED) {
      break;
    }
  }

  OPT_LOG(L2, (_T("[crash report code = %s]"), *report_id));

  CString product_name = GetProductNameForEventLogging(parameters);

  CString event_text;
  uint16 event_type(0);
  if (!report_id->IsEmpty()) {
    event_type = EVENTLOG_INFORMATION_TYPE;
    SafeCStringFormat(&event_text, _T("Crash uploaded. Id=%s."), *report_id);
  } else {
    ASSERT1(FAILED(hr));
    event_type = EVENTLOG_WARNING_TYPE;
    SafeCStringFormat(&event_text, _T("Crash not uploaded. Error=0x%x."), hr);
  }
  VERIFY_SUCCEEDED(WriteToWindowsEventLog(event_type,
                                           kCrashUploadEventId,
                                           product_name,
                                           event_text));

  UpdateCrashUploadMetrics(is_out_of_process, hr);

  return hr;
}

HRESULT CrashReporter::SaveLastCrash(const CString& crash_filename,
                                     const CString& product_name) {
  CORE_LOG(L3, (_T("[Crash::SaveLastCrash][%s][%s]"),
                crash_filename, product_name));

  if (product_name.IsEmpty()) {
    return E_INVALIDARG;
  }
  CString tmp;
  SafeCStringFormat(&tmp, _T("%s-last.dmp"), product_name);
  CString save_filename = ConcatenatePath(crash_dir_, tmp);
  if (save_filename.IsEmpty()) {
    return GOOPDATE_E_PATH_APPEND_FAILED;
  }

  CORE_LOG(L2, (_T("[Crash::SaveLastCrash][to %s][from %s]"),
                save_filename, crash_filename));

  if (0 == ::CopyFile(crash_filename, save_filename, false)) {
    HRESULT hr = HRESULTFromLastError();
    CORE_LOG(LE, (_T("[CopyFile failed][%#08x]"), hr));
    return hr;
  }

  return S_OK;
}

HRESULT CrashReporter::CleanStaleCrashes() {
  CORE_LOG(L3, (_T("[Crash::CleanStaleCrashes]")));

  // ??- sequence is a c++ trigraph corresponding to a ~. Escape it.
  const TCHAR kWildCards[] = _T("???????\?-???\?-???\?-???\?-????????????.dmp");
  std::vector<CString> crash_files;
  HRESULT hr = File::GetWildcards(crash_dir_, kWildCards, &crash_files);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[GetWildcards failed][%#08x]"), hr));
    return hr;
  }

  time64 now = GetCurrent100NSTime();
  for (size_t i = 0; i != crash_files.size(); ++i) {
    CORE_LOG(L3, (_T("[found crash file][%s]"), crash_files[i]));
    FILETIME creation_time = {0};
    if (SUCCEEDED(File::GetFileTime(crash_files[i],
                                    &creation_time,
                                    NULL,
                                    NULL))) {
      double time_diff =
          static_cast<double>(now - FileTimeToTime64(creation_time));
      if (abs(time_diff) >= kDaysTo100ns) {
        CORE_LOG(L3, (_T("[deleting stale crash file][%s]"), crash_files[i]));
        VERIFY1(::DeleteFile(crash_files[i]));
      }
    }
  }

  return S_OK;
}

// static
HRESULT CrashReporter::WriteToWindowsEventLog(uint16 type,
                                              uint32 id,
                                              const TCHAR* source,
                                              const TCHAR* description) {
  ASSERT1(source);
  ASSERT1(description);
  return EventLogger::ReportEvent(source,
                                  type,
                                  0,            // Category.
                                  id,
                                  1,            // Number of strings.
                                  &description,
                                  0,            // Raw data size.
                                  NULL);        // Raw data.
}

// static
CString CrashReporter::ReadMapValue(const ParameterMap& parameters,
                                    const CString& key) {
  ParameterMap::const_iterator it = parameters.find(key.GetString());
  if (it != parameters.end()) {
    return it->second.c_str();
  }
  return _T("");
}

// static
CString CrashReporter::ReadMapProductName(const ParameterMap& parameters) {
  CString prod = ReadMapValue(parameters, _T("prod"));
  if (!prod.IsEmpty()) {
    return prod;
  }
  return kDefaultProductName;
}

// static
CString CrashReporter::GetProductNameForEventLogging(
    const ParameterMap& parameters) {
  CString product_name = ReadMapProductName(parameters);
  if (product_name == kCrashOmahaProductName) {
    product_name.SetString(kAppName);
  }
  return product_name;
}

// static
void CrashReporter::UpdateCrashUploadMetrics(bool is_out_of_process,
                                             HRESULT hr) {
  switch (hr) {
    case S_OK:
      if (is_out_of_process) {
        ++metric_oop_crashes_uploaded;
      } else {
        ++metric_crashes_uploaded;
      }
      break;

    case E_FAIL:
      if (is_out_of_process) {
        ++metric_oop_crashes_failed;
      } else {
        ++metric_crashes_failed;
      }
      break;

    case GOOPDATE_E_CRASH_THROTTLED:
      if (is_out_of_process) {
        ++metric_oop_crashes_throttled;
      } else {
        ++metric_crashes_throttled;
      }
      break;

    case GOOPDATE_E_CRASH_REJECTED:
      if (is_out_of_process) {
        ++metric_oop_crashes_rejected;
      } else {
        ++metric_crashes_rejected;
      }
      break;

    default:
      ASSERT1(false);
      break;
  }
}

}  // namespace omaha

