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
TESTCLIENT=$TEST_TMPDIR/testclient
RANDOMSEED=`date +%s-%N`  # Crappy but changing seed.
TESTCONTENT=$TEST_TMPDIR/publickey.h
PRIVKEY=$TEST_TMPDIR/privatekey
SRCS="md5.h md5.c \
      sha.h sha.c \
      aes.h aes.c \
      rc4.h rc4.c \
      b64.h b64.c \
      rsa.h rsa.cc \
      hash-internal.h \
      challenger.h challenger.cc \
      testclient.cc"

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

# Copy source and Makefile to TMPDIR.
cd $TEST_SRCDIR/google3/security/util/lite || die "Cannot cd"
cp $SRCS $TEST_TMPDIR || die "Cannot cp files"
cd $TEST_TMPDIR || die "Cannot cd"

for ((i=0;i<8;i+=1)); do
let "KEYVERSION = $i / 5"
let "rem = $i % 5"

if [ "$rem" -eq 0 ]; then
# Use openssl to generate a random private key.
openssl genrsa -3 1024 > $PRIVKEY.$KEYVERSION || die "Cannot openssl"

# Produce public key in compilable format.
$KEYTOOL --key_version $KEYVERSION \
         --private_key_file $PRIVKEY \
         --pubout --output_file $TESTCONTENT \
      || die "Failed to run $KEYTOOL"

cat $TESTCONTENT || die "Cannot cat $TESTCONTENT"

# Build fresh test client, plain C, C++, no google3 dependencies.
gcc md5.c sha.c aes.c b64.c rc4.c rsa.cc challenger.cc \
    testclient.cc -Wall -Os -I /usr/include -o testclient -lstdc++ \
  || die "Cannot compile"
fi

# Morph seed.
RANDOMSEED=$RANDOMSEED.testing.$i
echo "Client seed: $RANDOMSEED"

# Have client encrypt a message.
ENCRYPTION_CMD="$TESTCLIENT --encrypt \"$RANDOMSEED\" \
                            --seed \"$RANDOMSEED\""
capture ENCRYPTION "$ENCRYPTION_CMD"

# Check server can decrypt the message.
DECRYPTION_CMD="$KEYTOOL --decrypt --input=\"$ENCRYPTION\""
capture DECRYPTION "$DECRYPTION_CMD"

check_eq "$DECRYPTION" "$RANDOMSEED"

# Have client produce a challenge.
CHALLENGE_CMD="$TESTCLIENT --seed \"$RANDOMSEED\" --challenge"
capture CHALLENGE "$CHALLENGE_CMD"
echo "Client challenge: $CHALLENGE"

# Compute hash of content.
HASH_CMD="$KEYTOOL --key_version $KEYVERSION \
                   --input_file $TESTCONTENT --hashout \
                   --private_key_file $PRIVKEY"
capture HASH "$HASH_CMD"
echo "Content hash: $HASH"

# Have server sign challenge + hash.
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

done

echo $PASS
