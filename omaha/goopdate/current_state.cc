// Copyright 2009 Google Inc.
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

#include "omaha/goopdate/current_state.h"
#include <stdint.h>
#include <limits>
#include <atlsafe.h>
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"

namespace omaha {

HRESULT CurrentAppState::Create(
    LONG state_value,
    const CString& available_version,
    ULONGLONG bytes_downloaded,
    ULONGLONG total_bytes_to_download,
    LONG download_time_remaining_ms,
    ULONGLONG next_retry_time,
    LONG install_progress_percentage,
    LONG install_time_remaining_ms,
    bool is_canceled,
    LONG error_code,
    LONG extra_code1,
    const CString& completion_message,
    LONG installer_result_code,
    LONG installer_result_extra_code1,
    const CString& post_install_launch_command_line,
    const CString& post_install_url,
    PostInstallAction post_install_action,
    CComObject<CurrentAppState>** state) {
  ASSERT1(state);
  ASSERT1(state_value);

  HRESULT hr = CComObject<CurrentAppState>::CreateInstance(state);
  if (FAILED(hr)) {
    return hr;
  }

  (*state)->state_value_ = state_value;
  (*state)->available_version_ = available_version.AllocSysString();
  (*state)->bytes_downloaded_ = bytes_downloaded;
  (*state)->total_bytes_to_download_ = total_bytes_to_download;
  (*state)->download_time_remaining_ms_ = download_time_remaining_ms;
  (*state)->next_retry_time_ = next_retry_time;
  (*state)->install_progress_percentage_ = install_progress_percentage;
  (*state)->install_time_remaining_ms_ = install_time_remaining_ms;
  (*state)->is_canceled_ = is_canceled ? VARIANT_TRUE : VARIANT_FALSE;
  (*state)->error_code_ = error_code;
  (*state)->extra_code1_ = extra_code1;
  (*state)->completion_message_ = completion_message.AllocSysString();
  (*state)->installer_result_code_ = installer_result_code;
  (*state)->installer_result_extra_code1_ = installer_result_extra_code1;
  (*state)->post_install_launch_command_line_ =
      post_install_launch_command_line.AllocSysString();
  (*state)->post_install_url_ = post_install_url.AllocSysString();
  (*state)->post_install_action_ = post_install_action;

  return S_OK;
}

CurrentAppState::CurrentAppState()
    : m_bRequiresSave(TRUE),
      state_value_(0),
      bytes_downloaded_(0),
      total_bytes_to_download_(0),
      download_time_remaining_ms_(0),
      next_retry_time_(0),
      install_progress_percentage_(0),
      install_time_remaining_ms_(0),
      is_canceled_(VARIANT_FALSE),
      error_code_(0),
      extra_code1_(0),
      installer_result_code_(0),
      installer_result_extra_code1_(0),
      post_install_action_(0) {
}

CurrentAppState::~CurrentAppState() {
}

// ICurrentState.
// No locks are necessary because a copy of this object is returned to the
// client.
// TODO(omaha3): Perhaps we should set all the properties to valid values
// regardless of the stateValue.
// Or perhaps there are some good asserts we can and probably should do. Maybe
// we need a helper method such as IsStateOrLater() that would handle the
// non-contiguous issues, such as STATE_NO_UPDATE and STATE_PAUSED. Then, we
// could ASSERT1(IsStateOrLater(STATE_UPDATE_AVAILABLE));

STDMETHODIMP CurrentAppState::get_stateValue(LONG* state_value) {
  ASSERT1(state_value);

  *state_value = state_value_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_availableVersion(BSTR* available_version) {
  ASSERT1(available_version);

  *available_version = available_version_.Copy();
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_bytesDownloaded(ULONG* bytes_downloaded) {
  ASSERT1(bytes_downloaded);

  // Firefox does not support uint32...
  if (bytes_downloaded_ >
      static_cast<ULONGLONG>(std::numeric_limits<int32_t>::max())) {
    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
  }
  *bytes_downloaded = static_cast<ULONG>(bytes_downloaded_);
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_totalBytesToDownload(
    ULONG* total_bytes_to_download) {
  ASSERT1(total_bytes_to_download);

  // Firefox does not support uint32...
  if (total_bytes_to_download_ >
      static_cast<ULONGLONG>(std::numeric_limits<int32_t>::max())) {
    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
  }
  *total_bytes_to_download = static_cast<ULONG>(total_bytes_to_download_);
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_downloadTimeRemainingMs(
    LONG* download_time_remaining_ms) {
  ASSERT1(download_time_remaining_ms);

  *download_time_remaining_ms = download_time_remaining_ms_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_nextRetryTime(ULONGLONG* next_retry_time) {
  ASSERT1(next_retry_time);

  *next_retry_time = next_retry_time_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_installProgress(
    LONG* install_progress_percentage) {

  ASSERT1(install_progress_percentage);
  *install_progress_percentage = install_progress_percentage_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_installTimeRemainingMs(
    LONG* install_time_remaining_ms) {

  ASSERT1(install_time_remaining_ms);
  *install_time_remaining_ms = install_time_remaining_ms_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_isCanceled(VARIANT_BOOL* is_canceled) {
  ASSERT1(is_canceled);

  *is_canceled = is_canceled_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_errorCode(LONG* error_code) {
  ASSERT1(error_code);

  *error_code = error_code_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_extraCode1(LONG* extra_code1) {
  ASSERT1(extra_code1);

  *extra_code1 = extra_code1_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_completionMessage(
    BSTR* completion_message) {
  ASSERT1(completion_message);

  *completion_message = completion_message_.Copy();
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_installerResultCode(
    LONG* installer_result_code) {
  ASSERT1(installer_result_code);

  *installer_result_code = installer_result_code_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_installerResultExtraCode1(
    LONG* installer_result_extra_code1) {
  ASSERT1(installer_result_extra_code1);

  *installer_result_extra_code1 = installer_result_extra_code1_;
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_postInstallLaunchCommandLine(
    BSTR* post_install_launch_command_line) {
  ASSERT1(post_install_launch_command_line);

  *post_install_launch_command_line =
      post_install_launch_command_line_.Copy();
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_postInstallUrl(BSTR* post_install_url) {
  ASSERT1(post_install_url);

  *post_install_url = post_install_url_.Copy();
  return S_OK;
}

STDMETHODIMP CurrentAppState::get_postInstallAction(
    LONG* post_install_action) {
  ASSERT1(post_install_action);
  *post_install_action = static_cast<LONG>(post_install_action_);
  return S_OK;
}

}  // namespace omaha
