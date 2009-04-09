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
// Class allows the worker to access products and components.

#ifndef OMAHA_WORKER_APPLICATION_MANAGER_H__
#define OMAHA_WORKER_APPLICATION_MANAGER_H__

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/debug.h"
#include "omaha/common/reg_key.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/update_response_data.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/product_data.h"

namespace omaha {

// Application manager allows the worker to access registry state.
class AppManager {
 public:
  explicit AppManager(bool is_machine);

  bool IsProductRegistered(const GUID& app_guid) const;
  bool IsComponentRegistered(const GUID& app_guid, const GUID& component_guid);

  void ConvertCommandLineToProductData(const CommandLineArgs& args,
                                       ProductDataVector* products);

  HRESULT GetRegisteredProducts(ProductDataVector* products) const;

  HRESULT GetUnRegisteredProducts(ProductDataVector* products) const;

  // Reads the application state from the registry.
  HRESULT ReadProductDataFromStore(const GUID& app_guid,
                                   ProductData* product_data);

  // Reads an individual AppData from the store.  Will read either product or
  // component level based on whether there's a parent or not.  This will not
  // recurse to read children since it's only the AppData portion.
  HRESULT ReadAppDataFromStore(const GUID& parent_app_guid,
                               const GUID& component_guid,
                               AppData* app_data);

  // Sets dynamic install parameters that the installer or app may use.
  // Call this method before calling the installer.
  HRESULT WritePreInstallData(const AppData& app_data);

  // Sets the initial state of the application in the registry. Call this
  // method after the initial install/update has completed.
  HRESULT InitializeApplicationState(AppData* app_data);

  // Updates the state of the application in the registry. Call this method
  // to update the state of the application after an update check.
  HRESULT UpdateApplicationState(AppData* app_data);

  // Write the TT Token with what the server returned.
  HRESULT WriteTTToken(const AppData& app_data,
                       const UpdateResponseData& response_data);

  // Stores information about the update available event for the app.
  // Call each time an update is available.
  void UpdateUpdateAvailableStats(const GUID& parent_app_guid,
                                  const GUID& app_guid);

  // Clears the stored information about update available events for the app.
  // Call when an update has succeeded.
  void ClearUpdateAvailableStats(const GUID& parent_app_guid,
                                 const GUID& app_guid);

  // Clears the OEM-installed flag for the app.
  void ClearOemInstalled(const GUID& parent_app_guid, const GUID& app_guid);

  // Obtains usage stats information from the stored information about update
  // available events for the app.
  void ReadUpdateAvailableStats(const GUID& parent_app_guid,
                                const GUID& app_guid,
                                DWORD* update_responses,
                                DWORD64* time_since_first_response_ms);

  // Removes all the registration entries under the client state for the
  // application.
  HRESULT RemoveClientState(const AppData& app_data);

  // Returns true if a server update check is due.
  bool ShouldCheckForUpdates() const;
  HRESULT UpdateLastChecked();

  static HRESULT ReadProductDataFromUserOrMachineStore(
      const GUID& guid,
      ProductData* product_data);

  HRESULT RegisterProduct(const GUID& product_guid,
                          const CString& product_name);
  HRESULT UnregisterProduct(const GUID& product_guid);

 private:
  CString GetClientKeyName(const GUID& parent_app_guid, const GUID& app_guid);
  CString GetClientStateKeyName(const GUID& parent_app_guid,
                                const GUID& app_guid);

  // Opens the app's Client key for read access.
  HRESULT OpenClientKey(const GUID& parent_app_guid,
                        const GUID& app_guid,
                        RegKey* client_key);
  // Opens the app's ClientState key with the specified access.
  HRESULT OpenClientStateKey(const GUID& parent_app_guid,
                             const GUID& app_guid,
                             REGSAM sam_desired,
                             RegKey* client_state_key);
  // Creates the app's ClientState key.
  HRESULT CreateClientStateKey(const GUID& parent_app_guid,
                               const GUID& app_guid,
                               RegKey* client_state_key);

  CString GetProductClientKeyName(const GUID& app_guid);
  CString GetProductClientComponentsKeyName(const GUID& app_guid);
  CString GetProductClientStateComponentsKeyName(const GUID& app_guid);
  CString GetComponentClientKeyName(const GUID& parent_app_guid,
                                    const GUID& app_guid);
  CString GetProductClientStateKeyName(const GUID& app_guid);
  CString GetProductClientStateMediumKeyName(const GUID& app_guid);
  CString GetComponentClientStateKeyName(const GUID& parent_app_guid,
                                         const GUID& app_guid);

  HRESULT ClearInstallationId(AppData* app_data,
                              const RegKey& client_state_key);
  HRESULT CopyVersionAndLanguageToClientState(AppData* app_data,
                                              const RegKey& client_state_key,
                                              const RegKey& client_key);

  const bool is_machine_;

  DISALLOW_EVIL_CONSTRUCTORS(AppManager);
};

}  // namespace omaha.

#endif  // OMAHA_WORKER_APPLICATION_MANAGER_H__

