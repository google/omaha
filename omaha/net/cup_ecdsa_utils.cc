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

#include "omaha/net/cup_ecdsa_utils.h"

#include <limits>
#include <vector>
#include "omaha/base/debug.h"
#include "omaha/base/error.h"
#include "omaha/base/logging.h"
#include "omaha/base/security/p256.h"
#include "omaha/base/security/p256_ecdsa.h"
#include "omaha/base/security/sha256.h"

namespace omaha {

namespace internal {

bool SafeSHA256Hash(const void* data, size_t len,
                    std::vector<uint8>* hash_out) {
  const size_t kMaxLen = static_cast<size_t>(std::numeric_limits<int>::max());

  ASSERT1(data);
  ASSERT1(len > 0 && len <= kMaxLen);
  ASSERT1(hash_out);

  if (len <= 0 || len > kMaxLen) {
    return false;
  }

  hash_out->resize(SHA256_DIGEST_SIZE);
  SHA256_hash(data, static_cast<int>(len), &hash_out->front());
  return true;
}

bool SafeSHA256Hash(const std::vector<uint8>& data,
                    std::vector<uint8>* hash_out) {
  ASSERT1(!data.empty());
  return SafeSHA256Hash(&data.front(), data.size(), hash_out);
}

EcdsaSignature::EcdsaSignature() {
  p256_init(&r_);
  p256_init(&s_);
}

// Converts an ASN.1 encoded ECDSA signature to an (R,S) pair usable by our
// ECDSA math libraries.  Returns true on success, false on failure.  On
// false, any prior state is cleared.
bool EcdsaSignature::DecodeFromBuffer(const std::vector<uint8>& asn1der) {
  bool result = DoDecodeFromBuffer(asn1der);
  if (!result) {
    p256_clear(&r_);
    p256_clear(&s_);
  }

  return result;
}

// We expect |asn1der| to contain a DER-encoded ASN.1 ECDSASignature.
// Specifically, it'll have this format (from RFC 3279, which mirrors X9.62):
//
// Ecdsa-Sig-Value  ::=  SEQUENCE  {
//         r     INTEGER,
//         s     INTEGER  }
//
// This code is largely hardcoded to this sequence, although it is tolerant
// against bad input.  It is NOT intended to be a general-purpose ASN.1-DER
// decoder.
//
// A typical signature:
//   304202200c7f2167d66387c2bca64ded8f9452bc40cecb42bd6337786ebef7968019
//   592b021e1ed536dfb4c39b1cea5c22ec916be3df967889e2e68568c3dfe237cb246a
//
// Decoded:
// 0x30 - SIGNATURE
//  0x42 - length of the sequence (66 bytes)
//   0x02 - INTEGER
//    0x20 - length of the int (32 bytes)
//     0x0c7f2167d66387c2bca64ded8f9452bc40cecb42bd6337786ebef7968019592b
//   0x02 - INTEGER
//    0x1e - length of the int (30 bytes)
//     0x1ed536dfb4c39b1cea5c22ec916be3df967889e2e68568c3dfe237cb246a
//
// Returns true on success, false on failure.  The state of EcdsaSignature on
// a false return is indeterminate.
bool EcdsaSignature::DoDecodeFromBuffer(const std::vector<uint8>& asn1der) {
  ASSERT1(!asn1der.empty());

  // The signature has to be a minimum of 8 bytes (tag, length, and two ints
  // that are three bytes each).  It can't be any bigger than 72 bytes (tag,
  // length, two ints that are 35 bytes each).  Reject all other sizes.
  if (asn1der.size() < 8 || asn1der.size() > 72) {
    return false;
  }

  const uint8* const buffer_begin = &asn1der[0];
  const uint8* const buffer_end = buffer_begin + asn1der.size();

  // The tag is expected to be 0x30: Native, Constructed, SEQUENCE.
  const uint8 tag = buffer_begin[0];
  if (tag != 0x30) {
    return false;
  }

  // Check the length for the complete sequence.  DER forces a definite-form
  // encoding for length, and we know (from the check above) that the buffer
  // is < 128 bytes.  This, in turn, mandates short-form encoding (one byte).
  const uint8 sequence_len = buffer_begin[1];
  if (static_cast<size_t>(sequence_len) != asn1der.size() - 2) {
    return false;
  }
  ASSERT1(buffer_begin + sequence_len + 2 == buffer_end);

  // Decode the first integer's TLV.
  const uint8* const first_int_begin = buffer_begin + 2;
  const uint8* const second_int_begin = DecodeDerInt256(first_int_begin,
                                                        buffer_end,
                                                        &r_);
  // DecodeDerInt256 returns NULL on failure, or returns a valid pointer to the
  // byte following the first integer's complete TLV.  This will also be a
  // pointer to the start of the second integer's TLV.
  if (!second_int_begin) {
    return false;
  }
  ASSERT1(second_int_begin > first_int_begin && second_int_begin < buffer_end);

  // Decode the second integer's TLV.
  const uint8* const second_int_end = DecodeDerInt256(second_int_begin,
                                                      buffer_end,
                                                      &s_);
  // The end of the second integer's TLV _should_ be non-NULL, and equal to the
  // the end of the outer sequence TLV.
  if (second_int_end != buffer_end) {
    return false;
  }

  return true;
}

// Helper function for DoDecodeFromBuffer() to validate and decode (r,s) ints.
// Only well-formed, positive integers of 256 bits or less are accepted.
//
// |buffer_begin| is expected to be non-NULL and should point to a DER-encoded
// ASN.1 TLV structure for an INTEGER.  |buffer_end| is expected to point to the
// first inaccessible byte following this buffer.  On success,  |int_out|
// contains the int, and we return a pointer to the byte immediately following
// the complete TLV.  On failure, we return NULL, and |int_out| is unmodified.

const uint8* EcdsaSignature::DecodeDerInt256(const uint8* buffer_begin,
                                             const uint8* buffer_end,
                                             p256_int* int_out) {
  ASSERT1(buffer_begin);
  ASSERT1(buffer_end);
  ASSERT1(buffer_begin < buffer_end);
  ASSERT1(int_out);

  // Start by checking length.
  // The buffer must be at least two bytes, in order to contain tag and length.
  if (buffer_begin + 2 >= buffer_end) {
    return NULL;
  }

  // The tag is expected to be 0x02: Native, Primitive, INTEGER.
  const uint8 kTag = buffer_begin[0];
  if (kTag != 0x02) {
    return NULL;
  }

  // Check that the length is sane.  DER encoding rules mandate that ints be
  // encoded in two's complement form, base-256, big-endian.  From this, we know
  // that the biggest possible input is 33 bytes of data: an 256-bit unsigned
  // int with the MSB set (32 bytes), with a leading 00 to turn it into a
  // positive signed number.
  //
  // DER encoding also mandates definite-form lengths; at 33 bytes or less, that
  // mandates a short-form encoding.  Reject any other length, including
  // long-form encodings.
  uint8 int_data_len = buffer_begin[1];
  if (int_data_len > 33 || int_data_len < 1) {
    return NULL;
  }

  // Verify that the int data is entirely inside our buffer.
  const uint8* int_data = buffer_begin + 2;
  if (int_data + int_data_len > buffer_end) {
    return NULL;
  }

  // Inspect the first two bytes of the integer to make sure it's unsigned:
  // * If the first byte is non-zero, its MSB must be clear.
  // * If the first byte is zero, then:
  //    * It must be the only byte
  //   OR
  //    * The second byte must be non-zero and MSB set.
  if (int_data[0] != 0) {
    if ((int_data[0] & 0x80) == 0x80) {
      return NULL;
    }
  } else {
    // int_data[0] == 0
    if (int_data_len > 1) {
      if ((int_data[1] & 0x80) != 0x80) {
        return NULL;
      }

      // OK, this is an unsigned int with MSB high, and DER-encoding has padded
      // with a leading 0 to convert it to a positive twos complement number.
      // Advance our int data pointer forward one byte, to compensate for this.
      ++int_data;
      --int_data_len;
    }
  }

  // Verify that the int is no more than 256 bits. (A 264-bit int with MSB
  // clear could pass the above tests, and get here.)
  if (int_data_len > P256_NBYTES) {
    return NULL;
  }

  // OK, this looks like a valid int.  Since it can be less than 256 bits,
  // copy it into a buffer, and then pad to the left with zeroes.  Then, call
  // our P256 libraries to convert it to our local format.
  std::vector<uint8> padded;
  padded.reserve(P256_NBYTES);

  padded.assign(int_data, int_data + int_data_len);
  padded.insert(padded.begin(), P256_NBYTES - int_data_len, 0);
  ASSERT1(padded.size() == P256_NBYTES);

  p256_from_bin(&padded[0], int_out);
  // Return a pointer to the byte immediately following this parsed int.
  return int_data + int_data_len;
}

EcdsaPublicKey::EcdsaPublicKey() : version_(0) {
  p256_init(&gx_);
  p256_init(&gy_);
}

// Decodes a buffer produced by cup_ecdsa_tool into a public key and version
// number that the server can reference.  Since the input for this function
// is always a hardcoded constant in the EXE, we do not do much validation.

void EcdsaPublicKey::DecodeFromBuffer(const uint8* encoded_pkey_in) {
  ASSERT1(encoded_pkey_in);

  // We expect |encoded_pkey_in| to contain a one-byte key ID, followed by
  // the ECC public key encoded in X9.62 format. (Should be 66 bytes in total.)
  //
  // We currently only support X9.62 uncompressed encodings, which are laid out
  // as follows:
  //   * 0x04 (header byte -- identical to DER's OCTET STRING)
  //   * Gx coordinate, big-endian
  //   * Gy coordinate, big-endian
  // X9.62 does define compressed encodings, which are denoted by a different
  // header byte. We don't bother implementing support for these encodings,
  // since we control the production of public keys.

  ASSERT1(encoded_pkey_in[0] > 0);
  version_ = encoded_pkey_in[0];

  ASSERT1(encoded_pkey_in[1] == 0x04);
  p256_from_bin(&encoded_pkey_in[2], &gx_);
  p256_from_bin(&encoded_pkey_in[2 + P256_NBYTES], &gy_);

  ASSERT1(p256_is_valid_point(&gx_, &gy_));
}

COMPILE_ASSERT(SHA256_DIGEST_SIZE == P256_NBYTES, sha256_digest_isnt_256_bits);

bool VerifyEcdsaSignature(const EcdsaPublicKey& public_key,
                          const std::vector<uint8>& buffer,
                          const EcdsaSignature& signature) {
  std::vector<uint8> digest;
  VERIFY1(SafeSHA256Hash(buffer, &digest));
  ASSERT1(digest.size() == P256_NBYTES);

  p256_int digest_as_int;
  p256_from_bin(&digest.front(), &digest_as_int);

  return p256_ecdsa_verify(public_key.gx(), public_key.gy(),
                           &digest_as_int,
                           signature.r(), signature.s()) != 0;
}

}  // namespace internal

}  // namespace omaha


