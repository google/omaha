// Copyright 2013 Google Inc.
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

#include "omaha/net/cup_ecdsa_metrics.h"

namespace omaha {

namespace internal {

DEFINE_METRIC_count(cup_ecdsa_total);
DEFINE_METRIC_count(cup_ecdsa_trusted);
DEFINE_METRIC_count(cup_ecdsa_captive_portal);
DEFINE_METRIC_count(cup_ecdsa_no_etag);
DEFINE_METRIC_count(cup_ecdsa_malformed_etag);
DEFINE_METRIC_count(cup_ecdsa_request_hash_mismatch);
DEFINE_METRIC_count(cup_ecdsa_signature_mismatch);
DEFINE_METRIC_count(cup_ecdsa_http_failure);
DEFINE_METRIC_count(cup_ecdsa_other_errors);

}  // namespace internal

}  // namespace omaha

