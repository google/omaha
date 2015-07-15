// Copyright 2015 Google Inc.
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

package main

import (
	"bytes"
	"io/ioutil"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"

	"google3/testing/gobase/googletest"
)

const directory = "google3/googleclient/installer/tools"

var (
	// tagBinary contains the path to the certificate_tag program.
	tagBinary string
	// sourceExe contains the path to a Chrome installer exe file.
	sourceExe string
)

func init() {
	tagBinary = filepath.Join(googletest.TestSrcDir, directory, "certificate_tag")
	sourceExe = filepath.Join(googletest.TestSrcDir, directory, "testdata/ChromeSetup.exe")
}

func TestPrintAppendedTag(t *testing.T) {
	cmd := exec.Command(tagBinary, "--dump-appended-tag", sourceExe)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatal(err)
	}

	const expected = ".....Gact.?omah"
	if out := string(output); !strings.Contains(out, expected) {
		t.Errorf("Output of --dump-appended-tag didn't contain %s, as expected. Got:\n%s", expected, out)
	}
}

// tempFileName returns a path that can be used as temp file. This is only safe
// because we know that only our process can write in the test's temp
// directory.
func tempFileName() string {
	f, err := ioutil.TempFile(googletest.TestTmpDir, "certificate_tag_test")
	if err != nil {
		panic(err)
	}
	path := f.Name()
	f.Close()
	return path
}

func TestSetSuperfluousCertTag(t *testing.T) {
	out := tempFileName()

	const expected = "34cf251b916a54dc9351b832bb0ac7ce"
	cmd := exec.Command(tagBinary, "--out", out, "--set-superfluous-cert-tag", expected, sourceExe)
	if err := cmd.Run(); err != nil {
		t.Fatal(err)
	}

	contents, err := ioutil.ReadFile(out)
	if err != nil {
		t.Fatalf("Failed to read output file: %s", err)
	}
	if !bytes.Contains(contents, []byte(expected)) {
		t.Error("Output doesn't contain expected bytes")
	}

	cmd = exec.Command(tagBinary, "--out", out, "--set-superfluous-cert-tag", expected, "--padded-length", "256", sourceExe)
	if err = cmd.Run(); err != nil {
		t.Fatal(err)
	}

	contents, err = ioutil.ReadFile(out)
	if err != nil {
		t.Fatalf("Failed to read output file: %s", err)
	}
	var zeros [16]byte
	if !bytes.Contains(contents, append([]byte(expected), zeros[:]...)) {
		t.Error("Output doesn't contain expected bytes with padding")
	}
}
