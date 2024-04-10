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

#include <vector>

#include "omaha/base/string.h"
#include "omaha/base/security/p256.h"
#include "omaha/net/cup_ecdsa_utils.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

namespace internal {

class EcdsaSignatureTest : public testing::Test {
 protected:
  EcdsaSignatureTest() {}

  bool DecodeInt(const CString& der_hex) {
    p256_int decoded_int;
    std::vector<uint8> der_bin;

    EXPECT_TRUE(SafeHexStringToVector(der_hex, &der_bin));
    EXPECT_FALSE(der_bin.empty());
    if (der_bin.empty()) {
      return false;
    }

    const uint8* const kStart = &der_bin[0];
    const uint8* const kEnd = kStart + der_bin.size();
    return NULL != EcdsaSignature::DecodeDerInt256(kStart, kEnd, &decoded_int);
  }

  bool DecodeIntAndCheckResult(const CString& der_hex,
                               const CString& expected_int_hex) {
    // Decode the input int, which is DER-encoded.
    std::vector<uint8> der_bin;
    EXPECT_TRUE(SafeHexStringToVector(der_hex, &der_bin));
    EXPECT_FALSE(der_bin.empty());
    if (der_bin.empty()) {
      return false;
    }

    p256_int decoded_int;
    const uint8* const kStart = &der_bin[0];
    const uint8* const kEnd = kStart + der_bin.size();
    if (NULL == EcdsaSignature::DecodeDerInt256(kStart, kEnd, &decoded_int)) {
      return false;
    }

    // Decode the expected int, which should be exactly 256 bits.
    std::vector<uint8> expected_bin;
    EXPECT_TRUE(SafeHexStringToVector(expected_int_hex, &expected_bin));
    EXPECT_EQ(P256_NBYTES, expected_bin.size());
    if (expected_bin.size() != P256_NBYTES) {
      return false;
    }

    p256_int expected_int;
    p256_from_bin(&expected_bin[0], &expected_int);

    return 0 == memcmp(&decoded_int, &expected_int, P256_NBYTES);
  }

  bool DecodeSig(const CString& der_hex) {
    std::vector<uint8> der_bin;

    EXPECT_TRUE(SafeHexStringToVector(der_hex, &der_bin));
    EXPECT_FALSE(der_bin.empty());

    EcdsaSignature sig;
    return sig.DecodeFromBuffer(der_bin);
  }

  bool DecodeSigAndCheckResult(const CString& der_hex,
                               const CString& expected_r_int_hex,
                               const CString& expected_s_int_hex) {
    // Decode the signature, which is DER-encoded.
    std::vector<uint8> der_bin;
    EXPECT_TRUE(SafeHexStringToVector(der_hex, &der_bin));
    EXPECT_FALSE(der_bin.empty());

    EcdsaSignature sig;
    if (!sig.DecodeFromBuffer(der_bin)) {
      return false;
    }

    // Decode the expected R/S ints, which should be exactly 256 bits.
    std::vector<uint8> expected_r_bin;
    std::vector<uint8> expected_s_bin;
    EXPECT_TRUE(SafeHexStringToVector(expected_r_int_hex, &expected_r_bin));
    EXPECT_TRUE(SafeHexStringToVector(expected_s_int_hex, &expected_s_bin));
    EXPECT_EQ(P256_NBYTES, expected_r_bin.size());
    EXPECT_EQ(P256_NBYTES, expected_s_bin.size());
    if (expected_r_bin.size() != P256_NBYTES ||
        expected_s_bin.size() != P256_NBYTES) {
      return false;
    }

    p256_int expected_r_int;
    p256_from_bin(&expected_r_bin[0], &expected_r_int);
    p256_int expected_s_int;
    p256_from_bin(&expected_s_bin[0], &expected_s_int);

    return 0 == memcmp(sig.r(), &expected_r_int, P256_NBYTES) &&
           0 == memcmp(sig.s(), &expected_s_int, P256_NBYTES);
  }
};

// A large number of the DecoderDerInt256 tests consist of two tests: one that
// handles unmodified input, and one that tests with a bit of garbage appended
// to the end of the buffer.  They should usually have the same result.
#define POST_INT_PADDING _T("0001020304")

// A couple of macros to help construct hex strings containing ASN.1-DER ints.
#define ASN1DER_TAG_INT _T("02")
#define ASN1DER_TAG_OCTSTR _T("04")
#define ASN1DER_TAG_NUL _T("05")
#define MAKE_ASN1DER_INT(len, data) ASN1DER_TAG_INT _T(len) _T(data)
#define MAKE_ASN1DER_OCTSTR(len, data) ASN1DER_TAG_OCTSTR _T(len) _T(data)

// A small macro to make it easier to distinguish input/expected output.
#define INP(data) _T(data)
#define EXP(data) _T(data)

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Accept_Zero) {
  // Accept: A well-formed zero.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("01", "00"),
      EXP("0000000000000000000000000000000000000000000000000000000000000000")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("01", "00") POST_INT_PADDING,
      EXP("0000000000000000000000000000000000000000000000000000000000000000")));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Accept_8Bit) {
  // MSB clear.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("01", "7f"),
      EXP("000000000000000000000000000000000000000000000000000000000000007f")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("01", "7f") POST_INT_PADDING,
      EXP("000000000000000000000000000000000000000000000000000000000000007f")));

  // MSB set.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("02", "00ff"),
      EXP("00000000000000000000000000000000000000000000000000000000000000ff")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("02", "00ff") POST_INT_PADDING,
      EXP("00000000000000000000000000000000000000000000000000000000000000ff")));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Accept_32Bit) {
  // MSB clear.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("04", "7fffffff"),
      EXP("000000000000000000000000000000000000000000000000000000007fffffff")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("04", "7fffffff") POST_INT_PADDING,
      EXP("000000000000000000000000000000000000000000000000000000007fffffff")));

  // MSB set.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("05", "00ffffffff"),
      EXP("00000000000000000000000000000000000000000000000000000000ffffffff")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("05", "00ffffffff") POST_INT_PADDING,
      EXP("00000000000000000000000000000000000000000000000000000000ffffffff")));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Accept_256Bit) {
  // MSB clear.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),  // NOLINT
      EXP("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING,  // NOLINT
      EXP("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));

  // MSB set.
  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("21", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"),  // NOLINT
      EXP("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));

  EXPECT_TRUE(DecodeIntAndCheckResult(
      MAKE_ASN1DER_INT("21", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING,  // NOLINT
      EXP("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_NoLength) {
  // No length field (malformed DER).
  EXPECT_FALSE(DecodeInt(ASN1DER_TAG_INT));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_ZeroLength) {
  // Int with a length field of zero (malformed DER).
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("00", "")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("00", "") POST_INT_PADDING));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_BufferTooSmall) {
  // Integer claims to be 256 bits of data, but the content is far too
  // small for it (malformed DER).
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "7fffffff")));
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_NotAnInt) {
  // Valid ASN.1 NULL.
  EXPECT_FALSE(DecodeInt(ASN1DER_TAG_NUL _T("00")));

  // The content is a valid 256-bit int (with MSB clear), but the type of
  // object is a OCTET STRING.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_OCTSTR("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_OCTSTR("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT

  // The content is a valid 256-bit int (with MSB set), but the type of
  // object is a OCTET STRING.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_OCTSTR("21", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_OCTSTR("21", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_IntLargerThan256Bits) {
  // Validly-encoded integer (MSB clear) with length > 256 bits.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("22", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("22", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_ActuallySigned) {
  // The int is of the proper length, but the MSB is set, and there's no
  // leading zero.  DER encoding rules use twos complement, so this is actually
  // a negative int, which is unacceptable for an ECDSA signature.  Test with
  // and without buffer padding.

  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("04", "ffffffff")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("04", "ffffffff") POST_INT_PADDING));

  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_LeadingZeroes) {
  // Zero should have a single 0 byte.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("02", "0000")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("02", "0000") POST_INT_PADDING));

  // Numbers with MSB clear shouldn't need any padding at all.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("02", "007f")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("02", "007f") POST_INT_PADDING));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("03", "00007f")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("03", "00007f") POST_INT_PADDING));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "00007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "00007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("22", "00007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("22", "00007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT

  // Numbers with MSB set should only have one byte of padding.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("03", "0000ff")));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("03", "0000ff") POST_INT_PADDING));
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("20", "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("21", "0000ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff") POST_INT_PADDING));  // NOLINT
}

TEST_F(EcdsaSignatureTest, DecodeDerInt256_Reject_LongFormLength) {
  // These are validly formed 256-bit ints, but specify their length in the
  // long form.  This is permissible in BER, but malformed in DER.
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("8120", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
  EXPECT_FALSE(DecodeInt(MAKE_ASN1DER_INT("8121", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")));  // NOLINT
}

#define ASN1DER_TAG_SEQ _T("30")
#define MAKE_ASN1DER_SIG(len, int1, int2) \
        ASN1DER_TAG_SEQ _T(len) int1 int2

#define ASN1DER_VALID_INT_04       MAKE_ASN1DER_INT("04", "7fffffff")
#define ASN1DER_VALID_INT_1F       MAKE_ASN1DER_INT("1F", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_VALID_INT_20       MAKE_ASN1DER_INT("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_BAD_INT_20_CONTENT MAKE_ASN1DER_INT("20", "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_BAD_INT_20_TRUNC   MAKE_ASN1DER_INT("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_BAD_INT_20_PADDED  MAKE_ASN1DER_INT("20", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00")  // NOLINT
#define ASN1DER_VALID_INT_21       MAKE_ASN1DER_INT("21", "00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_BAD_INT_21_CONTENT MAKE_ASN1DER_INT("21", "007fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT
#define ASN1DER_VALID_INT_22       MAKE_ASN1DER_INT("22", "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff")  // NOLINT

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Pass_CheckKnownInput) {
  // Test against some known input from the server.
  EXPECT_TRUE(DecodeSigAndCheckResult(
      INP("304202200c7f2167d66387c2bca64ded8f9452bc40cecb42bd6337786ebef7968019592b021e1ed536dfb4c39b1cea5c22ec916be3df967889e2e68568c3dfe237cb246a"),  // NOLINT
      EXP("0c7f2167d66387c2bca64ded8f9452bc40cecb42bd6337786ebef7968019592b"),
      EXP("00001ed536dfb4c39b1cea5c22ec916be3df967889e2e68568c3dfe237cb246a")));

  EXPECT_TRUE(DecodeSigAndCheckResult(
      INP("30440220650a3d34fbfbc1bba91b644147f9362077aebfaf79f8ca888907d9d088723ba802203e9914162fd938a5c3b3e2fc64027d76b1828fda0eb3581359a5fcbe7d64da16"),  // NOLINT
      EXP("650a3d34fbfbc1bba91b644147f9362077aebfaf79f8ca888907d9d088723ba8"),
      EXP("3e9914162fd938a5c3b3e2fc64027d76b1828fda0eb3581359a5fcbe7d64da16")));

  EXPECT_TRUE(DecodeSigAndCheckResult(
      INP("3045022100e4e6119eaee9e6b47e08c70354a50f31711f68bd40cf6db4fed285e82e2539980220479614b9677cc0eafc13c541fbebd363f78463421c9314f3b18ade948c3a2f45"),  // NOLINT
      EXP("e4e6119eaee9e6b47e08c70354a50f31711f68bd40cf6db4fed285e82e253998"),
      EXP("479614b9677cc0eafc13c541fbebd363f78463421c9314f3b18ade948c3a2f45")));

  EXPECT_TRUE(DecodeSigAndCheckResult(
      INP("304602210098de33f7695a260ed92a3e76461702458227b391522b2d897662bdc7724773c4022100b40e4bced7431d73e3e187524eb3366d90846b99a698ea72ae8e11d981691021"),  // NOLINT
      EXP("98de33f7695a260ed92a3e76461702458227b391522b2d897662bdc7724773c4"),
      EXP("b40e4bced7431d73e3e187524eb3366d90846b99a698ea72ae8e11d981691021")));
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Pass_SyntheticInput) {
  // Accept validly formed signatures with good integers.

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("0C", ASN1DER_VALID_INT_04, ASN1DER_VALID_INT_04)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("28", ASN1DER_VALID_INT_04, ASN1DER_VALID_INT_20)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("44", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_20)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("44", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_20)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("45", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_21)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("45", ASN1DER_VALID_INT_21, ASN1DER_VALID_INT_20)));

  EXPECT_TRUE(DecodeSig(
      MAKE_ASN1DER_SIG("46", ASN1DER_VALID_INT_21, ASN1DER_VALID_INT_21)));
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Fail_LargeInts) {
  // If one or both ints are larger than 256 bits, reject, even if they're
  // properly encoded.  (Note that the last test case will probably trigger
  // rejection by the early length check as well.)

  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("46", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_22)));

  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("46", ASN1DER_VALID_INT_22, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("48", ASN1DER_VALID_INT_22, ASN1DER_VALID_INT_22)));
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Fail_NoOuterSequence) {
  // Reject if there's no outer sequence, even if it's the right size for
  // an ECDSA signature and the contents are properly DER-encoded.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_OCTSTR("06", "020100020100")));
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Fail_BufferLengthMismatch) {
  // Reject if the sequence length is longer or shorter than the supplied
  // buffer.  (Malformed DER encoding.)
  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("45", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("43", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_20)));

  // Reject if there's more content in the buffer than the sequence length
  // specifies.
  EXPECT_FALSE(DecodeSig(
      MAKE_ASN1DER_SIG("44", ASN1DER_VALID_INT_20, ASN1DER_VALID_INT_20)
      POST_INT_PADDING));
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Fail_NotInts) {
  // Reject if either or both of the items in the sequence are not ints,
  // even if they're correctly DER-encoded items.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
         "44",
         ASN1DER_VALID_INT_20,
         MAKE_ASN1DER_OCTSTR("20", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"))));  // NOLINT

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
         "44",
         MAKE_ASN1DER_OCTSTR("20", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),  // NOLINT
         ASN1DER_VALID_INT_20)));  // NOLINT

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
         "44",
         MAKE_ASN1DER_OCTSTR("20", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),  // NOLINT
         MAKE_ASN1DER_OCTSTR("20", "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"))));  // NOLINT
}

TEST_F(EcdsaSignatureTest, DecodeFromBuffer_Fail_IntsMalformed) {
  // Reject if either or both of the ints are malformed.  This overlaps quite
  // a bit with the test cases for DecodeDerInt256():

  // * 32-byte ints with bad content.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_VALID_INT_20, ASN1DER_BAD_INT_20_CONTENT)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_CONTENT, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_CONTENT, ASN1DER_BAD_INT_20_CONTENT)));

  // * 33-byte ints with bad content.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "46", ASN1DER_VALID_INT_21, ASN1DER_BAD_INT_21_CONTENT)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "46", ASN1DER_BAD_INT_21_CONTENT, ASN1DER_VALID_INT_21)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "46", ASN1DER_BAD_INT_21_CONTENT, ASN1DER_BAD_INT_21_CONTENT)));


  // * Mixed 32 and 33 byte ints with bad content.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "45", ASN1DER_VALID_INT_20, ASN1DER_BAD_INT_21_CONTENT)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "45", ASN1DER_VALID_INT_21, ASN1DER_BAD_INT_20_CONTENT)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "45", ASN1DER_BAD_INT_21_CONTENT, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "45", ASN1DER_BAD_INT_20_CONTENT, ASN1DER_VALID_INT_21)));

  // * Ints that are truncated.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_VALID_INT_20, ASN1DER_BAD_INT_20_TRUNC)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_TRUNC, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_TRUNC, ASN1DER_BAD_INT_20_TRUNC)));

  // * Ints that are padded.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_VALID_INT_20, ASN1DER_BAD_INT_20_PADDED)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_PADDED, ASN1DER_VALID_INT_20)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_PADDED, ASN1DER_BAD_INT_20_PADDED)));

  // * A mix of truncation and padding.
  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_PADDED, ASN1DER_BAD_INT_20_TRUNC)));

  EXPECT_FALSE(DecodeSig(MAKE_ASN1DER_SIG(
      "44", ASN1DER_BAD_INT_20_TRUNC, ASN1DER_BAD_INT_20_PADDED)));
}

TEST(EcdsaPublicKey, DecodeFromBuffer_TestStagingKey) {
  EcdsaPublicKey key;

  uint8 kTestKey[] =
#include "omaha/net/cup_ecdsa_pubkey.3.h"
  ;   // NOLINT

  key.DecodeFromBuffer(kTestKey);
}

TEST(EcdsaPublicKey, DecodeFromBuffer_ProdKey) {
  EcdsaPublicKey key;

  uint8 kProdKey[] =
#include "omaha/net/cup_ecdsa_pubkey.14.h"
      ;  // NOLINT

  key.DecodeFromBuffer(kProdKey);
}

TEST(EcdsaPublicKey, DecodeSubjectPublicKeyInfo_Valid) {
  EcdsaPublicKey key;

  uint8 kSPKI[] = {
    0x30, 0x59,
      0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
      0x03, 0x42, 0x00,
        0x04,
          0x7F, 0x7F, 0x35, 0xA7, 0x97, 0x94, 0xC9, 0x50,
          0x06, 0x0B, 0x80, 0x29, 0xFC, 0x8F, 0x36, 0x3A,
          0x28, 0xF1, 0x11, 0x59, 0x69, 0x2D, 0x9D, 0x34,
          0xE6, 0xAC, 0x94, 0x81, 0x90, 0x43, 0x47, 0x35,

          0xF8, 0x33, 0xB1, 0xA6, 0x66, 0x52, 0xDC, 0x51,
          0x43, 0x37, 0xAF, 0xF7, 0xF5, 0xC9, 0xC7, 0x5D,
          0x67, 0x0C, 0x01, 0x9D, 0x95, 0xA5, 0xD6, 0x39,
          0xB7, 0x27, 0x44, 0xC6, 0x4A, 0x91, 0x28, 0xBB,
  };

  std::vector<uint8> spki(&kSPKI[0], &kSPKI[arraysize(kSPKI)]);
  EXPECT_TRUE(key.DecodeSubjectPublicKeyInfo(spki));
}

TEST(EcdsaPublicKey, DecodeSubjectPublicKeyInfo_InvalidSequenceLength) {
  EcdsaPublicKey key;

  const uint8 kSPKI[] = {
    0x30, 0xFF,
      0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
      0x03, 0x42, 0x00,
        0x04,
          0x7F, 0x7F, 0x35, 0xA7, 0x97, 0x94, 0xC9, 0x50,
          0x06, 0x0B, 0x80, 0x29, 0xFC, 0x8F, 0x36, 0x3A,
          0x28, 0xF1, 0x11, 0x59, 0x69, 0x2D, 0x9D, 0x34,
          0xE6, 0xAC, 0x94, 0x81, 0x90, 0x43, 0x47, 0x35,

          0xF8, 0x33, 0xB1, 0xA6, 0x66, 0x52, 0xDC, 0x51,
          0x43, 0x37, 0xAF, 0xF7, 0xF5, 0xC9, 0xC7, 0x5D,
          0x67, 0x0C, 0x01, 0x9D, 0x95, 0xA5, 0xD6, 0x39,
          0xB7, 0x27, 0x44, 0xC6, 0x4A, 0x91, 0x28, 0xBB,
  };

  std::vector<uint8> spki(&kSPKI[0], &kSPKI[arraysize(kSPKI)]);
  EXPECT_FALSE(key.DecodeSubjectPublicKeyInfo(spki));
}

TEST(EcdsaPublicKey,
     DecodeSubjectPublicKeyInfo_InvalidObjectIdentifiersLength) {
  EcdsaPublicKey key;

  const uint8 kSPKI[] = {
    0x30, 0x59,
      0x30, 0xFE,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
      0x03, 0x42, 0x00,
        0x04,
          0x7F, 0x7F, 0x35, 0xA7, 0x97, 0x94, 0xC9, 0x50,
          0x06, 0x0B, 0x80, 0x29, 0xFC, 0x8F, 0x36, 0x3A,
          0x28, 0xF1, 0x11, 0x59, 0x69, 0x2D, 0x9D, 0x34,
          0xE6, 0xAC, 0x94, 0x81, 0x90, 0x43, 0x47, 0x35,

          0xF8, 0x33, 0xB1, 0xA6, 0x66, 0x52, 0xDC, 0x51,
          0x43, 0x37, 0xAF, 0xF7, 0xF5, 0xC9, 0xC7, 0x5D,
          0x67, 0x0C, 0x01, 0x9D, 0x95, 0xA5, 0xD6, 0x39,
          0xB7, 0x27, 0x44, 0xC6, 0x4A, 0x91, 0x28, 0xBB,
  };

  std::vector<uint8> spki(&kSPKI[0], &kSPKI[arraysize(kSPKI)]);
  EXPECT_FALSE(key.DecodeSubjectPublicKeyInfo(spki));
}

TEST(EcdsaPublicKey, DecodeSubjectPublicKeyInfo_InvalidEncodingLength) {
  EcdsaPublicKey key;

  const uint8 kSPKI[] = {
    0x30, 0x59,
      0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
      0x03, 0xEE, 0x00,
        0x04,
          0x7F, 0x7F, 0x35, 0xA7, 0x97, 0x94, 0xC9, 0x50,
          0x06, 0x0B, 0x80, 0x29, 0xFC, 0x8F, 0x36, 0x3A,
          0x28, 0xF1, 0x11, 0x59, 0x69, 0x2D, 0x9D, 0x34,
          0xE6, 0xAC, 0x94, 0x81, 0x90, 0x43, 0x47, 0x35,

          0xF8, 0x33, 0xB1, 0xA6, 0x66, 0x52, 0xDC, 0x51,
          0x43, 0x37, 0xAF, 0xF7, 0xF5, 0xC9, 0xC7, 0x5D,
          0x67, 0x0C, 0x01, 0x9D, 0x95, 0xA5, 0xD6, 0x39,
          0xB7, 0x27, 0x44, 0xC6, 0x4A, 0x91, 0x28, 0xBB,
  };

  std::vector<uint8> spki(&kSPKI[0], &kSPKI[arraysize(kSPKI)]);
  EXPECT_FALSE(key.DecodeSubjectPublicKeyInfo(spki));
}

TEST(EcdsaPublicKey, DecodeSubjectPublicKeyInfo_UnsupportedEncoding) {
  EcdsaPublicKey key;

  const uint8 kSPKI[] = {
    0x30, 0x59,
      0x30, 0x13,
        0x06, 0x07, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01,
        0x06, 0x08, 0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07,
      0x03, 0x42, 0x00,
        0x03,
          0x7F, 0x7F, 0x35, 0xA7, 0x97, 0x94, 0xC9, 0x50,
          0x06, 0x0B, 0x80, 0x29, 0xFC, 0x8F, 0x36, 0x3A,
          0x28, 0xF1, 0x11, 0x59, 0x69, 0x2D, 0x9D, 0x34,
          0xE6, 0xAC, 0x94, 0x81, 0x90, 0x43, 0x47, 0x35,

          0xF8, 0x33, 0xB1, 0xA6, 0x66, 0x52, 0xDC, 0x51,
          0x43, 0x37, 0xAF, 0xF7, 0xF5, 0xC9, 0xC7, 0x5D,
          0x67, 0x0C, 0x01, 0x9D, 0x95, 0xA5, 0xD6, 0x39,
          0xB7, 0x27, 0x44, 0xC6, 0x4A, 0x91, 0x28, 0xBB,
  };

  std::vector<uint8> spki(&kSPKI[0], &kSPKI[arraysize(kSPKI)]);
  EXPECT_FALSE(key.DecodeSubjectPublicKeyInfo(spki));
}

}  // namespace internal

}  // namespace omaha


