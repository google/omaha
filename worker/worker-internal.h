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

#ifndef OMAHA_WORKER_WORKER_INTERNAL_H_
#define OMAHA_WORKER_WORKER_INTERNAL_H_

namespace omaha {

namespace internal {

void RecordUpdateAvailableUsageStats(bool is_machine);

// Sends a ping with self-update failure information if present in the registry.
void SendSelfUpdateFailurePing(bool is_machine);

// Sends an uninstall ping for any uninstalled products before Google Update
// uninstalls itself.
HRESULT SendFinalUninstallPingForApps(bool is_machine);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_WORKER_WORKER_INTERNAL_H_

