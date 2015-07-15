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
//
// certificate_tag.go is a tool for manipulating "tags" in Authenticode-signed,
// Windows binaries.
//
// Traditionally we have inserted tag data after the PKCS#7 blob in the file
// (called an "appended tag" here). This area is not hashed in when checking
// the signature so we can alter it at serving time without invalidating the
// Authenticode signature.
//
// However, Microsoft are changing the verification function to forbid that so
// this tool also handles "superfluous certificate" tags. These are dummy
// certificates, inserted into the PKCS#7 certificate chain, that can contain
// arbitrary data in extensions. Since they are also not hashed when verifying
// signatures, that data can also be changed without invalidating it.

package main

import (
	"bytes"
	"crypto/rand"
	"crypto/rsa"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"flag"
	"fmt"
	"io"
	"io/ioutil"
	"math/big"
	"os"
	"strings"
	"time"
)

const (
	// rsaKeyBits is the number of bits in the RSA modulus of the key that
	// we generate.
	rsaKeyBits = 2048
	// notBeforeTime and notAfterTime are the validity period of the
	// certificate that we generate. They are deliberately set so that they
	// are already expired.
	notBeforeTime = "Mon Jan 1 10:00:00 UTC 2013"
	notAfterTime  = "Mon Apr 1 10:00:00 UTC 2013"
)

// The structures here were taken from "Microsoft Portable Executable and
// Common Object File Format Specification".

const fileHeaderSize = 20

// fileHeader represents the IMAGE_FILE_HEADER structure from
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms680313(v=vs.85).aspx.
type fileHeader struct {
	Machine               uint16
	NumberOfSections      uint16
	TimeDateStamp         uint32
	PointerForSymbolTable uint32
	NumberOfSymbols       uint32
	SizeOfOptionalHeader  uint16
	Characteristics       uint16
}

// optionalHeader represents the IMAGE_OPTIONAL_HEADER structure from
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms680339(v=vs.85).aspx.
type optionalHeader struct {
	Magic                   uint16
	MajorLinkerVersion      uint8
	MinorLinkerVersion      uint8
	SizeOfCode              uint32
	SizeOfInitializedData   uint32
	SizeOfUninitializedData uint32
	AddressOfEntryPoint     uint32
	BaseOfCode              uint32
}

// dataDirectory represents the IMAGE_DATA_DIRECTORY structure from
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms680305(v=vs.85).aspx.
type dataDirectory struct {
	VirtualAddress uint32
	Size           uint32
}

// A subset of the known COFF "characteristic" flags found in
// fileHeader.Characteristics.
const (
	coffCharacteristicExecutableImage = 2
	coffCharacteristicDLL             = 0x2000
)

const (
	pe32Magic     = 0x10b
	pe32PlusMagic = 0x20b
)

const (
	certificateTableIndex = 4
)

// getAttributeCertificates takes a PE file and returns the offset and size of
// the attribute certificates section in the file, or an error. If found, it
// additionally returns an offset to the location in the file where the size of
// the table is stored.
func getAttributeCertificates(bin []byte) (offset, size, sizeOffset int, err error) {
	// offsetOfPEHeaderOffset is the offset into the binary where the
	// offset of the PE header is found.
	const offsetOfPEHeaderOffset = 0x3c
	if len(bin) < offsetOfPEHeaderOffset+4 {
		err = errors.New("binary truncated")
		return
	}

	peOffset := int(binary.LittleEndian.Uint32(bin[offsetOfPEHeaderOffset:]))
	if peOffset < 0 || peOffset+4 < peOffset {
		err = errors.New("overflow finding PE signature")
		return
	}
	if len(bin) < peOffset+4 {
		err = errors.New("binary truncated")
		return
	}
	pe := bin[peOffset:]
	if !bytes.Equal(pe[:4], []byte{'P', 'E', 0, 0}) {
		err = errors.New("PE header not found at expected offset")
		return
	}

	r := io.Reader(bytes.NewReader(pe[4:]))
	var fileHeader fileHeader
	if err = binary.Read(r, binary.LittleEndian, &fileHeader); err != nil {
		return
	}

	if fileHeader.Characteristics&coffCharacteristicExecutableImage == 0 {
		err = errors.New("file is not an executable image")
		return
	}

	if fileHeader.Characteristics&coffCharacteristicDLL != 0 {
		err = errors.New("file is a DLL")
		return
	}

	r = io.LimitReader(r, int64(fileHeader.SizeOfOptionalHeader))
	var optionalHeader optionalHeader
	if err = binary.Read(r, binary.LittleEndian, &optionalHeader); err != nil {
		return
	}

	// addressSize is the size of various fields in the Windows-specific
	// header to follow.
	var addressSize int

	switch optionalHeader.Magic {
	case pe32PlusMagic:
		addressSize = 8
	case pe32Magic:
		addressSize = 4

		// PE32 contains an additional field in the optional header.
		var baseOfData uint32
		if err = binary.Read(r, binary.LittleEndian, &baseOfData); err != nil {
			return
		}
	default:
		err = fmt.Errorf("unknown magic in optional header: %x", optionalHeader.Magic)
		return
	}

	// Skip the Windows-specific header section up to the number of data
	// directory entries.
	toSkip := addressSize + 40 + addressSize*4 + 4
	skipBuf := make([]byte, toSkip)
	if _, err = r.Read(skipBuf); err != nil {
		return
	}

	// Read the number of directory entries, which is also the last value
	// in the Windows-specific header.
	var numDirectoryEntries uint32
	if err = binary.Read(r, binary.LittleEndian, &numDirectoryEntries); err != nil {
		return
	}

	if numDirectoryEntries > 4096 {
		err = fmt.Errorf("invalid number of directory entries: %d", numDirectoryEntries)
		return
	}

	dataDirectory := make([]dataDirectory, numDirectoryEntries)
	if err = binary.Read(r, binary.LittleEndian, dataDirectory); err != nil {
		return
	}

	if numDirectoryEntries <= certificateTableIndex {
		err = errors.New("file does not have enough data directory entries for a certificate")
		return
	}
	certEntry := dataDirectory[certificateTableIndex]
	if certEntry.VirtualAddress == 0 {
		err = errors.New("file does not have certificate data")
		return
	}

	certEntryEnd := certEntry.VirtualAddress + certEntry.Size
	if certEntryEnd < certEntry.VirtualAddress {
		err = errors.New("overflow while calculating end of certificate entry")
		return
	}

	if int(certEntryEnd) != len(bin) {
		err = fmt.Errorf("certificate entry is not at end of file: %d vs %d", int(certEntryEnd), len(bin))
		return
	}

	var dummyByte [1]byte
	if _, readErr := r.Read(dummyByte[:]); readErr == nil || readErr != io.EOF {
		err = errors.New("optional header contains extra data after data directory")
		return
	}

	offset = int(certEntry.VirtualAddress)
	size = int(certEntry.Size)
	sizeOffset = int(peOffset) + 4 + fileHeaderSize + int(fileHeader.SizeOfOptionalHeader) - 8*(int(numDirectoryEntries)-certificateTableIndex) + 4

	if binary.LittleEndian.Uint32(bin[sizeOffset:]) != certEntry.Size {
		err = errors.New("internal error when calculating certificate data size offset")
		return
	}

	return
}

// Certificate constants. See
// http://msdn.microsoft.com/en-us/library/ms920091.aspx.
const (
	// Despite MSDN claiming that 0x100 is the only, current revision - in
	// practice it's 0x200.
	attributeCertificateRevision            = 0x200
	attributeCertificateTypePKCS7SignedData = 2
)

// processAttributeCertificates parses an attribute certificates section of a
// PE file and returns the ASN.1 data and trailing data of the sole attribute
// certificate included.
func processAttributeCertificates(certs []byte) (asn1, appendedTag []byte, err error) {
	if len(certs) < 8 {
		err = errors.New("attribute certificate truncated")
		return
	}

	// This reads a WIN_CERTIFICATE structure from
	// http://msdn.microsoft.com/en-us/library/ms920091.aspx.
	certLen := binary.LittleEndian.Uint32(certs[:4])
	revision := binary.LittleEndian.Uint16(certs[4:6])
	certType := binary.LittleEndian.Uint16(certs[6:8])

	if int(certLen) != len(certs) {
		err = errors.New("multiple attribute certificates found")
		return
	}

	if revision != attributeCertificateRevision {
		err = fmt.Errorf("unknown attribute certificate revision: %x", revision)
		return
	}

	if certType != attributeCertificateTypePKCS7SignedData {
		err = fmt.Errorf("unknown attribute certificate type: %d", certType)
		return
	}

	asn1 = certs[8:]

	if len(asn1) < 2 {
		err = errors.New("ASN.1 structure truncated")
		return
	}

	// Read the ASN.1 length of the object.
	var asn1Length int
	if asn1[1]&0x80 == 0 {
		// Short form length.
		asn1Length = int(asn1[1]) + 2
	} else {
		numBytes := int(asn1[1] & 0x7f)
		if numBytes == 0 || numBytes > 2 {
			err = fmt.Errorf("bad number of bytes in ASN.1 length: %d", numBytes)
			return
		}
		if len(asn1) < numBytes+2 {
			err = errors.New("ASN.1 structure truncated")
			return
		}
		asn1Length = int(asn1[2])
		if numBytes == 2 {
			asn1Length <<= 8
			asn1Length |= int(asn1[3])
		}
		asn1Length += 2 + numBytes
	}

	appendedTag = asn1[asn1Length:]
	asn1 = asn1[:asn1Length]

	return
}

// signedData represents a PKCS#7, SignedData strucure.
type signedData struct {
	Type  asn1.ObjectIdentifier
	PKCS7 struct {
		Version     int
		Digests     asn1.RawValue
		ContentInfo asn1.RawValue
		Certs       []asn1.RawValue `asn1:"tag:0,optional,set"`
		SignerInfos asn1.RawValue
	} `asn1:"explicit,tag:0"`
}

// Binary represents a PE binary.
type Binary struct {
	contents       []byte      // the full file
	attrCertOffset int         // the offset to the attribute certificates table
	certSizeOffset int         // the offset to the size of the attribute certificates table
	asn1Data       []byte      // the PKCS#7, SignedData in DER form.
	appendedTag    []byte      // the appended tag, if any.
	signedData     *signedData // the parsed, SignedData structure.
}

// NewBinary returns a Binary that contains details of the PE binary given in
// contents.
func NewBinary(contents []byte) (*Binary, error) {
	offset, size, certSizeOffset, err := getAttributeCertificates(contents)
	if err != nil {
		return nil, errors.New("authenticodetag: error parsing headers: " + err.Error())
	}

	attributeCertificates := contents[offset : offset+size]
	asn1Data, appendedTag, err := processAttributeCertificates(attributeCertificates)
	if err != nil {
		return nil, errors.New("authenticodetag: error parsing attribute certificate section: " + err.Error())
	}

	var signedData signedData
	if _, err := asn1.Unmarshal(asn1Data, &signedData); err != nil {
		return nil, errors.New("authenticodetag: error while parsing SignedData structure: " + err.Error())
	}

	der, err := asn1.Marshal(signedData)
	if err != nil {
		return nil, errors.New("authenticodetag: error while marshaling SignedData structure: " + err.Error())
	}

	if !bytes.Equal(der, asn1Data) {
		return nil, errors.New("authenticodetag: ASN.1 parse/unparse test failed: " + err.Error())
	}

	return &Binary{
		contents:       contents,
		attrCertOffset: offset,
		certSizeOffset: certSizeOffset,
		asn1Data:       asn1Data,
		appendedTag:    appendedTag,
		signedData:     &signedData,
	}, nil
}

// AppendedTag returns the appended tag, if any.
func (bin *Binary) AppendedTag() (data []byte, ok bool) {
	isAllZero := true
	for _, b := range bin.appendedTag {
		if b != 0 {
			isAllZero = false
			break
		}
	}

	if isAllZero && len(bin.appendedTag) < 8 {
		return nil, false
	}
	return bin.appendedTag, true
}

// buildBinary builds a PE binary based on bin but with the given SignedData
// and appended tag.
func (bin *Binary) buildBinary(asn1Data, tag []byte) (contents []byte) {
	contents = append(contents, bin.contents[:bin.certSizeOffset]...)
	for (len(asn1Data)+len(tag))&7 > 0 {
		tag = append(tag, 0)
	}
	attrCertSectionLen := uint32(8 + len(asn1Data) + len(tag))
	var lengthBytes [4]byte
	binary.LittleEndian.PutUint32(lengthBytes[:], attrCertSectionLen)
	contents = append(contents, lengthBytes[:4]...)
	contents = append(contents, bin.contents[bin.certSizeOffset+4:bin.attrCertOffset]...)

	var header [8]byte
	binary.LittleEndian.PutUint32(header[:], attrCertSectionLen)
	binary.LittleEndian.PutUint16(header[4:], attributeCertificateRevision)
	binary.LittleEndian.PutUint16(header[6:], attributeCertificateTypePKCS7SignedData)
	contents = append(contents, header[:]...)
	contents = append(contents, asn1Data...)
	return append(contents, tag...)
}

func (bin *Binary) RemoveAppendedTag() (contents []byte, err error) {
	if _, ok := bin.AppendedTag(); !ok {
		return nil, errors.New("authenticodetag: no appended tag found")
	}

	return bin.buildBinary(bin.asn1Data, nil), nil
}

func (bin *Binary) SetAppendedTag(tagContents []byte) (contents []byte, err error) {
	return bin.buildBinary(bin.asn1Data, tagContents), nil
}

// oidChromeTag is an OID that we use for the extension in the superfluous
// certificate. It's in the Google arc, but not officially assigned.
var oidChromeTag = asn1.ObjectIdentifier([]int{1, 3, 6, 1, 4, 1, 11129, 2, 1, 9999})

func (bin *Binary) getSuperfluousCert() (cert *x509.Certificate, err error) {
	n := len(bin.signedData.PKCS7.Certs)
	if n == 0 {
		return nil, nil
	}

	if cert, err = x509.ParseCertificate(bin.signedData.PKCS7.Certs[n-1].FullBytes); err != nil {
		return nil, err
	}

	for _, ext := range cert.Extensions {
		if !ext.Critical && ext.Id.Equal(oidChromeTag) {
			return cert, nil
		}
	}

	return nil, nil
}

func parseUnixTimeOrDie(unixTime string) time.Time {
	t, err := time.Parse(time.UnixDate, unixTime)
	if err != nil {
		panic(err)
	}
	return t
}

// SetSuperfluousCertTag returns a PE binary based on bin, but where the
// superfluous certificate contains the given tag data.
func (bin *Binary) SetSuperfluousCertTag(tag []byte) (contents []byte, err error) {
	cert, err := bin.getSuperfluousCert()
	if cert != nil {
		pkcs7 := &bin.signedData.PKCS7
		pkcs7.Certs = pkcs7.Certs[:len(pkcs7.Certs)-1]
	}

	notBefore := parseUnixTimeOrDie(notBeforeTime)
	notAfter := parseUnixTimeOrDie(notAfterTime)

	priv, err := rsa.GenerateKey(rand.Reader, rsaKeyBits)
	if err != nil {
		return nil, err
	}

	issuerTemplate := x509.Certificate{
		SerialNumber: new(big.Int).SetInt64(1),
		Subject: pkix.Name{
			CommonName: "Unknown issuer",
		},
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageCertSign,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageAny},
		SignatureAlgorithm:    x509.SHA1WithRSA,
		BasicConstraintsValid: true,
		IsCA: true,
	}

	template := x509.Certificate{
		SerialNumber: new(big.Int).SetInt64(1),
		Subject: pkix.Name{
			CommonName: "Dummy certificate",
		},
		Issuer: pkix.Name{
			CommonName: "Unknown issuer",
		},
		NotBefore:             notBefore,
		NotAfter:              notAfter,
		KeyUsage:              x509.KeyUsageCertSign,
		ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageAny},
		SignatureAlgorithm:    x509.SHA1WithRSA,
		BasicConstraintsValid: true,
		IsCA: false,
		ExtraExtensions: []pkix.Extension{
			{
				// This includes the tag in an extension in the
				// certificate.
				Id:    oidChromeTag,
				Value: tag,
			},
		},
	}

	derBytes, err := x509.CreateCertificate(rand.Reader, &template, &issuerTemplate, &priv.PublicKey, priv)
	if err != nil {
		return nil, err
	}

	bin.signedData.PKCS7.Certs = append(bin.signedData.PKCS7.Certs, asn1.RawValue{
		FullBytes: derBytes,
	})

	asn1Bytes, err := asn1.Marshal(*bin.signedData)
	if err != nil {
		return nil, err
	}

	return bin.buildBinary(asn1Bytes, bin.appendedTag), nil
}

var (
	dumpAppendedTag       *bool   = flag.Bool("dump-appended-tag", false, "If set, any appended tag is dumped to stdout.")
	removeAppendedTag     *bool   = flag.Bool("remove-appended-tag", false, "If set, any appended tag is removed and the binary rewritten.")
	loadAppendedTag       *string = flag.String("load-appended-tag", "", "If set, this flag contains a filename from which the contents of the appended tag will be saved")
	setSuperfluousCertTag *string = flag.String("set-superfluous-cert-tag", "", "If set, this flag contains a string and a superfluous certificate tag with that value will be set and the binary rewritten. If the string begins with '0x' then it will be interpreted as hex")
	paddedLength          *int    = flag.Int("padded-length", 0, "A superfluous cert tag will be padded with zeros to at least this number of bytes")
	savePKCS7             *string = flag.String("save-pkcs7", "", "If set to a filename, the PKCS7 data from the original binary will be written to that file.")
	outFilename           *string = flag.String("out", "", "If set, the updated binary is written to this file. Otherwise the binary is updated in place.")
)

func main() {
	flag.Parse()
	args := flag.Args()
	if len(args) != 1 {
		fmt.Fprintf(os.Stderr, "Usage: %s [flags] binary.exe\n", os.Args[0])
		os.Exit(255)
	}
	inFilename := args[0]
	if len(*outFilename) == 0 {
		outFilename = &inFilename
	}

	contents, err := ioutil.ReadFile(inFilename)
	if err != nil {
		panic(err)
	}

	bin, err := NewBinary(contents)
	if err != nil {
		fmt.Fprintf(os.Stderr, "%s\n", err)
		os.Exit(1)
	}

	didSomething := false

	if len(*savePKCS7) > 0 {
		if err := ioutil.WriteFile(*savePKCS7, bin.asn1Data, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "Error while writing file: %s\n", err)
			os.Exit(1)
		}
		didSomething = true
	}

	if *dumpAppendedTag {
		appendedTag, ok := bin.AppendedTag()
		if !ok {
			fmt.Fprintf(os.Stderr, "No appended tag found\n")
		} else {
			os.Stdout.WriteString(hex.Dump(appendedTag))
		}
		didSomething = true
	}

	if *removeAppendedTag {
		contents, err := bin.RemoveAppendedTag()
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while removing appended tag: %s\n", err)
			os.Exit(1)
		}
		if err := ioutil.WriteFile(*outFilename, contents, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "Error while writing updated file: %s\n", err)
			os.Exit(1)
		}
		didSomething = true
	}

	if len(*loadAppendedTag) > 0 {
		tagContents, err := ioutil.ReadFile(*loadAppendedTag)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while reading file: %s\n", err)
			os.Exit(1)
		}
		contents, err := bin.SetAppendedTag(tagContents)
		if err := ioutil.WriteFile(*outFilename, contents, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "Error while writing updated file: %s\n", err)
			os.Exit(1)
		}
		didSomething = true
	}

	if len(*setSuperfluousCertTag) > 0 {
		var tagContents []byte

		if strings.HasPrefix(*setSuperfluousCertTag, "0x") {
			tagContents, err = hex.DecodeString(*setSuperfluousCertTag)
			if err != nil {
				fmt.Fprintf(os.Stderr, "Failed to parse tag contents from command line: %s\n", err)
				os.Exit(1)
			}
		} else {
			tagContents = []byte(*setSuperfluousCertTag)
		}

		for len(tagContents) < *paddedLength {
			tagContents = append(tagContents, 0)
		}
		contents, err := bin.SetSuperfluousCertTag(tagContents)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while setting superfluous certificate tag: %s\n", err)
			os.Exit(1)
		}
		if err := ioutil.WriteFile(*outFilename, contents, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "Error while writing updated file: %s\n", err)
			os.Exit(1)
		}
		didSomething = true
	}

	if !didSomething {
		// By default, print basic information.
		appendedTag, ok := bin.AppendedTag()
		if !ok {
			fmt.Printf("No appended tag\n")
		} else {
			fmt.Printf("Appended tag included, %d bytes\n", len(appendedTag))
		}
	}
}
