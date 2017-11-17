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

#include "components/crx_file/crx_verifier.h"

#include <atlconv.h>

#include "omaha/base/app_util.h"
#include "omaha/base/path.h"
#include "omaha/base/string.h"
#include "omaha/testing/unit_test.h"

namespace {

using omaha::ConcatenatePath;

std::string TestFile(const std::string& file) {
  const CString base_path = ConcatenatePath(
                                omaha::app_util::GetCurrentModuleDirectory(),
                                _T("unittest_support"));
  return std::string(CT2A(ConcatenatePath(base_path, CString(file.c_str()))));
}

const char kOjjHash[] = "ojjgnpkioondelmggbekfhllhdaimnho";
const char kOjjKey[] =
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA230uN7vYDEhdDlb4/"
    "+pg2pfL8p0FFzCF/O146NB3D5dPKuLbnNphn0OUzOrDzR/Z1XLVDlDyiA6xnb+qeRp7H8n7Wk/"
    "/gvVDNArZyForlVqWdaHLhl4dyZoNJwPKsggf30p/"
    "MxCbNfy2rzFujzn2nguOrJKzWvNt0BFqssrBpzOQl69blBezE2ZYGOnYW8mPgQV29ekIgOfJk2"
    "GgXoJBQQRRsjoPmUY7GDuEKudEB/"
    "CmWh3+"
    "mCsHBHFWbqtGhSN4YCAw3DYQzwdTcIVaIA8f2Uo4AZ4INKkrEPRL8o9mZDYtO2YHIQg8pMSRMa"
    "6AawBNYi9tZScnmgl5L1qE6z5oIwIDAQAB";

}  // namespace

namespace crx_file {

using CrxVerifierTest = testing::Test;

TEST_F(CrxVerifierTest, ValidFullCrx3) {
  const std::vector<std::vector<uint8_t>> keys;
  const std::vector<uint8_t> hash;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::OK_FULL, Verify(TestFile("valid_no_publisher.crx3"),
                                            VerifierFormat::CRX2_OR_CRX3, keys,
                                            hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);
}

TEST_F(CrxVerifierTest, Crx3RejectsCrx2) {
  const std::vector<std::vector<uint8_t>> keys;
  const std::vector<uint8_t> hash;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::ERROR_HEADER_INVALID,
            Verify(TestFile("valid.crx2"), VerifierFormat::CRX3, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, VerifiesFileHash) {
  const std::vector<std::vector<uint8_t>> keys;
  std::vector<uint8_t> hash;
  EXPECT_TRUE(omaha::SafeHexStringToVector(
      "d033c510f9e4ee081ccb60ea2bf530dc2e5cb0e71085b55503c8b13b74515fe4",
      &hash));
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";

  EXPECT_EQ(VerifierResult::OK_FULL, Verify(TestFile("valid_no_publisher.crx3"),
                                            VerifierFormat::CRX2_OR_CRX3, keys,
                                            hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  hash.clear();
  EXPECT_TRUE(
      omaha::SafeHexStringToVector(std::string(32, '0').c_str(), &hash));
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_EXPECTED_HASH_INVALID,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);

  hash.clear();
  EXPECT_TRUE(
      omaha::SafeHexStringToVector(std::string(64, '0').c_str(), &hash));
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_FILE_HASH_FAILED,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, ChecksRequiredKeyHashes) {
  const std::vector<uint8_t> hash;

  std::vector<uint8_t> good_key;
  EXPECT_TRUE(omaha::SafeHexStringToVector(
      "e996dfa8eed34bc6614a57bb7308cd7e519bcc690841e1969f7cb173ef16800a",
      &good_key));
  const std::vector<std::vector<uint8_t>> good_keys = {good_key};
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(
      VerifierResult::OK_FULL,
      Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX2_OR_CRX3,
             good_keys, hash, &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  std::vector<uint8_t> bad_key;
  EXPECT_TRUE(
      omaha::SafeHexStringToVector(std::string(64, '0').c_str(), &bad_key));
  const std::vector<std::vector<uint8_t>> bad_keys = {bad_key};
  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_no_publisher.crx3"), VerifierFormat::CRX3,
                   bad_keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, ChecksPinnedKey) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ(std::string(kOjjHash), crx_id);
  EXPECT_EQ(std::string(kOjjKey), public_key);

  public_key = "UNSET";
  crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("valid_no_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

TEST_F(CrxVerifierTest, NullptrSafe) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  EXPECT_EQ(VerifierResult::OK_FULL,
            Verify(TestFile("valid_publisher.crx3"),
                   VerifierFormat::CRX3_WITH_PUBLISHER_PROOF, keys, hash,
                   nullptr, nullptr));
}

TEST_F(CrxVerifierTest, RequiresDeveloperKey) {
  const std::vector<uint8_t> hash;
  const std::vector<std::vector<uint8_t>> keys;
  std::string public_key = "UNSET";
  std::string crx_id = "UNSET";
  EXPECT_EQ(VerifierResult::ERROR_REQUIRED_PROOF_MISSING,
            Verify(TestFile("unsigned.crx3"), VerifierFormat::CRX2_OR_CRX3,
                   keys, hash, &public_key, &crx_id));
  EXPECT_EQ("UNSET", crx_id);
  EXPECT_EQ("UNSET", public_key);
}

}  // namespace crx_file
