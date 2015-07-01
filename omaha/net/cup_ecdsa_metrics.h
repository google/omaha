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

#ifndef OMAHA_NET_CUP_ECDSA_METRICS_H_
#define OMAHA_NET_CUP_ECDSA_METRICS_H_

#include "omaha/statsreport/metrics.h"

namespace omaha {

namespace internal {

// Number of CUP-ECDSA transactions.
DECLARE_METRIC_count(cup_ecdsa_total);

// Number of CUP-ECDSA transactions that passed authentication.
DECLARE_METRIC_count(cup_ecdsa_trusted);

// Number of CUP-ECDSA transactions thwarted by a captive portal.
DECLARE_METRIC_count(cup_ecdsa_captive_portal);

// Number of CUP-ECDSA transactions missing ETag header (non-captive-portal).
DECLARE_METRIC_count(cup_ecdsa_no_etag);

// Number of CUP-ECDSA transactions with malformed/unparsable ETag header.
DECLARE_METRIC_count(cup_ecdsa_malformed_etag);

// Number of CUP-ECDSA transactions with a request hash mismatch.
DECLARE_METRIC_count(cup_ecdsa_request_hash_mismatch);

// Number of CUP-ECDSA transactions with a signature mismatch.
DECLARE_METRIC_count(cup_ecdsa_signature_mismatch);

// Number of CUP-ECDSA transactions where the underlying HTTP request failed.
DECLARE_METRIC_count(cup_ecdsa_http_failure);

// Number of CUP-ECDSA transactions that failed for other reasons.
DECLARE_METRIC_count(cup_ecdsa_other_errors);

}  // namespace internal

}  // namespace omaha

#endif  // OMAHA_NET_CUP_ECDSA_METRICS_H_

