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

// Declares the usage metrics used by the client module.

#ifndef OMAHA_CLIENT_CLIENT_METRICS_H_
#define OMAHA_CLIENT_CLIENT_METRICS_H_

#include "omaha/statsreport/metrics.h"

namespace omaha {

// How many times the install client encountered another /install process
// for the same bundle already running.
// This metric was named worker_another_install_in_progress in Omaha 2.
DECLARE_METRIC_count(client_another_install_in_progress);

// How many times the update client encountered another /ua process already
// running.
// This metric was named worker_another_install_in_progress in Omaha 2.
DECLARE_METRIC_count(client_another_update_in_progress);

}  // namespace omaha

#endif  // OMAHA_CLIENT_CLIENT_METRICS_H_
