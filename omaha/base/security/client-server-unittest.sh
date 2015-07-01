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

#!/bin/bash
source googletest.sh || exit 1

KEYTOOL=$TEST_SRCDIR/google3/security/util/lite/keytool
TESTCLIENT=$TEST_SRCDIR/google3/security/util/lite/testclient
RANDOMSEED=`date +%s-%N`  # Crappy but changing seed.
KEYVERSION=4
TESTCONTENT=$TEST_TMPDIR/testcontent
PRIVKEY=$TEST_SRCDIR/google3/security/util/lite/testdata/privatekey

# $1 var to capture into
# $2 cmd to capture stdout from
function capture() {
  eval $2 > /dev/null || die "Failed to run $2"
  eval "$1=\"$(eval $2)\""
# Sometimes (on my workstation) a SIGPIPE (13) results in stdout capture
# failing, when running a Google3 binary (i.e. not TESTCLIENT)
# So we retry. Error 141 is 128 + 13, bash reporting SIGPIPE.
  while [ "$1" = "" ]; do
    if [ $? -eq 0 -o $? -eq 141 ]; then
      eval "$1=\"$(eval $2)\""
    else
      eval "$1=\"(null:$?)\""
    fi
  done
}

# Produce public key in compilable format.
$KEYTOOL --key_version=$KEYVERSION \
         --private_key_file=$PRIVKEY \
         --pubout --output_file $TESTCONTENT \
         --unittest \
      || die "Failed to run $KEYTOOL"
echo "Test content: "
cat $TESTCONTENT

echo "Client seed: $RANDOMSEED"

# Have client encrypt.
ENCRYPTION_CMD="$TESTCLIENT --seed \"$RANDOMSEED\" --encrypt \"$RANDOMSEED\""
capture ENCRYPTION "$ENCRYPTION_CMD"
echo "Encryption: $ENCRYPTION"

# Check we can decrypt.
DECRYPTION_CMD="$KEYTOOL --decrypt --input=\"$ENCRYPTION\" \
                         --private_key_file=$PRIVKEY"
capture DECRYPTION "$DECRYPTION_CMD"
echo "Decryption: $DECRYPTION"

check_eq "$DECRYPTION" "$RANDOMSEED"

# Have client challenge.
CHALLENGE_CMD="$TESTCLIENT --seed \"$RANDOMSEED\" --challenge --verbose"
capture CHALLENGE "$CHALLENGE_CMD"
echo "Client challenge: $CHALLENGE"

# Hash content.
HASH_CMD="$KEYTOOL --key_version $KEYVERSION \
                   --input_file $TESTCONTENT --hashout \
                   --private_key_file $PRIVKEY"
capture HASH "$HASH_CMD"
echo "Content hash: $HASH"

# Sign challenge + hash.
SIGNATURE_CMD="$KEYTOOL --key_version $KEYVERSION \
                        --input_file $TESTCONTENT \
                        --challenge=\"$CHALLENGE\" --sigout \
                        --private_key_file $PRIVKEY"
capture SIGNATURE "$SIGNATURE_CMD"
echo "Server proof: $SIGNATURE"

# Check client can verify signature.
VERIFY_CMD="$TESTCLIENT --seed \"$RANDOMSEED\" \
                        --signature \"$SIGNATURE\" \
                        --hash \"$HASH\" \
                        --input_file $TESTCONTENT"
capture PASS "$VERIFY_CMD"

check_eq "$PASS" "PASS"

echo $PASS
