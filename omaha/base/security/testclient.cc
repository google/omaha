// Copyright 2014 Google Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <stdint.h>

#include "challenger.h"
#include "b64.h"
#include "sha.h"
#include "rsa.h"

static RSA::PublicKeyInstance kPublicKey =
#include "publickey.h"
;

int main(int argc, char* argv[]) {
  const char* seed = "default seed";
  const char* signature = NULL;
  const char* hash = NULL;
  const char* input_file = NULL;
  const char* encrypt = NULL;
  int dochallenge = 0;
  int verbose = 0;

  for (int i = 1; i< argc; ++i) {
    if (!strcmp(argv[i], "--seed")) seed = argv[++i];
    else if (!strcmp(argv[i], "--signature")) signature = argv[++i];
    else if (!strcmp(argv[i], "--hash")) hash = argv[++i];
    else if (!strcmp(argv[i], "--input_file")) input_file = argv[++i];
    else if (!strcmp(argv[i], "--challenge")) dochallenge = 1;
    else if (!strcmp(argv[i], "--verbose")) verbose = 1;
    else if (!strcmp(argv[i], "--encrypt")) encrypt = argv[++i];
    else {
      fprintf(stderr, "unknown argument '%s'\n", argv[i]);
      exit(1);
    }
  }

  // Construct a Challenger from specified seed, so multiple invocations will
  // yield the same challenges.
  Challenger challenger(kPublicKey,
                        reinterpret_cast<const uint8_t*>(seed),
                        seed?strlen(seed):0);

  const char* challenge = challenger.challenge();

  if (dochallenge) {
    printf("%s", challenge);
    return 0;
  }

  if (encrypt) {
    RSA rsa(kPublicKey);
    uint8_t buf[10240];
    int buf_size = rsa.encrypt(reinterpret_cast<const uint8_t*>(encrypt),
                               strlen(encrypt),
                               reinterpret_cast<const uint8_t*>(seed),
                               seed?strlen(seed):0,
                               buf, sizeof(buf));
    if (buf_size) {
      char b64[sizeof(buf) * 2];
      B64_encode(buf, buf_size, b64, sizeof(b64));
      printf("%s", b64);
      return 0;
    } else {
      printf("rsa.encrypt FAILED!");
      return 1;
    }
  }

  if (verbose) {
    printf("seed: %s\n"
           "hash: %s\n"
           "challenge: %s\n"
           "signature: %s\n"
           "encrypt: %s\n",
           seed, hash, challenge, signature, encrypt);
  }

  if (input_file) {
    // Hash content of specified input file and compare against provided hash.
    FILE* fin = fopen(input_file, "rb");
    if (fin) {
      uint8_t buf[64];
      char b64hash[SHA_DIGEST_SIZE * 2];
      SHA_CTX ctx;
      int n;

      SHA_init(&ctx);
      while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
        SHA_update(&ctx, buf, n);
      }
      fclose(fin);

      B64_encode(SHA_final(&ctx), SHA_DIGEST_SIZE, b64hash, sizeof(b64hash));

      if (hash) {
        if (strcmp(hash, b64hash)) {
          fprintf(stderr, "hash mismatch: %s vs %s\n", b64hash, hash);
          return 1;
        }
      }
    }
  }

  if (hash && signature) {
    if (!challenger.verify(hash, signature)) {
      fprintf(stderr, "failed to verify signature\n");
      return 1;
    } else {
      // Signature verified.
      // We already verified the hash against the file content.
      // Done.

      printf("PASS\n");
    }
  }

  return 0;
}
