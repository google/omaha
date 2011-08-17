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

#ifndef OMAHA_GOOPDATE_WORKER_INTERNAL_H_
#define OMAHA_GOOPDATE_WORKER_INTERNAL_H_

#include <windows.h>

namespace omaha {

namespace xml {

class UpdateRequest;

}  // namespace xml

namespace internal {

void RecordUpdateAvailableUsageStats();

// Looks for uninstalled apps, adds them to app_bundle and adds an uninstall
// event to each. The pings will be sent along with other pings when app_bundle
// is destroyed.
HRESULT AddUninstalledAppsPings(AppBundle* app_bundle);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_WORKER_INTERNAL_H_
