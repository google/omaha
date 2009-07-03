// Copyright 2003-2009 Google Inc.
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
// Error codes and HRESULTS
//
// TODO(omaha): reduce the number of custom error codes below by searching and
// seeing what is handled and what is not.
//
// TODO(omaha): rename CI and GOOPDATE to OMAHA.

#ifndef OMAHA_COMMON_ERROR_H_
#define OMAHA_COMMON_ERROR_H_

#include <windows.h>

namespace omaha {

// Returns the last error as an HRESULT or E_FAIL if last error is NO_ERROR.
// This is not a drop in replacement for the HRESULT_FROM_WIN32 macro.
// The macro maps a NO_ERROR to S_OK, whereas the function below maps a
// NO_ERROR to E_FAIL. Also, the macro is evaluating arguments multiple times.
HRESULT HRESULTFromLastError();

// Returns the http status_code as an HRESULT.
HRESULT HRESULTFromHttpStatusCode(int status_code);

// HRESULTs
//
// Top bit indicates success (0) or failure (1)
// 16 bits available for 'code' field at end

#define kFacilityCi 67

#define CI_E_NOT_ENOUGH_DISK_SPACE                \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0003)

// CI_E_INVALID_MANIFEST is returned when the manifest file is missing a
// required field or has some other kind of semantic error
#define CI_E_INVALID_MANIFEST                     \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0012)

// CI_E_XML_LOAD_ERROR is returned when MSXML can't load the document into DOM.
// It may be because the document is not well formed (tags, namespaces, etc)
#define CI_E_XML_LOAD_ERROR                       \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0013)

// CI_E_INVALID_ARG is returned when invalid command line arugment is specified
#define CI_E_INVALID_ARG                          \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x001B)

// CI_E_PROXY_AUTH_REQUIRED is returned when proxy authentication is required
#define CI_E_PROXY_AUTH_REQUIRED                  \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0028)

// CI_E_INVALID_PROXY_AUTH_SCHEME is returned when the proxy authentication
// scheme is unknown or not supported
#define CI_E_INVALID_PROXY_AUTH_SCHEME            \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0029)

// CI_E_BITS_DISABLED is returned by the download manager when the BITS service
// is not enabled.
#define CI_E_BITS_DISABLED                        \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0030)

// CI_E_HTTPS_CERT_FAILURE is returned when the https connection fails.
// One cause of this is when the system clock is off by a significant
// amount which makes the server certificate appear invalid.
#define CI_E_HTTPS_CERT_FAILURE                   \
    MAKE_HRESULT(SEVERITY_ERROR, kFacilityCi, 0x0035)

// Return values from Process::WaitUntilDeadOrInterrupt
// TODO(omaha) Move this constants to the Process class. They do not look like
// error codes.
#define CI_S_PROCESSWAIT_DEAD                     \
    MAKE_HRESULT(SEVERITY_SUCCESS, kFacilityCi, 0x0100)
#define CI_S_PROCESSWAIT_TIMEOUT                  \
    MAKE_HRESULT(SEVERITY_SUCCESS, kFacilityCi, 0x0101)
#define CI_S_PROCESSWAIT_MESSAGE                  \
    MAKE_HRESULT(SEVERITY_SUCCESS, kFacilityCi, 0x0102)

// Signatures error codes.
#define SIGS_E_INVALID_PFX_CERTIFICATE            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0200)
#define SIGS_E_INVALID_DER_CERTIFICATE            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0201)
#define SIGS_E_INVALID_PASSWORD                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0202)
#define SIGS_E_INVALID_KEY_TYPE                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0203)
#define SIGS_E_INVALID_SIGNATURE                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x0204)

// Goopdate XML parser error codes.
#define GOOPDATEXML_E_STRTOUINT                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x300)
#define GOOPDATEXML_E_RESPONSESNODE               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x301)
#define GOOPDATEXML_E_XMLVERSION                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x302)
#define GOOPDATEXML_E_NEEDSADMIN                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x303)
#define GOOPDATEXML_E_UNEXPECTED_URI              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x304)
#define GOOPDATEXML_E_TOO_MANY_APPS               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x305)

// Goopdate job queue error codes.
// Errors 0x401 - 0x407 are legacy codes and should not be reused.

// Download Manager custom error codes.
// Obsolete: GOOPDATEDOWNLOAD_E_FILE_ALREADY_DOWNLOADED - 0x500.
// Obsolete: GOOPDATEDOWNLOAD_E_FAILED_GET_ERROR - 0x501.
#define GOOPDATEDOWNLOAD_E_INVALID_PATH             \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x502)
#define GOOPDATEDOWNLOAD_E_CRACKURL_FAILED          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x503)
#define GOOPDATEDOWNLOAD_E_FILE_NAME_EMPTY          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x504)
#define GOOPDATEDOWNLOAD_E_DEST_FILE_PATH_EMPTY     \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x505)
#define GOOPDATEDOWNLOAD_E_STORAGE_DIR_NOT_EXIST    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x506)
#define GOOPDATEDOWNLOAD_E_FILE_SIZE_ZERO           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x507)
#define GOOPDATEDOWNLOAD_E_FILE_SIZE_SMALLER        \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x508)
#define GOOPDATEDOWNLOAD_E_FILE_SIZE_LARGER         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x509)
#define GOOPDATEDOWNLOAD_E_UNIQUE_FILE_PATH_EMPTY   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x50A)
#define GOOPDATEDOWNLOAD_E_DEST_PATH_EMPTY          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x50B)
#define GOOPDATEDOWNLOAD_E_DEST_FILENAME_EMPTY      \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x50C)
#define GOOPDATEDOWNLOAD_E_FAILED_MOVE              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x5FF)

// Goopdate custom error codes.
// Obsolete this error when legacy support is removed.
#define GOOPDATE_E_NON_ADMINS_CANNOT_INSTALL_ADMIN  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x600)
// Obsolete: GOOPDATE_S_WORKER_ALREADY_RUNNING - 0x601.
// Obsolete: GOOPDATE_E_INVALID_ARG_COMBINATION - 0x602.
#define GOOPDATE_E_WORKER_ALREADY_RUNNING           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x603)
#define GOOPDATE_E_APP_BEING_INSTALLED              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x604)
#define GOOPDATE_E_RESOURCE_DLL_PATH_EMPTY          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x605)

// Setup and metainstaller custom error codes.
#define GOOPDATE_E_NONADMIN_INSTALL_ADMIN_APP       \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x700)
// Obsolete: GOOPDATE_E_MANIFEST_FILENAME_EMPTY - 0x701.
// Obsolete: GOOPDATE_E_MANIFEST_FILE_DOES_NOT_EXIST - 0x702.
// This error appears in the metainstaller.
#define GOOPDATE_E_RUNNING_INFERIOR_WINDOWS         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x703)
#define GOOPDATE_E_RUNNING_INFERIOR_MSXML           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x704)
// Obsolete: GOOPDATE_E_ELEVATION_FAILED - 0x705
#define GOOPDATE_E_FAILED_TO_GET_LOCK               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x706)
#define GOOPDATE_E_INSTANCES_RUNNING                \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x707)
#define GOOPDATE_E_HANDOFF_FAILED                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x708)
#define GOOPDATE_E_PATH_APPEND_FAILED               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x709)
#define GOOPDATE_E_SERVICE_NAME_EMPTY               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70a)
#define GOOPDATE_E_SETUP_LOCK_INIT_FAILED           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70b)
#define GOOPDATE_E_ACCESSDENIED_COPYING_CORE_FILES  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70c)
#define GOOPDATE_E_ACCESSDENIED_COPYING_SHELL       \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70d)
#define GOOPDATE_E_ACCESSDENIED_STOP_PROCESSES      \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70e)
#define GOOPDATE_E_ACCESSDENIED_SETUP_REG_ACCESS    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x70f)
// User may have accidentally run the metainstaller twice.
#define GOOPDATE_E_FAILED_TO_GET_LOCK_MATCHING_INSTALL_PROCESS_RUNNING  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x710)
#define GOOPDATE_E_FAILED_TO_GET_LOCK_NONMATCHING_INSTALL_PROCESS_RUNNING \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x711)
#define GOOPDATE_E_FAILED_TO_GET_LOCK_UPDATE_PROCESS_RUNNING  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x712)
#define GOOPDATE_E_VERIFY_SIGNEE_IS_GOOGLE_FAILED   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x713)
#define GOOPDATE_E_VERIFY_SIGNEE_IS_GOOGLE_FAILED_TIMESTAMP_CHECK \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x714)
#define GOOPDATE_E_INSTALL_ELEVATED_PROCESS_NEEDS_ELEVATION \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x715)
#define GOOPDATE_E_ELEVATION_FAILED_ADMIN           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x716)
#define GOOPDATE_E_ELEVATION_FAILED_NON_ADMIN       \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x717)
#define GOOPDATE_E_CANT_UNINSTALL                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x718)
#define GOOPDATE_E_SILENT_INSTALL_NEEDS_ELEVATION   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x719)
#define GOOPDATE_E_OEM_NOT_MACHINE_AND_PRIVILEGED_AND_AUDIT_MODE  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x71a)
#define GOOPDATE_E_OEM_WITH_ONLINE_INSTALLER        \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x71b)
#define GOOPDATE_E_NON_OEM_INSTALL_IN_AUDIT_MODE    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x71c)
#define GOOPDATE_E_OEM_INSTALL_SUCCEEDED_BUT_NOT_IN_OEM_INSTALLING_MODE \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x71d)
#define GOOPDATE_E_EULA_REQURED_WITH_ONLINE_INSTALLER \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x71e)

// Metainstaller custom error codes.
#define GOOPDATE_E_UNTAGGED_METAINSTALLER           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x750)


// Obsolete: GOOPDATE_E_NO_SERVER_RESPONSE - 0x800.
#define GOOPDATE_E_NO_NETWORK \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x801)
// Obsolete: GOOPDATE_E_UPDATE_CHECK_FAILED - 0x802.
// Obsolete: GOOPDATE_COULD_NOT_GET_IGOOGLEUPDATE - 0x803.
// Obsolete: GOOPDATE_E_BAD_SERVER_RESPONSE - 0x804.
#define GOOPDATE_E_UNKNOWN_SERVER_RESPONSE          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x805)
#define GOOPDATE_E_RESTRICTED_SERVER_RESPONSE       \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x806)
// Obsolete: GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED - 0x807.
// Obsolete: GOOPDATE_E_ABANDON_UPDATE            - 0x808.
#define GOOPDATE_E_NO_UPDATE_RESPONSE               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x809)
#define GOOPDATE_E_BUNDLE_ERROR                     \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80A)
#define GOOPDATE_E_APP_UNINSTALLED                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80B)
#define GOOPDATE_E_CALL_INPROGRESS                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80C)
#define GOOPDATE_E_UNKNOWN_APP_SERVER_RESPONSE      \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80D)
#define GOOPDATE_E_INTERNAL_ERROR_SERVER_RESPONSE   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80E)
#define GOOPDATE_E_NO_SERVER_RESPONSE               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x80F)
#define GOOPDATE_E_WORKER_CANCELLED                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x810)
#define GOOPDATE_E_OS_NOT_SUPPORTED                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x811)
#define GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x812)
#define GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x813)
#define GOOPDATE_E_APP_NOT_REGISTERED               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x814)
#define GOOPDATE_E_CANNOT_USE_NETWORK               \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x815)
#define GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x816)

//
// Network stack error codes.
//

// The CUP response is missing the ETag header containing the server proof.
#define OMAHA_NET_E_CUP_NO_SERVER_PROOF            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x880)

// The CUP response is not trusted.
#define OMAHA_NET_E_CUP_NOT_TRUSTED                \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x881)
#define OMAHA_NET_E_REQUEST_CANCELLED              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x882)

// CUP could not instantiate an http client.
#define OMAHA_NET_E_CUP_NO_HTTP_CLIENT              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x883)

// CUP could not generate random bytes.
#define OMAHA_NET_E_CUP_NO_ENTROPY                  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x884)

// Non-CUP network errors.
#define OMAHA_NET_E_WINHTTP_NOT_AVAILABLE           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x890)

// Install Manager custom error codes.
#define GOOPDATEINSTALL_E_FILENAME_INVALID         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x900)
#define GOOPDATEINSTALL_E_INSTALLER_FAILED_START   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x901)
#define GOOPDATEINSTALL_E_INSTALLER_FAILED         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x902)
#define GOOPDATEINSTALL_E_INSTALLER_INTERNAL_ERROR \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x903)
#define GOOPDATEINSTALL_E_INSTALLER_TIMED_OUT      \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x904)
#define GOOPDATEINSTALL_E_INSTALLER_DID_NOT_WRITE_CLIENT_KEY  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x905)
#define GOOPDATEINSTALL_E_CANNOT_GET_INSTALLER_LOCK           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x906)
#define GOOPDATEINSTALL_E_MSI_INSTALL_ALREADY_RUNNING         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x907)
#define GOOPDATEINSTALL_E_INSTALLER_DID_NOT_CHANGE_VERSION    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x908)
#define GOOPDATE_E_INVALID_INSTALL_DATA_INDEX                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x909)
#define GOOPDATE_E_INVALID_INSTALLER_DATA_IN_APPARGS          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x90A)

// GoopdateUtils custom error codes.
#define GOOPDATEUTILS_E_BROWSERTYPE                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA00)

// GoogleUpdate.exe shell custom error codes.
#define GOOGLEUPDATE_E_DLL_NOT_FOUND           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xB00)
#define GOOGLEUPDATE_E_VERIFY_SIGNEE_IS_GOOGLE_FAILED \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xB01)

// Command line parse custom error codes.
#define GOOGLEUPDATE_COMMANDLINE_E_NO_SCENARIO_HANDLER \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xC00)
#define GOOGLEUPDATE_COMMANDLINE_E_NO_SCENARIO_HANDLER_MATCHED \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xC01)

// OneClick custom error codes
#define GOOPDATE_E_ONECLICK_HOSTCHECK_FAILED        \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD01)
#define GOOPDATE_E_ONECLICK_NO_LANGUAGE_RESOURCE    \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD02)

// Usage stats / metrics error codes
#define GOOPDATE_E_METRICS_LOCK_INIT_FAILED         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD80)
#define GOOPDATE_E_METRICS_AGGREGATE_FAILED         \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD81)

// Core error codes
#define GOOPDATE_E_CORE_INTERNAL_ERROR              \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xE00)
#define GOOPDATE_E_CORE_MISSING_CMD                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xE01)

// UI & Observer error codes
#define GOOPDATE_E_UI_INTERNAL_ERROR                \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xE80)
#define GOOPDATE_E_OBSERVER_PROGRESS_WND_EVENTS_NULL  \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xE81)

// ApplyTagTool error codes.
#define APPLYTAG_E_ALREADY_TAGGED                   \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x1000)

// The range [0x2000, 0x2400) is reserved for certain network stack errors
// when the server returns an HTTP result code that is not a success code.
// The size of the range is 1024, which is enough to map all the HTTP result
// codes.
#define GOOPDATE_E_NETWORK_FIRST                            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2000)

// Http Status Code 401 -- Unauthorized.
#define GOOPDATE_E_NETWORK_UNAUTHORIZED                     \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2191)

// Http Status Code 403 -- Forbidden.
#define GOOPDATE_E_NETWORK_FORBIDDEN                        \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2193)

// Http Status Code 407 -- Proxy Authentication Required.
#define GOOPDATE_E_NETWORK_PROXYAUTHREQUIRED                \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2197)

#define GOOPDATE_E_NETWORK_LAST                             \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x23FF)

// Shared Memory Proxy error codes.
#define GOOPDATE_E_INVALID_SHARED_MEMORY_PTR                \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2401)
#define GOOPDATE_E_INVALID_INTERFACE_MARSHAL_SIZE           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0x2402)

// Crash handling error codes.

// The crash reporting cannot start the crash server for
// out-of-process crash handling.
#define GOOPDATE_E_CRASH_START_SERVER_FAILED                 \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFA)

// The crash reporting cannot set the security descriptors for
// a securable object.
#define GOOPDATE_E_CRASH_SECURITY_FAILED                     \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFB)

// The crash reporting could not get the crash reports dir.
#define GOOPDATE_E_CRASH_NO_DIR                             \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFC)

// The crash reporting failed due to the client side metering of the crashes.
#define GOOPDATE_E_CRASH_THROTTLED                          \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFD)

// The crash reporting failed due to the server rejecting the crash.
#define GOOPDATE_E_CRASH_REJECTED                           \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFE)

// Goopdate crash. The error code is returned when the process is terminated
// due to a crash handled by breakpad.
#define GOOPDATE_E_CRASH                            \
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xFFFF)


// This is the end of the range for FACILITY_ITF errors. Do not define errors
// below.

}  // namespace omaha

#endif  // OMAHA_COMMON_ERROR_H_

