// Copyright 2009 Google Inc.
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

#include "omaha/client/client_utils.h"
#include "omaha/base/debug.h"
#include "omaha/base/logging.h"
#include "omaha/base/vistautil.h"
#include "omaha/client/help_url_builder.h"
#include "omaha/common/goopdate_utils.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/ui/complete_wnd.h"
#include "omaha/ui/yes_no_dialog.h"

namespace omaha {

namespace client_utils {

namespace {

// Assumes this is not a silent process.
// Uses bundle_name in the title if provided; otherwise displays generic title.
bool DisplayErrorInMessageBox(const CString& error_text,
                              const CString& bundle_name) {
  CString msg_box_title = GetInstallerDisplayName(bundle_name);
  return (0 != ::MessageBox(NULL, error_text, msg_box_title, MB_OK));
}

class ErrorWndEvents : public CompleteWndEvents {
 public:
  explicit ErrorWndEvents(bool is_machine) : is_machine_(is_machine) {}

  // TODO(omaha3): Not sure if we need to do anything for DoClose.
  virtual void DoClose() {}

  virtual void DoExit() {
    ::PostQuitMessage(0);
  }

  // TODO(omaha3): Use the specified browser if available.
  // TODO(omaha3): Need to address elevated Vista installs. We could ask the
  // non-elevated /install instance to launch the browser for us using some
  // type of notification pipe like we use for out-of-process crashes.
  virtual bool DoLaunchBrowser(const CString& url) {
    CORE_LOG(L2, (_T("[ErrorWndEvents::DoLaunchBrowser %s]"), url));
    return SUCCEEDED(goopdate_utils::LaunchBrowser(
        is_machine_, BROWSER_DEFAULT, url));
  }

 private:
  bool is_machine_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ErrorWndEvents);
};

bool CanLaunchBrowser() {
  bool is_elevated_with_uac_on(false);
  if (FAILED(vista_util::IsElevatedWithUACOn(&is_elevated_with_uac_on))) {
    return false;
  }

  if (!is_elevated_with_uac_on) {
    return true;
  }

  CComPtr<IProcessLauncher> launcher;
  return SUCCEEDED(launcher.CoCreateInstance(CLSID_ProcessLauncherClass,
                                             NULL,
                                             CLSCTX_LOCAL_SERVER));
}

}  // namespace


// Assumes this is an interactive instance.
// If the Omaha UI fails to initialize, displays the error in a message box.
// Thus, some UI should always be displayed.
bool DisplayError(bool is_machine,
                  const CString& bundle_name,
                  HRESULT error,
                  int extra_code,
                  const CString& error_text,
                  const CString& app_id,
                  const CString& language_id,
                  const GUID& iid,
                  const CString& brand_code) {
  CMessageLoop message_loop;
  CompleteWnd error_wnd(&message_loop, NULL);

  error_wnd.set_is_machine(is_machine);
  error_wnd.set_bundle_name(bundle_name);
  CString help_url;
  HelpUrlBuilder url_builder(is_machine, language_id, iid, brand_code);
  std::vector<HelpUrlBuilder::AppResult> app_install_result;
  app_install_result.push_back(HelpUrlBuilder::AppResult(app_id,
                                                         error,
                                                         extra_code));

  // When running elevated and ProcessLauncherClass is not registered, the
  // browser launch from the link will fail. Don't display a link that will not
  // work.
  if (CanLaunchBrowser()) {
    VERIFY_SUCCEEDED(url_builder.BuildUrl(app_install_result, &help_url));
  }

  HRESULT hr = error_wnd.Initialize();
  if (FAILED(hr)) {
    CORE_LOG(LE, (_T("UI initialize failed][0x%08x]"), hr));
    bool result = DisplayErrorInMessageBox(error_text, bundle_name);
    ASSERT1(result);
    error_wnd.DestroyWindow();  // Window will not get to destroy itself.
    return result;
  }

  ErrorWndEvents error_wnd_events(is_machine);
  // error_wnd_events must not be destroyed until CMessageLoop::Run() returns.
  error_wnd.SetEventSink(&error_wnd_events);

  error_wnd.Show();
  error_wnd.DisplayCompletionDialog(false, error_text, help_url);
  message_loop.Run();
  return true;
}

bool DisplayContinueAsNonAdmin(const CString& bundle_name,
                               bool* should_continue) {
  ASSERT1(should_continue);
  *should_continue = false;

  CString title(client_utils::GetInstallerDisplayName(bundle_name));
  CString text;
  text.FormatMessage(IDS_CONTINUE_AS_NONADMIN, bundle_name);

  CMessageLoop message_loop;
  YesNoDialog continue_dialog(&message_loop, NULL);
  HRESULT hr = continue_dialog.Initialize(title, text);

  if (FAILED(hr)) {
    int button_id = ::MessageBox(NULL, text, title, MB_YESNO);
    *should_continue = button_id == IDYES;
    return button_id != 0;
  }

  VERIFY_SUCCEEDED(continue_dialog.Show());
  message_loop.Run();

  *should_continue = continue_dialog.yes_clicked();
  return true;
}

CString GetDefaultApplicationName() {
  CString company_name;
  VERIFY1(company_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));

  CString default_app_name;
  default_app_name.FormatMessage(IDS_DEFAULT_APP_DISPLAY_NAME, company_name);
  return default_app_name;
}

CString GetDefaultBundleName() {
  return GetDefaultApplicationName();
}

CString GetUpdateAllAppsBundleName() {
  // TODO(omaha3): If we ever productize interactive updates, we will need a
  // different string. This may be okay for the title bar, but it looks weird
  // in the completion strings. The current implementation uses the same string
  // for both.
  return GetDefaultApplicationName();
}

// If bundle_name is empty, the friendly company name is used.
CString GetInstallerDisplayName(const CString& bundle_name) {
  CString display_name = bundle_name;
  if (display_name.IsEmpty()) {
    VERIFY1(display_name.LoadString(IDS_FRIENDLY_COMPANY_NAME));
  }

  CString installer_name;
  installer_name.FormatMessage(IDS_INSTALLER_DISPLAY_NAME, display_name);
  return installer_name;
}

}  // namespace client_utils

}  // namespace omaha
