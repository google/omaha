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
// The download manager uses the network request to download the remote file.
// Once the download is complete, the download manager stores the file in
// the package cache, then it copies the file out to a location specified
// by the caller.

// TODO(omaha): the path where to copy the file is hardcoded. Change the
// class interface to allow the path as a parameter.

#include "omaha/goopdate/download_manager.h"

#include <shlwapi.h>

#include <algorithm>
#include <vector>

#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/file.h"
#include "omaha/base/logging.h"
#include "omaha/base/path.h"
#include "omaha/base/scoped_impersonation.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/string.h"
#include "omaha/base/synchronized.h"
#include "omaha/base/user_rights.h"
#include "omaha/base/utils.h"
#include "omaha/common/config_manager.h"
#include "omaha/common/const_goopdate.h"
#include "omaha/common/google_signaturevalidator.h"
#include "omaha/goopdate/model.h"
#include "omaha/goopdate/package_cache.h"
#include "omaha/goopdate/server_resource.h"
#include "omaha/goopdate/string_formatter.h"
#include "omaha/goopdate/worker_metrics.h"
#include "omaha/goopdate/worker_utils.h"
#include "omaha/net/bits_request.h"
#include "omaha/net/http_client.h"
#include "omaha/net/network_request.h"
#include "omaha/net/net_utils.h"
#include "omaha/net/simple_request.h"

namespace omaha {

namespace {

// Creates and initializes an instance of the NetworkRequest for the
// DownloadManager to use. Defines the fallback chain: BITS, WinHttp.
HRESULT CreateNetworkRequest(NetworkRequest** network_request_ptr) {
  NetworkConfig* network_config = NULL;
  NetworkConfigManager& network_manager = NetworkConfigManager::Instance();
  HRESULT hr = network_manager.GetUserNetworkConfig(&network_config);
  if (FAILED(hr)) {
    return hr;
  }
  const NetworkConfig::Session& session(network_config->session());
  NetworkRequest* network_request(new NetworkRequest(session));

  // TODO(omaha): provide a mechanism for different timeout values in
  // silent and interactive downloads.

  // BITS transfers files only when the job owner is logged on. If the process
  // "Run As" another user, an empty BITS job gets created in suspended state
  // but there is no way to manipulate the job, nor cancel it.
  bool is_logged_on = false;
  hr = UserRights::UserIsLoggedOnInteractively(&is_logged_on);
  if (SUCCEEDED(hr) && is_logged_on) {
    BitsRequest* bits_request(new BitsRequest);
    bits_request->set_minimum_retry_delay(kSecPerMin);
    bits_request->set_no_progress_timeout(5 * kSecPerMin);
    network_request->AddHttpRequest(bits_request);
  } else {
    ++metric_worker_download_skipped_bits_machine;
  }

  network_request->AddHttpRequest(new SimpleRequest);

  network_request->set_num_retries(1);
  *network_request_ptr = network_request;
  return S_OK;
}

// TODO(omaha): Unit test this method.
HRESULT ValidateSize(File* source_file, uint64 expected_size) {
  CORE_LOG(L3, (_T("[ValidateSize][%lld]"), expected_size));
  ASSERT1(source_file);
  ASSERT1(expected_size != 0);
  ASSERT(expected_size <= UINT_MAX,
         (_T("TODO(omaha): Add uint64 support to GetFileSizeUnopen().")));

  uint32 file_size(0);
  HRESULT hr = source_file->GetLength(&file_size);
  ASSERT1(SUCCEEDED(hr));
  if (FAILED(hr)) {
    return hr;
  }

  if (0 == file_size) {
    return GOOPDATEDOWNLOAD_E_FILE_SIZE_ZERO;
  } else if (file_size < expected_size) {
    return GOOPDATEDOWNLOAD_E_FILE_SIZE_SMALLER;
  } else if (file_size > expected_size) {
    return GOOPDATEDOWNLOAD_E_FILE_SIZE_LARGER;
  }

  ASSERT1(file_size == expected_size);
  return S_OK;
}

// Adds the corresponding EVENT_{INSTALL,UPDATE}_DOWNLOAD_FINISH ping events
// for the |download_metrics| provided as a parameter.
void AddDownloadMetricsPingEvents(
    const std::vector<DownloadMetrics>& download_metrics,
    App* app) {
  ASSERT1(app);

  for (size_t i = 0; i != download_metrics.size(); ++i) {
    const PingEvent::Results result = download_metrics[i].error ?
        PingEvent::EVENT_RESULT_ERROR : PingEvent::EVENT_RESULT_SUCCESS;
    PingEventPtr ping_event(new PingEventDownloadMetrics(app->is_update(),
                                                         result,
                                                         download_metrics[i]));
    app->AddPingEvent(ping_event);
  }
}

}  // namespace

DownloadManager::DownloadManager(bool is_machine)
    : lock_(NULL), is_machine_(false) {
  CORE_LOG(L3, (_T("[DownloadManager::DownloadManager]")));

  omaha::interlocked_exchange_pointer(&lock_,
                                      static_cast<Lockable*>(new LLock));
  __mutexScope(lock());

  is_machine_ = is_machine;

  package_cache_root_ =
      is_machine ?
      ConfigManager::Instance()->GetMachineSecureDownloadStorageDir() :
      ConfigManager::Instance()->GetUserDownloadStorageDir();

  CORE_LOG(L3, (_T("[package_cache_root][%s]"), package_cache_root()));

  package_cache_.reset(new PackageCache);
}

DownloadManager::~DownloadManager() {
  CORE_LOG(L3, (_T("[DownloadManager::~DownloadManager]")));

  ASSERT1(!IsBusy());

  delete &lock();
  omaha::interlocked_exchange_pointer(&lock_, static_cast<Lockable*>(NULL));
}

const Lockable& DownloadManager::lock() const {
  return *omaha::interlocked_exchange_pointer(&lock_, lock_);
}

bool DownloadManager::is_machine() const {
  __mutexScope(lock());
  return is_machine_;
}

CString DownloadManager::package_cache_root() const {
  __mutexScope(lock());
  return package_cache_root_;
}

PackageCache* DownloadManager::package_cache() {
  __mutexScope(lock());
  return package_cache_.get();
}

const PackageCache* DownloadManager::package_cache() const {
  __mutexScope(lock());
  return package_cache_.get();
}

HRESULT DownloadManager::Initialize() {
  HRESULT hr = package_cache()->Initialize(package_cache_root());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to initialize the package cache]0x%08x]"), hr));
    return hr;
  }

  hr = package_cache()->PurgeOldPackagesIfNecessary();
  if (FAILED(hr)) {
    CORE_LOG(LW, (_T("[PurgeOldPackagesIfNecessary failed][0x%08x]"), hr));
  }

  return S_OK;
}

CString DownloadManager::GetMessageForError(const ErrorContext& error_context,
                                            const CString& language) {
  CString message;
  StringFormatter formatter(language);

  switch (error_context.error_code) {
    case SIGS_E_INVALID_SIGNATURE:
    case GOOPDATEDOWNLOAD_E_FILE_SIZE_ZERO:
    case GOOPDATEDOWNLOAD_E_FILE_SIZE_SMALLER:
    case GOOPDATEDOWNLOAD_E_FILE_SIZE_LARGER:
    case GOOPDATEDOWNLOAD_E_AUTHENTICODE_VERIFICATION_FAILED:
      VERIFY_SUCCEEDED(formatter.LoadString(IDS_DOWNLOAD_HASH_MISMATCH,
                                             &message));
      break;
    case GOOPDATEDOWNLOAD_E_CACHING_FAILED:
      VERIFY_SUCCEEDED(formatter.FormatMessage(
          &message, IDS_CACHING_ERROR, error_context.extra_code1, &message));
      break;
    default:
      if (!worker_utils::FormatMessageForNetworkError(error_context.error_code,
                                                      language,
                                                      &message)) {
        VERIFY_SUCCEEDED(formatter.LoadString(IDS_DOWNLOAD_ERROR, &message));
      }
      break;
  }

  ASSERT1(!message.IsEmpty());
  return message;
}

HRESULT DownloadManager::DownloadApp(App* app) {
  CORE_LOG(L3, (_T("[DownloadManager::DownloadApp][0x%p]"), app));
  ASSERT1(app);

  // TODO(omaha3): Maybe rename these to include "app_". Maybe add package
  // metrics too.
  ++metric_worker_download_total;

  // We assume the number of packages does not change after download is started.
  // TODO(omaha3): Could be a problem if we allow installers to request more
  // packages (http://b/1969071), but we will have lots of other problems then.
  AppVersion* app_version = app->working_version();
  const size_t num_packages = app_version->GetNumberOfPackages();

  State* state = NULL;
  HRESULT hr = CreateStateForApp(app, &state);
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[CreateStateForApp failed][0x%08x]"), hr));
    return hr;
  }

  app->Downloading();

  CString message;
  hr = S_OK;

  for (size_t i = 0; i < num_packages; ++i) {
    Package* package(app_version->GetPackage(i));
    hr = DoDownloadPackage(package, state);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[DoDownloadPackage failed][%s][%s][0x%08x][%Iu]"),
                    app->display_name(), package->filename(), hr, i));
      message = GetMessageForError(ErrorContext(hr, error_extra_code1()),
                                   app->app_bundle()->display_language());
      break;
    }
  }

  if (SUCCEEDED(hr)) {
    app->DownloadComplete();
    app->MarkReadyToInstall();
  } else {
    app->Error(ErrorContext(hr, error_extra_code1()), message);
  }

  if (SUCCEEDED(hr)) {
    ++metric_worker_download_succeeded;
  }

  VERIFY_SUCCEEDED(DeleteStateForApp(app));

  return hr;
}

HRESULT DownloadManager::GetPackage(const Package* package,
                                    const CString& dir) const {
  const CString app_id(package->app_version()->app()->app_guid_string());
  const CString version(package->app_version()->version());
  const CString package_name(package->filename());

  const PackageCache::Key key(app_id, version, package_name);

  CORE_LOG(L3, (_T("[DownloadManager::GetPackage][%s]"), key.ToString()));

  const CString dest_file(ConcatenatePath(dir, package_name));
  CORE_LOG(L3, (_T("[destination file is '%s']"), dest_file));

  HRESULT hr = package_cache()->Get(key, dest_file, package->expected_hash());
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("[failed to get from cache][0x%08x]"), hr));
    return hr;
  }

  return S_OK;
}

bool DownloadManager::IsPackageAvailable(const Package* package) const {
  const CString app_id(package->app_version()->app()->app_guid_string());
  const CString version(package->app_version()->version());
  const CString package_name(package->filename());

  const PackageCache::Key key(app_id, version, package_name);

  CORE_LOG(L3, (_T("[DownloadManager::IsPackageAvailable][%s]"),
      key.ToString()));

  return package_cache()->IsCached(key, package->expected_hash());
}

// Attempts a package download by trying the fallback urls. It does not
// retry the download if the file validation fails.
// Assumes the packages are not created or destroyed while method is running.
HRESULT DownloadManager::DoDownloadPackage(Package* package, State* state) {
  ASSERT1(package);
  ASSERT1(state);

  App* app = package->app_version()->app();
  const CString app_id(app->app_guid_string());
  const CString version(package->app_version()->version());
  const CString package_name(package->filename());

  const ConfigManager& cm = *ConfigManager::Instance();
  // TODO(omaha): Since we don't currently have is_manual, check the least
  // restrictive case of true. It would be nice if we had is_manual. We'll see.
  ASSERT(SUCCEEDED(app->CheckGroupPolicy()),
         (_T("Downloading package app for disallowed app.")));

  if (app_id.IsEmpty() || package_name.IsEmpty()) {
    return E_INVALIDARG;
  }

  PackageCache::Key key(app_id, version, package_name);

  OPT_LOG(L3, (_T("[DownloadManager::DoDownloadPackage][%s]"),
      key.ToString()));

  if (!package_cache()->IsCached(key, package->expected_hash())) {
    CORE_LOG(L3, (_T("[The package is not cached]")));

    // TODO(omaha3): May need to consider the DownloadPackage case. Also, we may
    // want a error code that does not include "UPDATE". If this is a valid
    // case, need to add message for
    // GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED to GetMessageForError().
    // As of 9/7/2010, the offline case does not allow downloading if the
    // package cannot be found, so offline scenarios should never get here.
    if (!app->is_eula_accepted()) {
      ASSERT(false, (_T("Can't download because app EULA is not accepted.")));
      return GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED;
    }

    if (!ConfigManager::Instance()->CanUseNetwork(is_machine_)) {
      CORE_LOG(LE, (_T("[DoDownloadPackage][network use prohibited]")));
      return GOOPDATE_E_CANNOT_USE_NETWORK;
    }

    CString unique_filename_path;
    HRESULT hr = BuildUniqueFileName(package_name, &unique_filename_path);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[BuildUniqueFileName failed][0x%08x]"), hr));
      return hr;
    }

    NetworkRequest* network_request = state->network_request();

    network_request->set_callback(package);

    const std::vector<CString> download_base_urls(
        package->app_version()->download_base_urls());

    hr = E_FAIL;
    app->SetCurrentTimeAs(App::TIME_DOWNLOAD_START);
    for (size_t i = 0; i != download_base_urls.size(); ++i) {
      CString url;
      DWORD url_length(INTERNET_MAX_URL_LENGTH);
      hr = ::UrlCombine(download_base_urls[i],
                        package_name,
                        CStrBuf(url, INTERNET_MAX_URL_LENGTH),
                        &url_length,
                        0);
      if (FAILED(hr)) {
        continue;
      }

      ASSERT1(static_cast<DWORD>(url.GetLength()) == url_length);

      hr = DoDownloadPackageFromUrl(url, unique_filename_path, package, state);
      AddDownloadMetricsPingEvents(network_request->download_metrics(), app);
      if (SUCCEEDED(hr)) {
        app->set_source_url_index(static_cast<int>(i));
        break;
      }
    }

    VERIFY_SUCCEEDED(network_request->Close());
    DeleteBeforeOrAfterReboot(unique_filename_path);
    app->SetCurrentTimeAs(App::TIME_DOWNLOAD_COMPLETE);

    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[download failed from all urls][0x%08x]"), hr));
      return hr;
    }

    // Assumes that downloaded bytes equal to the expected package size.
    app->UpdateNumBytesDownloaded(package->expected_size());
  } else {
    OPT_LOG(L3, (_T("[package is cached]")));

    // TODO(omaha3): We probably need to update the download stats that
    // Package::OnProgress would set. It may be misleading to set
    // bytes_downloaded to anything other than zero, but App uses this to
    // calculate progress. I suppose we could add an is_complete field instead.
    // There is a related issue with the callback not being called with the
    // final size. See the TODO in the unit tests.
  }

  ASSERT1(package_cache()->IsCached(key, package->expected_hash()));
  return S_OK;
}

HRESULT DownloadManager::DoDownloadPackageFromUrl(const CString& url,
                                                  const CString& filename,
                                                  Package* package,
                                                  State* state) {
  OPT_LOG(L3, (_T("[starting download][from '%s'][to '%s']"), url, filename));

  // Downloading a file is a blocking call. It assumes the model is not
  // locked by the calling thread, otherwise other threads won't be able to
  // to access the model until the file download is complete.
  ASSERT1(!package->model()->IsLockedByCaller());

  NetworkRequest* network_request = state->network_request();

  HRESULT hr = network_request->DownloadFile(url, filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[DownloadFile failed][%#x]"), hr));
    worker_utils::AddHttpRequestDataToEventLog(
        hr,
        S_OK,
        network_request->http_status_code(),
        network_request->trace(),
        is_machine_);
    return hr;
  }

  // A file has been successfully downloaded from current url. Validate the file
  // and cache it.

  // We open the downloaded file as the current (impersonated) user. This
  // ensures that we are not reading any privileged files that are otherwise
  // inaccessible to the impersonated user.
  File source_file;
  hr = source_file.OpenShareMode(filename, false, false, FILE_SHARE_READ);
  if (FAILED(hr)) {
    return hr;
  }

  // We copy the file to the Package Cache unimpersonated, since the package
  // cache is in a privileged location.
  hr = CallAsSelfAndImpersonate3(this,
                                 &DownloadManager::CachePackage,
                                 static_cast<const Package*>(package),
                                 &source_file,
                                 &filename);
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[DownloadManager::CachePackage failed][%#x]"), hr));
  }

  return hr;
}


void DownloadManager::Cancel(App* app) {
  CORE_LOG(L3, (_T("[DownloadManager::Cancel][0x%p]"), app));
  ASSERT1(app);

  __mutexScope(lock());

  for (size_t i = 0; i != download_state_.size(); ++i) {
    if (app == download_state_[i]->app()) {
      VERIFY_SUCCEEDED(download_state_[i]->CancelNetworkRequest());
    }
  }
}

void DownloadManager::CancelAll() {
  CORE_LOG(L3, (_T("[DownloadManager::CancelAll]")));

  __mutexScope(lock());

  for (size_t i = 0; i != download_state_.size(); ++i) {
    VERIFY_SUCCEEDED(download_state_[i]->CancelNetworkRequest());
  }
}

bool DownloadManager::IsBusy() const {
  __mutexScope(lock());
  return !download_state_.empty();
}

HRESULT DownloadManager::PurgeAppLowerVersions(const CString& app_id,
                                               const CString& version) {
  return package_cache()->PurgeAppLowerVersions(app_id, version);
}

HRESULT DownloadManager::CachePackage(const Package* package,
                                      File* source_file,
                                      const CString* source_file_path) {
  ASSERT1(package);
  ASSERT1(source_file);

  const CString app_id(package->app_version()->app()->app_guid_string());
  const CString version(package->app_version()->version());
  const CString package_name(package->filename());
  PackageCache::Key key(app_id, version, package_name);

  HRESULT hr = E_UNEXPECTED;

  if (ConfigManager::Instance()->ShouldVerifyPayloadAuthenticodeSignature()) {
    hr = EnsureSignatureIsValid(*source_file_path);
    if (FAILED(hr)) {
      CORE_LOG(LE, (_T("[EnsureSignatureIsValid failed][%s][0x%08x]"),
                    package->filename(), hr));
      return GOOPDATEDOWNLOAD_E_AUTHENTICODE_VERIFICATION_FAILED;
    }
  }

  hr = package_cache()->Put(
      key, source_file, package->expected_hash());
  if (hr != SIGS_E_INVALID_SIGNATURE) {
    if (FAILED(hr)) {
      set_error_extra_code1(static_cast<int>(hr));
      return GOOPDATEDOWNLOAD_E_CACHING_FAILED;
    }
    return hr;
  }

  // Get a more specific error if possible.
  // TODO(omaha): It would be nice to detect that we downloaded a proxy
  // page and tell the user this. It would be even better if we could
  // display it; that would require a lot more plumbing.
  HRESULT size_hr = ValidateSize(source_file, package->expected_size());
  if (FAILED(size_hr)) {
    hr = size_hr;
  }

  return hr;
}

HRESULT DownloadManager::EnsureSignatureIsValid(const CString& file_path) {
  const TCHAR* ext = ::PathFindExtension(file_path);
  ASSERT1(ext);
  if (*ext != _T('\0')) {
    ext++;  // Skip the dot.
    for (size_t i = 0; i < arraysize(kAuthenticodeVerifiableExtensions); ++i) {
      if (CString(kAuthenticodeVerifiableExtensions[i]).CompareNoCase(ext)
          == 0) {
        return VerifyGoogleAuthenticodeSignature(file_path, true);
      }
    }
  }
  return S_OK;
}

// The file is initially downloaded to a temporary unique name, to account
// for the case where the same file is downloaded by multiple callers.
HRESULT DownloadManager::BuildUniqueFileName(const CString& filename,
                                             CString* unique_filename) {
  ASSERT1(unique_filename);

  GUID guid(GUID_NULL);
  HRESULT hr = ::CoCreateGuid(&guid);
  if (FAILED(hr)) {
    CORE_LOG(L3, (_T("[CoCreateGuid failed][0x%08x]"), hr));
    return hr;
  }

  const CString temp_dir(ConfigManager::Instance()->GetTempDownloadDir());
  if (temp_dir.IsEmpty()) {
    return E_UNEXPECTED;
  }

  // Format of the unique file name is: <temp_download_dir>/<guid>-<filename>.
  CString temp_filename;
  SafeCStringFormat(&temp_filename, _T("%s-%s"), GuidToString(guid), filename);
  *unique_filename = ConcatenatePath(temp_dir, temp_filename);

  return unique_filename->IsEmpty() ?
         GOOPDATEDOWNLOAD_E_UNIQUE_FILE_PATH_EMPTY : S_OK;
}

HRESULT DownloadManager::CreateStateForApp(App* app, State** state) {
  ASSERT1(app);
  ASSERT1(state);

  *state = NULL;

  NetworkRequest* network_request = NULL;
  HRESULT hr = CreateNetworkRequest(&network_request);
  if (FAILED(hr)) {
    return hr;
  }

  ASSERT1(network_request);

  const bool use_background_priority =
                  (app->app_bundle()->priority() < INSTALL_PRIORITY_HIGH);
  network_request->set_low_priority(use_background_priority);

  network_request->set_proxy_auth_config(
      app->app_bundle()->GetProxyAuthConfig());

  std::unique_ptr<State> state_ptr(new State(app, network_request));

  __mutexBlock(lock()) {
    download_state_.push_back(state_ptr.release());
    *state = download_state_.back();
  }

  return S_OK;
}

HRESULT DownloadManager::DeleteStateForApp(App* app) {
  ASSERT1(app);

  __mutexScope(lock());

  typedef std::vector<State*>::iterator Iter;
  for (Iter it(download_state_.begin()); it != download_state_.end(); ++it) {
    if (app == (*it)->app()) {
      delete *it;
      download_state_.erase(it);
      return S_OK;
    }
  }

  ASSERT1(false);

  return E_UNEXPECTED;
}

DownloadManager::State::State(App* app, NetworkRequest* network_request)
    : app_(app), network_request_(network_request) {
  ASSERT1(app);
  ASSERT1(network_request);
}

DownloadManager::State::~State() {
}

NetworkRequest* DownloadManager::State::network_request() const {
  ASSERT1(ConfigManager::Instance()->CanUseNetwork(
                                         app_->app_bundle()->is_machine()));

  return network_request_.get();
}

HRESULT DownloadManager::State::CancelNetworkRequest() {
  return network_request_->Cancel();
}

}  // namespace omaha
