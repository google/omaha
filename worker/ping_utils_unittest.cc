// Copyright 2008-2009 Google Inc.
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

#include <windows.h>
#include <objbase.h>
#include "base/scoped_ptr.h"
#include "omaha/common/utils.h"
#include "omaha/testing/unit_test.h"
#include "omaha/goopdate/command_line.h"
#include "omaha/worker/application_data.h"
#include "omaha/worker/job.h"
#include "omaha/worker/ping.h"
#include "omaha/worker/ping_utils.h"
#include "omaha/worker/product_data.h"

namespace omaha {

class PingUtilsTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ping_.reset(new Ping);
  }

  scoped_ptr<Ping> ping_;
};

TEST_F(PingUtilsTest, SendGoopdatePing) {
  CommandLineExtraArgs extra_args;

  // Test with no language.
  EXPECT_SUCCEEDED(
      ping_utils::SendGoopdatePing(false,
                                   extra_args,
                                   _T(""),
                                   PingEvent::EVENT_SETUP_INSTALL_FAILURE,
                                   S_OK,
                                   0,
                                   NULL,
                                   ping_.get()));

  // Test with a language.
  extra_args.language = _T("en");
  EXPECT_SUCCEEDED(
      ping_utils::SendGoopdatePing(false,
                                   extra_args,
                                   _T(""),
                                   PingEvent::EVENT_SETUP_INSTALL_FAILURE,
                                   S_OK,
                                   0,
                                   NULL,
                                   ping_.get()));

  // Test with additional data.
  CommandLineAppArgs extra;
  extra_args.installation_id = StringToGuid(
      _T("{98CEC468-9429-4984-AEDE-4F53C6A14869}"));
  extra_args.language = _T("de");
  extra_args.brand_code = _T("g00g");
  extra_args.client_id = _T("_some_partner");
  extra_args.browser_type = BROWSER_IE;
  extra_args.usage_stats_enable = TRISTATE_TRUE;
  extra_args.apps.push_back(extra);
  EXPECT_SUCCEEDED(ping_utils::SendGoopdatePing(
                       true,
                       extra_args,
                       _T("sourcefoo"),
                       PingEvent::EVENT_SETUP_UPDATE_FAILURE,
                       E_FAIL,
                       1234567890,
                       NULL,
                       ping_.get()));
}

TEST_F(PingUtilsTest, SendCompletedPingsForAllProducts_EmptyProducts) {
  ProductDataVector products;
  CompletionInfo info(COMPLETION_ERROR, 10, _T("test"));

  ASSERT_HRESULT_SUCCEEDED(ping_utils::SendCompletedPingsForAllProducts(
      products,
      false,
      false,
      info,
      ping_.get()));
}

TEST_F(PingUtilsTest, SendCompletedPingsForAllProducts) {
  ProductDataVector products;

  AppData data1(StringToGuid(_T("{E66F2139-5469-BAAD-AC99-7863798E3A0A}")),
                false);
  data1.set_version(_T("1.1.1.1"));
  data1.set_previous_version(_T("1.0.0.0"));
  data1.set_language(_T("en"));

  ProductData product_data1;
  product_data1.set_app_data(data1);
  products.push_back(product_data1);

  AppData data2(StringToGuid(_T("{E66F3140-5179-41ec-BAAD-7863798E3A0A}")),
                false);
  data2.set_version(_T("1.1.1.1"));
  data2.set_previous_version(_T("1.0.0.0"));
  data2.set_language(_T("de"));

  ProductData product_data2;
  product_data2.set_app_data(data2);
  products.push_back(product_data2);

  CompletionInfo info(COMPLETION_ERROR, 10, _T("test"));
  ASSERT_HRESULT_SUCCEEDED(ping_utils::SendCompletedPingsForAllProducts(
      products,
      false,
      false,
      info,
      ping_.get()));
}

void ValidateRequest(const ProductDataVector& products,
                     const CompletionInfo& expected_info,
                     const PingEvent::Types expected_type,
                     const Request& actual_request) {
  EXPECT_EQ(2, actual_request.get_request_count());
  AppRequestVector::const_iterator iter = actual_request.app_requests_begin();
  for (int i = 0; iter != actual_request.app_requests_end(); ++iter, ++i) {
    const AppRequest& app_request = *iter;
    const AppRequestData& app_request_data = app_request.request_data();

    EXPECT_EQ(1, app_request_data.num_ping_events());
    EXPECT_STREQ(GuidToString(app_request_data.app_data().app_guid()),
                 GuidToString(products[i].app_data().app_guid()));
    EXPECT_STREQ(app_request_data.app_data().version(),
                 products[i].app_data().version());
    EXPECT_STREQ(app_request_data.app_data().previous_version(),
                 products[i].app_data().previous_version());
    EXPECT_STREQ(app_request_data.app_data().language(),
                 products[i].app_data().language());

    PingEventVector::const_iterator iter =
        app_request_data.ping_events_begin();
    const PingEvent& ping_event = *iter;

    EXPECT_EQ(expected_type, ping_event.event_type());
    EXPECT_EQ(
        ping_utils::CompletionStatusToPingEventResult(expected_info.status),
        ping_event.event_result());
    EXPECT_EQ(expected_info.error_code, ping_event.error_code());
    EXPECT_EQ(app_request_data.app_data().previous_version(),
              ping_event.previous_version());
  }
}

TEST_F(PingUtilsTest, BuildCompletedPingForAllProducts_Failure) {
  ProductDataVector products;
  bool is_machine = false;
  AppData data1(StringToGuid(_T("{E66F2139-5469-BAAD-AC99-7863798E3A0A}")),
                is_machine);
  data1.set_version(_T("1.1.1.1"));
  data1.set_previous_version(_T("1.0.0.0"));
  data1.set_language(_T("en"));

  ProductData product_data1;
  product_data1.set_app_data(data1);
  products.push_back(product_data1);

  AppData data2(StringToGuid(_T("{E66F3140-5179-41ec-BAAD-7863798E3A0A}")),
                is_machine);
  data2.set_version(_T("1.1.1.1"));
  data2.set_previous_version(_T("1.0.0.0"));
  data2.set_language(_T("de"));

  ProductData product_data2;
  product_data2.set_app_data(data2);
  products.push_back(product_data2);

  CompletionInfo info(COMPLETION_ERROR, 10, _T("test"));

  Request actual_request(is_machine);
  ASSERT_HRESULT_SUCCEEDED(ping_utils::BuildCompletedPingForAllProducts(
      products,
      false,
      info,
      &actual_request));
  ValidateRequest(products,
                  info,
                  PingEvent::EVENT_INSTALL_COMPLETE,
                  actual_request);
}

TEST_F(PingUtilsTest, BuildCompletedPingForAllProducts_Update) {
  ProductDataVector products;
  bool is_machine = false;

  AppData data1(StringToGuid(_T("{E66F2139-5469-BAAD-AC99-7863798E3A0A}")),
                is_machine);
  data1.set_version(_T("1.1.1.1"));
  data1.set_previous_version(_T("1.0.0.0"));
  data1.set_language(_T("en"));

  ProductData product_data1;
  product_data1.set_app_data(data1);
  products.push_back(product_data1);

  AppData data2(StringToGuid(_T("{E66F3140-5179-41ec-BAAD-7863798E3A0A}")),
                is_machine);
  data2.set_version(_T("1.1.1.1"));
  data2.set_previous_version(_T("1.0.0.0"));
  data2.set_language(_T("de"));

  ProductData product_data2;
  product_data2.set_app_data(data2);
  products.push_back(product_data2);

  CompletionInfo info(COMPLETION_ERROR, 10, _T("test"));

  Request actual_request(is_machine);
  ASSERT_HRESULT_SUCCEEDED(ping_utils::BuildCompletedPingForAllProducts(
      products,
      true,
      info,
      &actual_request));

  ValidateRequest(products,
                  info,
                  PingEvent::EVENT_UPDATE_COMPLETE,
                  actual_request);
}

TEST_F(PingUtilsTest, BuildCompletedPingForAllProducts_Success) {
  ProductDataVector products;
  bool is_machine = false;

  AppData data1(StringToGuid(_T("{E66F2139-5469-BAAD-AC99-7863798E3A0A}")),
                is_machine);
  data1.set_version(_T("1.1.1.1"));
  data1.set_previous_version(_T("1.0.0.0"));
  data1.set_language(_T("en"));

  ProductData product_data1;
  product_data1.set_app_data(data1);
  products.push_back(product_data1);

  AppData data2(StringToGuid(_T("{E66F3140-5179-41ec-BAAD-7863798E3A0A}")),
                is_machine);
  data2.set_version(_T("1.1.1.1"));
  data2.set_previous_version(_T("1.0.0.0"));
  data2.set_language(_T("de"));

  ProductData product_data2;
  product_data2.set_app_data(data2);
  products.push_back(product_data2);

  CompletionInfo info(COMPLETION_SUCCESS, 0, _T("test"));

  Request actual_request(is_machine);
  ASSERT_HRESULT_SUCCEEDED(ping_utils::BuildCompletedPingForAllProducts(
      products,
      false,
      info,
      &actual_request));

  ValidateRequest(products,
                  info,
                  PingEvent::EVENT_INSTALL_COMPLETE,
                  actual_request);
}


}  // namespace omaha
