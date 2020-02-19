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
// Tool that takes a PEM for an ECDSA public key as input, and emits a C
// byte array as output, for integration into the Omaha3 source code.

#include <assert.h>
//#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/ecdsa.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#define X962_UNCOMPRESSED_HEADER 0x04
#define X962_UNCOMPRESSED_SIZE 65

int get_curr_year() {
  time_t now = time(NULL);
  struct tm* timeinfo = localtime(&now);
  return 1900 + timeinfo->tm_year;
}

int print_octets(const unsigned char* octets, size_t len, int version) {
  assert(octets);
  assert(len == X962_UNCOMPRESSED_SIZE);

  if (len != X962_UNCOMPRESSED_SIZE) {
    return 5;
  }

  printf(
      "// Copyright %d Google Inc.\n"
      "//\n"
      "// Licensed under the Apache License, Version 2.0 (the \"License\");\n"
      "// you may not use this file except in compliance with the License.\n"
      "// You may obtain a copy of the License at\n"
      "//\n"
      "//      http://www.apache.org/licenses/LICENSE-2.0\n"
      "//\n"
      "// Unless required by applicable law or agreed to in writing, software\n"
      "// distributed under the License is distributed on an \"AS IS\" BASIS,\n"
      "// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"   // NOLINT
      "// See the License for the specific language governing permissions and\n"
      "// limitations under the License.\n"
      "// ========================================================================\n"   // NOLINT
      "//\n"
      "// CUP-ECDSA public keys consist of a byte array, 66 bytes long, containing:\n"  // NOLINT
      "// * The key ID (one byte)\n"
      "// * The public key in X9.62 uncompressed encoding (65 bytes):\n"
      "//     * Uncompressed header byte (0x04)\n"
      "//     * Gx coordinate (256-bit integer, big-endian)\n"
      "//     * Gy coordinate (256-bit integer, big-endian)\n",
      get_curr_year());

  printf("{0x%02x,\n", (unsigned int) version);
  printf("0x%02x,", (unsigned int) octets[0]);

  const unsigned char* point_octets = octets + 1;
  const size_t point_octets_len = len - 1;

  for (size_t i = 0; i < point_octets_len; ++i) {
    if (i % 8 == 0) {
      printf("\n");
    }
    printf("0x%02x", (unsigned int) point_octets[i]);
    if (i != point_octets_len - 1) {
      printf(",");
      if (i % 8 != 7) {
        printf(" ");
      }
    }
  }
  printf("};\n");

  return 0;
}

int verify_x962_octets_are_on_p256(const unsigned char* octets, size_t len) {
  assert(octets);
  assert(len);

  EC_GROUP* p256group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
  if (!p256group) {
    fprintf(stderr, "error: EC_GROUP_new_by_curve_name() failed.\n");
    return 4;
  }

  EC_POINT* point = EC_POINT_new(p256group);
  if (!point) {
    fprintf(stderr, "error: EC_POINT_new() failed.\n");
    EC_GROUP_free(p256group);
    return 4;
  }

  if (0 == EC_POINT_oct2point(p256group, point, octets, len, NULL)) {
    fprintf(stderr, "error: EC_POINT_oct2point() failed.\n");
    EC_POINT_free(point);
    EC_GROUP_free(p256group);
    return 4;
  }

  if (0 == EC_POINT_is_on_curve(p256group, point, NULL)) {
    fprintf(stderr, "error: Public key point isn't on P-256 curve.\n");
    EC_POINT_free(point);
    EC_GROUP_free(p256group);
    return 4;
  }

  EC_POINT_free(point);
  EC_GROUP_free(p256group);
  return 0;
}

int convert_eckey_to_x962_and_print(const EC_KEY* eckey, int version) {
  assert(eckey);

  // We don't need to manage the memory for group/point since we're using get0.
  const EC_GROUP* group = EC_KEY_get0_group(eckey);
  if (!group) {
    fprintf(stderr, "error: EC_KEY_get0_group() failed.\n");
    return 3;
  }

  const EC_POINT* pubkey_point = EC_KEY_get0_public_key(eckey);
  if (!pubkey_point) {
    fprintf(stderr, "error: EC_KEY_get0_public_key() failed.\n");
    return 3;
  }

  size_t octets_len = 256;
  unsigned char octets[256];
  
  octets_len = EC_POINT_point2oct(group, pubkey_point,
                                  POINT_CONVERSION_UNCOMPRESSED,
                                  octets, octets_len, NULL);
  if (0 == octets_len) {
    fprintf(stderr, "error: EC_POINT_point2oct() failed.\n");
    return 3;
  }

  if (octets_len != X962_UNCOMPRESSED_SIZE) {
    fprintf(stderr, "error: EC_POINT_point2oct() produced %lu bytes.\n",
            (unsigned long) octets_len);
    return 3;
  }

  if (octets[0] != X962_UNCOMPRESSED_HEADER) {
    fprintf(stderr, "error: EC_POINT_point2oct() produced compressed data.\n");
    return 3;
  }

  int result = verify_x962_octets_are_on_p256(octets, octets_len);
  if (result != 0) {
    return result;
  }

  return print_octets(octets, octets_len, version);
}

int extract_and_print_eckey(EVP_PKEY* pubkey, int version) {
  assert(pubkey);

  EC_KEY* eckey = EVP_PKEY_get1_EC_KEY(pubkey);
  if (!eckey) {
    fprintf(stderr, "error: EVP_PKEY_get1_EC_KEY() failed.\n");
    return 2;
  }

  if (!EC_KEY_check_key(eckey)) {
    fprintf(stderr, "error: EC_KEY_check_key() failed.\n");
    EC_KEY_free(eckey);
    return 2;
  }
  
  int result = convert_eckey_to_x962_and_print(eckey, version);
  EC_KEY_free(eckey);
  return result;
}

int load_and_print_pubkey(FILE* pemFile, int version) {
  assert(pemFile);

  EVP_PKEY* pubkey = PEM_read_PUBKEY(pemFile, NULL, NULL, NULL);
  if (!pubkey) {
    fprintf(stderr, "error: PEM_read_PUBKEY() failed.\n");
    return 1;
  }

  if (EVP_PKEY_base_id(pubkey) != EVP_PKEY_EC) {
    fprintf(stderr, "error: PEM does not contain an EC key.\n");
    EVP_PKEY_free(pubkey);
    return 1;
  }
  
  int result = extract_and_print_eckey(pubkey, version);
  EVP_PKEY_free(pubkey);
  return result;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "usage: eckeytool PEM-file version\n");
    return -1;
  }

  int version = atoi(argv[2]);
  if (version < 1 || version > 255) {
    fprintf(stderr, "error: version must be in the range [1, 255].\n");
    return -2;
  }

  FILE* pemFile = fopen(argv[1], "r");
  if (!pemFile) {
    fprintf(stderr, "error: couldn't open \"%s\" for reading.\n", argv[1]);
    return -3;
  }

  int result = load_and_print_pubkey(pemFile, version);
  fclose(pemFile);

  return result;
}
