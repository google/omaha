// Copyright 2003-2009 Google Inc.
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
// signatures.h
//
// Classes and functions related to crypto-hashes of buffers and digital
// signatures of buffers. The hash algorithm used throughout is SHA1 except for
// limited support for SHA256 in the CryptHash class.

#ifndef OMAHA_BASE_SIGNATURES_H_
#define OMAHA_BASE_SIGNATURES_H_

#include <windows.h>
#include <wincrypt.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/base/security/sha256.h"
#include "omaha/third_party/smartany/scoped_any.h"

namespace omaha {

class CryptoHash;

namespace CryptDetails {

class HashInterface {
 public:
  virtual ~HashInterface() {}

  virtual void update(const void* data, unsigned int len) = 0;
  virtual const uint8_t* final() = 0;
  virtual size_t hash_size() const = 0;
};

HashInterface* CreateHasher();

}  // namespace CryptDetails

// Compute and validate SHA256 hashes of data.
class CryptoHash {
 public:
  CryptoHash() = default;
  ~CryptoHash() = default;

  // Hash a file
  HRESULT Compute(const TCHAR * filepath,
                  uint64 max_len,
                  std::vector<byte>* hash_out);

  // Hash a list of files
  HRESULT Compute(const std::vector<CString>& filepaths,
                  uint64 max_len,
                  std::vector<byte>* hash_out);

  // Hash a buffer
  HRESULT Compute(const std::vector<byte>& buffer_in,
                  std::vector<byte>* hash_out);

  // Verify hash of a file
  HRESULT Validate(const TCHAR * filepath,
                   uint64 max_len,
                   const std::vector<byte>& hash_in);

  // Verify hash of a list of files
  HRESULT Validate(const std::vector<CString>& filepaths,
                   uint64 max_len,
                   const std::vector<byte>& hash_in);

  // Verify hash of a buffer
  HRESULT Validate(const std::vector<byte>& buffer_in,
                   const std::vector<byte>& hash_in);

  bool IsValidSize(size_t size) const {
    return size == hash_size();
  }

  size_t hash_size() const {
    return SHA256_DIGEST_SIZE;
  }

 private:
  // Compute or verify hash of a file
  HRESULT ComputeOrValidate(const std::vector<CString>& filepaths,
                            uint64 max_len,
                            const std::vector<byte>* hash_in,
                            std::vector<byte>* hash_out);

  // Compute or verify hash of a buffer
  HRESULT ComputeOrValidate(const std::vector<byte>& buffer_in,
                            const std::vector<byte>* hash_in,
                            std::vector<byte>* hash_out);

  DISALLOW_COPY_AND_ASSIGN(CryptoHash);
};

// Verifies that the files' SHA256 hash is the expected_hash. The hash is
// hex-digit encoded.
HRESULT VerifyFileHashSha256(const std::vector<CString>& files,
                             const CString& expected_hash);

}  // namespace omaha

#endif  // OMAHA_BASE_SIGNATURES_H_
