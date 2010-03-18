// Copyright 2008-2010 Google Inc.
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

#include <atlbase.h>
#include <msxml2.h>
#include "omaha/common/app_util.h"
#include "omaha/common/error.h"
#include "omaha/common/omaha_version.h"
#include "omaha/common/path.h"
#include "omaha/common/process.h"
#include "omaha/common/scoped_ptr_address.h"
#include "omaha/goopdate/config_manager.h"
#include "omaha/goopdate/const_goopdate.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/goopdate/request.h"
#include "omaha/goopdate/stats_uploader.h"
#include "omaha/net/cup_request.h"
#include "omaha/net/simple_request.h"
#include "omaha/testing/unit_test.h"
#include "omaha/worker/i_job_observer_mock.h"
#include "omaha/worker/job_creator.h"
#include "omaha/worker/job_observer_mock.h"
#include "omaha/worker/ping_event.h"
#include "omaha/worker/ping_mock.h"
#include "omaha/worker/worker_job.h"
#include "omaha/worker/worker_job_strategy.h"
#include "omaha/worker/worker_metrics.h"

CComModule module;

namespace omaha {

namespace {

#define APP_GUID _T("{2D8F7DCC-86F9-464c-AF80-B986B551927B}")
#define APP_GUID2 _T("{7234E9E5-3870-4561-9533-B1D91696A8BA}")
#define APP_GUID3 _T("{5F5FC3BC-A40E-4dfc-AE0B-FD039A343EE8}")
const TCHAR* const kAppGuid = APP_GUID;
const TCHAR* const kAppGuid2 = APP_GUID2;
const TCHAR* const kAppGuid3 = APP_GUID3;

const TCHAR* const kPolicyKey =
    _T("HKLM\\Software\\Policies\\Google\\Update\\");
const TCHAR* const kInstallPolicyApp = _T("Install") APP_GUID;
const TCHAR* const kUpdatePolicyApp = _T("Update") APP_GUID;
const TCHAR* const kInstallPolicyApp2 = _T("Install") APP_GUID2;
const TCHAR* const kUpdatePolicyApp2 = _T("Update") APP_GUID2;
const TCHAR* const kInstallPolicyApp3 = _T("Install") APP_GUID3;
const TCHAR* const kUpdatePolicyApp3 = _T("Update") APP_GUID3;
const TCHAR* const kUpdatePolicyGoopdate = _T("Update") GOOPDATE_APP_ID;

const TCHAR* const kMachineClientStatePathApp =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\") APP_GUID;
const TCHAR* const kMachineClientStatePathApp2 =
    _T("HKLM\\Software\\Google\\Update\\ClientState\\") APP_GUID2;
const TCHAR* const kMachineClientStateMediumPathApp =
    _T("HKLM\\Software\\Google\\Update\\ClientStateMedium\\") APP_GUID;

const CString handoff_cmd_line(_T("/handoff /lang en"));
const CString finish_setup_cmd_line(_T("/ig /lang en"));
const CString false_extra_args(
    _T("\"appguid={2D8F7DCC-86F9-464c-AF80-B986B551927B}")
    _T("&appname=FooBar&needsadmin=False\""));
const CString true_extra_args(
    _T("\"appguid={2D8F7DCC-86F9-464c-AF80-B986B551927B}")
    _T("&appname=FooBar&needsadmin=True\""));

// Helper to write policies to the registry. Eliminates ambiguity of which
// overload of SetValue to use without the need for static_cast.
HRESULT SetPolicy(const TCHAR* policy_name, DWORD value) {
  return RegKey::SetValue(kPolicyKey, policy_name, value);
}

// Returns E_FAIL from SendPing().
class PingMockFail : public PingMock {
 public:
  virtual HRESULT SendPing(Request* req) {
    VERIFY1(SUCCEEDED(PingMock::SendPing(req)));
    return E_FAIL;
  }
};

// Records the last request buffer and if a request has been sent.
class MockRequestSave : public SimpleRequest {
 public:
  MockRequestSave() : is_sent_(false) {}

  // Sets is_sent and performs the send.
  virtual HRESULT Send() {
    is_sent_ = true;
    return SimpleRequest::Send();
  }

  // Assumes the buffer is valid UTF-8.
  virtual void set_request_buffer(const void* buffer, size_t buffer_length) {
    sent_request_utf8_.SetString(static_cast<const char*>(buffer),
                                 buffer_length);
    SimpleRequest::set_request_buffer(buffer, buffer_length);
  }

  const CStringA& sent_request_utf8() const { return sent_request_utf8_; }
  bool is_sent() const { return is_sent_; }

 private:
  CStringA sent_request_utf8_;
  bool is_sent_;
};


// Performs the Send(), but always returns true and a response with "noupdate"
// for the specified apps.
// The actual return value of Send() is inconsistent behavior due to HKLM being
// overridden because DNS lookup may or may not fail depending on earlier tests.
class MockRequestSaveNoUpdate : public MockRequestSave {
 public:
  explicit MockRequestSaveNoUpdate(
      const std::vector<CString>& expected_app_guids)
      : expected_app_guids_(expected_app_guids) {
  }

  virtual HRESULT Send() {
    MockRequestSave::Send();
    return S_OK;
  }

  virtual int GetHttpStatusCode() const {
    return 200;
  }

  virtual std::vector<uint8> GetResponse() const {
    CStringA no_update_response =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
        "<gupdate xmlns=\"http://www.google.com/update2/response\" "
        "protocol=\"2.0\">";
    for (size_t i = 0; i < expected_app_guids_.size(); ++i) {
      no_update_response.Append("<app appid=\"");
      no_update_response.Append(WideToAnsiDirect(expected_app_guids_[i]));
      no_update_response.Append(
        "\" status=\"ok\">"
        "  <updatecheck status=\"noupdate\"/><ping status=\"ok\"/>"
        "</app>");
    }
    no_update_response.Append("</gupdate>");

    const size_t response_length = strlen(no_update_response);
    std::vector<uint8> response;
    response.resize(response_length);
    EXPECT_EQ(0, memcpy_s(&response[0],
                          response_length,
                          no_update_response,
                          response_length));
    return response;
  }

 private:
  std::vector<CString> expected_app_guids_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MockRequestSaveNoUpdate);
};

// Returns kUpdateCheckForcedFailure from Send().
class MockRequestSaveFail : public MockRequestSave {
 public:
  // Performs the send and fails with kUpdateCheckForcedFailure regardless of
  // the result.
  virtual HRESULT Send() {
    MockRequestSave::Send();
    return kUpdateCheckForcedFailure;
  }

  static const HRESULT kUpdateCheckForcedFailure = 0x81234567;
};

void VerifyUpdateCheckInRequest(const std::vector<CString>& expected_app_guids,
                                const std::vector<CString>& disabled_app_guids,
                                const CStringA& request_utf8,
                                bool is_install,
                                bool is_on_demand) {
  EXPECT_NE(0, expected_app_guids.size());
  const char* kOmahaRequestUpdate =
      "<o:app appid=\"{430FD4D0-B729-4F61-AA34-91526481799D}\" "
      "version=\"5.6.7.8\" lang=\"\" brand=\"\" client=\"\"><o:updatecheck/>"
      "<o:ping r=\"-1\"/>"
      "</o:app>";

  // The 'c' in "464c" element of the app GUID is capitalized in the request.
  const char* kAppRequestFormat=
      "<o:app appid=\"%s\" "
      "version=\"%s\" lang=\"\" brand=\"\" client=\"\"%s%s><o:updatecheck%s/>"
      "%s</o:app>";

  EXPECT_EQ(0, request_utf8.Find(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<o:gupdate xmlns:o=\"http://www.google.com/update2/request\" "
      "protocol=\"2.0\" version=\"1.2."));
  EXPECT_NE(-1, request_utf8.Find("\" ismachine=\"1\" "));
  EXPECT_NE(-1, request_utf8.Find("\" requestid=\"{"));
  EXPECT_NE(-1, request_utf8.Find("}\"><o:os platform=\"win\" version=\""));
  EXPECT_NE(-1, request_utf8.Find("\" sp=\""));
  EXPECT_NE(-1, request_utf8.Find("\"/><o:app"));

  // Verify the expected number of app elements.
  int app_element_index = request_utf8.Find("<o:app");
  int num_app_elements = 0;
  while (-1 != app_element_index) {
    num_app_elements++;
    app_element_index = request_utf8.Find("<o:app", app_element_index + 1);
  }
  EXPECT_EQ(num_app_elements, expected_app_guids.size());

  for (size_t i = 0; i < expected_app_guids.size(); ++i) {
    const CString& expected_app = expected_app_guids[i];

    bool is_disabled_expected = false;
    for (size_t j = 0; j < disabled_app_guids.size(); ++j) {
      if (expected_app == disabled_app_guids[j]) {
        is_disabled_expected = true;
        break;
      }
    }

    if (expected_app == kGoogleUpdateAppId) {
      ASSERT1(!is_on_demand && !is_install);
      EXPECT_NE(-1, request_utf8.Find(kOmahaRequestUpdate));
    } else {
      ASSERT1(expected_app == kAppGuid || expected_app == kAppGuid2);
      const CStringA app_guid = WideToAnsiDirect(expected_app == kAppGuid ?
                                                 CString(kAppGuid).MakeUpper() :
                                                 kAppGuid2);
      CStringA expected_app_element;
      expected_app_element.Format(
          kAppRequestFormat,
          app_guid,
          is_install ? "" : "1.2.3.4",
          is_install ? " installage=\"-1\"" : "",
          is_on_demand ? " installsource=\"ondemandupdate\"" : "",
          is_disabled_expected ? " updatedisabled=\"true\"" : "",
          is_install ? "" : "<o:ping r=\"-1\"/>");
      EXPECT_NE(-1, request_utf8.Find(expected_app_element)) <<
          _T("Expected: ") <<
          Utf8ToWideChar(expected_app_element.GetString(),
                         expected_app_element.GetLength()).GetString() <<
          std::endl << _T("In: ") <<
          Utf8ToWideChar(request_utf8.GetString(),
                         request_utf8.GetLength()).GetString() << std::endl;
    }
  }

  EXPECT_NE(-1, request_utf8.Find("</o:app></o:gupdate>"));

  EXPECT_FALSE(::testing::Test::HasFailure()) <<
      _T("Actual Request: ") << request_utf8;
}

}  // namespace

class WorkerJobTest : public testing::Test {
 protected:
  WorkerJobTest()
      : mock_network_request_(NULL),
        mock_encryped_request_(NULL) {
    exe_to_test_ = ConcatenatePath(
        app_util::GetCurrentModuleDirectory(),
        _T("unittest_support\\does_not_shutdown\\GoogleUpdate.exe"));
  }

  void SetIsMachine(bool is_machine) {
    worker_job_->is_machine_ = is_machine;
  }

  bool IsAppInstallWorkerRunning() {
    return goopdate_utils::IsAppInstallWorkerRunning(worker_job_->is_machine_);
  }

  PingEvent::Types GetPingType(const Request& request) {
    const AppRequest& app_request = *(request.app_requests_begin());
    const AppRequestData& app_request_data = app_request.request_data();
    const PingEvent& ping_event = *(app_request_data.ping_events_begin());
    return ping_event.event_type();
  }

  void SetWorkerJobPing(Ping* ping) {
    worker_job_->ping_.reset(ping);
  }

  // Sets WorkerJob to use MockRequestSave for all types of requests.
  void SetMockMockRequestSave() {
    SetMockRequest(new MockRequestSave, new MockRequestSave);
  }

  // Sets WorkerJob to use MockRequestSaveNoUpdate for all types of requests.
  void SetMockRequestSaveNoUpdate(
      const std::vector<CString>& expected_app_guids) {
    SetMockRequest(new MockRequestSaveNoUpdate(expected_app_guids),
                   new MockRequestSaveNoUpdate(expected_app_guids));
  }

  // Sets WorkerJob to use MockRequestSaveFail for all types of requests.
  void SetMockMockRequestSaveFail() {
    SetMockRequest(new MockRequestSaveFail, new MockRequestSaveFail);
  }

  // Sets WorkerJob to use the specified HTTP request objects.
  // Prevents multiple calls to the mock request by specifying the Config and
  // avoiding auto-detection.
  void SetMockRequest(MockRequestSave* normal_request,
                      MockRequestSave* encryped_request) {
    ASSERT1(worker_job_.get());
    const NetworkConfig::Session& session(NetworkConfig::Instance().session());
    const Config config;

    mock_network_request_ = normal_request;
    mock_encryped_request_ = encryped_request;

    worker_job_->network_request_.reset(new NetworkRequest(session));
    worker_job_->network_request_->set_network_configuration(&config);
    worker_job_->network_request_->AddHttpRequest(normal_request);

    worker_job_->network_request_encrypted_.reset(new NetworkRequest(session));
    worker_job_->network_request_encrypted_->set_network_configuration(&config);
    worker_job_->network_request_encrypted_->AddHttpRequest(
        encryped_request);
  }

  void VerifyInstallUpdateCheck(
      const std::vector<CString>& expected_app_guids) const {
    ASSERT1(mock_network_request_ && mock_encryped_request_);
    EXPECT_TRUE(mock_network_request_->is_sent());
    VerifyUpdateCheckInRequest(expected_app_guids,
                               std::vector<CString>(),
                               mock_network_request_->sent_request_utf8(),
                               true,
                               false);
    EXPECT_FALSE(mock_encryped_request_->is_sent());
  }

  void VerifyAutoUpdateCheckWithDisabledApps(
      const std::vector<CString>& expected_app_guids,
      const std::vector<CString>& disabled_app_guids) const {
    ASSERT1(mock_network_request_ && mock_encryped_request_);
    EXPECT_TRUE(mock_network_request_->is_sent());
    VerifyUpdateCheckInRequest(expected_app_guids,
                               disabled_app_guids,
                               mock_network_request_->sent_request_utf8(),
                               false,
                               false);
    EXPECT_FALSE(mock_encryped_request_->is_sent());
  }

  void VerifyAutoUpdateCheck(
      const std::vector<CString>& expected_app_guids) const {
      VerifyAutoUpdateCheckWithDisabledApps(expected_app_guids,
                                            std::vector<CString>());
  }

  void VerifyOnDemandUpdateCheck(
      const std::vector<CString>& expected_app_guids) const {
    ASSERT1(mock_network_request_ && mock_encryped_request_);
    EXPECT_TRUE(mock_network_request_->is_sent());
    VerifyUpdateCheckInRequest(expected_app_guids,
                               std::vector<CString>(),
                               mock_network_request_->sent_request_utf8(),
                               false,
                               true);
    EXPECT_FALSE(mock_encryped_request_->is_sent());
  }

  void VerifyNoUpdateCheckSent() const {
    ASSERT1(mock_network_request_ && mock_encryped_request_);
    EXPECT_FALSE(mock_network_request_->is_sent());
    EXPECT_FALSE(mock_encryped_request_->is_sent());
  }

  scoped_ptr<WorkerJob> worker_job_;
  CString exe_to_test_;
  MockRequestSave* mock_network_request_;
  MockRequestSave* mock_encryped_request_;
};

class WorkerJobRegistryProtectedTest : public WorkerJobTest {
 protected:
  WorkerJobRegistryProtectedTest()
      : hive_override_key_name_(kRegistryHiveOverrideRoot), xml_cookie_(0) {
  }

  virtual void SetUp() {
    // Registers the MSXML CoClass before doing registry redirection. Without
    // this registration, CoCreation of the DOMDocument2 within the test would
    // fail on Vista, as a side-effect of the registry redirection.
    CComPtr<IClassFactory> factory;
    EXPECT_SUCCEEDED(::CoGetClassObject(__uuidof(DOMDocument2),
                                        CLSCTX_INPROC_SERVER,
                                        NULL,
                                        IID_IClassFactory,
                                        reinterpret_cast<void**>(&factory)));
    EXPECT_SUCCEEDED(::CoRegisterClassObject(__uuidof(DOMDocument2),
                                             factory,
                                             CLSCTX_INPROC_SERVER,
                                             REGCLS_MULTIPLEUSE,
                                             &xml_cookie_));
    RegKey::DeleteKey(hive_override_key_name_, true);
    OverrideRegistryHives(hive_override_key_name_);
  }

  virtual void TearDown() {
    RestoreRegistryHives();
    EXPECT_SUCCEEDED(RegKey::DeleteKey(hive_override_key_name_, true));
    if (xml_cookie_) {
      EXPECT_SUCCEEDED(::CoRevokeClassObject(xml_cookie_));
    }
  }

  CString hive_override_key_name_;
  DWORD xml_cookie_;
};

class WorkerJobDoProcessTest : public WorkerJobRegistryProtectedTest {
 protected:
  // Must be re-entrant for statics because it is called once for each subclass.
  static void SetUpTestCase() {
    // Initialize the global metrics collection.
    stats_report::g_global_metrics.Initialize();

    if (app_guids_omaha_.empty()) {
      app_guids_omaha_.push_back(kGoogleUpdateAppId);
    }

    if (app_guids_omaha_app1_.empty()) {
      app_guids_omaha_app1_ = app_guids_omaha_;
      app_guids_omaha_app1_.push_back(kAppGuid);
    }

    if (app_guids_omaha_app2_.empty()) {
      app_guids_omaha_app2_ = app_guids_omaha_;
      app_guids_omaha_app2_.push_back(kAppGuid2);
    }

    if (app_guids_omaha_app1_app2_.empty()) {
      app_guids_omaha_app1_app2_ = app_guids_omaha_app1_;
      app_guids_omaha_app1_app2_ .push_back(kAppGuid2);
    }

    if (app_guids_app1_.empty()) {
      app_guids_app1_.push_back(kAppGuid);
    }

    if (app_guids_app2_.empty()) {
      app_guids_app2_.push_back(kAppGuid2);
    }
  }

  static void TearDownTestCase() {
    // The global metrics collection must be uninitialized before the metrics
    // destructors are called.
    stats_report::g_global_metrics.Uninitialize();
  }

  virtual void SetUp() {
    WorkerJobRegistryProtectedTest::SetUp();
#ifdef _DEBUG
    // There is an assert that expects this value to be set.
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                      kRegValueInstalledVersion,
                                      GetVersionString()));
#endif
    metric_worker_apps_not_updated_eula.Set(0);
    metric_worker_apps_not_updated_group_policy.Set(0);
    metric_worker_apps_not_installed_group_policy.Set(0);
  }

  // These tests cause the action to fail. This method validates the app request
  // for an individual app generated by
  // ping_utils::SendCompletedPingsForAllProducts().
  // The expected ping data depends on whether the WorkerJob was canceled.
  static void ValidateCompletedPingForProduct(const AppRequest& app_request,
                                              const GUID& expected_app_id,
                                              bool is_canceled) {
    const PingEvent& ping_event = GetSingleEventFromAppRequest(app_request,
                                                               expected_app_id,
                                                               true);
    EXPECT_EQ(PingEvent::EVENT_UPDATE_COMPLETE, ping_event.event_type());
    EXPECT_EQ(is_canceled ? PingEvent::EVENT_RESULT_CANCELLED :
                            PingEvent::EVENT_RESULT_ERROR,
              ping_event.event_result());
    EXPECT_EQ(is_canceled ? GOOPDATE_E_WORKER_CANCELLED :
                            MockRequestSaveFail::kUpdateCheckForcedFailure,
              ping_event.error_code());
    EXPECT_EQ(0x100000ff, ping_event.extra_code1());

    EXPECT_EQ(::IsEqualGUID(kGoopdateGuid, expected_app_id) ? _T("5.6.7.8") :
                                                              _T("1.2.3.4"),
              ping_event.previous_version());
  }

  // TODO(omaha): I think this is a bug that we ping without an event.
  // If not, fix this and replace this method with a check for no pings.
  static void ValidateNoPingEventForProduct(const AppRequest& app_request,
                                            const GUID& expected_app_id) {
    const AppRequestData& app_request_data = app_request.request_data();

    const AppData& app_data = app_request_data.app_data();
    EXPECT_TRUE(::IsEqualGUID(expected_app_id, app_data.app_guid()));
    EXPECT_TRUE(::IsEqualGUID(GUID_NULL, app_data.parent_app_guid()));
    EXPECT_EQ(true, app_data.is_machine_app());
    EXPECT_TRUE(!app_data.version().IsEmpty());
    EXPECT_TRUE(!app_data.previous_version().IsEmpty());

    EXPECT_EQ(0, app_request_data.num_ping_events());
  }

  static void ValidateForcedFailureObserved(
      const JobObserverMock& job_observer) {
    EXPECT_EQ(COMPLETION_CODE_ERROR, job_observer.completion_code);
    EXPECT_STREQ(_T("Installation failed. Please try again. Error code = ")
                 _T("0x81234567"), job_observer.completion_text);
    EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
              job_observer.completion_error_code);
  }

  static void ValidateNoEventObserved(
      const JobObserverMock& job_observer) {
    EXPECT_EQ(static_cast<CompletionCodes>(-1), job_observer.completion_code);
    EXPECT_TRUE(job_observer.completion_text.IsEmpty());
    EXPECT_EQ(0, job_observer.completion_error_code);
  }

  // Only applies to installs.
  static void ValidateNoUpdateErrorObserved(
      const JobObserverMock& job_observer) {
    EXPECT_EQ(COMPLETION_CODE_ERROR, job_observer.completion_code);
    EXPECT_STREQ(_T("Installation failed. Please try again. Error code = ")
                 _T("0x80040809"), job_observer.completion_text);
    EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE,
              job_observer.completion_error_code);
  }

  // JobObserverCOMDecorator does not pass along the text and the COM interface
  // does not support passing the error code so the COM mock sets E_UNEXPECTED.
  static void ValidateComFailureObserved(
      const JobObserverMock& job_observer) {
    EXPECT_EQ(COMPLETION_CODE_ERROR, job_observer.completion_code);
    EXPECT_TRUE(job_observer.completion_text.IsEmpty());
    EXPECT_EQ(E_UNEXPECTED, job_observer.completion_error_code);
  }

  static void ValidateComSuccessObserved(
      const JobObserverMock& job_observer) {
    EXPECT_EQ(COMPLETION_CODE_SUCCESS, job_observer.completion_code);
    EXPECT_TRUE(job_observer.completion_text.IsEmpty());
    EXPECT_EQ(E_UNEXPECTED, job_observer.completion_error_code);
  }

  const std::vector<CString>& app_guids_omaha() const {
      return app_guids_omaha_;
  }
  const std::vector<CString>& app_guids_omaha_app1() const {
      return app_guids_omaha_app1_;
  }
  const std::vector<CString>& app_guids_omaha_app2() const {
      return app_guids_omaha_app2_;
  }
  const std::vector<CString>& app_guids_omaha_app1_app2() const {
      return app_guids_omaha_app1_app2_;
  }
  const std::vector<CString>& app_guids_app1() const { return app_guids_app1_; }
  const std::vector<CString>& app_guids_app2() const { return app_guids_app2_; }

 private:
  static std::vector<CString> app_guids_omaha_;
  static std::vector<CString> app_guids_omaha_app1_;
  static std::vector<CString> app_guids_omaha_app2_;
  static std::vector<CString> app_guids_omaha_app1_app2_;
  static std::vector<CString> app_guids_app1_;
  static std::vector<CString> app_guids_app2_;
};

std::vector<CString> WorkerJobDoProcessTest::app_guids_omaha_;
std::vector<CString> WorkerJobDoProcessTest::app_guids_omaha_app1_;
std::vector<CString> WorkerJobDoProcessTest::app_guids_omaha_app2_;
std::vector<CString> WorkerJobDoProcessTest::app_guids_omaha_app1_app2_;
std::vector<CString> WorkerJobDoProcessTest::app_guids_app1_;
std::vector<CString> WorkerJobDoProcessTest::app_guids_app2_;

class WorkerJobDoProcessUpdateMachineTest : public WorkerJobDoProcessTest {
 protected:
  virtual void SetUp() {
    WorkerJobDoProcessTest::SetUp();

    // Omaha is always registered.
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS_GOOPDATE,
                                      _T("pv"),
                                      _T("5.6.7.8")));
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE_GOOPDATE,
                                      _T("pv"),
                                      _T("5.6.7.8")));

    // Register the product to check for updates.
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID,
                                      _T("pv"),
                                      _T("1.2.3.4")));
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                                      _T("pv"),
                                      _T("1.2.3.4")));

    args_.mode = COMMANDLINE_MODE_UA;
    worker_job_.reset(
        WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));

    ping_mock_ = new PingMock;
    SetWorkerJobPing(ping_mock_);
  }

  // These tests cause the action to fail. This method validates the ping
  // generated by ping_utils::SendCompletedPingsForAllProducts().
  void ValidateCompletedPingsForAllProducts(const Request& ping_request,
                                            bool is_canceled) {
    EXPECT_TRUE(ping_request.is_machine());
    EXPECT_EQ(2, ping_request.get_request_count());

    AppRequestVector::const_iterator iter = ping_request.app_requests_begin();
    ValidateCompletedPingForProduct(*iter, StringToGuid(kAppGuid), is_canceled);

    ++iter;
    ValidateCompletedPingForProduct(*iter, kGoopdateGuid, is_canceled);
    StringToGuid(kAppGuid);
  }

  // TODO(omaha): I think this is a bug that we ping without an event.
  // If not, fix this and replace this method with a check for no pings.
  void ValidateNoPingEventForAllProducts(const Request& ping_request) {
    EXPECT_TRUE(ping_request.is_machine());
    EXPECT_EQ(2, ping_request.get_request_count());

    AppRequestVector::const_iterator iter = ping_request.app_requests_begin();
    ValidateNoPingEventForProduct(*iter, StringToGuid(kAppGuid));

    ++iter;
    ValidateNoPingEventForProduct(*iter, kGoopdateGuid);
    StringToGuid(kAppGuid);
  }

  CommandLineArgs args_;
  JobObserverMock job_observer_;
  PingMock* ping_mock_;
};

class WorkerJobDoProcessInstallMachineTest : public WorkerJobDoProcessTest {
 protected:
  virtual void SetUp() {
    WorkerJobDoProcessTest::SetUp();

    args_.is_silent_set = true;  // Prevent browser from launching on errors.
    args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
    args_.extra.apps.push_back(CommandLineAppArgs());
    args_.extra.apps[0].app_guid = StringToGuid(kAppGuid);
    args_.extra.apps[0].app_name = _T("Foo Bar");
    args_.extra.apps[0].needs_admin = true;

    worker_job_.reset(
        WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  }

  CommandLineArgs args_;
  JobObserverMock job_observer_;
};

class WorkerJobDoProcessInstallGoogleUpdateMachineTest
    : public WorkerJobDoProcessTest {
 protected:
  virtual void SetUp() {
    WorkerJobDoProcessTest::SetUp();

    args_.is_silent_set = true;  // Prevent browser from launching on errors.
    args_.mode = COMMANDLINE_MODE_IG;
    args_.extra.apps.push_back(CommandLineAppArgs());
    args_.extra.apps[0].app_guid = StringToGuid(kAppGuid);
    args_.extra.apps[0].needs_admin = true;

    AppData app_data;
    app_data.set_app_guid(StringToGuid(kAppGuid));
    app_data.set_is_machine_app(true);
    products_.push_back(ProductData(app_data));

    worker_job_.reset(
        WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  }

  CommandLineArgs args_;
  ProductDataVector products_;
  JobObserverMock job_observer_;
};

class WorkerJobDoProcessOnDemandUpdateMachineTest
    : public WorkerJobDoProcessTest {
 protected:
  virtual void SetUp() {
    WorkerJobDoProcessTest::SetUp();

    // Register the product as required by OnDemandUpdateStrategy::Init().
    ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID,
                                      _T("pv"),
                                      _T("1.2.3.4")));

    HRESULT hr = CComObject<IJobObserverMock>::CreateInstance(&job_observer_);
    ASSERT_EQ(S_OK, hr);
    job_holder_ = job_observer_;

    worker_job_.reset();
    ASSERT_SUCCEEDED(WorkerJobFactory::CreateOnDemandWorkerJob(
        true,   // is_machine
        false,  // is_update_check_only
        _T("en"),
        StringToGuid(kAppGuid),
        job_holder_,
        new WorkerComWrapperShutdownCallBack(false),
        address(worker_job_)));
  }

  CComObject<IJobObserverMock>* job_observer_;
  CComPtr<IJobObserver> job_holder_;
};

class WorkerJobIsAppInstallWorkerRunningTest : public WorkerJobTest {
 protected:
  WorkerJobIsAppInstallWorkerRunningTest() {
    args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
    CommandLineAppArgs app_args;
    app_args.app_name = _T("WorkerJobIsAppInstallWorkerRunningTest");
    args_.extra.apps.push_back(app_args);
  }

  void TestIsAppInstallWorkerRunning(const CString& cmd_line,
                                     bool is_machine,
                                     bool expected_running) {
    Process p(exe_to_test_, NULL);
    ASSERT_TRUE(p.Start(cmd_line));

    // Wait for the process to be ready. IsAppInstallWorkerRunning uses
    // Process::GetCommandLine, which fails if it cannot ::ReadProcessMemory().
    // Waiting for GetCommandLine() to succeed should ensure that the process is
    // sufficiently initialized for this test.
    // TODO(omaha): If we change to using Job Objects, we will not need this
    // if we use ::AssignProcessToJobObject() from this test.
    HRESULT hr = E_FAIL;
    CString process_cmd;
    for (int tries = 0; tries < 100 && FAILED(hr); ++tries) {
      ::Sleep(50);
      hr = Process::GetCommandLine(p.GetId(), &process_cmd);
    }
    EXPECT_SUCCEEDED(hr);

    SetIsMachine(is_machine);
    EXPECT_EQ(expected_running, IsAppInstallWorkerRunning());
    EXPECT_TRUE(p.Terminate(1000));
  }

  CommandLineArgs args_;
  CString cmd_line_;
  JobObserverMock job_observer_;
};

// TODO(omaha): Test all methods of WorkerJob.

TEST_F(WorkerJobDoProcessTest, OnDemandUpdate_AppNotRegistered) {
  CComObject<IJobObserverMock>* job_observer;
  CComPtr<IJobObserver> job_holder;

  HRESULT hr = CComObject<IJobObserverMock>::CreateInstance(&job_observer);
  ASSERT_EQ(S_OK, hr);
  job_holder = job_observer;

  worker_job_.reset();
  EXPECT_EQ(GOOPDATE_E_APP_NOT_REGISTERED,
      WorkerJobFactory::CreateOnDemandWorkerJob(
      true,   // is_machine
      false,  // is_update_check_only
      _T("en"),
      StringToGuid(kAppGuid),
      job_holder,
      new WorkerComWrapperShutdownCallBack(false),
      address(worker_job_)));
}

//
// Update apps tests.
//

// Also tests that the OemInstallPing is not sent.
TEST_F(WorkerJobDoProcessUpdateMachineTest, UpdateCheckFails) {
  SetMockMockRequestSaveFail();

  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->DoProcess());
  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->error_code());

  ValidateForcedFailureObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateCompletedPingsForAllProducts(*ping_mock_->ping_requests()[0], false);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// Also tests that the OemInstallPing is not sent.
TEST_F(WorkerJobDoProcessUpdateMachineTest, GroupPolicy_NoPolicy) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());
  ValidateNoEventObserved(job_observer_);

  // TODO(omaha): I don't think there should be any pings.
  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, GroupPolicy_InstallProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());
  ValidateNoEventObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest,
       GroupPolicy_UpdateProhibitedForSingleApp) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_omaha_app1(),
                                        app_guids_app1());
  ValidateNoEventObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest,
       GroupPolicy_ManualUpdateOnlyForSingleApp) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 2));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_omaha_app1(),
                                        app_guids_app1());
  ValidateNoEventObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// Use the default behavior when the value is not supported.
TEST_F(WorkerJobDoProcessUpdateMachineTest, GroupPolicy_InvalidUpdateValue) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 3));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());
  ValidateNoEventObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// Omaha updates will always be performed.
TEST_F(WorkerJobDoProcessUpdateMachineTest,
       GroupPolicy_UpdateProhibitedForOmahaAndAllApps) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyGoopdate, 0));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_omaha_app1(),
                                        app_guids_app1());
  ValidateNoEventObserved(job_observer_);

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// This case should not happen because Omaha is not registered.
TEST_F(WorkerJobDoProcessUpdateMachineTest,
       GroupPolicy_UpdateProhibitedForAllRegisteredApps) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  ASSERT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_CLIENTS_GOOPDATE, _T("pv")));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_app1(), app_guids_app1());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  const Request& ping_request = *ping_mock_->ping_requests()[0];
  EXPECT_TRUE(ping_request.is_machine());
  EXPECT_EQ(1, ping_request.get_request_count());
  ValidateNoPingEventForProduct(*ping_request.app_requests_begin(),
                                StringToGuid(kAppGuid));

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// This case should not happen because Omaha is not registered.
TEST_F(WorkerJobDoProcessUpdateMachineTest,
       GroupPolicy_UpdateProhibitedForAllAppsByPolicyOrEula) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  ASSERT_SUCCEEDED(RegKey::DeleteValue(MACHINE_REG_CLIENTS_GOOPDATE, _T("pv")));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp2,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_app1(), app_guids_app1());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  const Request& ping_request = *ping_mock_->ping_requests()[0];
  EXPECT_TRUE(ping_request.is_machine());
  EXPECT_EQ(1, ping_request.get_request_count());
  ValidateNoPingEventForProduct(*ping_request.app_requests_begin(),
                                StringToGuid(kAppGuid));

  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// Although a ping request is received, the real Ping object would not send it.
TEST_F(WorkerJobDoProcessUpdateMachineTest, GoogleUpdateEulaNotAccepted) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_UPDATE,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_EQ(GOOPDATE_E_CANNOT_USE_NETWORK, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_CANNOT_USE_NETWORK, worker_job_->error_code());

  VerifyNoUpdateCheckSent();

  ASSERT_EQ(1, ping_mock_->ping_requests().size());

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, AppEulaNotAccepted) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp2,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  // Update checks are sent for Omaha and App1 but not App2.
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, AppEulaAcceptedInClientState) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest,
       AppEulaNotAcceptedInClientStateButIsInClientStateMedium) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStateMediumPathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(1)));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[0]);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// EULA is checked first.
TEST_F(WorkerJobDoProcessUpdateMachineTest,
       AppEulaNotAcceptedAndUpdateProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_omaha());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());

  // Only Omaha is in the completed ping because the app is disabled by EULA.
  const Request& ping_request = *ping_mock_->ping_requests()[0];
  EXPECT_TRUE(ping_request.is_machine());
  EXPECT_EQ(1, ping_request.get_request_count());
  ValidateNoPingEventForProduct(*ping_request.app_requests_begin(),
                                kGoopdateGuid);

  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest,
       AppEulaNotAcceptedOtherAppUpdateProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app2());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp2, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheckWithDisabledApps(app_guids_omaha_app2(),
                                        app_guids_app2());

  ASSERT_EQ(1, ping_mock_->ping_requests().size());

  // Only Omaha is in the completed ping because both apps are disabled.
  const Request& ping_request = *ping_mock_->ping_requests()[0];
  EXPECT_TRUE(ping_request.is_machine());
  EXPECT_EQ(2, ping_request.get_request_count());
  ValidateNoPingEventForProduct(*ping_request.app_requests_begin(),
                                kGoopdateGuid);
  ValidateNoPingEventForProduct(*(++ping_request.app_requests_begin()),
                                StringToGuid(kAppGuid2));

  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, OemInstallPing) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                                    _T("oeminstall"),
                                    _T("1")));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());

  ASSERT_EQ(2, ping_mock_->ping_requests().size());

  const PingEvent& ping_event = GetSingleEventFromRequest(
      *ping_mock_->ping_requests()[0],
      StringToGuid(kAppGuid),
      true);
  EXPECT_EQ(PingEvent::EVENT_INSTALL_OEM_FIRST_CHECK, ping_event.event_type());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, ping_event.event_result());
  EXPECT_EQ(0, ping_event.error_code());
  EXPECT_EQ(0, ping_event.extra_code1());
  EXPECT_EQ(_T("1.2.3.4"), ping_event.previous_version());

  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[1]);

  EXPECT_FALSE(RegKey::HasValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                                _T("oeminstall")));
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, OemInstallPing_Failed) {
  SetMockRequestSaveNoUpdate(app_guids_omaha_app1());
  ping_mock_ = new PingMockFail();
  SetWorkerJobPing(ping_mock_);

  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                                    _T("oeminstall"),
                                    _T("1")));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyAutoUpdateCheck(app_guids_omaha_app1());

  ASSERT_EQ(2, ping_mock_->ping_requests().size());

  const PingEvent& ping_event = GetSingleEventFromRequest(
      *ping_mock_->ping_requests()[0],
      StringToGuid(kAppGuid),
      true);
  EXPECT_EQ(PingEvent::EVENT_INSTALL_OEM_FIRST_CHECK, ping_event.event_type());
  EXPECT_EQ(PingEvent::EVENT_RESULT_SUCCESS, ping_event.event_result());
  EXPECT_EQ(0, ping_event.error_code());
  EXPECT_EQ(0, ping_event.extra_code1());
  EXPECT_EQ(_T("1.2.3.4"), ping_event.previous_version());

  ValidateNoPingEventForAllProducts(*ping_mock_->ping_requests()[1]);

  EXPECT_TRUE(RegKey::HasValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                               _T("oeminstall")));
}

TEST_F(WorkerJobDoProcessUpdateMachineTest, OemInstallPing_WorkerJobCanceled) {
  SetMockMockRequestSave();
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                                    _T("oeminstall"),
                                    _T("1")));
  worker_job_->Cancel();

  EXPECT_EQ(GOOPDATE_E_WORKER_CANCELLED, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_WORKER_CANCELLED, worker_job_->error_code());

  VerifyNoUpdateCheckSent();

  ASSERT_EQ(1, ping_mock_->ping_requests().size());
  ValidateCompletedPingsForAllProducts(*ping_mock_->ping_requests()[0], true);

  EXPECT_TRUE(RegKey::HasValue(MACHINE_REG_CLIENT_STATE APP_GUID,
                               _T("oeminstall")));
}

//
// Install tests.
//

TEST_F(WorkerJobDoProcessInstallMachineTest, UpdateCheckFails) {
  SetMockMockRequestSaveFail();

  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->DoProcess());
  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->error_code());

  ValidateForcedFailureObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest, GroupPolicy_NoPolicy) {
  SetMockRequestSaveNoUpdate(app_guids_app1());

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest, GroupPolicy_InstallProhibited) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->error_code());

  EXPECT_EQ(COMPLETION_CODE_ERROR, job_observer_.completion_code);
  EXPECT_STREQ(_T("Your network administrator has applied a Group Policy that ")
               _T("prevents installation of Foo Bar."),
               job_observer_.completion_text);
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            job_observer_.completion_error_code);

  VerifyNoUpdateCheckSent();
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

// Use the default behavior when the value is not supported.
TEST_F(WorkerJobDoProcessInstallMachineTest, GroupPolicy_InvalidInstallValue) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 2));

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// Tests which app name is displayed.
TEST_F(WorkerJobDoProcessInstallMachineTest,
       GroupPolicy_InstallThreeAppsLastTwoProhibited) {
  SetMockMockRequestSave();
  args_.extra.apps.push_back(CommandLineAppArgs());
  args_.extra.apps[1].app_guid = StringToGuid(kAppGuid2);
  args_.extra.apps[1].app_name = _T("App 2");
  args_.extra.apps[1].needs_admin = true;
  args_.extra.apps.push_back(CommandLineAppArgs());
  args_.extra.apps[2].app_guid = StringToGuid(kAppGuid3);
  args_.extra.apps[2].app_name = _T("Third App");
  args_.extra.apps[2].needs_admin = true;

  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp2, 0));
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp3, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();

  EXPECT_EQ(COMPLETION_CODE_ERROR, job_observer_.completion_code);
  EXPECT_STREQ(_T("Your network administrator has applied a Group Policy that ")
               _T("prevents installation of App 2."),
               job_observer_.completion_text);
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            job_observer_.completion_error_code);

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(2, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest, GroupPolicy_UpdateProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest,
       GroupPolicy_InstallAndUpdateProhibited) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest,
       GroupPolicy_InstallOfDifferentAppProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp2, 0));

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest,
       GroupPolicy_InstallTwoAppsOneProhibited) {
  SetMockMockRequestSave();
  args_.extra.apps.push_back(CommandLineAppArgs());
  args_.extra.apps[1].app_guid = StringToGuid(kAppGuid2);
  args_.extra.apps[1].app_name = _T("App 2");
  args_.extra.apps[1].needs_admin = true;

  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest, AppEulaNotAccepted) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest,
       AppEulaNotAcceptedAndInstallProhibited) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallMachineTest,
       AppEulaNotAcceptedAndOtherAppInstallProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp2, 0));

  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_NO_UPDATE_RESPONSE, worker_job_->error_code());

  VerifyInstallUpdateCheck(app_guids_app1());
  ValidateNoUpdateErrorObserved(job_observer_);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

//
// Install Google Update and app tests.
//
// We cannot run DoProcess for the InstallGoopdateAndAppsStrategy case because
// it attempts to install Google Update before checking the Group Policy.
// Therefore, test RemoveDisallowedApps() directly.

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest, GroupPolicy_NoPolicy) {
  EXPECT_SUCCEEDED(worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_EQ(1, products_.size());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest,
       GroupPolicy_InstallProhibited) {
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_TRUE(products_.empty());

  EXPECT_SUCCEEDED(worker_job_->error_code()) <<
      _T("error_code code does not run.");

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest,
       GroupPolicy_UpdateProhibited) {
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_EQ(1, products_.size());

  EXPECT_SUCCEEDED(worker_job_->error_code()) <<
      _T("error_code code does not run.");

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest,
       GroupPolicy_InstallAndUpdateProhibited) {
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_TRUE(products_.empty());

  EXPECT_SUCCEEDED(worker_job_->error_code()) <<
      _T("error_code code does not run.");

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest,
       GroupPolicy_InstallOfDifferentAppProhibited) {
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp2, 0));

  EXPECT_SUCCEEDED(worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_EQ(1, products_.size());

  EXPECT_SUCCEEDED(worker_job_->error_code()) <<
      _T("error_code code does not run.");

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessInstallGoogleUpdateMachineTest,
       GroupPolicy_InstallTwoAppsOneProhibited) {
  args_.extra.apps.push_back(CommandLineAppArgs());
  args_.extra.apps[1].app_guid = StringToGuid(kAppGuid2);
  args_.extra.apps[1].app_name = _T("App 2");
  args_.extra.apps[1].needs_admin = true;

  AppData app_data;
  app_data.set_app_guid(StringToGuid(kAppGuid2));
  app_data.set_is_machine_app(true);
  products_.push_back(ProductData(app_data));
  ASSERT_EQ(2, products_.size());

  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_INSTALL_DISABLED_BY_POLICY,
            worker_job_->strategy()->RemoveDisallowedApps(&products_));
  EXPECT_EQ(1, products_.size());  // One of two was removed.

  EXPECT_SUCCEEDED(worker_job_->error_code()) <<
      _T("error_code code does not run.");

  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(1, metric_worker_apps_not_installed_group_policy.value());
}

//
// On-demand update tests.
//

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest, UpdateCheckFails) {
  SetMockMockRequestSaveFail();

  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->DoProcess());
  EXPECT_EQ(MockRequestSaveFail::kUpdateCheckForcedFailure,
            worker_job_->error_code());

  ValidateComFailureObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest, GroupPolicy_NoPolicy) {
  SetMockRequestSaveNoUpdate(app_guids_app1());

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyOnDemandUpdateCheck(app_guids_app1());
  ValidateComSuccessObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       GroupPolicy_InstallProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyOnDemandUpdateCheck(app_guids_app1());
  ValidateComSuccessObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       GroupPolicy_UpdateProhibited) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  ValidateComFailureObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       GroupPolicy_ManualUpdateOnly) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 2));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyOnDemandUpdateCheck(app_guids_app1());
  ValidateComSuccessObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       GroupPolicy_InstallAndUpdateProhibited) {
  SetMockMockRequestSave();
  EXPECT_SUCCEEDED(SetPolicy(kInstallPolicyApp, 0));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY, worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  ValidateComFailureObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(1, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       GroupPolicy_UpdateOfDifferentAppProhibited) {
  SetMockRequestSaveNoUpdate(app_guids_app1());
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp2, 0));

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyOnDemandUpdateCheck(app_guids_app1());
  ValidateComSuccessObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest, AppEulaNotAccepted) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  worker_job_.reset();
  EXPECT_SUCCEEDED(WorkerJobFactory::CreateOnDemandWorkerJob(
      true,   // is_machine
      false,  // is_update_check_only
      _T("en"),
      StringToGuid(kAppGuid),
      job_holder_,
      new WorkerComWrapperShutdownCallBack(false),
      address(worker_job_)));
  SetMockMockRequestSave();

  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  ValidateComFailureObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest, OtherAppEulaNotAccepted) {
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENTS APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  ASSERT_SUCCEEDED(RegKey::SetValue(MACHINE_REG_CLIENT_STATE APP_GUID2,
                                    _T("pv"),
                                    _T("1.2.3.4")));
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp2,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));

  worker_job_.reset();
  EXPECT_SUCCEEDED(WorkerJobFactory::CreateOnDemandWorkerJob(
      true,   // is_machine
      false,  // is_update_check_only
      _T("en"),
      StringToGuid(kAppGuid),
      job_holder_,
      new WorkerComWrapperShutdownCallBack(false),
      address(worker_job_)));
  SetMockRequestSaveNoUpdate(app_guids_app1());

  EXPECT_SUCCEEDED(worker_job_->DoProcess());
  EXPECT_SUCCEEDED(worker_job_->error_code());

  VerifyOnDemandUpdateCheck(app_guids_app1());
  ValidateComSuccessObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(0, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// EULA is checked first.
TEST_F(WorkerJobDoProcessOnDemandUpdateMachineTest,
       AppEulaNotAcceptedAndUpdateProhibited) {
  EXPECT_SUCCEEDED(RegKey::SetValue(kMachineClientStatePathApp,
                                    _T("eulaaccepted"),
                                    static_cast<DWORD>(0)));
  EXPECT_SUCCEEDED(SetPolicy(kUpdatePolicyApp, 0));

  worker_job_.reset();
  EXPECT_SUCCEEDED(WorkerJobFactory::CreateOnDemandWorkerJob(
      true,   // is_machine
      false,  // is_update_check_only
      _T("en"),
      StringToGuid(kAppGuid),
      job_holder_,
      new WorkerComWrapperShutdownCallBack(false),
      address(worker_job_)));
  SetMockMockRequestSave();

  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            worker_job_->DoProcess());
  EXPECT_EQ(GOOPDATE_E_APP_UPDATE_DISABLED_EULA_NOT_ACCEPTED,
            worker_job_->error_code());

  VerifyNoUpdateCheckSent();
  ValidateComFailureObserved(job_observer_->job_observer_mock);
  EXPECT_EQ(1, metric_worker_apps_not_updated_eula.value());
  EXPECT_EQ(0, metric_worker_apps_not_updated_group_policy.value());
  EXPECT_EQ(0, metric_worker_apps_not_installed_group_policy.value());
}

// TODO(omaha): Add some user tests for the above.

// Create a worker with the /handoff switch and needs_admin=false.
// Call IsAppInstallWorkerRunning for machine. Should return false.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, HandoffNeedsAdminFalseMachine) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), handoff_cmd_line, false_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, true, false);
}

// Create a worker with the /handoff switch and needs_admin=true.
// Call IsAppInstallWorkerRunning for machine. Should return true.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, HandoffNeedsAdminTrueMachine) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), handoff_cmd_line, true_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, true, true);
}

// Create a worker with the /handoff switch and needs_admin=false.
// Call IsAppInstallWorkerRunning for user. Should return true.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, HandoffNeedsAdminFalseUser) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(false, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), handoff_cmd_line, false_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, false, true);
}

// Create a worker with the /handoff switch and needs_admin=true.
// Call IsAppInstallWorkerRunning for user. Should return false.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, HandoffNeedsAdminTrueUser) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(false, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), handoff_cmd_line, true_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, false, false);
}

// Create a worker with the /ig switch and needs_admin=false.
// Call IsAppInstallWorkerRunning for machine. Should return false.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, IgNeedsAdminFalseMachine) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), finish_setup_cmd_line, false_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, true, false);
}

// Create a worker with the /ig switch and needs_admin=true.
// Call IsAppInstallWorkerRunning for machine. Should return true.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, IgNeedsAdminTrueMachine) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), finish_setup_cmd_line, true_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, true, true);
}

// Create a worker with the /ig switch and needs_admin=false.
// Call IsAppInstallWorkerRunning for user. Should return true.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, IgNeedsAdminFalseUser) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(false, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), finish_setup_cmd_line, false_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, false, true);
}

// Create a worker with the /ig switch and needs_admin=true.
// Call IsAppInstallWorkerRunning for user. Should return false.
TEST_F(WorkerJobIsAppInstallWorkerRunningTest, IgNeedsAdminTrueUser) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(false, args_, &job_observer_));
  cmd_line_.Format(_T("%s %s"), finish_setup_cmd_line, true_extra_args);
  TestIsAppInstallWorkerRunning(cmd_line_, false, false);
}

TEST_F(WorkerJobIsAppInstallWorkerRunningTest, NoArgsUser) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(false, args_, &job_observer_));
  TestIsAppInstallWorkerRunning(_T(""), false, false);
}

TEST_F(WorkerJobIsAppInstallWorkerRunningTest, NoArgsMachine) {
  args_.mode = COMMANDLINE_MODE_HANDOFF_INSTALL;
  worker_job_.reset(
      WorkerJobFactory::CreateWorkerJob(true, args_, &job_observer_));
  TestIsAppInstallWorkerRunning(_T(""), true, false);
}

TEST_F(WorkerJobRegistryProtectedTest,
       BuildUninstallPing_User_NoUninstalledApp) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(USER_REG_CLIENT_STATE));

  scoped_ptr<Request> uninstall_ping;
  EXPECT_HRESULT_SUCCEEDED(BuildUninstallPing(false, address(uninstall_ping)));
  EXPECT_EQ(0, uninstall_ping->get_request_count());
}

// Create a mock client state for one app and build an uninstall ping.
// Expect the client state to be cleared at the end.
TEST_F(WorkerJobRegistryProtectedTest, BuildUninstallPing_User_UninstalledApp) {
  CString client_state_appkey(USER_REG_CLIENT_STATE);
  client_state_appkey.Append(_T("{C78D67E2-D7E9-4b62-9869-FCDCFC4C9323}"));
  EXPECT_HRESULT_SUCCEEDED(
    RegKey::SetValue(client_state_appkey, kRegValueProductVersion, _T("1.0")));

  scoped_ptr<Request> uninstall_ping;
  EXPECT_HRESULT_SUCCEEDED(BuildUninstallPing(false, address(uninstall_ping)));
  EXPECT_EQ(1, uninstall_ping->get_request_count());
  EXPECT_EQ(PingEvent::EVENT_UNINSTALL, GetPingType(*uninstall_ping));
  EXPECT_FALSE(RegKey::HasKey(client_state_appkey));
}

TEST_F(WorkerJobRegistryProtectedTest,
       BuildUninstallPing_Machine_NoUninstalledApp) {
  ASSERT_SUCCEEDED(RegKey::CreateKey(MACHINE_REG_CLIENT_STATE));

  scoped_ptr<Request> uninstall_ping;
  EXPECT_HRESULT_SUCCEEDED(BuildUninstallPing(true, address(uninstall_ping)));
  EXPECT_EQ(0, uninstall_ping->get_request_count());
}

// Create a mock client state for one app and build an uninstall ping.
// Expect the client state to be cleared at the end.
TEST_F(WorkerJobRegistryProtectedTest,
       BuildUninstallPing_Machine_UninstalledApp) {
  CString client_state_appkey = MACHINE_REG_CLIENT_STATE;
  client_state_appkey.Append(_T("{C78D67E2-D7E9-4b62-9869-FCDCFC4C9323}"));
  EXPECT_HRESULT_SUCCEEDED(
    RegKey::SetValue(client_state_appkey, kRegValueProductVersion, _T("1.0")));

  scoped_ptr<Request> uninstall_ping;
  EXPECT_HRESULT_SUCCEEDED(BuildUninstallPing(true, address(uninstall_ping)));
  EXPECT_EQ(1, uninstall_ping->get_request_count());
  EXPECT_EQ(PingEvent::EVENT_UNINSTALL, GetPingType(*uninstall_ping));
  EXPECT_FALSE(RegKey::HasKey(client_state_appkey));
}

}  // namespace omaha
