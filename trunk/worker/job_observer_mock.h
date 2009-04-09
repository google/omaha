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

#ifndef OMAHA_WORKER_JOB_OBSERVER_MOCK_H_
#define OMAHA_WORKER_JOB_OBSERVER_MOCK_H_

#include "omaha/worker/job_observer.h"

namespace omaha {

class JobObserverMock : public JobObserver {
 public:
  JobObserverMock()
      : completion_code(static_cast<CompletionCodes>(-1)),
        completion_error_code(S_OK) {}
  virtual void OnShow() {}
  virtual void OnCheckingForUpdate() {}
  virtual void OnUpdateAvailable(const TCHAR* version_string) {
    UNREFERENCED_PARAMETER(version_string);
  }
  virtual void OnWaitingToDownload() {}
  virtual void OnDownloading(int time_remaining_ms, int pos) {
    UNREFERENCED_PARAMETER(time_remaining_ms);
    UNREFERENCED_PARAMETER(pos);
  }
  virtual void OnWaitingToInstall() {}
  virtual void OnInstalling() {}
  virtual void OnPause() {}
  virtual void OnComplete(CompletionCodes code,
                          const TCHAR* text,
                          DWORD error_code) {
    completion_code = code;
    completion_text = text;
    completion_error_code = error_code;
  }
  virtual void SetEventSink(ProgressWndEvents* event_sink) {
    UNREFERENCED_PARAMETER(event_sink);
  }
  virtual void Uninitialize() {}

  CompletionCodes completion_code;
  CString completion_text;
  DWORD completion_error_code;
};

}  // namespace omaha

#endif  // OMAHA_WORKER_JOB_OBSERVER_MOCK_H_

