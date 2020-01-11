// Copyright 2009-2010 Google Inc.
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

#include "omaha/goopdate/package.h"

#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/time.h"
#include "omaha/base/utils.h"
#include "omaha/goopdate/model.h"

namespace omaha {

Package::Package(AppVersion* app_version)
    : ModelObject(app_version->model()),
      app_version_(app_version),
      expected_size_(0),
      bytes_downloaded_(0),
      bytes_total_(0),
      next_download_retry_time_(0),
      progress_sampler_(5 * kMsPerSec,    // Max sample time range.
                        1 * kMsPerSec),   // Min range for meaningful average.
      is_downloading_(false) {
}

Package::~Package() {
}

AppVersion* Package::app_version() {
  __mutexScope(model()->lock());
  return app_version_;
}

const AppVersion* Package::app_version() const {
  __mutexScope(model()->lock());
  return app_version_;
}

STDMETHODIMP Package::get(BSTR dir) const {
  return model()->GetPackage(this, CString(dir));
}

STDMETHODIMP Package::get_isAvailable(VARIANT_BOOL* is_available) const {
  ASSERT1(is_available);
  *is_available = model()->IsPackageAvailable(this);
  return S_OK;
}

STDMETHODIMP Package::get_filename(BSTR* filename_as_bstr) const {
  __mutexScope(model()->lock());
  ASSERT1(filename_as_bstr);
  *filename_as_bstr = CComBSTR(filename()).Detach();
  return S_OK;
}

// status_text can be NULL.
// TODO(omaha): Change bytes and bytes_total to uint64. Any logging will
// need to use %llu.
void Package::OnProgress(int bytes,
                         int bytes_total,
                         int status,
                         const TCHAR* status_text) {
  __mutexScope(model()->lock());

  UNREFERENCED_PARAMETER(status);
  UNREFERENCED_PARAMETER(status_text);
  ASSERT1(status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE ||
          status == WINHTTP_CALLBACK_STATUS_CONNECTING_TO_SERVER);

  CORE_LOG(L5, (_T("[Package::OnProgress][bytes %d][bytes_total %d][status %d]")
                _T("[status_text '%s']"),
                bytes, bytes_total, status, status_text));

  // TODO(omaha): What do we do if the following condition - bytes_total
  // is not what we expect - fails? Omaha 2 just divided bytes by bytes_total
  // to come up with the percentage, ignoring size. It didn't have to deal with
  // multiple downloads (much) and pre-determined total download size.
  // As an example, App::GetDownloadProgress() needs to know the expected
  // size before we've downloaded any bytes, but it might also want to know if
  // we are going to download less bytes. This could happen, for example, if
  // a proxy returned an HTML page instead. (I had to disable the first assert
  // because it fails when the actual file does not match the expected size.)
  // Even worse, what if the bytes_total has a different non-zero number on
  // successive calls?

  // ASSERT1(bytes_total == 0 ||
  //         bytes_total == static_cast<int>(expected_size_));
  ASSERT1(bytes <= bytes_total);

  bytes_downloaded_ = bytes;
  bytes_total_ = bytes_total;

  progress_sampler_.AddSampleWithCurrentTimeStamp(bytes_downloaded_);
}

void Package::OnRequestBegin() {
  __mutexScope(model()->lock());
  next_download_retry_time_ = 0;
  bytes_downloaded_ = 0;
  bytes_total_ = 0;
  progress_sampler_.Reset();
}

void Package::OnRequestRetryScheduled(time64 next_download_retry_time) {
  __mutexScope(model()->lock());
  ASSERT1(next_download_retry_time >= GetCurrent100NSTime());
  next_download_retry_time_ = next_download_retry_time;
}

void Package::SetFileInfo(const CString& filename,
                          uint64 size,
                          const CString& expected_hash) {
  __mutexScope(model()->lock());

  ASSERT1(!filename.IsEmpty());
  ASSERT1(0 < size);
  ASSERT1(!expected_hash.IsEmpty());

  filename_ = filename;
  expected_size_ = size;
  expected_hash_ = expected_hash;
}

CString Package::filename() const {
  __mutexScope(model()->lock());
  ASSERT1(!filename_.IsEmpty());
  return filename_;
}

uint64 Package::expected_size() const {
  __mutexScope(model()->lock());
  return expected_size_;
}

CString Package::expected_hash() const {
  __mutexScope(model()->lock());
  ASSERT1(!expected_hash_.IsEmpty());
  return expected_hash_;
}

uint64 Package::bytes_downloaded() const {
  __mutexScope(model()->lock());
  return bytes_downloaded_;
}

time64 Package::next_download_retry_time() const {
  __mutexScope(model()->lock());
  return next_download_retry_time_;
}

LONG Package::GetEstimatedRemainingDownloadTimeMs() const {
  __mutexScope(model()->lock());

  const LONG kUnknownRemainingTime = -1;

  if (bytes_total_ == 0) {  // Don't know how many bytes to download.
    return kUnknownRemainingTime;
  }

  if (bytes_total_ == bytes_downloaded_) {
    return 0;
  }

  LONG time_remaining_ms = kUnknownRemainingTime;
  int average_speed = progress_sampler_.GetAverageProgressPerMs();
  if (average_speed == ProgressSampler<int>::kUnknownProgressPerMs) {
    return kUnknownRemainingTime;
  }

  if (bytes_total_ >= bytes_downloaded_ && average_speed > 0) {
    time_remaining_ms = static_cast<LONG>(
        CeilingDivide(bytes_total_ - bytes_downloaded_, average_speed));
  }

  return time_remaining_ms;
}

STDMETHODIMP PackageWrapper::get(BSTR dir) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get(dir);
}

STDMETHODIMP PackageWrapper::get_isAvailable(VARIANT_BOOL* is_available) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_isAvailable(is_available);
}

STDMETHODIMP PackageWrapper::get_filename(BSTR* filename) {
  __mutexScope(model()->lock());
  return wrapped_obj()->get_filename(filename);
}

}  // namespace omaha
