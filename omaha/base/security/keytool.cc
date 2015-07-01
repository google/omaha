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
//
// Generate key with 'openssl genrsa -3 1024 > private-key.N' with N the
// version number.

#include <iostream>
#include <string>

#include "base/commandlineflags.h"
#include "base/init_google.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "file/base/file.h"
#include "file/base/helpers.h"
#include "security/util/sha1.h"
#include "strings/escaping.h"
#include "strings/strutil.h"
#include "third_party/openssl/evp.h"  // for OpenSSL_add_all_ciphers
#include "third_party/openssl/rc4.h"
#include "third_party/openssl/sha.h"
#include "util/sig/privatekey.h"
#include "util/sig/publickey.h"
#include "util/task/status.h"

DEFINE_string(input, "", "base-64 encoded input to decrypt");
DEFINE_string(input_file, "", "input file to hash, sign or decrypt");
DEFINE_bool(pubout, false, "print publickey as C style array initializer");
DEFINE_string(private_key_file, "privatekey",
              "input base filename for private key;"
              " .key_version gets appended.");
DEFINE_bool(sigout, false, "output signature in base-64");
DEFINE_bool(hashout, false, "output content hash in base-64");
DEFINE_string(challenge, "", "input challenge in base-64");
DEFINE_string(key_version, "1", "input key version number");
DEFINE_bool(decrypt, false, "decrypt input");
DEFINE_string(output_file, "", "file to write to");
DEFINE_bool(unittest, false, "suppress volatile output");
DEFINE_bool(fail, false, "fail for testing");

// Little helper that writes to cout or --output_file
static void WriteOutput(const string& output) {
  if (FLAGS_output_file.length()) {
    CHECK_OK(file::SetContents(FLAGS_output_file, output, file::Defaults()));
  } else {
    cout << output;
    flush(cout);
  }
}

static void ProcessCommandLine(const char* self) {
  CHECK(!FLAGS_fail) << "Failing for testing.";

  string input;
  if (FLAGS_input.length())
    CHECK(strings::WebSafeBase64Unescape(FLAGS_input, &input));
  // Input_file presence trumps challenge.
  if (FLAGS_input_file.length())
    CHECK_OK(file::GetContents(FLAGS_input_file, &input, file::Defaults()));

  if (FLAGS_decrypt) {
    CHECK_GE(input.length(), 1 + 4);

    // Check the wire-format. Currently, only 0 is understood.
    CHECK_EQ(input.data()[0], 0);

    // Get key version.
    unsigned int key_version =
        ((input.data()[1] & 255) << 24) |
        ((input.data()[2] & 255) << 16) |
        ((input.data()[3] & 255) << 8) |
        ((input.data()[4] & 255) << 0);

    string privkeyfile = FLAGS_private_key_file + "." +
        SimpleItoa(key_version);

    // Get the right private key.
    string privPEM;
    CHECK_OK(file::GetContents(privkeyfile, &privPEM, file::Defaults()));
    PrivateKey priv(privPEM.c_str());

    CHECK_GE(input.length(), 1 + 4 + priv.RawSize());

    // Unwrap RC4 key as encrypted with RSA.
    string wrappedKey = input.substr(1 + 4, priv.RawSize());
    string keyAndHash;
    CHECK(priv.RawDecrypt(wrappedKey, &keyAndHash));
    CHECK_EQ(keyAndHash.length(), priv.RawSize());

    RC4_KEY rc4;
    RC4_set_key(&rc4,
                keyAndHash.length(),
                reinterpret_cast<const unsigned char*>(keyAndHash.data()));

    // Warm up RC4 by dropping 1536 bytes.
    unsigned char wasted[1536];
    RC4(&rc4, sizeof(wasted), wasted, wasted);

    int len = input.length() - 1 - 4 - priv.RawSize();
    scoped_ptr<char[]> buf(new char[len]);

    // Decrypt message.
    RC4(&rc4,
        len,
        reinterpret_cast<const unsigned char*>
            (input.data() + 1 + 4 + priv.RawSize()),
        reinterpret_cast<unsigned char*>(buf.get()));

    // Compute the mask which obscures the lsb plain hash value.
    unsigned char mask[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(keyAndHash.data()),
         priv.RawSize() - SHA_DIGEST_LENGTH,
         mask);

    // Hash the decrypted output.
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(buf.get()),
         len,
         hash);

    // Mask the computed hash.
    for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
      hash[i] ^= mask[i];

    // Check masked hash against one that was encrypted under RSA.
    CHECK(!memcmp(hash,
                  keyAndHash.data() + priv.RawSize() - SHA_DIGEST_LENGTH,
                  SHA_DIGEST_LENGTH));

    string output(buf.get(), len);

    WriteOutput(output);
    return;
  }

  string privPEM;
  string privkeyfile = "(null)";

  if (FLAGS_private_key_file.length()) {
    privkeyfile = FLAGS_private_key_file + "." + FLAGS_key_version;
    CHECK_OK(file::GetContents(privkeyfile, &privPEM, file::Defaults()));
  }

  PrivateKey priv(privPEM.c_str());
  CHECK(priv.IsValid()) << "Failed to load private key from file "
                        << "\"" << privkeyfile << "\"";

  if (FLAGS_pubout) {
    // Output the corresponding public in a format that can be
    // compiled into the clientlibrary. Basically an initialized C array of
    // uint32.
    uint32 mod[2 * 64 + 1];
    int mod_size = sizeof(mod) / sizeof(mod[0]);
    mod_size = priv.PrecomputedArrayInitializer(mod, mod_size);
    CHECK(mod_size) << "PrecomputeArrayInitializer() failed: key too large?";
    // The public key format to embed into client code is:
    // {version, modulus-length-in-words, <PrecomputeArrayInitializer() output>}
    string pub;
    if (!FLAGS_unittest) {
      StringAppendF(&pub, "/* generated with:\n"
           "%s --private_key_file \"%s\" --key_version %s --pubout\n*/\n",
           self, FLAGS_private_key_file.c_str(), FLAGS_key_version.c_str());
    }
    StringAppendF(&pub, "{");
    StringAppendF(&pub, "%s,", FLAGS_key_version.c_str());  // version
    StringAppendF(&pub, "%d,",
                  (mod_size - 1) / 2);  // length of modulus in words
    for (int i = 0; i < mod_size; ++i) {
      if (i) StringAppendF(&pub, ",");
      StringAppendF(&pub, "0x%08x", mod[i]);
    }
    StringAppendF(&pub, "}\n");

    WriteOutput(pub);
    return;
  }

  // Hash content to sign.
  security::SHA1 sha;
  sha.Update(input.data(), input.size());
  string b64hash;
  strings::WebSafeBase64Escape(sha.Digest(), &b64hash);

  LOG(INFO) << "Input hash: " << b64hash;

  if (FLAGS_hashout) {
    string unb64hash;
    CHECK(strings::WebSafeBase64Unescape(b64hash, &unb64hash));
    CHECK_EQ(unb64hash.length(), SHA_DIGEST_LENGTH);

    WriteOutput(b64hash);
    return;
  }

  // Sign concatenation of challenge and hash.
  // In inefficient base64 format for ease of debugging.
  string msg(FLAGS_challenge + ':' + b64hash);

  LOG(INFO) << "Message to be signed: " << msg;

  // At this point, look up / verify key version number.
  // The client challenge has the form NN:XX with NN being its
  // public key version number and XX being a random challenge.
  // A normal server would have multiple active keys in a lookup structure.
  // And would not CHECK input based failures ever, of course.
  CHECK_EQ(FLAGS_challenge.find(FLAGS_key_version + ":"), 0);

  string signature;
  CHECK(priv.SignAndEmbedMessage(msg, &signature)) <<
      "SignAndEmbedMessage() failed: input too large?";

  CHECK_EQ(signature.length(), 128) << "1024 bit RSA keys only please";

  string b64signature;
  strings::WebSafeBase64Escape(signature, &b64signature);

  // Verify signature we made, paranoia check.
  string verified;
  string unb64signature;
  CHECK(strings::WebSafeBase64Unescape(b64signature, &unb64signature));
  CHECK_EQ(unb64signature.length(), 128);
  CHECK_EQ(unb64signature, signature);
  CHECK(priv.VerifyAndRecoverMessage(unb64signature, &verified));
  CHECK_EQ(verified, msg);

  if (FLAGS_sigout) {
    WriteOutput(b64signature);
    return;
  }
}

int main(int argc, char* argv[]) {
  InitGoogle("keytool", &argc, &argv, true);

  OpenSSL_add_all_ciphers();

  ProcessCommandLine(argv[0]);

  return 0;
}
