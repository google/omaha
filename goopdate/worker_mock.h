// Copyright 2010 Google Inc.
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

#ifndef OMAHA_GOOPDATE_WORKER_MOCK_H_
#define OMAHA_GOOPDATE_WORKER_MOCK_H_

#include "omaha/goopdate/worker.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class MockWorker : public WorkerModelInterface {
 public:
  MOCK_METHOD1(CheckForUpdateAsync,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(DownloadAsync,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(DownloadAndInstallAsync,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(UpdateAllAppsAsync,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(DownloadPackageAsync,
      HRESULT(Package* package));
  MOCK_METHOD1(Stop,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(Pause,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD1(Resume,
      HRESULT(AppBundle* app_bundle));
  MOCK_METHOD2(GetPackage,
      HRESULT(const Package* package, const CString& dir));
  MOCK_CONST_METHOD1(IsPackageAvailable,
      bool(const Package* package));      // NOLINT
  MOCK_METHOD2(PurgeAppLowerVersions,
      HRESULT(const CString&, const CString&));
  MOCK_METHOD0(Lock,
      int());
  MOCK_METHOD0(Unlock,
      int());
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_WORKER_MOCK_H_

