// Copyright 2017 Google Inc.
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
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_CRX_VERIFIER_H_
#define OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_CRX_VERIFIER_H_

#include <atlpath.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace base {
class FilePath;
}  // namespace base

namespace crx_file {

// The magic string embedded in the header.
const char kCrx2FileHeaderMagic[] = "Cr24";
const char kCrxDiffFileHeaderMagic[] = "CrOD";
const int kCrx2FileHeaderMagicSize = 4;

enum class VerifierFormat {
  CRX2_OR_CRX3,               // Accept Crx2 or Crx3.
  CRX3,                       // Accept only Crx3.
  CRX3_WITH_PUBLISHER_PROOF,  // Accept only Crx3 with a publisher proof.
};

enum class VerifierResult {
  OK_FULL,   // The file verifies as a correct full CRX file.
  OK_DELTA,  // The file verifies as a correct differential CRX file.
  ERROR_FILE_NOT_READABLE,      // Cannot open the CRX file.
  ERROR_HEADER_INVALID,         // Failed to parse or understand CRX header.
  ERROR_EXPECTED_HASH_INVALID,  // Expected hash is not well-formed.
  ERROR_FILE_HASH_FAILED,       // The file's actual hash != the expected hash.
  ERROR_SIGNATURE_INITIALIZATION_FAILED,  // A signature or key is malformed.
  ERROR_SIGNATURE_VERIFICATION_FAILED,    // A signature doesn't match.
  ERROR_REQUIRED_PROOF_MISSING,           // RequireKeyProof was unsatisfied.
};

// Verify the file at |crx_path| as a valid Crx of |format|. The Crx must be
// well-formed, contain no invalid proofs, match the |required_file_hash| (if
// non-empty), and contain a proof with each of the |required_key_hashes|.
// If and only if this function returns OK_FULL or OK_DELTA, and only if
// |public_key| / |crx_id| are non-null, they will be updated to contain the
// public key (PEM format, without the header/footer) and crx id (encoded in
// base16 using the characters [a-p]).
VerifierResult Verify(
    const std::string& crx_path,
    const VerifierFormat& format,
    const std::vector<std::vector<uint8_t>>& required_key_hashes,
    const std::vector<uint8_t>& required_file_hash,
    std::string* public_key,
    std::string* crx_id);

// Unzips the given crx file |crx_path| into the directory |to_dir|.
bool Crx3Unzip(const CPath& crx_path, const CPath& to_dir);

}  // namespace crx_file

#endif  // OMAHA_THIRD_PARTY_CHROME_FILES_SRC_COMPONENTS_CRX_FILE_CRX_VERIFIER_H_
