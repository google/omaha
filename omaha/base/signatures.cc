// Copyright 2003-2010 Google Inc.
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
// signatures.cpp
//
// Classes and functions related to crypto-hashes of buffers and digital
// signatures of buffers.

#include "omaha/base/signatures.h"
#include <intsafe.h>
#include <memory>
#include <vector>

#include "omaha/base/const_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/string.h"
#include "omaha/base/utils.h"

namespace omaha {

// Maximum file size allowed for performing authentication.
constexpr size_t kMaxFileSizeForAuthentication = 1024 * 1024 * 1024;  // 1GB.

// Buffer size used to read files from disk.
constexpr size_t kFileReadBufferSize = 1024 * 1024;  // 1MB.

namespace CryptDetails {

class SHA256Hash : public HashInterface {
 public:
  SHA256Hash() {
    SHA256_init(&ctx2_);
  }
  virtual ~SHA256Hash() {}

  virtual void update(const void* data, unsigned int len) {
    SHA256_update(&ctx2_, data, len);
  }

  virtual const uint8_t* final() {
    return SHA256_final(&ctx2_);
  }

  virtual size_t hash_size() const {
    return SHA256_DIGEST_SIZE;
  }

 private:
  LITE_SHA256_CTX ctx2_;

  DISALLOW_COPY_AND_ASSIGN(SHA256Hash);
};

CryptDetails::HashInterface* CreateHasher() {
  return new CryptDetails::SHA256Hash;
}

}  // namespace CryptDetails

HRESULT CryptoHash::Compute(const TCHAR* filepath,
                            uint64 max_len,
                            std::vector<byte>* hash_out) {
  ASSERT1(filepath);
  ASSERT1(hash_out);

  std::vector<CString> filepaths;
  filepaths.push_back(filepath);
  return Compute(filepaths, max_len, hash_out);
}

HRESULT CryptoHash::Compute(const std::vector<CString>& filepaths,
                            uint64 max_len,
                            std::vector<byte>* hash_out) {
  ASSERT1(filepaths.size() > 0);
  ASSERT1(hash_out);

  return ComputeOrValidate(filepaths, max_len, NULL, hash_out);
}

HRESULT CryptoHash::Compute(const std::vector<byte>& buffer_in,
                            std::vector<byte>* hash_out) {
  ASSERT1(hash_out);

  return ComputeOrValidate(buffer_in, NULL, hash_out);
}

HRESULT CryptoHash::Validate(const TCHAR* filepath,
                             uint64 max_len,
                             const std::vector<byte>& hash_in) {
  ASSERT1(filepath);
  ASSERT1(IsValidSize(hash_in.size()));

  std::vector<CString> filepaths;
  filepaths.push_back(filepath);
  return Validate(filepaths, max_len, hash_in);
}

HRESULT CryptoHash::Validate(const std::vector<CString>& filepaths,
                             uint64 max_len,
                             const std::vector<byte>& hash_in) {
  ASSERT1(IsValidSize(hash_in.size()));

  return ComputeOrValidate(filepaths, max_len, &hash_in, NULL);
}


HRESULT CryptoHash::Validate(const std::vector<byte>& buffer_in,
                             const std::vector<byte>& hash_in) {
  ASSERT1(IsValidSize(hash_in.size()));

  return ComputeOrValidate(buffer_in, &hash_in, NULL);
}

HRESULT CryptoHash::ComputeOrValidate(const std::vector<CString>& filepaths,
                                      uint64 max_len,
                                      const std::vector<byte>* hash_in,
                                      std::vector<byte>* hash_out) {
  ASSERT1(filepaths.size() > 0);
  ASSERT1(hash_in && !hash_out || !hash_in && hash_out);
  UTIL_LOG(L1, (_T("[CryptoHash::ComputeOrValidate]")));

  uint64 curr_len = 0;
  std::vector<byte> buf(kFileReadBufferSize);
  static_assert(kFileReadBufferSize <= INT_MAX);

  std::unique_ptr<CryptDetails::HashInterface> hasher(
      CryptDetails::CreateHasher());

  for (size_t i = 0; i < filepaths.size(); ++i) {
    scoped_hfile file_handle(::CreateFile(filepaths[i],
                                          FILE_READ_DATA,
                                          FILE_SHARE_READ,
                                          NULL,
                                          OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL,
                                          NULL));
    if (!file_handle) {
      return HRESULTFromLastError();
    }

    if (max_len) {
      LARGE_INTEGER file_size = {0};
      if (!::GetFileSizeEx(get(file_handle), &file_size)) {
        return HRESULTFromLastError();
      }
      curr_len += file_size.QuadPart;
      if (curr_len > max_len) {
        UTIL_LOG(LE, (_T("[exceed max len][curr_len=%lu][max_len=%lu]"),
                      curr_len, max_len));
        return SIGS_E_FILE_SIZE_TOO_BIG;
      }
    }

    DWORD bytes_read = 0;
    do {
      if (!::ReadFile(get(file_handle),
                      &buf[0],
                      static_cast<DWORD>(buf.size()),
                      &bytes_read,
                      NULL)) {
        return HRESULTFromLastError();
      }

      if (bytes_read > 0) {
        hasher->update(&buf[0], bytes_read);
      }
    } while (bytes_read == buf.size());
  }

  DWORD digest_size = static_cast<DWORD>(hash_size());
  std::vector<char> digest_data(digest_size);

  memcpy(&digest_data.front(), hasher->final(), digest_size);

  if (hash_in) {
    int res = memcmp(&hash_in->front(), &digest_data.front(), digest_size);
    if (res == 0) {
      return S_OK;
    }

    CStringA base64_encoded_hash;
    Base64Escape(digest_data.data(), digest_size, &base64_encoded_hash, true);
    REPORT_LOG(L1, (_T("[actual hash=%S]"), base64_encoded_hash));
    return SIGS_E_INVALID_SIGNATURE;
  } else {
    hash_out->resize(digest_size);
    memcpy(&hash_out->front(), &digest_data.front(), digest_size);
    return S_OK;
  }
}

HRESULT CryptoHash::ComputeOrValidate(const std::vector<byte>& buffer_in,
                                      const std::vector<byte>* hash_in,
                                      std::vector<byte>* hash_out) {
  ASSERT1(hash_in && !hash_out || !hash_in && hash_out);
  UTIL_LOG(L1, (_T("[CryptoHash::ComputeOrValidate]")));

  if (buffer_in.size() > INT_MAX) {
    return E_INVALIDARG;
  }

  std::unique_ptr<CryptDetails::HashInterface> hasher(
      CryptDetails::CreateHasher());

  const size_t datalen = buffer_in.size();
  const uint8* data = datalen ? &buffer_in.front() : NULL;

  hasher->update(data, static_cast<unsigned int>(datalen));

  DWORD digest_size = static_cast<DWORD>(hash_size());
  std::vector<uint8> digest_data(digest_size);

  memcpy(&digest_data.front(), hasher->final(), digest_size);

  if (hash_in) {
    int res = memcmp(&hash_in->front(), &digest_data.front(), digest_size);
    return (res == 0) ? S_OK : SIGS_E_INVALID_SIGNATURE;
  } else {
    hash_out->resize(digest_size);
    memcpy(&hash_out->front(), &digest_data.front(), digest_size);
    return S_OK;
  }
}

HRESULT VerifyFileHashSha256(const std::vector<CString>& files,
                             const CString& expected_hash) {
  ASSERT1(!files.empty());

  std::vector<uint8> hash_vector;
  if (!SafeHexStringToVector(expected_hash, &hash_vector)) {
    return E_INVALIDARG;
  }

  CryptoHash crypto;
  if (!crypto.IsValidSize(hash_vector.size())) {
    return E_INVALIDARG;
  }
  return crypto.Validate(files, kMaxFileSizeForAuthentication, hash_vector);
}

}  // namespace omaha
