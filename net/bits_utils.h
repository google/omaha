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

#ifndef OMAHA_NET_BITS_UTILS_H__
#define OMAHA_NET_BITS_UTILS_H__

#include <windows.h>
#include <bits.h>
#include <atlbase.h>
#include <atlstr.h>
#include <functional>

namespace omaha {

// Gets the instance of BITS manager.
HRESULT GetBitsManager(IBackgroundCopyManager** bits_manager);

// Compares the local name of a job.
struct JobLocalNameEqual
    : public std::binary_function<IBackgroundCopyJob*, const TCHAR*, bool> {

  bool operator()(IBackgroundCopyJob* job, const TCHAR* local_name) const;
};

// Compares the display name of a job.
struct JobDisplayNameEqual
    : public std::binary_function<IBackgroundCopyJob*, const TCHAR*, bool> {

  bool operator()(IBackgroundCopyJob* job, const TCHAR* local_name) const;
};

// Finds a job that matches the given predicate.
// TODO(omaha): do we need to search across all users?
template<class Predicate>
HRESULT FindBitsJobIf(Predicate pred,
                      IBackgroundCopyManager* bits_manager,
                      IBackgroundCopyJob** job) {
  if (!bits_manager || !job || *job) {
    return E_INVALIDARG;
  }

  // Enumerate the jobs that belong to the calling user.
  CComPtr<IEnumBackgroundCopyJobs> jobs;
  HRESULT hr = bits_manager->EnumJobs(0, &jobs);
  if (FAILED(hr)) {
    return hr;
  }
  ULONG job_count = 0;
  hr = jobs->GetCount(&job_count);
  if (FAILED(hr)) {
    return hr;
  }
  for (size_t i = 0; i != job_count; ++i) {
    CComPtr<IBackgroundCopyJob> current_job;
    if (jobs->Next(1, &current_job, NULL) != S_OK) {
      break;
    }
    if (pred(current_job)) {
      *job = current_job.Detach();
      return S_OK;
    }
  }
  return E_FAIL;
}

// Sets using the implicit credentials to authenticate to proxy servers.
HRESULT SetProxyAuthImplicitCredentials(IBackgroundCopyJob* job,
                                        BG_AUTH_SCHEME auth_scheme);

// Sets credentials to authenticate to proxy servers. username and password can
// be NULL, in which case BITS will try connecting with implicit/autologon
// credentials.
HRESULT SetProxyAuthCredentials(IBackgroundCopyJob* job,
                                TCHAR* username,
                                TCHAR* password,
                                BG_AUTH_SCHEME auth_scheme);

// Returns an HTTP status code from the BITS error code.
int GetHttpStatusFromBitsError(HRESULT error);

// Cancels a job.
HRESULT CancelBitsJob(IBackgroundCopyJob* job);

// Converts a job state to a string.
CString JobStateToString(BG_JOB_STATE job_state);

// Converts a BITS auth scheme to a string.
CString BitsAuthSchemeToString(int auth_scheme);

}   // namespace omaha

#endif  // OMAHA_NET_BITS_UTILS_H__

