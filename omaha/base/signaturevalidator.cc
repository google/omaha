// Copyright 2002-2009 Google Inc.
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


#include "omaha/base/signaturevalidator.h"

#include <atltime.h>
#include <softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#pragma warning(push)
// C4100: unreferenced formal parameter
// C4310: cast truncates constant value
// C4548: expression before comma has no effect
#pragma warning(disable : 4100 4310 4548)
#include "base/basictypes.h"
#pragma warning(pop)
#include "omaha/base/constants.h"
#include "omaha/base/error.h"

namespace omaha {

namespace {

const LPCTSTR kEmptyStr = _T("");
const DWORD kCertificateEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;

// Gets a handle to the certificate store and optionally the cryptographic
// message from the specified file.
// The caller is responsible for closing the store and message.
// message can be NULL if the handle is not needed.
HRESULT GetCertStoreFromFile(const wchar_t* signed_file,
                             HCERTSTORE* cert_store,
                             HCRYPTMSG* message) {
  if (!signed_file || !cert_store) {
    return E_INVALIDARG;
  }

  // Get message handle and store handle from the signed file.
  if (!::CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                          signed_file,
                          CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                          CERT_QUERY_FORMAT_FLAG_BINARY,
                          0,              // reserved, must be 0
                          NULL,           // pdwMsgAndCertEncodingType
                          NULL,           // pdwContentType
                          NULL,           // pdwFormatType
                          cert_store,
                          message,
                          NULL)) {        // ppvContext
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

// Gets the signer info from the crypt message.
// The caller is responsible for freeing the signer info using LocalFree.
HRESULT GetSignerInfo(HCRYPTMSG message, PCMSG_SIGNER_INFO* signer_info) {
  if (!signer_info) {
    return E_INVALIDARG;
  }
  *signer_info = NULL;

  DWORD info_size = 0;
  if (!::CryptMsgGetParam(message,
                          CMSG_SIGNER_INFO_PARAM,
                          0,
                          NULL,
                          &info_size)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  *signer_info = static_cast<PCMSG_SIGNER_INFO>(::LocalAlloc(LPTR, info_size));
  if (!*signer_info) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (!::CryptMsgGetParam(message,
                          CMSG_SIGNER_INFO_PARAM,
                          0,
                          *signer_info,
                          &info_size)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

// Gets the signer info for the time stamp signature in the specified signature.
HRESULT GetTimeStampSignerInfo(PCMSG_SIGNER_INFO signer_info,
                               PCMSG_SIGNER_INFO* countersigner_info) {
  if (!signer_info || !countersigner_info) {
    return E_INVALIDARG;
  }
  *countersigner_info = NULL;

  PCRYPT_ATTRIBUTE attr = NULL;

  // The countersigner info is contained in the unauthenticated attributes and
  // indicated by the szOID_RSA_counterSign OID.
  for (size_t i = 0; i < signer_info->UnauthAttrs.cAttr; ++i) {
    if (lstrcmpA(szOID_RSA_counterSign,
                 signer_info->UnauthAttrs.rgAttr[i].pszObjId) == 0) {
      attr = &signer_info->UnauthAttrs.rgAttr[i];
      break;
    }
  }

  if (!attr) {
    return E_FAIL;
  }

  // Decode and get CMSG_SIGNER_INFO structure for the timestamp certificate.
  DWORD data_size = 0;
  if (!::CryptDecodeObject(kCertificateEncoding,
                           PKCS7_SIGNER_INFO,
                           attr->rgValue[0].pbData,
                           attr->rgValue[0].cbData,
                           0,
                           NULL,
                           &data_size)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  *countersigner_info =
      static_cast<PCMSG_SIGNER_INFO>(::LocalAlloc(LPTR, data_size));
  if (!*countersigner_info) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (!::CryptDecodeObject(kCertificateEncoding,
                           PKCS7_SIGNER_INFO,
                           attr->rgValue[0].pbData,
                           attr->rgValue[0].cbData,
                           0,
                           *countersigner_info,
                           &data_size)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

// Gets the time of the date stamp for the specified signature.
// The time is in UTC.
HRESULT GetDateOfTimeStamp(PCMSG_SIGNER_INFO signer_info,
                           SYSTEMTIME* system_time) {
  if (!signer_info || !system_time) {
    return E_INVALIDARG;
  }

  PCRYPT_ATTRIBUTE attr = NULL;

  // The signing time is contained in the authenticated attributes and
  // indicated by the szOID_RSA_signingTime OID.
  for (size_t i = 0; i < signer_info->AuthAttrs.cAttr; ++i) {
    if (lstrcmpA(szOID_RSA_signingTime,
                 signer_info->AuthAttrs.rgAttr[i].pszObjId) == 0) {
      attr = &signer_info->AuthAttrs.rgAttr[i];
      break;
    }
  }

  if (!attr) {
    return E_FAIL;
  }

  FILETIME file_time = {0};

  // Decode and get FILETIME structure.
  DWORD data_size = sizeof(file_time);
  if (!::CryptDecodeObject(kCertificateEncoding,
                           szOID_RSA_signingTime,
                           attr->rgValue[0].pbData,
                           attr->rgValue[0].cbData,
                           0,
                           &file_time,
                           &data_size)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  if (!::FileTimeToSystemTime(&file_time, system_time)) {
    return HRESULT_FROM_WIN32(::GetLastError());
  }

  return S_OK;
}

}  // namespace

CertInfo::CertInfo(const CERT_CONTEXT* given_cert_context)
    : cert_context_(NULL) {
  if (given_cert_context) {
    // CertDuplicateCertificateContext just increases reference count of a given
    // CERT_CONTEXT.
    cert_context_ = CertDuplicateCertificateContext(given_cert_context);
    not_valid_before_ = cert_context_->pCertInfo->NotBefore;
    not_valid_after_ = cert_context_->pCertInfo->NotAfter;
    // Extract signed party details.
    ExtractIssuerInfo(cert_context_,
                      &issuing_company_name_,
                      &issuing_dept_name_,
                      &trust_authority_name_);
  }
}

CertInfo::~CertInfo() {
  // Decrement reference count, if needed.
  if (cert_context_)
    CertFreeCertificateContext(cert_context_);
}


bool CertInfo::IsValidNow() const {
  // we cannot directly get current time in FILETIME format.
  // so first get it in SYSTEMTIME format and convert it into FILETIME.
  SYSTEMTIME now;
  GetSystemTime(&now);
  FILETIME filetime_now;
  SystemTimeToFileTime(&now, &filetime_now);
  // CompareFileTime() is a windows function
  return ((CompareFileTime(&filetime_now, &not_valid_before_) > 0)
          && (CompareFileTime(&filetime_now, &not_valid_after_) < 0));
}


CString CertInfo::FileTimeToString(const FILETIME* ft) {
  if (ft == NULL)
    return _T("");
  SYSTEMTIME st;
  if (!FileTimeToSystemTime(ft, &st))
    return _T("");

  // Build a string showing the date and time.
  CString time_str;
  time_str.Format(_T("%02d/%02d/%d  %02d:%02d"), st.wDay, st.wMonth, st.wYear,
    st.wHour, st.wMinute);
  return time_str;
}


bool CertInfo::ExtractField(const CERT_CONTEXT* cert_context,
                            const char* field_name,
                            CString* field_value) {
  if ((!cert_context) || (!field_name) || (!field_value)) {
    return false;
  }

  field_value->Empty();

  DWORD num_chars = ::CertGetNameString(cert_context,
                                        CERT_NAME_ATTR_TYPE,
                                        0,
                                        const_cast<char*>(field_name),
                                        NULL,
                                        0);
  if (num_chars > 1) {
    num_chars = ::CertGetNameString(cert_context,
                                  CERT_NAME_ATTR_TYPE,
                                  0,
                                  const_cast<char*>(field_name),
                                  CStrBuf(*field_value, num_chars),
                                  num_chars);
  }

  return num_chars > 1 ? true : false;
}


bool CertInfo::ExtractIssuerInfo(const CERT_CONTEXT* cert_context,
                                 CString* orgn_name,
                                 CString* orgn_dept_name,
                                 CString* trust_authority) {
  // trust-authority is optional, so no check.
  if ((!orgn_name) || (!orgn_dept_name)) {
    return false;
  }

  ExtractField(cert_context, szOID_COMMON_NAME, orgn_name);
  ExtractField(cert_context, szOID_ORGANIZATIONAL_UNIT_NAME, orgn_dept_name);
  if (trust_authority != NULL) {
    ExtractField(cert_context, szOID_ORGANIZATION_NAME, trust_authority);
  }

  return true;
}


void CertList::FindFirstCert(CertInfo** result_cert_info,
                             const CString &company_name_to_match,
                             const CString &orgn_unit_to_match,
                             const CString &trust_authority_to_match,
                             bool allow_test_variant,
                             bool check_cert_is_valid_now) {
  if (!result_cert_info)
    return;
  (*result_cert_info) = NULL;

  for (CertInfoList::const_iterator cert_iter = cert_list_.begin();
       cert_iter != cert_list_.end();
       ++cert_iter) {
    // If any of the criteria does not match, continue on to next certificate
    if (!company_name_to_match.IsEmpty()) {
      const TCHAR* certificate_company_name =
          (*cert_iter)->issuing_company_name_;
      bool names_match = company_name_to_match == certificate_company_name;
      if (!names_match && allow_test_variant) {
        CString test_variant = company_name_to_match;
        test_variant += _T(" (TEST)");
        names_match = test_variant == certificate_company_name;
      }
      if (!names_match)
        continue;
    }
    if (!orgn_unit_to_match.IsEmpty() &&
        orgn_unit_to_match != (*cert_iter)->issuing_dept_name_)
      continue;
    if (!trust_authority_to_match.IsEmpty() &&
        trust_authority_to_match != (*cert_iter)->trust_authority_name_)
      continue;
    // All the criteria matched. But, add only if it is a valid certificate.
    if (!check_cert_is_valid_now || (*cert_iter)->IsValidNow()) {
      (*result_cert_info) = (*cert_iter);
      return;
    }
  }
}


void ExtractAllCertificatesFromSignature(const wchar_t* signed_file,
                                         CertList* cert_list) {
  if ((!signed_file) || (!cert_list))
    return;

  DWORD encoding_type = 0, content_type = 0, format_type = 0;
  // If successful, cert_store will be populated by
  // a store containing all the certificates related to the file signature.
  HCERTSTORE cert_store = NULL;
  BOOL succeeded = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                    signed_file,
                    CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                    CERT_QUERY_FORMAT_FLAG_ALL,
                    0,               // has to be zero as documentation says
                    &encoding_type,  // DWORD *pdwMsgAndCertEncodingType,
                    &content_type,   // DWORD *pdwContentType,
                    &format_type,    // DWORD *pdwFormatType,
                    &cert_store,     // HCERTSTORE *phCertStore,
                    NULL,            // HCRYPTMSG *phMsg,
                    NULL);           // const void** pvContext

  if (succeeded && (cert_store != NULL)) {
    PCCERT_CONTEXT   cert_context_ptr = NULL;
    while ((cert_context_ptr =
            CertEnumCertificatesInStore(cert_store, cert_context_ptr))
           != NULL) {
      CertInfo* cert_info = new CertInfo(cert_context_ptr);
      cert_list->AddCertificate(cert_info);
    }
  }
  if (cert_store) {
    CertCloseStore(cert_store, 0);
  }
  return;
}

// Only check the CN. The OU can change.
// TODO(omaha): A better way to implement the valid now check would be to add
// a parameter to VerifySignature that adds WTD_LIFETIME_SIGNING_FLAG.
bool VerifySigneeIsGoogleInternal(const wchar_t* signed_file,
                                  bool check_cert_is_valid_now) {
  CertList cert_list;
  ExtractAllCertificatesFromSignature(signed_file, &cert_list);
  if (cert_list.size() > 0) {
    CertInfo* required_cert = NULL;
    // now, see if one of the certificates in the signature belongs to Google.
    cert_list.FindFirstCert(&required_cert,
                            kCertificateSubjectName,
                            CString(),
                            CString(),
                            true,
                            check_cert_is_valid_now);
    if (required_cert != NULL) {
      return true;
    }
  }
  return false;
}

// Does not verify that the certificate is currently valid.
// VerifySignature verifies that the certificate was valid at signing time as
// part of the normal signature verification.
bool VerifySigneeIsGoogle(const wchar_t* signed_file) {
  return VerifySigneeIsGoogleInternal(signed_file, false);
}

HRESULT VerifySignature(const wchar_t* signed_file, bool allow_network_check) {
  // Don't pop up any windows
  HWND const kWindowMode = reinterpret_cast<HWND>(INVALID_HANDLE_VALUE);

  // Verify file & certificates
  GUID verification_type = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  // Info for the file we're going to verify
  WINTRUST_FILE_INFO file_info = {0};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = signed_file;

  // Info for request to WinVerifyTrust
  WINTRUST_DATA trust_data;
  ZeroMemory(&trust_data, sizeof(trust_data));
  trust_data.cbStruct = sizeof(trust_data);
  trust_data.dwUIChoice = WTD_UI_NONE;               // no graphics
  // No additional revocation checking -- note that this flag does not
  // cancel the flag we set in dwProvFlags; it specifies that no -additional-
  // checks are to be performed beyond the provider-specified ones.
  trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  trust_data.dwProvFlags = WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT;

  if (!allow_network_check)
    trust_data.dwProvFlags |= WTD_CACHE_ONLY_URL_RETRIEVAL;

  trust_data.dwUnionChoice = WTD_CHOICE_FILE;        // check a file
  trust_data.pFile = &file_info;                     // check this file

  // If the trust provider verifies that the subject is trusted for the
  // specified action, the return value is zero. No other value besides zero
  // should be considered a successful return.
  LONG result = ::WinVerifyTrust(kWindowMode, &verification_type, &trust_data);
  if (result != 0) {
    return FAILED(result) ? result : HRESULT_FROM_WIN32(result);
  }
  return S_OK;
}

// This method must not return until the end to avoid leaking memory.
// More info on Authenticode Signatures Time Stamping can be found at
// http://msdn2.microsoft.com/en-us/library/bb931395.aspx.
HRESULT GetSigningTime(const wchar_t* signed_file, SYSTEMTIME* signing_time) {
  if (!signed_file || !signing_time) {
    return E_INVALIDARG;
  }

  HCERTSTORE cert_store = NULL;
  HCRYPTMSG message = NULL;
  PCMSG_SIGNER_INFO signer_info = NULL;
  PCMSG_SIGNER_INFO countersigner_info = NULL;

  HRESULT hr = GetCertStoreFromFile(signed_file, &cert_store, &message);

  if (SUCCEEDED(hr)) {
    hr = GetSignerInfo(message, &signer_info);
  }

  if (SUCCEEDED(hr)) {
    hr = GetTimeStampSignerInfo(signer_info, &countersigner_info);
  }

  if (SUCCEEDED(hr)) {
    hr = GetDateOfTimeStamp(countersigner_info, signing_time);
  }

  if (cert_store) {
    ::CertCloseStore(cert_store, 0);
  }
  if (message) {
    ::CryptMsgClose(message);
  }
  ::LocalFree(signer_info);
  ::LocalFree(countersigner_info);

  return hr;
}

HRESULT VerifyFileSignedWithinDays(const wchar_t* signed_file, int days) {
  if (!signed_file || days <= 0) {
    return E_INVALIDARG;
  }

  SYSTEMTIME signing_time = {0};
  HRESULT hr = GetSigningTime(signed_file, &signing_time);
  if (FAILED(hr)) {
    return hr;
  }

  // Use the Win32 API instead of CTime::GetCurrentTime() because the latter
  // is broken in VS 2003 and 2005 and doesn't account for the timezone.
  SYSTEMTIME current_system_time = {0};
  ::GetSystemTime(&current_system_time);

  CTime signed_time(signing_time);
  CTime current_time(current_system_time);

  if (current_time <= signed_time) {
    return TRUST_E_TIME_STAMP;
  }

  CTimeSpan time_since_signed = current_time - signed_time;
  CTimeSpan max_duration(days, 0, 0, 0);

  if (max_duration < time_since_signed) {
    return TRUST_E_TIME_STAMP;
  }

  return S_OK;
}

}  // namespace omaha

