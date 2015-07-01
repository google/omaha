// Copyright 2009-2010 Google Inc.
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


#ifndef OMAHA_CLIENT_BUNDLE_INSTALLER_H_
#define OMAHA_CLIENT_BUNDLE_INSTALLER_H_

#include <windows.h>
#include <atlstr.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/base/safe_format.h"
#include "omaha/base/wtl_atlapp_wrapper.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/client/install_progress_observer.h"

namespace omaha {

// TODO(omaha): These should be declared in their own file.
namespace internal {

CString GetAppDisplayName(IApp* app);

// Builds a list of app names of the format "one, two, three".
// TODO(omaha): If this does not end up using AppCompletionInfo, move it to
// client_utils.
CString BuildAppNameList(const std::vector<CString>& app_names);

// Helper function that returns the error information from the ICurrentState.
HRESULT GetCompletionInformation(IApp* app,
                                 CurrentState* current_state,
                                 AppCompletionInfo* app_info);

// Gets the completion message for an app.
void GetAppCompletionMessage(IApp* app,
                             AppCompletionInfo* app_info);

// Gets the completion message for the bundle.
// If the bundle name cannot be obtained, pass an empty string in bundle_name.
// TODO(omaha): If AppCompletionInfo is exposed, maybe move to client_utils.
CString GetBundleCompletionMessage(
    const CString& bundle_name,
    const std::vector<AppCompletionInfo>& apps_info,
    bool is_only_no_update,
    bool is_canceled);

}  // namespace internal

class HelpUrlBuilder;
class ShutdownCallback;

class BundleInstaller
    : public CWindowImpl<BundleInstaller,
                         CWindow,
                         CWinTraits<WS_OVERLAPPED, WS_EX_TOOLWINDOW> > {
 public:
  // Takes ownership of help_url_builder.
  BundleInstaller(HelpUrlBuilder* help_url_builder,
                  bool is_update_all_apps,
                  bool is_update_check_only,
                  bool is_browser_type_supported);
  ~BundleInstaller();

  HRESULT Initialize();
  void Uninitialize();

  // Installs a bundle. The installer takes the ownership of the bundle and
  // releases the inteface before function returns.
  HRESULT InstallBundle(bool is_machine,
                        bool listen_to_shutdown_event,
                        IAppBundle* app_bundle,
                        InstallProgressObserver* observer);

  void SetBundleParentWindow(HWND parent_window);

  // Message loop that pumps messages during installation.
  CMessageLoop* message_loop() { return &message_loop_; }

  // Handles asynchronous requests for the application to close.
  void DoClose();

  // Handles requests to exit the BundleInstaller message loop. Should only be
  // called after the BundleInstaller is in the kComplete state, i.e., after
  // OnComplete().
  void DoExit();

  // Handles asynchronous requests to cancel install.
  void DoCancel();

  // Polls the COM server and advances the install state appropriately.
  // Returns true if the caller should continue polling.
  bool PollServer();

  HRESULT result();

 private:
  enum State {
    kInit,
    kProcessing,
    kComplete,
  };

  // Contains additional information about the bundle completion.
  struct BundleCompletionInfo {
    CompletionCodes completion_code;
    HRESULT bundle_result;  // Result to return up call stack on completion.
    CString bundle_completion_message;
    std::vector<AppCompletionInfo> apps_info;

    BundleCompletionInfo(CompletionCodes code,
                         HRESULT result,
                         const CString& message)
        : completion_code(code),
          bundle_result(result),
          bundle_completion_message(message) {}

#ifdef DEBUG
    CString ToString() const {
      CString result;
      SafeCStringFormat(&result, _T("[BundleCompletionInfo][%d][0x%x][%s]"),
                        completion_code,
                        bundle_result,
                        bundle_completion_message);
      for (size_t i = 0; i < apps_info.size(); ++i) {
        SafeCStringAppendFormat(&result, _T("[%s]"), apps_info[i].ToString());
      }
      return result;
    }
#endif
  };

  // Does the work for PollServer.
  HRESULT DoPollServer();

  // Performs the polling while checking for update.
  HRESULT HandleUpdateAvailable();

  // Handles the first call to PollServer().
  HRESULT HandleInitState();

  // Performs all subsequent calls to PollServer() until the state is complete.
  HRESULT HandleProcessingState();

  // Makes installer listen to the shutdown event.
  HRESULT ListenToShutdownEvent(bool is_machine);

  // Stops listening to the shutdown event if the installer is currently
  // listening. Otherwise no effect.
  void StopListenToShutdownEvent(bool is_machine);

  // These functions update the UI during HandleProcessingState().
  // TODO(omaha): Rename these to Notify*.
  HRESULT NotifyUpdateAvailable(IApp* app);
  HRESULT NotifyDownloadProgress(IApp* app, ICurrentState* icurrent_state);
  HRESULT NotifyWaitingToInstall(IApp* app);
  HRESULT NotifyInstallProgress(IApp* app, ICurrentState* icurrent_state);
  HRESULT NotifyBundleUpdateCheckOnlyComplete();
  HRESULT NotifyBundleInstallComplete();

  // Helper functions for the Notify* functions.
  HRESULT HandleUpdateCheckResults(int* num_updates);
  void GetAppDownloadProgress(ICurrentState* icurrent_state,
                              int* time_remaining_ms,
                              int* percentage,
                              time64* next_retry_time);
  void GetAppInstallProgress(ICurrentState* icurrent_state,
                             int* time_remaining_ms,
                             int* percentage);

  void CancelBundle();

  // Sets the state to complete and informs the UI.
  void Complete(const BundleCompletionInfo& bundle_info);

  BEGIN_MSG_MAP(BundleInstaller)
    MESSAGE_HANDLER(WM_CLOSE, OnClose)
    MESSAGE_HANDLER(WM_TIMER, OnTimer)
  END_MSG_MAP()

  static const int kPollingTimerId = 1;
  static const int kPollingTimerPeriodMs = 100;

  // The main use case for this OnClose() handler is the shutdown handler via a
  // PostMessage in the /UA scenario.
  LRESULT OnClose(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT

  // Calls BundleInstaller::PollServer() at periodic intervals.
  LRESULT OnTimer(UINT msg,
                  WPARAM wparam,
                  LPARAM lparam,
                  BOOL& handled);  // NOLINT

  void ReleaseAppBundle();

  InstallProgressObserver* observer_;
  scoped_ptr<HelpUrlBuilder> help_url_builder_;

  // The bundle to be installed.
  CComPtr<IAppBundle> app_bundle_;

  // Bundle parent window.
  HWND parent_window_;

  // Message loop that pumps messages during installation.
  CMessageLoop message_loop_;

  // Shutdown event listener.
  scoped_ptr<ShutdownCallback> shutdown_callback_;

  // The apps in app_bundle_. Allows easier and quicker access to the apps than
  // going through app_bundle_.
  typedef CComPtr<IApp> ComPtrIApp;
  typedef CAdapt<ComPtrIApp> AdaptIApp;
  std::vector<AdaptIApp> apps_;

  State state_;
  HRESULT result_;
  bool is_canceled_;
  bool is_handling_message_;

  const bool is_update_all_apps_;
  const bool is_update_check_only_;  // Only used by legacy OnDemand.
  const bool is_browser_type_supported_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(BundleInstaller);
};

}  // namespace omaha

#endif  // OMAHA_CLIENT_BUNDLE_INSTALLER_H_
