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
// Program certificate_tag manipulates "tags" in Authenticode-signed
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
//
// The tool supports PE32 exe files and MSI files.
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

func lengthAsn1(asn1 []byte) (asn1Length int, err error) {
	// Read the ASN.1 length of the object.
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
	return
}

func parseSignedData(asn1Data []byte) (*signedData, error) {
	var signedData signedData
	if _, err := asn1.Unmarshal(asn1Data, &signedData); err != nil {
		return nil, errors.New("authenticodetag: error while parsing SignedData structure: " + err.Error())
	}

	der, err := asn1.Marshal(signedData)
	if err != nil {
		return nil, errors.New("authenticodetag: error while marshaling SignedData structure: " + err.Error())
	}

	if !bytes.Equal(der, asn1Data) {
		return nil, errors.New("authenticodetag: ASN.1 parse/unparse test failed")
	}
	return &signedData, nil
}

func getSuperfluousCert(signedData *signedData) (cert *x509.Certificate, index int, err error) {
	n := len(signedData.PKCS7.Certs)
	if n == 0 {
		return nil, -1, nil
	}

	for index, certASN1 := range signedData.PKCS7.Certs {
		if cert, err = x509.ParseCertificate(certASN1.FullBytes); err != nil {
			return nil, -1, err
		}

		for _, ext := range cert.Extensions {
			if !ext.Critical && ext.Id.Equal(oidChromeTag) {
				return cert, index, nil
			}
		}
	}

	return nil, -1, nil
}

// SetSuperfluousCertTag modifies signedData, adding the superfluous cert with the given tag.
// It returns the asn1 serialization of the modified signedData.
func SetSuperfluousCertTag(signedData *signedData, tag []byte) ([]byte, error) {
	cert, index, err := getSuperfluousCert(signedData)
	if err != nil {
		return nil, fmt.Errorf("couldn't identity if any existing certificates are superfluous because of parse error: %w", err)
	}

	if cert != nil {
		pkcs7 := &signedData.PKCS7
		certs := pkcs7.Certs

		var newCerts []asn1.RawValue
		newCerts = append(newCerts, certs[:index]...)
		newCerts = append(newCerts, certs[index+1:]...)
		pkcs7.Certs = newCerts
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
		IsCA:                  true,
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
		IsCA:                  false,
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

	signedData.PKCS7.Certs = append(signedData.PKCS7.Certs, asn1.RawValue{
		FullBytes: derBytes,
	})

	asn1Bytes, err := asn1.Marshal(*signedData)
	if err != nil {
		return nil, err
	}
	return asn1Bytes, nil
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

	asn1Length, err := lengthAsn1(asn1)
	if err != nil {
		return
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

// Binary represents a taggable binary of any format.
type Binary interface {
	AppendedTag() (data []byte, ok bool)
	asn1Data() []byte
	buildBinary(asn1Data, tag []byte) ([]byte, error) // the tag argument is a legacy-style tag.
	RemoveAppendedTag() (contents []byte, err error)
	SetAppendedTag(tagContents []byte) (contents []byte, err error)
	getSuperfluousCert() (cert *x509.Certificate, index int, err error)
	SetSuperfluousCertTag(tag []byte) (contents []byte, err error)
	certificateOffset() int64
}

// PE32Binary represents a PE binary.
type PE32Binary struct {
	contents       []byte      // the full file
	attrCertOffset int         // the offset to the attribute certificates table
	certSizeOffset int         // the offset to the size of the attribute certificates table
	asn1Bytes      []byte      // the PKCS#7, SignedData in DER form.
	appendedTag    []byte      // the appended tag, if any.
	signedData     *signedData // the parsed SignedData structure.
}

// NewBinary returns a Binary that contains details of the PE32 or MSI binary given in |contents|.
// |contents| is modified if it is an MSI file.
func NewBinary(contents []byte) (Binary, error) {
	pe, peErr := NewPE32Binary(contents)
	if peErr == nil {
		return pe, peErr
	}
	msi, msiErr := NewMSIBinary(contents)
	if msiErr == nil {
		return msi, msiErr
	}
	return nil, errors.New("Could not parse input as either PE32 or MSI:\nPE32: " + peErr.Error() + "\nMSI: " + msiErr.Error())
}

// NewPE32Binary returns a Binary that contains details of the PE32 binary given in contents.
func NewPE32Binary(contents []byte) (*PE32Binary, error) {
	offset, size, certSizeOffset, err := getAttributeCertificates(contents)
	if err != nil {
		return nil, errors.New("authenticodetag: error parsing headers: " + err.Error())
	}

	attributeCertificates := contents[offset : offset+size]
	asn1Data, appendedTag, err := processAttributeCertificates(attributeCertificates)
	if err != nil {
		return nil, errors.New("authenticodetag: error parsing attribute certificate section: " + err.Error())
	}

	signedData, err := parseSignedData(asn1Data)
	if err != nil {
		return nil, err
	}

	return &PE32Binary{
		contents:       contents,
		attrCertOffset: offset,
		certSizeOffset: certSizeOffset,
		asn1Bytes:      asn1Data,
		appendedTag:    appendedTag,
		signedData:     signedData,
	}, nil
}

func (bin *PE32Binary) certificateOffset() int64 {
	return int64(bin.attrCertOffset)
}

func (bin *PE32Binary) asn1Data() []byte {
	return bin.asn1Bytes
}

// AppendedTag returns the appended tag, if any.
func (bin *PE32Binary) AppendedTag() (data []byte, ok bool) {
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
func (bin *PE32Binary) buildBinary(asn1Data, tag []byte) (contents []byte, err error) {
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
	return append(contents, tag...), nil
}

// RemoveAppendedTag removes a legacy-style tag from the end of the signedData container.
func (bin *PE32Binary) RemoveAppendedTag() (contents []byte, err error) {
	if _, ok := bin.AppendedTag(); !ok {
		return nil, errors.New("authenticodetag: no appended tag found")
	}

	return bin.buildBinary(bin.asn1Data(), nil)
}

// SetAppendedTag adds a legacy-style tag at the end of the signedData container.
func (bin *PE32Binary) SetAppendedTag(tagContents []byte) (contents []byte, err error) {
	return bin.buildBinary(bin.asn1Data(), tagContents)
}

// oidChromeTag is an OID that we use for the extension in the superfluous
// certificate. It's in the Google arc, but not officially assigned.
var oidChromeTag = asn1.ObjectIdentifier([]int{1, 3, 6, 1, 4, 1, 11129, 2, 1, 9999})

// oidChromeTagSearchBytes is used to find the final location of the tag buffer.
// This is followed by the 2-byte length of the buffer, and then the buffer itself.
// x060b - OID and length; 11 bytes of OID; x0482 - Octet string, 2-byte length prefix.
// (In practice our tags are 8206 bytes, so the size fits in two bytes.)
var oidChromeTagSearchBytes = []byte{0x06, 0x0b, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x01, 0xce, 0x0f, 0x04, 0x82}

func (bin *PE32Binary) getSuperfluousCert() (cert *x509.Certificate, index int, err error) {
	return getSuperfluousCert(bin.signedData)
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
// The (parsed) bin.signedData is modified; but bin.asn1Bytes, which contains
// the raw original bytes, is not.
func (bin *PE32Binary) SetSuperfluousCertTag(tag []byte) (contents []byte, err error) {
	asn1Bytes, err := SetSuperfluousCertTag(bin.signedData, tag)
	if err != nil {
		return nil, err
	}

	return bin.buildBinary(asn1Bytes, bin.appendedTag)
}

// Variables now defined as secT and offT were initially hardcoded as |int| for simplicity,
// but this produced errors when run on a Windows machine, which defaulted to a 32-bit arch.
// See b/172261939.

// secT is the type of a sector ID, or an index into the FAT (which describes what is in
// that sector), or a number of sectors.
type secT uint32

// offT is the type of an offset into the MSI file contents, or a number of bytes.
type offT uint64

// MSIBinary represents an MSI binary.
// |headerBytes| and |contents| are non-overlapping slices of the same backing array.
type MSIBinary struct {
	headerBytes     []byte       // the header (512 bytes).
	header          *MSIHeader   // the parsed msi header.
	sector          SectorFormat // sector parameters.
	contents        []byte       // the file content (no header), with SignedData removed.
	sigDirOffset    offT         // the offset of the signedData stream directory in |contents|.
	sigDirEntry     *MSIDirEntry // the parsed contents of the signedData stream directory.
	signedDataBytes []byte       // the PKCS#7, SignedData in asn1 DER form.
	signedData      *signedData  // the parsed SignedData structure.
	fatEntries      []secT       // a copy of the FAT entries in one list.
	difatEntries    []secT       // a copy of the DIFAT entries in one list.
	difatSectors    []secT       // a list of the dedicated DIFAT sectors (if any), for convenience.
}

// MSIHeader represents a parsed MSI header.
type MSIHeader struct {
	Magic                      [8]byte
	Clsid                      [16]byte
	MinorVersion               uint16
	DllVersion                 uint16
	ByteOrder                  uint16
	SectorShift                uint16
	MiniSectorShift            uint16
	Reserved                   [6]byte
	NumDirSectors              uint32
	NumFatSectors              uint32
	FirstDirSector             uint32
	TransactionSignatureNumber uint32
	MiniStreamCutoffSize       uint32
	FirstMiniFatSector         uint32
	NumMiniFatSectors          uint32
	FirstDifatSector           uint32
	NumDifatSectors            uint32
}

// MSIDirEntry represents a parsed MSI directory entry for a stream.
type MSIDirEntry struct {
	Name              [64]byte
	NumNameBytes      uint16
	ObjectType        uint8
	ColorFlag         uint8
	Left              uint32
	Right             uint32
	Child             uint32
	Clsid             [16]byte
	StateFlags        uint32
	CreateTime        uint64
	ModifyTime        uint64
	StreamFirstSector uint32
	StreamSize        uint64
}

// SectorFormat represents parameters of an MSI file sector.
type SectorFormat struct {
	Size offT // the size of a sector in bytes; 512 for dll v3 and 4096 for v4.
	Ints int  // the number of int32s in a sector.
}

const (
	numHeaderContentBytes = 76
	numHeaderTotalBytes   = 512
	numDifatHeaderEntries = 109
	numDirEntryBytes      = 128
	miniStreamSectorSize  = 64
	miniStreamCutoffSize  = 4096
	// Constants and names from https://docs.microsoft.com/en-us/openspecs/windows_protocols/ms-cfb/
	fatFreesect   = 0xFFFFFFFF // An unallocated sector (used in the FAT or DIFAT).
	fatEndofchain = 0xFFFFFFFE // End of a linked chain (in the FAT); or end of DIFAT sector chain.
	fatFatsect    = 0xFFFFFFFD // A FAT sector (used in the FAT).
	fatDifsect    = 0xFFFFFFFC // A DIFAT sector (used in the FAT).
	fatReserved   = 0xFFFFFFFB // Reserved value.
)

func newSectorFormat(sectorShift uint16) (format SectorFormat, err error) {
	sectorSize := offT(1) << sectorShift
	if sectorSize != 4096 && sectorSize != 512 {
		return format, fmt.Errorf("unexpected msi sector shift, wanted sector size 4096 or 512, got %d", sectorSize)
	}
	return SectorFormat{
		Size: sectorSize,
		Ints: int(sectorSize / 4),
	}, nil
}

// isLastInSector returns whether the index into difatEntries corresponds to the last entry in
// a sector.
//
// The last entry in each difat sector is a pointer to the next difat sector.
// (Or is an end-of-chain marker.)
// This does not apply to the last entry stored in the MSI header.
func (format SectorFormat) isLastInSector(index int) bool {
	return index > numDifatHeaderEntries && (index-numDifatHeaderEntries+1)%format.Ints == 0
}

// readStream reads the stream starting at the given start sector. The name is optional,
// it is only used for error reporting.
func (bin *MSIBinary) readStream(name string, start secT, streamSize offT, forceFAT, freeData bool) (stream []byte, err error) {
	var sectorSize offT
	var fatEntries []secT // May be FAT or mini FAT.
	var contents []byte   // May be file contents or mini stream.
	if forceFAT || streamSize >= miniStreamCutoffSize {
		fatEntries = bin.fatEntries
		contents = bin.contents
		sectorSize = bin.sector.Size
	} else {
		// Load the mini FAT.
		s, err := bin.readStream("mini FAT", secT(bin.header.FirstMiniFatSector), offT(bin.header.NumMiniFatSectors)*bin.sector.Size, true, false)
		if err != nil {
			return nil, err
		}
		for offset := 0; offset < len(s); offset += 4 {
			fatEntries = append(fatEntries, secT(binary.LittleEndian.Uint32(s[offset:])))
		}
		// Load the mini stream. (root directory's stream, root must be dir entry zero)
		root := &MSIDirEntry{}
		offset := offT(bin.header.FirstDirSector) * bin.sector.Size
		binary.Read(bytes.NewBuffer(bin.contents[offset:]), binary.LittleEndian, root)
		contents, err = bin.readStream("mini stream", secT(root.StreamFirstSector), offT(root.StreamSize), true, false)
		if err != nil {
			return nil, err
		}
		sectorSize = miniStreamSectorSize
	}
	sector := start
	size := streamSize
	for size > 0 {
		if sector == fatEndofchain || sector == fatFreesect {
			return nil, fmt.Errorf("msi readStream: ran out of sectors in copying stream %q", name)
		}
		n := size
		if n > sectorSize {
			n = sectorSize
		}
		offset := sectorSize * offT(sector)
		stream = append(stream, contents[offset:offset+n]...)
		size -= n

		// Zero out the existing stream bytes, if requested.
		// For example, new signedData will be written at the end of
		// the file (which may be where the existing stream is, but this works regardless).
		// The stream bytes could be left as unused junk, but unused bytes in an MSI file are
		// typically zeroed.

		// Set the data in the sector to zero.
		if freeData {
			for i := offT(0); i < n; i++ {
				contents[offset+i] = 0
			}
		}
		// Find the next sector, then free the FAT entry of the current sector.
		old := sector
		sector = fatEntries[sector]
		if freeData {
			fatEntries[old] = fatFreesect
		}
	}
	return stream, nil
}

// Parse-time functionality is broken out into populate*() methods for clarity.

// populateFatEntries does what it says and should only be called from NewMSIBinary().
func (bin *MSIBinary) populateFatEntries() error {
	var fatEntries []secT
	for i, sector := range bin.difatEntries {
		// The last entry in a difat sector is a chaining entry.
		isLastInSector := bin.sector.isLastInSector(i)
		if sector == fatFreesect || sector == fatEndofchain || isLastInSector {
			continue
		}
		offset := offT(sector) * bin.sector.Size
		for i := 0; i < bin.sector.Ints; i++ {
			fatEntries = append(fatEntries, secT(binary.LittleEndian.Uint32(bin.contents[offset+offT(i)*4:])))
		}
	}
	bin.fatEntries = fatEntries
	return nil
}

// populateDifatEntries does what it says and should only be called from NewMSIBinary().
func (bin *MSIBinary) populateDifatEntries() error {
	// Copy the difat entries and make a list of difat sectors (if any).
	// The first 109 difat entries must exist and are read from the MSI header, the rest come from
	// optional additional sectors.
	difatEntries := make([]secT, numDifatHeaderEntries, numDifatHeaderEntries+int(bin.header.NumDifatSectors)*bin.sector.Ints)
	for i := 0; i < numDifatHeaderEntries; i++ {
		difatEntries[i] = secT(binary.LittleEndian.Uint32(bin.headerBytes[numHeaderContentBytes+i*4:]))
	}

	// Code (here and elsewhere) that manages additional difat sectors probably won't run in prod,
	// but is implemented to avoid a hidden scaling limit.
	// (109 difat sector entries) x (1024 fat sector entries/difat sector) x (4096 bytes/ fat sector)
	// => files up to ~457 MB in size don't require additional difat sectors.
	var difatSectors []secT
	for i := 0; i < int(bin.header.NumDifatSectors); i++ {
		var sector secT
		if i == 0 {
			sector = secT(bin.header.FirstDifatSector)
		} else {
			sector = difatEntries[len(difatEntries)-1]
		}
		difatSectors = append(difatSectors, sector)
		start := offT(sector) * bin.sector.Size
		for j := 0; j < bin.sector.Ints; j++ {
			difatEntries = append(difatEntries, secT(binary.LittleEndian.Uint32(bin.contents[start+offT(j)*4:])))
		}
	}
	bin.difatEntries = difatEntries
	bin.difatSectors = difatSectors
	return nil
}

var (
	// UTF-16 for "\05DigitalSignature"
	signatureName = []byte{0x05, 0x00, 0x44, 0x00, 0x69, 0x00, 0x67, 0x00, 0x69, 0x00, 0x74, 0x00, 0x61, 0x00, 0x6c, 0x00, 0x53, 0x00, 0x69, 0x00, 0x67, 0x00, 0x6e, 0x00, 0x61, 0x00, 0x74, 0x00, 0x75, 0x00, 0x72, 0x00, 0x65, 0x00, 0x00, 0x00}
)

// signedDataDirFromSector returns the directory entry for the signedData stream,
// if it exists in the given sector.
func (bin *MSIBinary) signedDataDirFromSector(dirSector secT) (sigDirEntry *MSIDirEntry, offset offT, found bool) {
	sigDirEntry = &MSIDirEntry{}
	// Fixed 128 byte directory entry size.
	for i := offT(0); i < bin.sector.Size/numDirEntryBytes; i++ {
		offset = offT(dirSector)*bin.sector.Size + i*numDirEntryBytes
		binary.Read(bytes.NewBuffer(bin.contents[offset:]), binary.LittleEndian, sigDirEntry)
		if bytes.Equal(sigDirEntry.Name[:sigDirEntry.NumNameBytes], signatureName) {
			return sigDirEntry, offset, true
		}
	}
	return
}

// populateSignatureDirEntry does what it says and should only be called from NewMSIBinary().
func (bin *MSIBinary) populateSignatureDirEntry() error {
	dirSector := secT(bin.header.FirstDirSector)
	for {
		if sigDirEntry, sigDirOffset, found := bin.signedDataDirFromSector(dirSector); found {
			bin.sigDirEntry = sigDirEntry
			bin.sigDirOffset = sigDirOffset
			return nil
		}
		// Did not find the entry, go to the next directory sector.
		// This is run on MSIs that Google creates, so don't worry about a malicious infinite loop
		// in the entries.
		dirSector = bin.fatEntries[dirSector]
		if dirSector == fatEndofchain {
			return errors.New("did not find signature stream in MSI file")
		}
	}
}

// populateSignedData does what it says and should only be called from NewMSIBinary().
func (bin *MSIBinary) populateSignedData() (err error) {
	sector := secT(bin.sigDirEntry.StreamFirstSector)
	size := offT(bin.sigDirEntry.StreamSize)
	if bin.header.DllVersion == 3 {
		size = size & 0x7FFFFFFF
	}
	stream, err := bin.readStream("signedData", sector, size, false, true)
	if err != nil {
		return err
	}
	bin.signedDataBytes = stream
	bin.signedData, err = parseSignedData(bin.signedDataBytes)
	if err != nil {
		return err
	}
	return nil
}

var (
	msiHeaderSignature = []byte{0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1}
	msiHeaderClsid     = make([]byte, 16)
)

// NewMSIBinary returns a Binary that contains details of the MSI binary given in |contents|.
// |contents| is modified; the region occupied by the cert section is zeroed out.
func NewMSIBinary(fileContents []byte) (*MSIBinary, error) {
	// Parses the MSI header, the directory entry for the SignedData, and the SignedData itself.
	// Makes copies of the list of FAT and DIFAT entries, for easier manipulation.
	// Zeroes out the SignedData stream in |contents|, as it may move.
	// When writing, the elements: (header, dir entry, SignedData, FAT and DIFAT entries)
	// are considered dirty (modified), and written back into fileContents.
	if len(fileContents) < numHeaderTotalBytes {
		return nil, fmt.Errorf("msi file is too short to contain header, want >= %d bytes got %d bytes", numHeaderTotalBytes, len(fileContents))
	}

	// Parse the header.
	headerBytes := fileContents[:numHeaderTotalBytes]
	var header MSIHeader
	binary.Read(bytes.NewBuffer(headerBytes[:numHeaderContentBytes]), binary.LittleEndian, &header)
	if !bytes.Equal(header.Magic[:], msiHeaderSignature) || !bytes.Equal(header.Clsid[:], msiHeaderClsid) {
		return nil, fmt.Errorf("msi file is not an msi file: either the header signature is missing or the clsid is not zero as required")
	}

	format, err := newSectorFormat(header.SectorShift)
	if err != nil {
		return nil, err
	}
	if offT(len(fileContents)) < format.Size {
		return nil, fmt.Errorf("msi file is too short to contain a full header sector, want >= %d bytes got %d bytes", format.Size, len(fileContents))
	}
	contents := fileContents[format.Size:]

	bin := &MSIBinary{
		headerBytes: headerBytes,
		header:      &header,
		sector:      format,
		contents:    contents,
	}

	// The difat entries must be populated before the fat entries.
	if err := bin.populateDifatEntries(); err != nil {
		return nil, err
	}
	if err := bin.populateFatEntries(); err != nil {
		return nil, err
	}
	// The signature dir entry must be populated before the signed data.
	if err := bin.populateSignatureDirEntry(); err != nil {
		return nil, err
	}
	if err := bin.populateSignedData(); err != nil {
		return nil, err
	}
	return bin, nil
}

// firstFreeFatEntry returns the index of the first free entry at the end of a slice of fat entries.
// It returns one past the end of list if there are no free entries at the end.
func firstFreeFatEntry(entries []secT) secT {
	firstFreeIndex := secT(len(entries))
	for entries[firstFreeIndex-1] == fatFreesect {
		firstFreeIndex--
	}
	return firstFreeIndex
}

func (bin *MSIBinary) firstFreeFatEntry() secT {
	return firstFreeFatEntry(bin.fatEntries)
}

// ensureFreeFatEntries ensures there are at least n free entries at the end of the FAT list,
// and returns the first free entry.
//
// The bin.fatEntry slice may be modified, any local references to the slice are invalidated.
// bin.fatEntry elements may be assigned, so any local references to entries (such as the
// first free index) are also invalidated.
// The function is re-entrant.
func (bin *MSIBinary) ensureFreeFatEntries(n secT) secT {
	sizeFat := secT(len(bin.fatEntries))
	firstFreeIndex := bin.firstFreeFatEntry() // Is past end of slice if there are no free entries.
	if sizeFat-firstFreeIndex >= n {
		// Nothing to do, there were already enough free sectors.
		return firstFreeIndex
	}
	// Append another FAT sector.
	for i := 0; i < bin.sector.Ints; i++ {
		bin.fatEntries = append(bin.fatEntries, fatFreesect)
	}
	// firstFreeIndex is free; assign it to the created FAT sector.
	// (Do not change the order of these calls; assignDifatEntry() could invalidate firstFreeIndex.)
	bin.fatEntries[firstFreeIndex] = fatFatsect
	bin.assignDifatEntry(firstFreeIndex)

	// Update the MSI header.
	bin.header.NumFatSectors++

	// If n is large enough, it's possible adding an additional sector was insufficient.
	// This won't happen for our use case; but the call to verify or fix it is cheap.
	bin.ensureFreeFatEntries(n)

	return bin.firstFreeFatEntry()
}

// assignDifatEntries assigns an entry (the sector# of a FAT sector) to the end of the difat list.
//
// The bin.fatEntry slice may be modified, any local references to the slice are invalidated.
// bin.fatEntry elements may be assigned, so any local references to entries (such as the
// first free index) are also invalidated.
func (bin *MSIBinary) assignDifatEntry(fatSector secT) {
	bin.ensureFreeDifatEntry()
	// Find first free entry at end of list.
	i := len(bin.difatEntries) - 1

	// If there are sectors, i could be pointing to a fatEndofchain marker, but in that case
	// it is guaranteed (by ensureFreeDifatEntry()) that the prior element is a free sector,
	// and the following loop works.

	// As long as the prior element is a free sector, decrement i.
	// If the prior element is at the end of a difat sector, skip over it.
	for bin.difatEntries[i-1] == fatFreesect ||
		(bin.sector.isLastInSector(i-1) && bin.difatEntries[i-2] == fatFreesect) {
		i--
	}
	bin.difatEntries[i] = fatSector
}

// ensureFreeDifatEntry ensures there is at least one free entry at the end of the DIFAT list.
//
// The bin.fatEntry slice may be modified, any local references to the slice are invalidated.
// bin.fatEntry elements may be assigned, so any local references to entries (such as the
// first free index) are also invalidated.
func (bin *MSIBinary) ensureFreeDifatEntry() {
	// By construction, difatEntries is at least numDifatHeaderEntries (109) long.
	i := len(bin.difatEntries) - 1
	if bin.difatEntries[i] == fatEndofchain {
		i--
	}
	if bin.difatEntries[i] == fatFreesect {
		return // There is at least one free entry.
	}

	oldDifatTail := len(bin.difatEntries) - 1

	// Allocate another sector of difat entries.
	for i := 0; i < bin.sector.Ints; i++ {
		bin.difatEntries = append(bin.difatEntries, fatFreesect)
	}
	bin.difatEntries[len(bin.difatEntries)-1] = fatEndofchain

	// Assign the new difat sector in the FAT.
	sector := bin.ensureFreeFatEntries(1)
	bin.fatEntries[sector] = fatDifsect

	// Assign the "next sector" pointer in the previous sector or header.
	if bin.header.NumDifatSectors == 0 {
		bin.header.FirstDifatSector = uint32(sector)
	} else {
		bin.difatEntries[oldDifatTail] = sector
	}
	bin.header.NumDifatSectors++
	bin.difatSectors = append(bin.difatSectors, sector) // A helper slice.
}

// AppendedTag is not supported for MSI files.
func (bin *MSIBinary) AppendedTag() (data []byte, ok bool) {
	return nil, false
}

func (bin *MSIBinary) asn1Data() []byte {
	return bin.signedDataBytes
}

// buildBinary builds an MSI binary based on bin but with the given SignedData and appended tag.
// Appended tag is not supported for MSI.
// buildBinary may add free sectors to |bin|, but otherwise does not modify it.
func (bin *MSIBinary) buildBinary(signedData, tag []byte) ([]byte, error) {
	if len(tag) > 0 {
		return nil, errors.New("appended tags not supported in MSI files")
	}
	// Writing to the mini FAT is not supported.
	if len(signedData) < miniStreamCutoffSize {
		return nil, fmt.Errorf("writing SignedData less than %d bytes is not supported", len(signedData))
	}
	// Ensure enough free FAT entries for the signedData.
	numSignedDataSectors := secT((offT(len(signedData))-1)/bin.sector.Size) + 1
	firstSignedDataSector := bin.ensureFreeFatEntries(numSignedDataSectors)

	// Allocate sectors for the signedData, in a copy of the FAT entries.
	newFatEntries := make([]secT, len(bin.fatEntries))
	copy(newFatEntries, bin.fatEntries)
	for i := secT(0); i < numSignedDataSectors-1; i++ {
		newFatEntries[firstSignedDataSector+i] = firstSignedDataSector + i + 1
	}
	newFatEntries[firstSignedDataSector+numSignedDataSectors-1] = fatEndofchain

	// Update the signedData stream's directory entry (location and size), in copy of dir entry.
	newSigDirEntry := *bin.sigDirEntry
	newSigDirEntry.StreamFirstSector = uint32(firstSignedDataSector)
	newSigDirEntry.StreamSize = uint64(len(signedData))

	// Write out the...
	// ...header,
	headerSectorBytes := make([]byte, bin.sector.Size)
	out := new(bytes.Buffer)
	binary.Write(out, binary.LittleEndian, bin.header)
	copy(headerSectorBytes[:], out.Bytes())
	for i := 0; i < numDifatHeaderEntries; i++ {
		binary.LittleEndian.PutUint32(headerSectorBytes[numHeaderContentBytes+i*4:], uint32(bin.difatEntries[i]))
	}
	// ...content,
	// Make a copy of the content bytes, since new data will be overlaid on it.
	// The new content slice should accommodate the new content size.
	firstFreeSector := firstFreeFatEntry(newFatEntries)
	contents := make([]byte, bin.sector.Size*offT(firstFreeSector)) // zero-based sector counting.
	copy(contents, bin.contents)

	// ...signedData directory entry (from local modified copy),
	out.Reset()
	binary.Write(out, binary.LittleEndian, &newSigDirEntry)
	copy(contents[bin.sigDirOffset:], out.Bytes())

	// ...difat entries,
	// They might have been modified, although usually not.
	for i, sector := range bin.difatSectors {
		index := numDifatHeaderEntries + i*bin.sector.Ints
		offset := offT(sector) * bin.sector.Size
		for j := 0; j < bin.sector.Ints; j++ {
			binary.LittleEndian.PutUint32(contents[offset+offT(j)*4:], uint32(bin.difatEntries[index+j]))
		}
	}
	// ...fat entries (from local modified copy),
	index := 0
	for i, sector := range bin.difatEntries {
		// The last entry in each difat sector is a pointer to the next difat sector.
		// This does not apply to the header entries.
		isLastInSector := bin.sector.isLastInSector(i)
		if sector != fatFreesect && sector != fatEndofchain && !isLastInSector {
			offset := offT(sector) * bin.sector.Size
			for i := 0; i < bin.sector.Ints; i++ {
				binary.LittleEndian.PutUint32(contents[offset+offT(i)*4:], uint32(newFatEntries[index+i]))
			}
			index += bin.sector.Ints
		}
	}
	// ...signedData
	// |contents| was zero-initialized, so no need to add padding to end of sector.
	// The sectors allocated for signedData were guaranteed contiguous.
	copy(contents[offT(firstSignedDataSector)*bin.sector.Size:], signedData)

	return append(headerSectorBytes, contents...), nil
}

// RemoveAppendedTag is not supported for MSI files.
func (bin *MSIBinary) RemoveAppendedTag() (contents []byte, err error) {
	return nil, errors.New("authenticodetag: appended tags not supported in MSI files")
}

// SetAppendedTag is not supported for MSI files.
func (bin *MSIBinary) SetAppendedTag(tagContents []byte) (contents []byte, err error) {
	return nil, errors.New("authenticodetag: appended tags not supported in MSI files")
}

func (bin *MSIBinary) getSuperfluousCert() (cert *x509.Certificate, index int, err error) {
	return getSuperfluousCert(bin.signedData)
}

// SetSuperfluousCertTag returns an MSI binary based on bin, but where the
// superfluous certificate contains the given tag data.
// The (parsed) bin.signedData is modified; but bin.signedDataBytes, which contains
// the raw original bytes, is not.
func (bin *MSIBinary) SetSuperfluousCertTag(tag []byte) (contents []byte, err error) {
	asn1Bytes, err := SetSuperfluousCertTag(bin.signedData, tag)
	if err != nil {
		return nil, err
	}

	return bin.buildBinary(asn1Bytes, nil)
}

func (bin *MSIBinary) certificateOffset() int64 {
	// The signedData will be written at the first free sector at the end of file.
	return int64(offT(bin.firstFreeFatEntry()) * bin.sector.Size)
}

// findTag returns the offset of the superfluous-cert tag in |contents|, or (-1, 0) if not found.
// The caller should restrict the search to the certificate section of the contents, if known.
func findTag(contents []byte, start int64) (offset, length int64, err error) {
	// An MSI can have a tagged Omaha inside of it, but that is the wrong tag -- it should be the
	// one on the outermost container, or none.
	contents = contents[start:]
	lenContents := int64(len(contents))

	// Find the oidChromeTag in the contents. The search string includes everything up to the
	// asn1 length specification right before the Omaha2.0 marker.
	offset = int64(bytes.LastIndex(contents, oidChromeTagSearchBytes))
	if offset < 0 { // Not an error, simply not found.
		return -1, 0, nil
	}
	offset += int64(len(oidChromeTagSearchBytes))
	if offset > lenContents-2 {
		return -1, 0, fmt.Errorf("failed in findTag, want offset plus tag size bytes to fit in file size %d, but offset %d is too large", lenContents, offset)
	}
	length = int64(binary.BigEndian.Uint16(contents[offset:]))
	offset += 2
	if offset+length > lenContents {
		return -1, 0, fmt.Errorf("failed in findTag, want tag buffer to fit in file size %d, but offset (%d) plus length (%d) is %d", lenContents, offset, length, offset+length)
	}
	return start + offset, length, nil
}

var (
	dumpAppendedTag       *bool   = flag.Bool("dump-appended-tag", false, "If set, any appended tag is dumped to stdout.")
	removeAppendedTag     *bool   = flag.Bool("remove-appended-tag", false, "If set, any appended tag is removed and the binary rewritten.")
	loadAppendedTag       *string = flag.String("load-appended-tag", "", "If set, this flag contains a filename from which the contents of the appended tag will be saved")
	setSuperfluousCertTag *string = flag.String("set-superfluous-cert-tag", "", "If set, this flag contains a string and a superfluous certificate tag with that value will be set and the binary rewritten. If the string begins with '0x' then it will be interpreted as hex")
	paddedLength          *int    = flag.Int("padded-length", 0, "A superfluous cert tag will be padded with zeros to at least this number of bytes")
	savePKCS7             *string = flag.String("save-pkcs7", "", "If set to a filename, the PKCS7 data from the original binary will be written to that file.")
	outFilename           *string = flag.String("out", "", "If set, the updated binary is written to this file. Otherwise the binary is updated in place.")
	printTagDetails       *bool   = flag.Bool("print-tag-details", false, "IF set, print to stdout the location and size of the superfluous cert's Gact2.0 marker plus buffer.")
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

	var finalContents []byte
	didSomething := false

	if len(*savePKCS7) > 0 {
		if err := ioutil.WriteFile(*savePKCS7, bin.asn1Data(), 0644); err != nil {
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
		finalContents = contents
		didSomething = true
	}

	if len(*loadAppendedTag) > 0 {
		tagContents, err := ioutil.ReadFile(*loadAppendedTag)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while reading file: %s\n", err)
			os.Exit(1)
		}
		contents, err := bin.SetAppendedTag(tagContents)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while setting appended tag: %s\n", err)
			os.Exit(1)
		}
		if err := ioutil.WriteFile(*outFilename, contents, 0644); err != nil {
			fmt.Fprintf(os.Stderr, "Error while writing updated file: %s\n", err)
			os.Exit(1)
		}
		finalContents = contents
		didSomething = true
	}

	if len(*setSuperfluousCertTag) > 0 {
		var tagContents []byte

		if strings.HasPrefix(*setSuperfluousCertTag, "0x") {
			tagContents, err = hex.DecodeString((*setSuperfluousCertTag)[2:])
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
		// print-tag-details only works if the length requires 2 bytes to specify. (The length bytes
		// length is part of the search string.)
		// Lorry only tags properly (aside from tag-in-zip) if the length is 8206 or more. b/173139534
		// Omaha may or may not have a practical buffer size limit; 8206 is known to work.
		if len(tagContents) < 0x100 || len(tagContents) > 0xffff {
			fmt.Fprintf(os.Stderr, "Want final tag length in range [256, 65535], got %d\n", len(tagContents))
			os.Exit(1)
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
		finalContents = contents
		didSomething = true
	}

	if *printTagDetails {
		if finalContents == nil {
			// Re-read the input, as NewBinary() may modify it.
			finalContents, err = ioutil.ReadFile(inFilename)
			if err != nil {
				panic(err)
			}
		}
		offset, length, err := findTag(finalContents, bin.certificateOffset())
		if err != nil {
			fmt.Fprintf(os.Stderr, "Error while searching for tag in file bytes: %s\n", err)
			os.Exit(1)
		}
		fmt.Printf("Omaha Tag offset, length: (%d, %d)\n", offset, length)
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
