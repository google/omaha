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
// Header files to precompile for the installer.

#ifndef  OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTECUSTOMACTION_H__
#define  OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTECUSTOMACTION_H__

#include <windows.h>
#include <msi.h>
#include <atlbase.h>

namespace omaha {

class CustomActionModule : public CAtlDllModuleT<CustomActionModule> {
 public:
  CustomActionModule() {}
  ~CustomActionModule() {}
};

}  // namespace omaha

extern "C"
UINT __stdcall VerifyFileAndExecute(MSIHANDLE);

#endif  // OMAHA_RECOVERY_REPAIR_EXE_CUSTOM_ACTION_EXECUTECUSTOMACTION_H__
