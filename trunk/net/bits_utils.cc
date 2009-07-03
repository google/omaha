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

#include "omaha/net/bits_utils.h"

#include <windows.h>
#include <winhttp.h>
#include <cstring>
#include "omaha/common/debug.h"
#include "omaha/common/error.h"
#include "omaha/common/logging.h"
#include "omaha/common/utils.h"
#include "omaha/common/scoped_ptr_cotask.h"

namespace omaha {

// Gets the instance of BITS manager.
HRESULT GetBitsManager(IBackgroundCopyManager** bits_manager) {
  ASSERT1(bits_manager);
  if (*bits_manager) {
    (*bits_manager)->Release();
  }
  *bits_manager = NULL;

  CComPtr<IBackgroundCopyManager> object;
  HRESULT hr = object.CoCreateInstance(__uuidof(BackgroundCopyManager));
  if (FAILED(hr)) {
    OPT_LOG(LE, (_T("[failed to get BITS interface][0x%08x]"), hr));
    const HRESULT kServiceDisabled(HRESULT_FROM_WIN32(ERROR_SERVICE_DISABLED));
    return (hr == kServiceDisabled) ? CI_E_BITS_DISABLED : hr;
  }
  *bits_manager = object.Detach();
  return S_OK;
}

bool JobLocalNameEqual::operator()(IBackgroundCopyJob* job,
                                   const TCHAR* name) const {
  ASSERT1(job);
  ASSERT1(name);
  CComPtr<IEnumBackgroundCopyFiles> files;
  if (FAILED(job->EnumFiles(&files))) {
    return false;
  }
  ULONG file_count = 0;
  if (FAILED(files->GetCount(&file_count))) {
    return false;
  }
  for (size_t i = 0; i != file_count; ++i) {
    CComPtr<IBackgroundCopyFile> file;
    if (files->Next(1, &file, NULL) == S_OK) {
      scoped_ptr_cotask<TCHAR> local_name;
      if (SUCCEEDED(file->GetLocalName(address(local_name)))) {
        if (_tcscmp(local_name.get(), name) == 0) {
          return true;
        }
      }
    }
  }
  return false;
}

bool JobDisplayNameEqual::operator()(IBackgroundCopyJob* job,
                                     const TCHAR* name) const {
  ASSERT1(job);
  ASSERT1(name);
  scoped_ptr_cotask<TCHAR> display_name;
  HRESULT hr = job->GetDisplayName(address(display_name));
  if (SUCCEEDED(hr)) {
    return _tcscmp(display_name.get(), name) == 0;
  }
  return false;
}

HRESULT SetProxyAuthImplicitCredentials(IBackgroundCopyJob* job,
                                        BG_AUTH_SCHEME auth_scheme) {
  return SetProxyAuthCredentials(job, NULL, NULL, auth_scheme);
}

HRESULT SetProxyAuthCredentials(IBackgroundCopyJob* job,
                                TCHAR* username,
                                TCHAR* password,
                                BG_AUTH_SCHEME auth_scheme) {
  ASSERT1(job);
  CComQIPtr<IBackgroundCopyJob2> job2(job);
  if (!job2) {
    return E_NOINTERFACE;
  }
  BG_AUTH_CREDENTIALS auth_cred;
  SetZero(auth_cred);
  auth_cred.Target = BG_AUTH_TARGET_PROXY;
  auth_cred.Scheme = auth_scheme;
  auth_cred.Credentials.Basic.UserName = username;
  auth_cred.Credentials.Basic.Password = password;
  return job2->SetCredentials(&auth_cred);
}

int GetHttpStatusFromBitsError(HRESULT error) {
  // Bits errors are defined in bitsmsg.h. Although not documented, it is
  // clear that all errors corresponding to http status code have the high
  // word equal to 0x8019.
  bool is_valid = HIWORD(error) == 0x8019 &&
                  LOWORD(error) >= HTTP_STATUS_FIRST &&
                  LOWORD(error) <= HTTP_STATUS_LAST;
  return is_valid ? LOWORD(error) : 0;
}

HRESULT CancelBitsJob(IBackgroundCopyJob* job) {
  if (job) {
    BG_JOB_STATE job_state = BG_JOB_STATE_ERROR;
    HRESULT hr = job->GetState(&job_state);
    if (SUCCEEDED(hr) &&
        job_state != BG_JOB_STATE_CANCELLED &&
        job_state != BG_JOB_STATE_ACKNOWLEDGED) {
      HRESULT hr = job->Cancel();
      if (FAILED(hr)) {
        NET_LOG(LW, (_T("[CancelBitsJob failed][0x%08x]"), hr));
      }
      return hr;
    }
  }
  return S_OK;
}

#define RETURN_TSTR(x) case (x): return _T(#x)
CString JobStateToString(BG_JOB_STATE job_state) {
  switch (job_state) {
    RETURN_TSTR(BG_JOB_STATE_QUEUED);
    RETURN_TSTR(BG_JOB_STATE_CONNECTING);
    RETURN_TSTR(BG_JOB_STATE_TRANSFERRING);
    RETURN_TSTR(BG_JOB_STATE_TRANSIENT_ERROR);
    RETURN_TSTR(BG_JOB_STATE_ERROR);
    RETURN_TSTR(BG_JOB_STATE_TRANSFERRED);
    RETURN_TSTR(BG_JOB_STATE_SUSPENDED);
    RETURN_TSTR(BG_JOB_STATE_ACKNOWLEDGED);
    RETURN_TSTR(BG_JOB_STATE_CANCELLED);
  }
  return _T("");
}

CString BitsAuthSchemeToString(int auth_scheme) {
  switch (auth_scheme) {
    RETURN_TSTR(BG_AUTH_SCHEME_NEGOTIATE);
    RETURN_TSTR(BG_AUTH_SCHEME_NTLM);
    RETURN_TSTR(BG_AUTH_SCHEME_DIGEST);
    RETURN_TSTR(BG_AUTH_SCHEME_BASIC);
    RETURN_TSTR(0);
  }
  return _T("");
}

}   // namespace omaha

