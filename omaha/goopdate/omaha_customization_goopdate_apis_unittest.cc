// Copyright 2010 Google Inc.
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
// Tests the constants that vary depending on the customization of Omaha.
// The test checks for the Google Update variations, but can be modified for
// your purposes.

#include <windows.h>
#include <tchar.h>
#include <atlbase.h>
#include <oleauto.h>
#include "omaha/base/browser_utils.h"
#include "omaha/base/utils.h"
#include "omaha/common/const_goopdate.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/testing/omaha_customization_test.h"

// Most of the tests are intentionally not using the omaha namespace. Most of
// the values being tested are not in this namespace, and being in the global
// namespace is required by TEST_GU_INT_F to catch conflicts with Google types
// when building non-Google versions.

class OmahaCustomizationGoopdateComInterfaceTest
    : public OmahaCustomizationTypeLibComInterfaceTest {
 protected:
  OmahaCustomizationGoopdateComInterfaceTest()
      : OmahaCustomizationTypeLibComInterfaceTest(omaha::kOmahaDllName) {
  }
};

// Fixture for testing interfaces that are not in a TypeLib.
// We can only verify the uuid of the interfaces and classes.
class OmahaCustomizationGoopdateComInterfaceNoTypeLibTest
    : public testing::Test {
};

//
// Omaha 3 COM Constants.
//

namespace omaha {

// TODO(omaha): We should probably move these to a separate
// const_com_customization.h in goopdate\.
TEST(OmahaCustomizationTest, Constants_ComProgIds) {
  EXPECT_GU_STREQ(_T("GoogleUpdate.OnDemandCOMClassUser"), kProgIDOnDemandUser);
  EXPECT_GU_STREQ(_T("GoogleUpdate.OnDemandCOMClassMachine"),
                  kProgIDOnDemandMachine);
  EXPECT_GU_STREQ(_T("GoogleUpdate.OnDemandCOMClassSvc"), kProgIDOnDemandSvc);

  EXPECT_GU_STREQ(_T("GoogleUpdate.Update3WebUser"), kProgIDUpdate3WebUser);
  EXPECT_GU_STREQ(_T("GoogleUpdate.Update3WebMachine"),
                  kProgIDUpdate3WebMachine);
  EXPECT_GU_STREQ(_T("GoogleUpdate.Update3WebSvc"), kProgIDUpdate3WebSvc);

  EXPECT_GU_STREQ(_T("GoogleUpdate.CoreClass"), kProgIDGoogleUpdateCoreService);

  EXPECT_GU_STREQ(_T("GoogleUpdate.ProcessLauncher"), kProgIDProcessLauncher);

  EXPECT_GU_STREQ(_T("GoogleUpdate.PolicyStatusUser"), kProgIDPolicyStatusUser);
  EXPECT_GU_STREQ(_T("GoogleUpdate.PolicyStatusMachine"),
                  kProgIDPolicyStatusMachine);
  EXPECT_GU_STREQ(_T("GoogleUpdate.PolicyStatusMachineFallback"),
                  kProgIDPolicyStatusMachineFallback);
  EXPECT_GU_STREQ(_T("GoogleUpdate.PolicyStatusSvc"), kProgIDPolicyStatusSvc);
}

}  // namespace omaha

//
// Omaha 3 COM Interfaces Enums.
//

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, BrowserType) {
  EXPECT_EQ(0, BROWSER_UNKNOWN);
  EXPECT_EQ(1, BROWSER_DEFAULT);
  EXPECT_EQ(2, BROWSER_INTERNET_EXPLORER);
  EXPECT_EQ(3, BROWSER_FIREFOX);
  EXPECT_EQ(4, BROWSER_CHROME);
}

// There are two different BrowserType definitions, one in the IDL and one
// in browser_utils. Verify they are identical.
TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       BrowserType_DefinitionsMatch) {
  EXPECT_EQ(::BROWSER_UNKNOWN,            omaha::BROWSER_UNKNOWN);
  EXPECT_EQ(::BROWSER_DEFAULT,            omaha::BROWSER_DEFAULT);
  EXPECT_EQ(::BROWSER_INTERNET_EXPLORER,  omaha::BROWSER_IE);
  EXPECT_EQ(::BROWSER_FIREFOX,            omaha::BROWSER_FIREFOX);
  EXPECT_EQ(::BROWSER_CHROME,             omaha::BROWSER_CHROME);

  EXPECT_EQ(::BROWSER_CHROME + 1, omaha::BROWSER_MAX)
      << _T("A browser has been added without updating test and/or the IDL");
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, CurrentState) {
  EXPECT_EQ(1,  STATE_INIT);
  EXPECT_EQ(2,  STATE_WAITING_TO_CHECK_FOR_UPDATE);
  EXPECT_EQ(3,  STATE_CHECKING_FOR_UPDATE);
  EXPECT_EQ(4,  STATE_UPDATE_AVAILABLE);
  EXPECT_EQ(5,  STATE_WAITING_TO_DOWNLOAD);
  EXPECT_EQ(6,  STATE_RETRYING_DOWNLOAD);
  EXPECT_EQ(7,  STATE_DOWNLOADING);
  EXPECT_EQ(8,  STATE_DOWNLOAD_COMPLETE);
  EXPECT_EQ(9,  STATE_EXTRACTING);
  EXPECT_EQ(10, STATE_APPLYING_DIFFERENTIAL_PATCH);
  EXPECT_EQ(11, STATE_READY_TO_INSTALL);
  EXPECT_EQ(12, STATE_WAITING_TO_INSTALL);
  EXPECT_EQ(13, STATE_INSTALLING);
  EXPECT_EQ(14, STATE_INSTALL_COMPLETE);
  EXPECT_EQ(15, STATE_PAUSED);
  EXPECT_EQ(16, STATE_NO_UPDATE);
  EXPECT_EQ(17, STATE_ERROR);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, InstallPriority) {
  EXPECT_EQ(0,  INSTALL_PRIORITY_LOW);
  EXPECT_EQ(10, INSTALL_PRIORITY_HIGH);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, PostInstallAction) {
  EXPECT_EQ(0, POST_INSTALL_ACTION_DEFAULT);
  EXPECT_EQ(1, POST_INSTALL_ACTION_EXIT_SILENTLY);
  EXPECT_EQ(2, POST_INSTALL_ACTION_LAUNCH_COMMAND);
  EXPECT_EQ(3, POST_INSTALL_ACTION_EXIT_SILENTLY_ON_LAUNCH_COMMAND);
  EXPECT_EQ(4, POST_INSTALL_ACTION_RESTART_BROWSER);
  EXPECT_EQ(5, POST_INSTALL_ACTION_RESTART_ALL_BROWSERS);
  EXPECT_EQ(6, POST_INSTALL_ACTION_REBOOT);
}

//
// Omaha 3 COM Interfaces.
//

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, TypeLib) {
  EXPECT_GU_ID_EQ(_T("{655DD85A-3C0D-4674-9C58-AF7168C5861E}"),
                  LIBID_GoogleUpdate3Lib);

  EXPECT_SUCCEEDED(GetTypeLibDocumentation());
  EXPECT_STREQ(_T("GoogleUpdate3Lib"), item_name_);
  EXPECT_GU_STREQ(_T("Google Update 3.0 Type Library"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IGoogleUpdate3) {
  // TODO(omaha): Test uuid constants after extracting from IDLs.
  EXPECT_GU_ID_EQ(_T("{6DB17455-4E85-46e7-9D23-E555E4B005AF}"),
                  __uuidof(IGoogleUpdate3));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IGoogleUpdate3")));
  EXPECT_STREQ(_T("IGoogleUpdate3 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// The IAppBundle interface name does not change for non-Google builds, but the
// ID must. The same is true for many of the interfaces.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppBundle) {
  EXPECT_GU_ID_EQ(_T("{fe908cdd-22bb-472a-9870-1a0390e42f36}"),
                  __uuidof(IAppBundle));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppBundle")));
  EXPECT_STREQ(_T("IAppBundle Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// This appears in the typelib for unknown reasons.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ULONG_PTR) {
  EXPECT_SUCCEEDED(GetDocumentation(_T("ULONG_PTR")));
  EXPECT_TRUE(!item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IApp) {
  EXPECT_GU_ID_EQ(_T("{76F7B787-A67C-4c73-82C7-31F5E3AABC5C}"),
                  __uuidof(IApp));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IApp")));
  EXPECT_STREQ(_T("IApp Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IApp2) {
  EXPECT_GU_ID_EQ(_T("{084D78A8-B084-4E14-A629-A2C419B0E3D9}"),
                  __uuidof(IApp2));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IApp2")));
  EXPECT_STREQ(_T("IApp2 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppCommand) {
  EXPECT_GU_ID_EQ(_T("{4DE778FE-F195-4EE3-9DAB-FE446C239221}"),
                  __uuidof(IAppCommand));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppCommand")));
  EXPECT_STREQ(_T("IAppCommand Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppCommand2) {
  EXPECT_GU_ID_EQ(_T("{3D05F64F-71E3-48A5-BF6B-83315BC8AE1F}"),
                  __uuidof(IAppCommand2));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppCommand2")));
  EXPECT_STREQ(_T("IAppCommand2 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppVersion) {
  EXPECT_GU_ID_EQ(_T("{BCDCB538-01C0-46d1-A6A7-52F4D021C272}"),
                  __uuidof(IAppVersion));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppVersion")));
  EXPECT_STREQ(_T("IAppVersion Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IPackage) {
  EXPECT_GU_ID_EQ(_T("{DCAB8386-4F03-4dbd-A366-D90BC9F68DE6}"),
                  __uuidof(IPackage));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IPackage")));
  EXPECT_STREQ(_T("IPackage Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ICurrentState) {
  EXPECT_GU_ID_EQ(_T("{247954F9-9EDC-4E68-8CC3-150C2B89EADF}"),
                  __uuidof(ICurrentState));

  EXPECT_SUCCEEDED(GetDocumentation(_T("ICurrentState")));
  EXPECT_STREQ(_T("ICurrentState Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// Not in the TypeLib because it derives from IUnknown.
TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IRegistrationUpdateHook) {
  EXPECT_GU_ID_EQ(_T("{4E223325-C16B-4eeb-AEDC-19AA99A237FA}"),
                  __uuidof(IRegistrationUpdateHook));
}

// Not in the TypeLib because it derives from IUnknown.
TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, ICoCreateAsync) {
  EXPECT_GU_ID_EQ(_T("{DAB1D343-1B2A-47f9-B445-93DC50704BFE}"),
                  __uuidof(ICoCreateAsync));
}

// Not in the TypeLib because it derives from IUnknown.
TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest, ICredentialDialog) {
  EXPECT_GU_ID_EQ(_T("{b3a47570-0a85-4aea-8270-529d47899603}"),
                  __uuidof(ICredentialDialog));
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IPolicyStatus) {
  EXPECT_GU_ID_EQ(_T("{F63F6F8B-ACD5-413C-A44B-0409136D26CB}"),
                  __uuidof(IPolicyStatus));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IPolicyStatus")));
  EXPECT_STREQ(_T("IPolicyStatus Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IPolicyStatusValue) {
  EXPECT_GU_ID_EQ(_T("{27634814-8E41-4C35-8577-980134A96544}"),
                  __uuidof(IPolicyStatusValue));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IPolicyStatusValue")));
  EXPECT_STREQ(_T("IPolicyStatusValue Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IPolicyStatus2) {
  EXPECT_GU_ID_EQ(_T("{34527502-D3DB-4205-A69B-789B27EE0414}"),
                  __uuidof(IPolicyStatus2));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IPolicyStatus2")));
  EXPECT_STREQ(_T("IPolicyStatus2 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IPolicyStatus3) {
  EXPECT_GU_ID_EQ(_T("{05A30352-EB25-45B6-8449-BCA7B0542CE5}"),
                  __uuidof(IPolicyStatus3));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IPolicyStatus3")));
  EXPECT_STREQ(_T("IPolicyStatus3 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IGoogleUpdate3Web) {
  EXPECT_GU_ID_EQ(_T("{494B20CF-282E-4BDD-9F5D-B70CB09D351E}"),
                  __uuidof(IGoogleUpdate3Web));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IGoogleUpdate3Web")));
  EXPECT_STREQ(_T("IGoogleUpdate3Web Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// Not in the TypeLib because it derives from IUnknown.
TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IGoogleUpdate3WebSecurity) {
  EXPECT_GU_ID_EQ(_T("{2D363682-561D-4c3a-81C6-F2F82107562A}"),
                  __uuidof(IGoogleUpdate3WebSecurity));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppBundleWeb) {
  EXPECT_GU_ID_EQ(_T("{DD42475D-6D46-496a-924E-BD5630B4CBBA}"),
                  __uuidof(IAppBundleWeb));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppBundleWeb")));
  EXPECT_STREQ(_T("IAppBundleWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppWeb) {
  EXPECT_GU_ID_EQ(_T("{18D0F672-18B4-48e6-AD36-6E6BF01DBBC4}"),
                  __uuidof(IAppWeb));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppWeb")));
  EXPECT_STREQ(_T("IAppWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppCommandWeb) {
  EXPECT_GU_ID_EQ(_T("{8476CE12-AE1F-4198-805C-BA0F9B783F57}"),
                  __uuidof(IAppCommandWeb));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppCommandWeb")));
  EXPECT_STREQ(_T("IAppCommandWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppVersionWeb) {
  EXPECT_GU_ID_EQ(_T("{0CD01D1E-4A1C-489d-93B9-9B6672877C57}"),
                  __uuidof(IAppVersionWeb));

  EXPECT_SUCCEEDED(GetDocumentation(_T("IAppVersionWeb")));
  EXPECT_STREQ(_T("IAppVersionWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ICoCreateAsyncStatus) {
  EXPECT_GU_ID_EQ(_T("{2E629606-312A-482f-9B12-2C4ABF6F0B6D}"),
                  __uuidof(ICoCreateAsyncStatus));

  EXPECT_SUCCEEDED(GetDocumentation(_T("ICoCreateAsyncStatus")));
  EXPECT_STREQ(_T("ICoCreateAsyncStatus Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3UserClass) {
  EXPECT_GU_ID_EQ(_T("{022105BD-948A-40c9-AB42-A3300DDF097F}"),
                  __uuidof(GoogleUpdate3UserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3UserClass")));
  EXPECT_STREQ(_T("GoogleUpdate3 Class for per-user applications"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3ServiceClass) {
  EXPECT_GU_ID_EQ(_T("{4EB61BAC-A3B6-4760-9581-655041EF4D69}"),
                  __uuidof(GoogleUpdate3ServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3ServiceClass")));
  EXPECT_STREQ(_T("GoogleUpdate3 Service Class for machine applications"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebUserClass) {
  EXPECT_GU_ID_EQ(_T("{22181302-A8A6-4f84-A541-E5CBFC70CC43}"),
                  __uuidof(GoogleUpdate3WebUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3WebUserClass")));
  EXPECT_STREQ(_T("GoogleUpdate3Web for user applications"),
                  item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebMachineClass) {
  EXPECT_GU_ID_EQ(_T("{8A1D4361-2C08-4700-A351-3EAA9CBFF5E4}"),
                  __uuidof(GoogleUpdate3WebMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3WebMachineClass")));
  EXPECT_STREQ(
      _T("Pass-through broker for the GoogleUpdate3WebServiceClass"),
      item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebServiceClass) {
  EXPECT_GU_ID_EQ(_T("{534F5323-3569-4f42-919D-1E1CF93E5BF6}"),
                  __uuidof(GoogleUpdate3WebServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdate3WebServiceClass")));
  EXPECT_STREQ(_T("GoogleUpdate3Web"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebMachineFallbackClass) {
  EXPECT_GU_ID_EQ(_T("{598FE0E5-E02D-465d-9A9D-37974A28FD42}"),
                  __uuidof(GoogleUpdate3WebMachineFallbackClass));

  EXPECT_SUCCEEDED(
      GetDocumentation(_T("GoogleUpdate3WebMachineFallbackClass")));
  EXPECT_STREQ(L"Fallback mechanism if GoogleUpdate3WebServiceClass fails",
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CurrentStateUserClass) {
  EXPECT_GU_ID_EQ(_T("{E8CF3E55-F919-49d9-ABC0-948E6CB34B9F}"),
                  __uuidof(CurrentStateUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("CurrentStateUserClass")));
  EXPECT_STREQ(_T("CurrentStateUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CurrentStateMachineClass) {
  EXPECT_GU_ID_EQ(_T("{9D6AA569-9F30-41ad-885A-346685C74928}"),
                  __uuidof(CurrentStateMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("CurrentStateMachineClass")));
  EXPECT_STREQ(_T("CurrentStateMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CoCreateAsyncClass) {
  EXPECT_GU_ID_EQ(_T("{7DE94008-8AFD-4c70-9728-C6FBFFF6A73E}"),
                  __uuidof(CoCreateAsyncClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("CoCreateAsyncClass")));
  EXPECT_STREQ(_T("CoCreateAsyncClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CredentialDialogUserClass) {
  EXPECT_GU_ID_EQ(_T("{e67be843-bbbe-4484-95fb-05271ae86750}"),
                  __uuidof(CredentialDialogUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("CredentialDialogUserClass")));
  EXPECT_STREQ(_T("CredentialDialogUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CredentialDialogMachineClass) {
  EXPECT_GU_ID_EQ(_T("{25461599-633d-42b1-84fb-7cd68d026e53}"),
                  __uuidof(CredentialDialogMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("CredentialDialogMachineClass")));
  EXPECT_STREQ(_T("CredentialDialogMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusValueUserClass) {
  EXPECT_GU_ID_EQ(_T("{85D8EE2F-794F-41F0-BB03-49D56A23BEF4}"),
                  __uuidof(PolicyStatusValueUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusValueUserClass")));
  EXPECT_STREQ(_T("PolicyStatusValueUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusValueMachineClass) {
  EXPECT_GU_ID_EQ(_T("{C6271107-A214-4F11-98C0-3F16BC670D28}"),
                  __uuidof(PolicyStatusValueMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusValueMachineClass")));
  EXPECT_STREQ(_T("PolicyStatusValueMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusUserClass) {
  EXPECT_GU_ID_EQ(_T("{6DDCE70D-A4AE-4E97-908C-BE7B2DB750AD}"),
                  __uuidof(PolicyStatusUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusUserClass")));
  EXPECT_STREQ(_T("Policy Status for per-user applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusMachineClass) {
  EXPECT_GU_ID_EQ(_T("{521FDB42-7130-4806-822A-FC5163FAD983}"),
                  __uuidof(PolicyStatusMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusMachineClass")));
  EXPECT_STREQ(_T("Policy Status pass-through broker ")
               _T("for machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusMachineServiceClass) {
  EXPECT_GU_ID_EQ(_T("{1C4CDEFF-756A-4804-9E77-3E8EB9361016}"),
                  __uuidof(PolicyStatusMachineServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusMachineServiceClass")));
  EXPECT_STREQ(_T("Policy Status for per-machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              PolicyStatusMachineFallbackClass) {
  EXPECT_GU_ID_EQ(_T("{ADDF22CF-3E9B-4CD7-9139-8169EA6636E4}"),
                  __uuidof(PolicyStatusMachineFallbackClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("PolicyStatusMachineFallbackClass")));
  EXPECT_STREQ(_T("Fallback for if PolicyStatusMachineServiceClass fails."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleComProxyMachineClass) {
  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleComProxyMachineClass")));
  EXPECT_STREQ(_T("GoogleComProxyMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleComProxyUserClass) {
  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleComProxyUserClass")));
  EXPECT_STREQ(_T("GoogleComProxyUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              ProcessLauncherClass) {
  EXPECT_GU_ID_EQ(_T("{ABC01078-F197-4b0b-ADBC-CFE684B39C82}"),
                  __uuidof(ProcessLauncherClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("ProcessLauncherClass")));
  EXPECT_STREQ(_T("ProcessLauncherClass Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandUserAppsClass) {
  EXPECT_GU_ID_EQ(_T("{2F0E2680-9FF5-43c0-B76E-114A56E93598}"),
                  __uuidof(OnDemandUserAppsClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("OnDemandUserAppsClass")));
  EXPECT_STREQ(_T("OnDemand updates for per-user applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsClass) {
  EXPECT_GU_ID_EQ(_T("{6F8BD55B-E83D-4a47-85BE-81FFA8057A69}"),
                  __uuidof(OnDemandMachineAppsClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("OnDemandMachineAppsClass")));
  EXPECT_STREQ(_T("OnDemand pass-through broker for machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsServiceClass) {
  EXPECT_GU_ID_EQ(_T("{9465B4B4-5216-4042-9A2C-754D3BCDC410}"),
                  __uuidof(OnDemandMachineAppsServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("OnDemandMachineAppsServiceClass")));
  EXPECT_STREQ(_T("OnDemand updates for per-machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsFallbackClass) {
  EXPECT_GU_ID_EQ(_T("{B3D28DBD-0DFA-40e4-8071-520767BADC7E}"),
                  __uuidof(OnDemandMachineAppsFallbackClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("OnDemandMachineAppsFallbackClass")));
  EXPECT_STREQ(_T("Fallback for if OnDemandMachineAppsServiceClass fails."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdateCoreClass) {
  EXPECT_GU_ID_EQ(_T("{E225E692-4B47-4777-9BED-4FD7FE257F0E}"),
                  __uuidof(GoogleUpdateCoreClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdateCoreClass")));
  EXPECT_STREQ(_T("GoogleUpdateCore Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdateCoreMachineClass) {
  EXPECT_GU_ID_EQ(_T("{9B2340A0-4068-43d6-B404-32E27217859D}"),
                  __uuidof(GoogleUpdateCoreMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(_T("GoogleUpdateCoreMachineClass")));
  EXPECT_STREQ(_T("GoogleUpdateCore Machine Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// Verifies there are no new interfaces in the TypeLib.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, VerifyNoNewInterfaces) {
  EXPECT_EQ(46, type_lib_->GetTypeInfoCount())
      << _T("A new interface may have been added. If so, add the interface to ")
      << _T("to kIIDsToRegister, and add test(s) for new interface(s).");
}

//
// Omaha 2 COM Interfaces.
//
// TODO(omaha): We should make it so open source versions do not need these
// legacy interfaces.

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IBrowserHttpRequest2) {
  EXPECT_GU_ID_EQ(_T("{5B25A8DC-1780-4178-A629-6BE8B8DEFAA2}"),
                  __uuidof(IBrowserHttpRequest2));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IProcessLauncher) {
  EXPECT_GU_ID_EQ(_T("{128C2DA6-2BC0-44c0-B3F6-4EC22E647964}"),
                  __uuidof(IProcessLauncher));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IProgressWndEvents) {
  EXPECT_GU_ID_EQ(_T("{1C642CED-CA3B-4013-A9DF-CA6CE5FF6503}"),
                  __uuidof(IProgressWndEvents));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IJobObserver) {
  EXPECT_GU_ID_EQ(_T("{49D7563B-2DDB-4831-88C8-768A53833837}"),
                  __uuidof(IJobObserver));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IGoogleUpdate) {
  EXPECT_GU_ID_EQ(_T("{31AC3F11-E5EA-4a85-8A3D-8E095A39C27B}"),
                  __uuidof(IGoogleUpdate));
}

TEST_F(OmahaCustomizationGoopdateComInterfaceNoTypeLibTest,
       IGoogleUpdateCore) {
  EXPECT_GU_ID_EQ(_T("{909489C2-85A6-4322-AA56-D25278649D67}"),
                  __uuidof(IGoogleUpdateCore));
}
