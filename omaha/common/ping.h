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

// Functions related to the sending the setup pings. The functionality provided
// by this class is used in by install, handoff, and update features.
// This is an overview of how the setup pings work.
// In the case of install, the execution flow includes the following steps:
// elevation if needed, setting up Omaha, and installing the applications
// specified in the tag.
// The code guarantees that an EVENT_INSTALL_COMPLETE(2) ping for Omaha is sent
// in all cases, except trivial errors that may happen before the execution flow
// reaches the Install function.
// A EVENT_INSTALL_COMPLETE(2) ping for the apps is also sent in all cases.
//
// Where the code fails affects how the pings are generated and sent, as
// following:
// * if the elevation was required but the elevated process failed to run,
//   then both pings are sent from the medium integrity /install
//   process.
// * if the Omaha setup code ran but it errored out or the handoff failed to
//   launch, then both pings are sent from the /install process or the
//   elevated /install process if elevation was successful. The pings will be
//   split in two different http transactions in the case setup completed
//   successfully but it failed to handoff.
// * if the /handoff process launched but an error occurred in the handoff
//   process itself, then the Omaha "2" ping is sent from the /install process
//   and the apps "2" ping is sent from the /handoff process.
//   The apps ping is only sent if the handoff code did not proceed far enough
//   to create a bundle of  applications. Beyond that point, the bundle takes
//   over the responsibility of sending "2" pings for each app in the bundle.
//
// There is an IPC mechanism between /install and /handoff processes based
// on detected input idle to avoid overlapping error handling and
// displaying redundant error messages in different processes. Usually ping
// handling, error handling, and displaying error UI is done in the same layer.
// When an error happens in the chain of /install, elevated install,
// and /handoff  processes, then UI is displayed by one of these processes only
// if the child process did not display UI. Since UI is displayed in the
// /handoff process in both the success and error cases, this information can't
// be useful to handle the pings, therefore pings only rely on a weaker
// guarantee, which is whether the child process has launched or not.

// TODO(omaha): unify the install and bundle pings mechanisms. There is
// no facility to cancel the install pings in the current implementation.

// TODO(omaha): use a pimpl to avoid the dependency on UpdateRequest.

#ifndef OMAHA_COMMON_PING_H_
#define OMAHA_COMMON_PING_H_

#include <windows.h>
#include <atlstr.h>
#include <utility>
#include <memory>
#include <vector>

#include "base/basictypes.h"
#include "gtest/gtest_prod.h"
#include "omaha/common/app_registry_utils.h"
#include "omaha/common/ping_event.h"
#include "omaha/common/update_request.h"
#include "omaha/common/web_services_client.h"

namespace omaha {

struct CommandLineExtraArgs;
class App;

// Loads, builds, serializes, and sends pings. There are two ways to manipulate
// ping instances. The simplest way is to have another entity, such as
// AppBundle, drive this class by repeatedly calling BuildRequest. This is
// usually the case when a set of ping events contained by different apps
// have to be sent. In some cases, there is no model object to hold the
// metadata about apps and their ping events. The metadata has to come from a
// source, and therefore, must be loaded from the registry, or the extra args
// provided on the command line.
// After this class has loaded the apps metadata, an Omaha ping or an apps ping
// can be built and serialized. This is usually the case when pings have to be
// sent from the setup, before any model object could be created.
class Ping {
 public:
  Ping(bool is_machine,
       const CString& session_id,
       const CString& install_source,
       const CString& request_id);
  Ping(bool is_machine,
       const CString& session_id,
       const CString& install_source);
  ~Ping();

  // TODO(omaha): Consider moving everything except the functionality that
  // actually sends the pings out of the Ping class into builder classes. A
  // dependency on the model App is not desirable here.
  void BuildRequest(const App* app);

  // Loads app data from a location other than the Omaha state machine.
  void LoadAppDataFromExtraArgs(const CommandLineExtraArgs& extra_args);
  void LoadAppDataFromRegistry(const std::vector<CString>& apps);
  void LoadOmahaDataFromRegistry();

  // Add additional one-off experiment labels to the Omaha app.
  void AddExtraOmahaLabel(const CString& label_set);

  // Builds pings for Omaha or apps loaded previously.
  void BuildOmahaPing(const CString& version,
                      const CString& next_version,
                      const PingEventPtr& ping_event);

  void BuildOmahaPing(const CString& version,
                      const CString& next_version,
                      const PingEventPtr& ping_event1,
                      const PingEventPtr& ping_event2);

  void BuildAppsPing(const PingEventPtr& ping_event);

  // Serializes a ping request as a string.
  HRESULT BuildRequestString(CString* request_string) const;

  // Sends the ping events. The pings could be sent out-of-process,
  // using the installed Omaha or in-process, if the out-of-process delivery
  // fails.
  //
  // Sending pings is attempted out-of-process first, with a timeout
  // of 60 seconds, after which the in-process delivery kicks in. The pinging
  // process pinging is terminated before the in-process pinging is attempted
  // in order to avoid duplicate pings and prevent run away processes.
  //
  // The 'is_fire_and_forget' argument only applies to the out-of-process
  // delivery mechanism. This allows the execution flow to return to the caller
  // as soon as possible and it is useful for sending success pings.
  // The in-process pinging is always blocking.
  //
  // If the caller is local system and a user is logged on, the function
  // impersonatates that user.
  //
  // Pings are always sent with a background interactivity, even if they
  // correspond to on-demand, foreground installs.
  //
  // The function returns S_OK if the ping was successfully sent using either
  // mechanism.
  HRESULT Send(bool is_fire_and_forget);

  // Persists the current Ping object to the registry.
  HRESULT PersistPing();

  // Sends all persisted pings. Deletes successful or expired pings.
  static HRESULT SendPersistedPings(bool is_machine);

  // Sends a ping string to the server, in-process. The ping_string must be web
  // safe base64 encoded and it will be decoded before the ping is sent.
  static HRESULT HandlePing(bool is_machine, const CString& ping_string);

 private:
  FRIEND_TEST(PingTest, BuildOmahaPing);
  FRIEND_TEST(PingTest, BuildOmahaPingWithSessionOverride);
  FRIEND_TEST(PingTest, BuildAppsPing);
  FRIEND_TEST(PingTest, BuildAppsPingFromRegistry);
  FRIEND_TEST(PingTest, DISABLED_SendString);
  FRIEND_TEST(PingTest, SendInProcess);
  FRIEND_TEST(PingTest, IsPingExpired_PastTime);
  FRIEND_TEST(PingTest, IsPingExpired_CurrentTime);
  FRIEND_TEST(PingTest, IsPingExpired_FutureTime);
  FRIEND_TEST(PingTest, LoadPersistedPings_NoPersistedPings);
  FRIEND_TEST(PingTest, LoadAndDeletePersistedPings);
  FRIEND_TEST(PingTest, PersistPing);
  FRIEND_TEST(PingTest, PersistPing_Load_Delete);
  FRIEND_TEST(PingTest, PersistAndSendPersistedPings);
  FRIEND_TEST(PingTest, DISABLED_SendUsingGoogleUpdate);
  FRIEND_TEST(PersistedPingsTest, AddPingEvents);

  // pair<unique_id, pair<ping_time, ping_string>>.
  typedef
      std::vector<std::pair<CString, std::pair<time64, CString> > > PingsVector;
  static const TCHAR* const kRegKeyPersistedPings;
  static const TCHAR* const kRegValuePersistedPingTime;
  static const TCHAR* const kRegValuePersistedPingString;
  static const time64 kPersistedPingExpiry100ns;

  void Initialize(bool is_machine,
                  const CString& session_id,
                  const CString& install_source,
                  const CString& request_id);

  // Sends pings using the installed GoogleUpdate, which runs in the
  // ping mode. the function waits for the pings to be sent if wait_timeout_ms
  // is not zero. Returns S_OK if the pings have been successfully sent.
  HRESULT SendUsingGoogleUpdate(const CString& request_string,
                                DWORD wait_timeout_ms) const;

  // Sends ping events in process. Returns S_OK if the pings have been
  // sent to the server and the server response is 200 OK;
  HRESULT SendInProcess(const CString& request_string) const;

  xml::request::App BuildOmahaApp(const CString& version,
                                  const CString& next_version) const;

  // Persistent Ping utility functions.
  static CString GetPersistedPingsRegPath(bool is_machine);
  static HRESULT LoadPersistedPings(bool is_machine,
                                    PingsVector* persisted_pings);
  static bool IsPingExpired(time64 persisted_time);
  static HRESULT DeletePersistedPing(bool is_machine,
                                     const CString& persisted_subkey_name);
  void DeletePersistedPingOnSuccess(const HRESULT& hr);
  CString GetPersistedPingRegPath();

  // Sends a string to the server.
  static HRESULT SendString(bool is_machine,
                            const HeadersVector& headers,
                            const CString& request_string);

  bool is_machine_;

  // The request id is the unique key that is sent out in Ping requests to the
  // Omaha server. Persisted Pings are also stored in the registry under this
  // unique key.
  CString request_id_;

  // Information about apps.
  struct AppData {
    AppData() : install_time_diff_sec(0), day_of_install(0) {}

    CString app_id;
    CString language;
    CString brand_code;
    CString client_id;
    CString installation_id;
    CString pv;
    CString experiment_labels;
    Cohort cohort;
    int install_time_diff_sec;
    int day_of_install;
  };
  std::vector<AppData> apps_data_;
  AppData omaha_data_;

  std::unique_ptr<xml::UpdateRequest> ping_request_;

  DISALLOW_COPY_AND_ASSIGN(Ping);
};

// Helper function that calls Ping::PersistPing() followed by Ping::Send().
// Returns the HRESULT that Send() returns.
HRESULT SendReliablePing(Ping* ping, bool is_fire_and_forget);

}  // namespace omaha

#endif  // OMAHA_COMMON_PING_H_

