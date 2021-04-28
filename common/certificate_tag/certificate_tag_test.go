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
	"encoding/binary"
	"flag"
	"fmt"
	"io/ioutil"
	"os/exec"
	"path/filepath"
	"strings"
	"testing"
)

// Modified from the google3/googleclient/tools version so it can run outside of google3.
// Build certificate_tag separately, and point flag tag-binary-dir to the build location.
//
// Here is an example of testing the 32-bit version on Linux:
//
// $ GOARCH=386 CC=gcc go build -o /tmp/certificate_tag common/certificate_tag/certificate_tag.go
// $ GOARCH=386 CC=gcc go test common/certificate_tag/certificate_tag_test.go common/certificate_tag/certificate_tag.go
//
// Here is an example of testing the 32-bit version on Windows 10.
//
// $ go build -o C:/tmp/certificate_tag common/certificate_tag/certificate_tag.go
// $ go test common/certificate_tag/certificate_tag_test.go common/certificate_tag/certificate_tag.go -tag-binary-dir "C:/tmp"

var (
	tagBinaryDir *string = flag.String("tag-binary-dir", "/tmp", "Path to directory with the tag binary.")
	// tagBinary contains the path to the certificate_tag program.
	tagBinary string
	// sourceExe contains the path to a Chrome installer exe file.
	sourceExe string
	// sourceMSI* contains the path to a signed MSI file.
	sourceMSI1, sourceMSI2, sourceMSI3, sourceMSI4 string
)

// existingTagSubstring is a segment of the superfluous-cert tag that's already
// in ChromeSetup.exe.
const existingTagSubstring = ".....Gact.?omah"

func TestMain(m *testing.M) {
	flag.Parse()

	tagBinary = filepath.Join(*tagBinaryDir, "certificate_tag")
	sourceExe = filepath.Join("testdata/ChromeSetup.exe")
	sourceMSI1 = filepath.Join("testdata/googlechromestandaloneenterprise.msi")
	sourceMSI2 = filepath.Join("testdata/test7zSigned.msi")
	sourceMSI3 = filepath.Join("testdata/OmahaTestSigned.msi")
	sourceMSI4 = filepath.Join("testdata/test7zSigned-smallcert.msi")

	m.Run()
}

func TestPrintAppendedTag(t *testing.T) {
	cmd := exec.Command(tagBinary, "--dump-appended-tag", sourceExe)
	output, err := cmd.CombinedOutput()
	if err != nil {
		t.Fatalf("Error executing %q: %v; output:\n%s", tagBinary, err, output)
	}

	if out := string(output); !strings.Contains(out, existingTagSubstring) {
		t.Errorf("Output of --dump-appended-tag didn't contain %s, as expected. Got:\n%s", existingTagSubstring, out)
	}
}

// tempFileName returns a path that can be used as temp file. This is only safe
// because we know that only our process can write in the test's temp
// directory.
func tempFileName(t *testing.T) string {
	f, err := ioutil.TempFile(t.TempDir(), "certificate_tag_test")
	if err != nil {
		panic(err)
	}
	path := f.Name()
	f.Close()
	return path
}

func SetSuperfluousCertTagHelper(t *testing.T, source string) {
	out := tempFileName(t)

	expected := "34cf251b916a54dc9351b832bb0ac7ce" + strings.Repeat(" ", 256)
	cmd := exec.Command(tagBinary, "--out", out, "--set-superfluous-cert-tag", expected, source)
	if output, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("Test input %s, error executing %q: %v; output:\n%s", source, tagBinary, err, output)
	}

	contents, err := ioutil.ReadFile(out)
	if err != nil {
		t.Fatalf("Test input %s, failed to read output file: %s", source, err)
	}
	if !bytes.Contains(contents, []byte(expected)) {
		t.Errorf("Test input %s, output doesn't contain expected bytes", source)
	}
	if bytes.Contains(contents, []byte(existingTagSubstring)) {
		t.Errorf("Test input %s, output still contains old tag that should have been replaced", source)
	}

	cmd = exec.Command(tagBinary, "--out", out, "--set-superfluous-cert-tag", expected, "--padded-length", "512", source)
	if output, err := cmd.CombinedOutput(); err != nil {
		t.Fatalf("Test input %s, error executing %q: %v; output:\n%s", source, tagBinary, err, output)
	}

	contents, err = ioutil.ReadFile(out)
	if err != nil {
		t.Fatalf("Test input %s, failed to read output file: %s", source, err)
	}
	var zeros [16]byte
	if !bytes.Contains(contents, append([]byte(expected), zeros[:]...)) {
		t.Errorf("Test input %s, output doesn't contain expected bytes with padding", source)
	}
}

func TestSetSuperfluousCertTag(t *testing.T) {
	expect := []struct {
		infile string
	}{
		{sourceExe},
		{sourceMSI1},
		{sourceMSI2},
		{sourceMSI3},
	}
	for _, e := range expect {
		SetSuperfluousCertTagHelper(t, e.infile)
	}
}

func TestIsLastInSector(t *testing.T) {
	expect := []struct {
		in    int
		shift uint16 // 12 for 4096-byte sectors, 9 for 512-byte sectors.
		want  bool
	}{
		{0, 12, false},
		{1, 12, false},
		{107, 12, false},
		{108, 12, false},
		{109, 12, false},
		{1131, 12, false},
		{1132, 12, true},
		{1133, 12, false},
		{2156, 12, true},
		{0, 9, false},
		{1, 9, false},
		{107, 9, false},
		{108, 9, false},
		{109, 9, false},
		{236, 9, true},
		{364, 9, true},
	}
	for _, e := range expect {
		format, _ := newSectorFormat(e.shift)
		got := format.isLastInSector(e.in)
		if got != e.want {
			t.Errorf("Arguments (%d, %d): got %t, want %t", e.in, e.shift, got, e.want)
		}
	}
}

func TestFirstFreeFatEntry(t *testing.T) {
	expect := []struct {
		in   int
		want secT
	}{
		{1023, 1024},
		{1000, 1001},
		{10, 11},
		{0, 1},
	}
	for _, e := range expect {
		entries := make([]secT, 0, 1024)
		for i := 0; i < 1024; i++ {
			entries = append(entries, fatFreesect)
		}
		entries[e.in] = 1
		got := firstFreeFatEntry(entries)
		if got != e.want {
			t.Errorf("Argument %d, got %d, want %d", e.in, got, e.want)
		}
	}
}

func getFat(sectors, free int) []secT {
	// Zero is a valid sector. In a valid file, though, the sector number wouldn't repeat like this.
	used := 1024*sectors - free // 1024 int32 entries per sector.
	entries := make([]secT, used, used+free)

	// Set a non-contiguous free sector before the end, which shouldn't affect anything.
	if used > 2 {
		entries[used-2] = fatFreesect
	}
	for i := 0; i < free; i++ {
		entries = append(entries, fatFreesect)
	}
	return entries
}

func getDifat(sectors, free int) []secT {
	// Similar to getFat, but there are always 109 (non-sector) elements from the header;
	// and the last element of any sectors should be fatEndofchain or a sector #.
	entries := make([]secT, 109, 109+sectors*1024)
	sentinel := secT(123) // Some sector number
	for ; sectors > 0; sectors-- {
		new := make([]secT, 1024)
		if sectors == 1 {
			new[1023] = fatEndofchain
		} else {
			new[1023] = sentinel
		}
		entries = append(entries, new...)
	}
	i := len(entries) - 1
	for free > 0 {
		if entries[i] != fatEndofchain && entries[i] != sentinel {
			entries[i] = fatFreesect
			free--
		}
		i--
	}
	return entries
}

func getBin(fatEntries, difatEntries []secT) *MSIBinary {
	// Uses dll version 4, 4096-byte sectors.
	// There are difat sectors only if len(difatEntries) > 109.
	n := 0
	if len(difatEntries) > numDifatHeaderEntries {
		n = (len(difatEntries)-numDifatHeaderEntries-1)/1024 + 1
	}
	// If n>0, zero is a fine sector number.
	difatSectors := make([]secT, n)

	header := &MSIHeader{
		DllVersion:      4,
		SectorShift:     12,
		NumDifatSectors: uint32(n),
	}

	// Make copies so we can compare before and after method calls.
	fat := make([]secT, len(fatEntries))
	copy(fat, fatEntries)
	difat := make([]secT, len(difatEntries))
	copy(difat, difatEntries)

	format, _ := newSectorFormat(12)
	return &MSIBinary{
		headerBytes:     nil,
		header:          header,
		sector:          format,
		contents:        nil,
		sigDirOffset:    0,
		sigDirEntry:     nil,
		signedDataBytes: nil,
		signedData:      nil,
		fatEntries:      fat,
		difatEntries:    difat,
		difatSectors:    difatSectors,
	}
}

func verifyAndRemoveDifatChaining(t *testing.T, entries []secT, which, name string, id int) []secT {
	format, _ := newSectorFormat(12)
	for i := len(entries) - 1; i >= 0; i-- {
		if format.isLastInSector(i) {
			if i == len(entries)-1 && entries[i] != fatEndofchain {
				t.Errorf("%s end of chain %s was modified, case %d, i %d: wanted %d (fatEndofchain), got %d", which, name, id, i, secT(fatEndofchain), entries[i])
			}
			if i != len(entries)-1 && entries[i] >= fatReserved {
				t.Errorf("%s %s entries weren't chained, case %d: wanted (< %d) (fatReserved), got %d", which, name, id, secT(fatReserved), entries[i])
			}
			if i == len(entries)-1 {
				entries = entries[:i]
			} else {
				entries = append(entries[:i], entries[i+1:]...)
			}
		}
	}
	return entries
}

func verifyEntries(t *testing.T, name string, id, added int, changed, old, new []secT, isDifat bool) {
	if len(new)-len(old) != added {
		t.Errorf("Wrong num added %s entries, case %d: wanted %d, got %d", name, id, added, len(new)-len(old))
	}
	// If this is difat, check and remove the chaining entries. This simplifies the checks below.
	if isDifat {
		// If there is an error in "old", the test case wasn't set up correctly.
		old = verifyAndRemoveDifatChaining(t, old, "old", name, id)
		new = verifyAndRemoveDifatChaining(t, new, "new", name, id)
	}
	firstFree := len(old) // Can be past end of slice.
	for firstFree > 0 && old[firstFree-1] == fatFreesect {
		firstFree--
	}
	same := new[:firstFree]
	diff := new[firstFree : firstFree+len(changed)]
	free := new[firstFree+len(changed):]
	for i := 0; i < len(same); i++ {
		if old[i] != same[i] {
			t.Errorf("Entry in %s should not be changed, case %d, i %d: wanted %d, got %d", name, id, i, old[i], same[i])
		}
	}
	for i := 0; i < len(diff); i++ {
		if changed[i] != diff[i] {
			t.Errorf("Entry in %s is not changed or not changed to correct value, case %d, offset %d, i %d: wanted %d, got %d", name, id, firstFree, i, changed[i], diff[i])
		}
	}
	for i := 0; i < len(free); i++ {
		if free[i] != fatFreesect {
			t.Errorf("Entry in %s should be free but isn't, case %d, offset %d, i %d: wanted %d (fatFreesect), got %d", name, id, firstFree+len(changed), i, secT(fatFreesect), free[i])
		}
	}
}

func TestEnsureFreeDifatEntry(t *testing.T) {
	expect := []struct {
		id           int    // case id
		difatSectors int    // in: # difat sectors
		difatFree    int    // in: # free difat entries
		changedDifat []secT // expect: value of changed difat entries
		addedDifat   int    // expect: # difat entries added
		fatSectors   int    // in: # fat sectors
		fatFree      int    // in: # free fat entries
		changedFat   []secT // expect: value of changed fat entries
		addedFat     int    // expect: # fat entries added
	}{
		// Note: The number of difat used entries should imply the # of fat sectors.
		// But that inconsistency doesn't affect these tests.

		// Free difat entry in header, no change.
		{0, 0, 108, []secT{}, 0, 1, 40, []secT{}, 0},
		// No free difat entry, add a difat sector (1024 entries).
		{1, 0, 0, []secT{}, 1024, 1, 40, []secT{fatDifsect}, 0},
		// Free difat entry in sector, no change.
		{2, 1, 1, []secT{}, 0, 1, 40, []secT{}, 0},
		// No free difat entry, add a difat sector.
		{3, 1, 0, []secT{}, 1024, 1, 40, []secT{fatDifsect}, 0},
		// Additional sector is completely empty, no change.
		{4, 1, 1023, []secT{}, 0, 1, 40, []secT{}, 0},
		// Free difat entry; No free fat entry. No change to either.
		{5, 0, 10, []secT{}, 0, 1, 0, []secT{}, 0},
		// No free difat entry; add a difat sector. No free fat entry; add a fat sector.
		{6, 0, 0, []secT{1024}, 1024, 1, 0, []secT{fatFatsect, fatDifsect}, 1024},
		{7, 1, 0, []secT{1024}, 1024, 1, 0, []secT{fatFatsect, fatDifsect}, 1024},
	}

	for _, e := range expect {
		fat := getFat(e.fatSectors, e.fatFree)
		difat := getDifat(e.difatSectors, e.difatFree)
		bin := getBin(fat, difat)
		bin.ensureFreeDifatEntry()

		// Check added entries.
		verifyEntries(t, "difat", e.id, e.addedDifat, e.changedDifat, difat, bin.difatEntries, true)
		verifyEntries(t, "fat", e.id, e.addedFat, e.changedFat, fat, bin.fatEntries, false)
	}
}

func TestEnsureFreeFatEntries(t *testing.T) {
	expect := []struct {
		id           int    // case id
		difatSectors int    // in: # difat sectors
		difatFree    int    // in: # free difat entries
		changedDifat []secT // expect: value of changed difat entries
		addedDifat   int    // expect: # difat entries added
		fatSectors   int    // in: # fat sectors
		fatFree      int    // in: # free fat entries available
		fatRequest   secT   // in: # free fat entries requested
		changedFat   []secT // expect: value of changed fat entries
		addedFat     int    // expect: # fat entries added
	}{
		// Note: The number of difat used entries should imply the # of fat sectors.
		// But that inconsistency doesn't affect these tests.

		{0, 0, 1, []secT{}, 0, 1, 2, 2, []secT{}, 0},
		{1, 0, 0, []secT{}, 0, 1, 2, 2, []secT{}, 0},
		{2, 0, 1, []secT{1022}, 0, 1, 2, 4, []secT{fatFatsect}, 1024},
		{3, 0, 0, []secT{1022}, 1024, 1, 2, 4, []secT{fatFatsect, fatDifsect}, 1024},
		{4, 0, 1, []secT{1024}, 0, 1, 0, 4, []secT{fatFatsect}, 1024},
		{5, 0, 0, []secT{1024}, 1024, 1, 0, 4, []secT{fatFatsect, fatDifsect}, 1024},
		{6, 1, 1, []secT{1022}, 0, 1, 2, 4, []secT{fatFatsect}, 1024},
		{7, 1, 0, []secT{1022}, 1024, 1, 2, 4, []secT{fatFatsect, fatDifsect}, 1024},
		{8, 2, 1, []secT{2046}, 0, 2, 2, 4, []secT{fatFatsect}, 1024},
		{9, 2, 0, []secT{2046}, 1024, 2, 2, 4, []secT{fatFatsect, fatDifsect}, 1024},

		// These are unlikely cases, but they should work.
		// Request exactly one more sector free. (The difat sector will consume a fat entry as well.)
		{10, 0, 1, []secT{1022}, 0, 1, 2, 1025, []secT{fatFatsect}, 1024},
		// Request more than one more sector.
		{11, 0, 2, []secT{1022, 1023}, 0, 1, 2, 1026, []secT{fatFatsect, fatFatsect}, 2048},
		// Request more than one sector because of additional difat sector.
		{12, 0, 0, []secT{1022, 1024}, 1024, 1, 2, 1025, []secT{fatFatsect, fatDifsect, fatFatsect}, 2048},
	}

	for _, e := range expect {
		fat := getFat(e.fatSectors, e.fatFree)
		difat := getDifat(e.difatSectors, e.difatFree)
		bin := getBin(fat, difat)
		bin.ensureFreeFatEntries(e.fatRequest)

		// Check added entries.
		verifyEntries(t, "difat", e.id, e.addedDifat, e.changedDifat, difat, bin.difatEntries, true)
		verifyEntries(t, "fat", e.id, e.addedFat, e.changedFat, fat, bin.fatEntries, false)
	}
}

func TestAssignDifatEntry(t *testing.T) {
	expect := []struct {
		id            int  // case id
		difatSectors  int  // in: # difat sectors
		difatFree     int  // in: # free difat entries
		assignedIndex int  // expect: which difat index assigned
		assignedValue secT // in/expect: value assigned
		fatSectors    int  // in: # fat sectors
		fatFree       int  // in: # free fat entries
	}{
		{1, 0, 1, 108, 1000, 1, 23},
		{2, 0, 0, 109, 1000, 1, 23},
		{3, 1, 1, 1131, 1000, 1, 23},
		{4, 1, 0, 1133, 1000, 1, 23},
	}
	for _, e := range expect {
		fat := getFat(e.fatSectors, e.fatFree)
		difat := getDifat(e.difatSectors, e.difatFree)
		bin := getBin(fat, difat)
		bin.assignDifatEntry(e.assignedValue)

		if len(bin.difatEntries) < e.assignedIndex+1 {
			t.Errorf("Slice too short, index not valid, case %d. Wanted index %d, got slice length %d", e.id, e.assignedIndex, len(bin.difatEntries))
		} else {
			if bin.difatEntries[e.assignedIndex] != e.assignedValue {
				t.Errorf("Wrong index assigned, case %d. At index %d, wanted %d, got %d", e.id, e.assignedIndex, e.assignedValue, bin.difatEntries[e.assignedIndex])
			}
		}
	}
}

// Validate returns an error if the MSI doesn't pass internal consistency checks.
// If another MSIBinary is provided, Validate checks that data streams are bitwise identical.
// It also returns whether the dummy certificate was found.
func (bin MSIBinary) Validate(other *MSIBinary) (bool, error) {
	// Check that fat sectors are marked as such in the fat.
	for i, s := range bin.difatEntries {
		if s != fatFreesect && !bin.sector.isLastInSector(i) && bin.fatEntries[s] != fatFatsect {
			return false, fmt.Errorf("fat sector %d (index %d) is not marked as such in the fat", s, i)
		}
	}
	// Check that difat sectors are marked as such in the fat.
	s := secT(bin.header.FirstDifatSector)
	i := numDifatHeaderEntries - 1
	num := 0
	for s != fatEndofchain {
		if bin.fatEntries[s] != fatDifsect {
			return false, fmt.Errorf("difat sector %d (offset %d in chain) is not marked as such in the fat", s, num)
		}
		i += int(bin.sector.Ints)
		s = bin.difatEntries[i]
		num++
	}
	if num != int(bin.header.NumDifatSectors) {
		return false, fmt.Errorf("wrong number of difat sectors found, wanted %d got %d", bin.header.NumDifatSectors, num)
	}

	// Enumerate the directory entries.
	// 1) Validate streams in the fat: Walk the chain, validate the stream length,
	//    and mark sectors in a copy of the fat so we can tell if any sectors are re-used.
	// 2) Compare bytes in the data streams, to validate none of them changed.
	//    In principle we should match stream names, but in practice the directory entries are not
	//    reordered and the streams are not moved.
	fatEntries := make([]secT, len(bin.fatEntries))
	copy(fatEntries, bin.fatEntries)
	dirSector := secT(bin.header.FirstDirSector)
	var entry MSIDirEntry
	for {
		// Fixed 128 byte directory entry size.
		for i := offT(0); i < bin.sector.Size/numDirEntryBytes; i++ {
			offset := offT(dirSector)*bin.sector.Size + i*numDirEntryBytes
			binary.Read(bytes.NewBuffer(bin.contents[offset:]), binary.LittleEndian, &entry)

			// The mini fat hasn't been parsed, so skip those. The size check also skips non-stream
			// entries. The signature stream has been freed, so skip that one too.
			if entry.StreamSize < miniStreamCutoffSize ||
				bytes.Equal(entry.Name[:entry.NumNameBytes], signatureName) {
				continue
			}
			allocatedSize := offT(0)
			sector := secT(entry.StreamFirstSector)
			for {
				allocatedSize += bin.sector.Size
				if fatEntries[sector] != fatEndofchain && fatEntries[sector] >= fatReserved {
					return false, fmt.Errorf("Found bad/reused fat entry at sector %d; wanted value < %d (fatReserved), got %d", sector, secT(fatReserved), fatEntries[sector])
				}
				// Technically we need not check beyond the end of stream data, but these sectors
				// should not be modified at all.
				if other != nil {
					offset := offT(sector) * bin.sector.Size
					if !bytes.Equal(bin.contents[offset:offset+bin.sector.Size], other.contents[offset:offset+bin.sector.Size]) {
						return false, fmt.Errorf("Found difference in streams at sector %d", sector)
					}
				}
				next := fatEntries[sector]
				fatEntries[sector] = fatReserved // Detect if this is re-used.
				if next == fatEndofchain {
					break
				}
				sector = next
			}
			if uint64(allocatedSize) < entry.StreamSize {
				return false, fmt.Errorf("Found stream with size greater than allocation, starting sector %d", entry.StreamFirstSector)
			}
		}
		// Go to the next directory sector.
		dirSector = bin.fatEntries[dirSector]
		if dirSector == fatEndofchain {
			break
		}
	}

	// Compare certs and signatures (other than dummy).
	cert, index, err := getSuperfluousCert(bin.signedData)
	if err != nil {
		return false, fmt.Errorf("parse error in bin.signedData: %w", err)
	}
	if other != nil {
		_, index2, err := getSuperfluousCert(other.signedData)
		if err != nil {
			return false, fmt.Errorf("parse error in other.signedData: %w", err)
		}
		pkcs7 := bin.signedData.PKCS7
		pkcs7Other := other.signedData.PKCS7
		i := 0
		i2 := 0
		for {
			if i == index {
				i++
			}
			if i2 == index2 {
				i2++
			}
			if i >= len(pkcs7.Certs) || i2 >= len(pkcs7Other.Certs) {
				if i < len(pkcs7.Certs) || i2 < len(pkcs7Other.Certs) {
					return false, fmt.Errorf("number of certs mismatch, compare other %d vs this %d (possibly including dummy cert)", len(pkcs7Other.Certs), len(pkcs7.Certs))
				}
				break
			}
			if !bytes.Equal(pkcs7.Certs[i].FullBytes, pkcs7Other.Certs[i2].FullBytes) {
				return false, fmt.Errorf("cert contents mismatch, other cert index %d vs this cert index %d", i2, i)
			}
			i++
			i2++
		}
	}

	return cert != nil, nil
}

func TestMsiSuperfluousCert(t *testing.T) {
	const tag = "258c 6320 e4c4 0258 169b 481a def0 8856" // Random data
	expect := []struct {
		infile string
	}{
		{sourceMSI1},
		{sourceMSI2},
		{sourceMSI3},
	}
	for _, e := range expect {
		contents, err := ioutil.ReadFile(e.infile)
		if err != nil {
			t.Fatalf("Error reading test input %s: %v", e.infile, err)
		}

		bin, err := NewBinary(contents)
		if err != nil {
			t.Fatalf("Error creating MSIBinary from test input %s: %v", e.infile, err)
		}
		msiBin := bin.(*MSIBinary)
		hasDummy, err := msiBin.Validate(nil)
		if err != nil {
			t.Errorf("Input binary doesn't validate, created from test input %s: %v", e.infile, err)
		} else if hasDummy {
			t.Errorf("Input binary has the dummy cert (it shouldn't), created from test input %s", e.infile)
		}

		// Note this adds the dummy cert to |bin|.
		contents, err = bin.SetSuperfluousCertTag([]byte(tag))
		if err != nil {
			t.Errorf("Error tagging test input %s: %v", e.infile, err)
			continue
		}
		binTagged, err := NewBinary(contents)
		if err != nil {
			t.Errorf("Error parsing tagged binary from test input %s: %v", e.infile, err)
			continue
		}

		msiBinTagged := binTagged.(*MSIBinary)
		hasDummy, err = msiBinTagged.Validate(msiBin)
		if err != nil {
			t.Errorf("Tagged binary doesn't validate, created from test input %s: %v", e.infile, err)
		} else if !hasDummy {
			t.Errorf("Tagged binary doesn't have the dummy cert (it should), created from test input %s", e.infile)
		}
	}
}

func TestFindTag(t *testing.T) {
	oid := []byte{
		0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0xce, 0x0f, 0x04, 0x82}
	oidStr := string(oid)
	oidSize := int64(len(oid) + 2) // includes size bytes
	marker := []byte{0x47, 0x61, 0x63, 0x74, 0x32, 0x2e, 0x30, 0x4f, 0x6d, 0x61, 0x68, 0x61}

	// Create test strings.
	expect := []struct {
		name   string
		in     string
		start  int64
		offset int64
		length int64
		hasErr bool
	}{
		{"no padding", oidStr + "\x00\x10" + strings.Repeat("0", 16), 0, oidSize, 16, false},
		{"start padding", "1111" + oidStr + "\x00\x10" + strings.Repeat("1", 16), 0, 4 + oidSize, 16, false},
		{"start and end padding", "2222" + oidStr + "\x00\x10" + strings.Repeat("2", 20), 0, 4 + oidSize, 16, false},
		{"no tag", "3333" + "\x00\x10" + strings.Repeat("3", 20), 0, -1, 0, false},
		{"tag prior to search start", "4444" + oidStr + "\x00\x10" + strings.Repeat("4", 20), 10, -1, 0, false},
		{"error no length bytes", "5555" + oidStr, 0, -1, 0, true},
		{"error length too long", "6666" + oidStr + "\x00\x10" + strings.Repeat("6", 15), 0, -1, 0, true},
	}

	for _, e := range expect {
		offset, length, err := findTag([]byte(e.in), e.start)
		if offset != e.offset {
			t.Errorf("test %s, got offset %d, want %d", e.name, offset, e.offset)
		}
		if length != e.length {
			t.Errorf("test %s, got length %d, want %d", e.name, length, e.length)
		}
		if (err != nil) != e.hasErr {
			t.Errorf("test %s, got error %v, want %v", e.name, err, e.hasErr)
		}
	}

	// Test end-to-end with testdata files.
	expect2 := []struct {
		infile string
		size   int64
	}{
		{sourceExe, 2048},
		{sourceExe, 1000},
		{sourceExe, 256},
		{sourceMSI1, 2048},
		{sourceMSI2, 2048},
		{sourceMSI2, 1000},
		{sourceMSI2, 256},
		{sourceMSI3, 2048},
		{sourceMSI4, 2048},
	}
	for i, e := range expect2 {
		contents, err := ioutil.ReadFile(e.infile)
		if err != nil {
			t.Fatalf("Case %d, error reading test input %s: %v", i, e.infile, err)
		}
		bin, err := NewBinary(contents)
		if err != nil {
			t.Fatalf("Case %d, error creating MSIBinary from test input %s: %v", i, e.infile, err)
		}
		// NewBinary may modify |contents|.
		contents, err = ioutil.ReadFile(e.infile)
		if err != nil {
			t.Fatalf("Case %d, error reading test input %s: %v", i, e.infile, err)
		}

		// No tag before tagging.
		offset, _, err := findTag(contents, bin.certificateOffset())
		if err != nil {
			t.Errorf("Case %d, error in findTag for untagged source %s: %v", i, e.infile, err)
		}
		if offset != -1 {
			t.Errorf("Case %d, found tag in untagged input %s, want offset -1 got %d", i, e.infile, offset)
		}

		// Apply a tag; find tag in contents; verify it's at marker with right size.
		tag := make([]byte, e.size)
		copy(tag[:], marker)
		contents, err = bin.SetSuperfluousCertTag(tag)
		if err != nil {
			t.Fatalf("Case %d, error tagging source %s: %v", i, e.infile, err)
		}
		offset, length, err := findTag(contents, bin.certificateOffset())
		if err != nil {
			t.Errorf("Case %d, error in findTag for source %s: %v", i, e.infile, err)
		}
		if length != e.size {
			t.Errorf("Case %d, error in findTag for source %s: wanted returned length %d, got %d", i, e.infile, e.size, length)
		}
		if offset < 0 {
			t.Errorf("Case %d, error in findTag for source %s: wanted returned offset >=0, got %d", i, e.infile, offset)
		} else {
			// Expect to find size bytes just prior to offset.
			size := int64(binary.BigEndian.Uint16(contents[offset-2:]))
			if size != e.size {
				// Either the size is wrong, or (more likely) we found the wrong offset.
				t.Errorf("Case %d, error in findTag for source %s, offset %d: wanted embedded size %d, got %d", i, e.infile, offset, e.size, size)
			}
			// Expect to find the marker at |offset|
			idx := bytes.Index(contents[offset:], marker)
			if idx != 0 {
				t.Errorf("Case %d, error in findTag for source %s: after offset %d, wanted to find marker at idx 0, got %d", i, e.infile, offset, idx)
			}
		}
	}
}
