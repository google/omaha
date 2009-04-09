// Copyright 2007-2009 Google Inc.
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
// Service-related utilities.

#ifndef OMAHA_COMMON_SERVICE_UTILS_H__
#define OMAHA_COMMON_SERVICE_UTILS_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

namespace omaha {

// Utility functions for working with the SCM database.
class ScmDatabase {
 public:
  // Callback function type for EnumerateServices. This gets called for each
  // service in the registry.
  //
  // @param callback_context Passed unchanged from the caller of
  // EnumerateServices to the callback function.
  // @param service_name The name of the service (not the display name but
  // rather the canonical name, to be used with e.g. ::OpenService()).  Note
  // that because this function is based on enumerating the registry, it's
  // possible that services that were recently deleted will show up in the
  // enumeration; therefore, it should not be considered an error if you try
  // to ::OpenService() on this name and it fails with a last error of
  // ERROR_SERVICE_DOES_NOT_EXIST or possibly ERROR_INVALID_NAME (if
  // somebody messed up the registry by hand).
  //
  // @return S_OK to continue enumeration, S_FALSE or a COM error code to
  // stop enumeration.  The return value will be propagated to the caller
  // of Enumerate.
  //
  // @note The initial version of this function used EnumServicesStatusEx
  // but it turns out the function is a fair bit flaky, not returning all
  // recently created (e.g. created but never started) services.
  typedef HRESULT(*EnumerateServicesCallback)(void* callback_context,
                                              const wchar_t* service_name);

  // Calls 'callback' for each of the services in the registry.
  //
  // @param callback Callback function to call
  // @param callback_context Passed unchanged to your callback function
  //
  // @return S_OK or a COM error code.
  static HRESULT EnumerateServices(EnumerateServicesCallback callback,
                                   void* callback_context);

  // Returns true iff the service passed in is in the indicated state.
  //
  // @param service An open handle to a service.  The handle must have at
  // least SERVICE_QUERY_CONFIG rights.
  // @param state One of the SERVICE_XXX constants indicating the state of a
  // service (e.g. SERVICE_DISABLED).
  //
  // @return True iff 'service' is in state 'state'.
  static bool IsServiceStateEqual(SC_HANDLE service, DWORD state);

  // Returns true iff the service passed in has been marked deleted.
  //
  // @param service An open handle to a service.  The handle must have at
  // least SERVICE_QUERY_CONFIG and SERVICE_CHANGE_CONFIG rights.
  //
  // @return True iff 'service' has been marked deleted.
  static bool IsServiceMarkedDeleted(SC_HANDLE service);

 private:
  DISALLOW_EVIL_CONSTRUCTORS(ScmDatabase);
};

// Utility functions for the service's installation, overinstall etc.
class ServiceInstall {
 public:

  // Generates a versioned service name based on the current system time.
  static CString GenerateServiceName(const TCHAR* service_prefix);

  // Uninstalls all versions of the service other than the one that matches
  // the service name passed in. Pass in NULL to uninstall everything.
  static HRESULT UninstallServices(const TCHAR* service_prefix,
                                   const TCHAR* exclude_service);

  static bool IsServiceInstalled(const TCHAR* service_name);

  // @return True if the current service can be installed without rebooting,
  // false if a reboot is required before it can be installed.  The cases
  // where the current service can be installed without rebooting are:
  // a) when no service exists with the current name
  // b) when there is an existing service with the current name but it is
  //    not marked for deletion
  static bool CanInstallWithoutReboot();

  // Given a service name, stops it if it is already running.
  static HRESULT StopService(const CString& service_name);

 protected:
  // Context passed to the UninstallIfNotCurrent function; this is made a
  // parameter so we can unit test the function without mucking with the
  // "actual" services.
  struct UninstallByPrefixParams {
    CString prefix;  // prefix of services we want to uninstall
    CString unless_matches;  // name of current service, to not touch
  };

  // Uninstalls a given service if it matches a given prefix but does not match
  // a given full service name.
  //
  // This is an ScmDatabase::EnumerateServicesCallback function.
  //
  // @param context Pointer to an UninstallByPrefix structure.
  static HRESULT UninstallByPrefix(void* context, const wchar_t* service_name);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ServiceInstall);
};

}  // namespace omaha

#endif  // OMAHA_COMMON_SERVICE_UTILS_H__

