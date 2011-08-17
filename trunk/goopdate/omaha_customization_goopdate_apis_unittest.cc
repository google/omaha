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

// TODO(omaha): Add tests for to detect interface changes that would require
// rolling _OMAHA3_IDL_PROXY_CLSID_IS. These include:
// 1) interface changes invovlving the number or signature of methods
// 2) or that new interfaces have been added
// For #2, we already have the InvalidIndex test for interfaces in the TypeLib,
// so we just need to add checks for interfaces not in the TypeLib.
//
// ITypeLib and ITypeInfo methods might be useful. See:
// http://msdn.microsoft.com/en-us/library/aa912648.aspx
// http://msdn.microsoft.com/en-us/library/aa909031.aspx
//
// I do not know how to get information about interfaces not in a TypeLib.
// Fortunately, most Omaha 3 interfaces are in one.
//
// If we can not get all the information we need, we can always save a "golden"
// idl.h file and diff against it.


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

  EXPECT_SUCCEEDED(GetDocumentation(-1));
  EXPECT_STREQ(_T("GoogleUpdate3Lib"), item_name_);
  EXPECT_GU_STREQ(_T("Google Update 3.0 Type Library"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IGoogleUpdate3) {
  // TODO(omaha): Test uuid constants after extracting from IDLs.
  EXPECT_GU_ID_EQ(_T("{6DB17455-4E85-46e7-9D23-E555E4B005AF}"),
                  __uuidof(IGoogleUpdate3));

  EXPECT_SUCCEEDED(GetDocumentation(0));
  EXPECT_STREQ(_T("IGoogleUpdate3"), item_name_);
  EXPECT_STREQ(_T("IGoogleUpdate3 Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// The IAppBundle interface name does not change for non-Google builds, but the
// ID must. The same is true for many of the interfaces.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppBundle) {
  EXPECT_GU_ID_EQ(_T("{313cfb25-4888-4fc6-9e19-764d8c5fc8f8}"),
                  __uuidof(IAppBundle));

  EXPECT_SUCCEEDED(GetDocumentation(1));
  EXPECT_STREQ(_T("IAppBundle"), item_name_);
  EXPECT_STREQ(_T("IAppBundle Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// This appears in the typelib for unknown reasons.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ULONG_PTR) {
  EXPECT_SUCCEEDED(GetDocumentation(2));
  EXPECT_STREQ(_T("ULONG_PTR"), item_name_);
  EXPECT_TRUE(!item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IApp) {
  EXPECT_GU_ID_EQ(_T("{D999CE21-98B3-4894-BACB-A49A1D50848F}"),
                  __uuidof(IApp));

  EXPECT_SUCCEEDED(GetDocumentation(3));
  EXPECT_STREQ(_T("IApp"), item_name_);
  EXPECT_STREQ(_T("IApp Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppVersion) {
  EXPECT_GU_ID_EQ(_T("{BCDCB538-01C0-46d1-A6A7-52F4D021C272}"),
                  __uuidof(IAppVersion));

  EXPECT_SUCCEEDED(GetDocumentation(4));
  EXPECT_STREQ(_T("IAppVersion"), item_name_);
  EXPECT_STREQ(_T("IAppVersion Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IPackage) {
  EXPECT_GU_ID_EQ(_T("{DCAB8386-4F03-4dbd-A366-D90BC9F68DE6}"),
                  __uuidof(IPackage));

  EXPECT_SUCCEEDED(GetDocumentation(5));
  EXPECT_STREQ(_T("IPackage"), item_name_);
  EXPECT_STREQ(_T("IPackage Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ICurrentState) {
  EXPECT_GU_ID_EQ(_T("{247954F9-9EDC-4E68-8CC3-150C2B89EADF}"),
                  __uuidof(ICurrentState));

  EXPECT_SUCCEEDED(GetDocumentation(6));
  EXPECT_STREQ(_T("ICurrentState"), item_name_);
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

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest, IGoogleUpdate3Web) {
  EXPECT_GU_ID_EQ(_T("{494B20CF-282E-4BDD-9F5D-B70CB09D351E}"),
                  __uuidof(IGoogleUpdate3Web));

  EXPECT_SUCCEEDED(GetDocumentation(7));
  EXPECT_STREQ(_T("IGoogleUpdate3Web"), item_name_);
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

  EXPECT_SUCCEEDED(GetDocumentation(8));
  EXPECT_STREQ(_T("IAppBundleWeb"), item_name_);
  EXPECT_STREQ(_T("IAppBundleWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppWeb) {
  EXPECT_GU_ID_EQ(_T("{C6398F88-69CE-44ac-B6A7-1D3E2AA46679}"),
                  __uuidof(IAppWeb));

  EXPECT_SUCCEEDED(GetDocumentation(9));
  EXPECT_STREQ(_T("IAppWeb"), item_name_);
  EXPECT_STREQ(_T("IAppWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, IAppVersionWeb) {
  EXPECT_GU_ID_EQ(_T("{0CD01D1E-4A1C-489d-93B9-9B6672877C57}"),
                  __uuidof(IAppVersionWeb));

  EXPECT_SUCCEEDED(GetDocumentation(10));
  EXPECT_STREQ(_T("IAppVersionWeb"), item_name_);
  EXPECT_STREQ(_T("IAppVersionWeb Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_F(OmahaCustomizationGoopdateComInterfaceTest, ICoCreateAsyncStatus) {
  EXPECT_GU_ID_EQ(_T("{2E629606-312A-482f-9B12-2C4ABF6F0B6D}"),
                  __uuidof(ICoCreateAsyncStatus));

  EXPECT_SUCCEEDED(GetDocumentation(11));
  EXPECT_STREQ(_T("ICoCreateAsyncStatus"), item_name_);
  EXPECT_STREQ(_T("ICoCreateAsyncStatus Interface"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3UserClass) {
  EXPECT_GU_ID_EQ(_T("{022105BD-948A-40c9-AB42-A3300DDF097F}"),
                  __uuidof(GoogleUpdate3UserClass));

  EXPECT_SUCCEEDED(GetDocumentation(12));
  EXPECT_STREQ(_T("GoogleUpdate3UserClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdate3 Class for per-user applications"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3ServiceClass) {
  EXPECT_GU_ID_EQ(_T("{4EB61BAC-A3B6-4760-9581-655041EF4D69}"),
                  __uuidof(GoogleUpdate3ServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(13));
  EXPECT_STREQ(_T("GoogleUpdate3ServiceClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdate3 Service Class for machine applications"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebUserClass) {
  EXPECT_GU_ID_EQ(_T("{22181302-A8A6-4f84-A541-E5CBFC70CC43}"),
                  __uuidof(GoogleUpdate3WebUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(14));
  EXPECT_STREQ(_T("GoogleUpdate3WebUserClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdate3Web for user applications"),
                  item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebMachineClass) {
  EXPECT_GU_ID_EQ(_T("{8A1D4361-2C08-4700-A351-3EAA9CBFF5E4}"),
                  __uuidof(GoogleUpdate3WebMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(15));
  EXPECT_STREQ(_T("GoogleUpdate3WebMachineClass"), item_name_);
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

  EXPECT_SUCCEEDED(GetDocumentation(16));
  EXPECT_STREQ(_T("GoogleUpdate3WebServiceClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdate3Web"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdate3WebMachineFallbackClass) {
  EXPECT_GU_ID_EQ(_T("{598FE0E5-E02D-465d-9A9D-37974A28FD42}"),
                  __uuidof(GoogleUpdate3WebMachineFallbackClass));

  EXPECT_SUCCEEDED(GetDocumentation(17));
  EXPECT_STREQ(_T("GoogleUpdate3WebMachineFallbackClass"), item_name_);
  EXPECT_STREQ(L"Fallback mechanism if GoogleUpdate3WebServiceClass fails",
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CurrentStateUserClass) {
  EXPECT_GU_ID_EQ(_T("{E8CF3E55-F919-49d9-ABC0-948E6CB34B9F}"),
                  __uuidof(CurrentStateUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(18));
  EXPECT_STREQ(_T("CurrentStateUserClass"), item_name_);
  EXPECT_STREQ(_T("CurrentStateUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CurrentStateMachineClass) {
  EXPECT_GU_ID_EQ(_T("{9D6AA569-9F30-41ad-885A-346685C74928}"),
                  __uuidof(CurrentStateMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(19));
  EXPECT_STREQ(_T("CurrentStateMachineClass"), item_name_);
  EXPECT_STREQ(_T("CurrentStateMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CoCreateAsyncClass) {
  EXPECT_GU_ID_EQ(_T("{7DE94008-8AFD-4c70-9728-C6FBFFF6A73E}"),
                  __uuidof(CoCreateAsyncClass));

  EXPECT_SUCCEEDED(GetDocumentation(20));
  EXPECT_STREQ(_T("CoCreateAsyncClass"), item_name_);
  EXPECT_STREQ(_T("CoCreateAsyncClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CredentialDialogUserClass) {
  EXPECT_GU_ID_EQ(_T("{e67be843-bbbe-4484-95fb-05271ae86750}"),
                  __uuidof(CredentialDialogUserClass));

  EXPECT_SUCCEEDED(GetDocumentation(21));
  EXPECT_STREQ(_T("CredentialDialogUserClass"), item_name_);
  EXPECT_STREQ(_T("CredentialDialogUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              CredentialDialogMachineClass) {
  EXPECT_GU_ID_EQ(_T("{25461599-633d-42b1-84fb-7cd68d026e53}"),
                  __uuidof(CredentialDialogMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(22));
  EXPECT_STREQ(_T("CredentialDialogMachineClass"), item_name_);
  EXPECT_STREQ(_T("CredentialDialogMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleComProxyMachineClass) {
  EXPECT_SUCCEEDED(GetDocumentation(23));
  EXPECT_STREQ(_T("GoogleComProxyMachineClass"), item_name_);
  EXPECT_STREQ(_T("GoogleComProxyMachineClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleComProxyUserClass) {
  EXPECT_SUCCEEDED(GetDocumentation(24));
  EXPECT_STREQ(_T("GoogleComProxyUserClass"), item_name_);
  EXPECT_STREQ(_T("GoogleComProxyUserClass"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              ProcessLauncherClass) {
  EXPECT_GU_ID_EQ(_T("{ABC01078-F197-4b0b-ADBC-CFE684B39C82}"),
                  __uuidof(ProcessLauncherClass));

  EXPECT_SUCCEEDED(GetDocumentation(25));
  EXPECT_STREQ(_T("ProcessLauncherClass"), item_name_);
  EXPECT_STREQ(_T("ProcessLauncherClass Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OneClickUserProcessLauncherClass) {
  EXPECT_GU_ID_EQ(_T("{51F9E8EF-59D7-475b-A106-C7EA6F30C119}"),
                  __uuidof(OneClickUserProcessLauncherClass));

  EXPECT_SUCCEEDED(GetDocumentation(26));
  EXPECT_STREQ(_T("OneClickUserProcessLauncherClass"), item_name_);
  EXPECT_STREQ(_T("OneClickUserProcessLauncherClass Class"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              IOneClickProcessLauncher) {
  EXPECT_GU_ID_EQ(_T("{5CCCB0EF-7073-4516-8028-4C628D0C8AAB}"),
                  __uuidof(IOneClickProcessLauncher));

  EXPECT_SUCCEEDED(GetDocumentation(27));
  EXPECT_STREQ(_T("IOneClickProcessLauncher"), item_name_);
  EXPECT_STREQ(_T("Google Update IOneClickProcessLauncher Interface"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OneClickMachineProcessLauncherClass) {
  EXPECT_GU_ID_EQ(_T("{AAD4AE2E-D834-46d4-8B09-490FAC9C722B}"),
                  __uuidof(OneClickMachineProcessLauncherClass));

  EXPECT_SUCCEEDED(GetDocumentation(28));
  EXPECT_STREQ(_T("OneClickMachineProcessLauncherClass"), item_name_);
  EXPECT_STREQ(_T("OneClickMachineProcessLauncherClass Class"),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandUserAppsClass) {
  EXPECT_GU_ID_EQ(_T("{2F0E2680-9FF5-43c0-B76E-114A56E93598}"),
                  __uuidof(OnDemandUserAppsClass));

  EXPECT_SUCCEEDED(GetDocumentation(29));
  EXPECT_STREQ(_T("OnDemandUserAppsClass"), item_name_);
  EXPECT_STREQ(_T("OnDemand updates for per-user applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsClass) {
  EXPECT_GU_ID_EQ(_T("{6F8BD55B-E83D-4a47-85BE-81FFA8057A69}"),
                  __uuidof(OnDemandMachineAppsClass));

  EXPECT_SUCCEEDED(GetDocumentation(30));
  EXPECT_STREQ(_T("OnDemandMachineAppsClass"), item_name_);
  EXPECT_STREQ(_T("OnDemand pass-through broker for machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsServiceClass) {
  EXPECT_GU_ID_EQ(_T("{9465B4B4-5216-4042-9A2C-754D3BCDC410}"),
                  __uuidof(OnDemandMachineAppsServiceClass));

  EXPECT_SUCCEEDED(GetDocumentation(31));
  EXPECT_STREQ(_T("OnDemandMachineAppsServiceClass"), item_name_);
  EXPECT_STREQ(_T("OnDemand updates for per-machine applications."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              OnDemandMachineAppsFallbackClass) {
  EXPECT_GU_ID_EQ(_T("{B3D28DBD-0DFA-40e4-8071-520767BADC7E}"),
                  __uuidof(OnDemandMachineAppsFallbackClass));

  EXPECT_SUCCEEDED(GetDocumentation(32));
  EXPECT_STREQ(_T("OnDemandMachineAppsFallbackClass"), item_name_);
  EXPECT_STREQ(_T("Fallback for if OnDemandMachineAppsServiceClass fails."),
               item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdateCoreClass) {
  EXPECT_GU_ID_EQ(_T("{E225E692-4B47-4777-9BED-4FD7FE257F0E}"),
                  __uuidof(GoogleUpdateCoreClass));

  EXPECT_SUCCEEDED(GetDocumentation(33));
  EXPECT_STREQ(_T("GoogleUpdateCoreClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdateCore Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

TEST_GU_INT_F(OmahaCustomizationGoopdateComInterfaceTest,
              GoogleUpdateCoreMachineClass) {
  EXPECT_GU_ID_EQ(_T("{9B2340A0-4068-43d6-B404-32E27217859D}"),
                  __uuidof(GoogleUpdateCoreMachineClass));

  EXPECT_SUCCEEDED(GetDocumentation(34));
  EXPECT_STREQ(_T("GoogleUpdateCoreMachineClass"), item_name_);
  EXPECT_STREQ(_T("GoogleUpdateCore Machine Class"), item_doc_string_);
  EXPECT_EQ(0, help_context_);
  EXPECT_TRUE(!help_file_);
}

// Verifies there are no new interfaces in the TypeLib.
TEST_F(OmahaCustomizationGoopdateComInterfaceTest, VerifyNoNewInterfaces) {
  EXPECT_EQ(TYPE_E_ELEMENTNOTFOUND, GetDocumentation(35))
      << _T("A new interface may have been added. If so, roll ")
      << _T("PROXY_CLSID_IS_MACHINE/USER and GoogleComProxyMachine/UserClass, ")
      << _T("add the interface to kIIDsToRegister, and add test(s) for new ")
      << _T("interface(s).");
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

