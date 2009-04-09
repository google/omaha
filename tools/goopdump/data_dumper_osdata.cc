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

#include "omaha/tools/goopdump/data_dumper_osdata.h"

#include <atltime.h>
#include <pdh.h>
#include <psapi.h>
#include <security.h>
#include "omaha/common/reg_key.h"
#include "omaha/common/scope_guard.h"
#include "omaha/common/time.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

namespace omaha {

DataDumperOSData::DataDumperOSData() {
}

DataDumperOSData::~DataDumperOSData() {
}

HRESULT DataDumperOSData::Process(const DumpLog& dump_log,
                                    const GoopdumpCmdLineArgs& args) {
  UNREFERENCED_PARAMETER(args);

  DumpHeader header(dump_log, _T("Operating System Data"));

  CString os_version;
  CString service_pack;
  goopdate_utils::GetOSInfo(&os_version, &service_pack);
  dump_log.WriteLine(_T("OS Version:\t%s"), os_version);
  dump_log.WriteLine(_T("Service Pack:\t%s"), service_pack);

  TCHAR computer_name[MAX_PATH] = {0};
  ULONG computer_name_size = arraysize(computer_name);
  ::GetComputerNameEx(ComputerNameDnsFullyQualified,
                      computer_name,
                      &computer_name_size);
  dump_log.WriteLine(_T("Computer Name:\t%s"), computer_name);

  TCHAR user_name[MAX_PATH] = {0};
  ULONG user_name_size = arraysize(user_name);
  ::GetUserNameEx(NameSamCompatible, user_name, &user_name_size);
  dump_log.WriteLine(_T("User Name:\t%s"), user_name);

  TCHAR user_friendly_name[MAX_PATH] = {0};
  ULONG user_friendly_name_size = arraysize(user_friendly_name);
  ::GetUserNameEx(NameDisplay, user_friendly_name, &user_friendly_name_size);
  dump_log.WriteLine(_T("Friendly Name:\t%s"), user_friendly_name);

  CString system_uptime;
  if (SUCCEEDED(GetSystemUptime(&system_uptime))) {
    dump_log.WriteLine(_T("System Uptime:\t%s"), system_uptime);
  } else {
    dump_log.WriteLine(_T("System Uptime:\tNot Available"));
  }

  PERFORMANCE_INFORMATION perf_info = {0};
  perf_info.cb = sizeof(perf_info);
  if (::GetPerformanceInfo(&perf_info, sizeof(perf_info))) {
    dump_log.WriteLine(_T("Process Count: %d"), perf_info.ProcessCount);
    dump_log.WriteLine(_T("Handle Count:  %d"), perf_info.HandleCount);
    dump_log.WriteLine(_T("Thread Count:  %d"), perf_info.ThreadCount);

    size_t page_size = perf_info.PageSize;
    size_t mb = 1024 * 1024;

    dump_log.WriteLine(_T("Commit Total(MB):     %ld"),
                       perf_info.CommitTotal * page_size / mb);
    dump_log.WriteLine(_T("Commit Limit(MB):     %ld"),
                       perf_info.CommitLimit * page_size / mb);
    dump_log.WriteLine(_T("Commit Peak(MB):      %ld"),
                       perf_info.CommitPeak * page_size / mb);
    dump_log.WriteLine(_T("Kernel Total(MB):     %ld"),
                       perf_info.KernelTotal * page_size / mb);
    dump_log.WriteLine(_T("Kernel Paged(MB):     %ld"),
                       perf_info.KernelPaged * page_size / mb);
    dump_log.WriteLine(_T("Kernel NonPaged(MB):  %ld"),
                       perf_info.KernelNonpaged * page_size / mb);
    dump_log.WriteLine(_T("Page Size (KB):       %ld"),
                       perf_info.PageSize / 1024);
    dump_log.WriteLine(_T("Physical Avail(MB):   %ld"),
                       perf_info.PhysicalAvailable * page_size / mb);
    dump_log.WriteLine(_T("Physical Total(MB):   %ld"),
                       perf_info.PhysicalTotal * page_size / mb);
    dump_log.WriteLine(_T("System Cache(MB):     %ld"),
                       perf_info.SystemCache * page_size / mb);
  } else {
    dump_log.WriteLine(_T("Unable to Get Performance Info"));
  }

  return S_OK;
}

HRESULT DataDumperOSData::GetSystemUptime(CString* uptime) {
  ASSERT1(uptime);

  const TCHAR* kUptimeCounterPath = _T("\\\\.\\System\\System Up Time");

  HRESULT hr = S_OK;

  PDH_HQUERY perf_query = NULL;
  PDH_STATUS status = ::PdhOpenQuery(NULL, 0, &perf_query);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  ScopeGuard guard = MakeGuard(::PdhCloseQuery, perf_query);

  PDH_HCOUNTER uptime_counter = NULL;
  status = ::PdhAddCounter(perf_query,
                           kUptimeCounterPath,
                           0,
                           &uptime_counter);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  status = ::PdhCollectQueryData(perf_query);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  PDH_FMT_COUNTERVALUE uptime_value = {0};
  status = ::PdhGetFormattedCounterValue(uptime_counter,
                                         PDH_FMT_LARGE,
                                         NULL,
                                         &uptime_value);
  if (status != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(status);
  }

  CTimeSpan uptime_span(uptime_value.largeValue);
  *uptime = uptime_span.Format(_T("%D days, %H hours, %M minutes, %S seconds"));

  return S_OK;
}

}  // namespace omaha

