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

#include <cstdio>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "omaha/common/app_util.h"
#include "omaha/common/browser_utils.h"
#include "omaha/net/detector.h"
#include "omaha/net/network_config.h"
#include "omaha/testing/unit_test.h"

namespace omaha {

class FirefoxProxyDetectorTest : public testing::Test {
 public:
  FirefoxProxyDetectorTest() {}

  virtual void SetUp() {
    detector_.reset(new FirefoxProxyDetector);
  }

  virtual void TearDown() {
  }

  HRESULT BuildProxyString(const CString& http_host,
                           const CString& http_port,
                           const CString& ssl_host,
                           const CString& ssl_port,
                           CString* proxy) {
    return detector_->BuildProxyString(http_host,
                                       http_port,
                                       ssl_host,
                                       ssl_port,
                                       proxy);
  }

  void ParsePrefsLine(const char* ansi_line,
                      CString* proxy_type,
                      CString* proxy_config_url,
                      CString* proxy_http_host,
                      CString* proxy_http_port,
                      CString* proxy_ssl_host,
                      CString* proxy_ssl_port) {
    detector_->ParsePrefsLine(ansi_line,
                              proxy_type,
                              proxy_config_url,
                              proxy_http_host,
                              proxy_http_port,
                              proxy_ssl_host,
                              proxy_ssl_port);
  }


  HRESULT ParsePrefsFile(const TCHAR* name,
                         const TCHAR* file_path,
                         Config* config) {
    return detector_->ParsePrefsFile(name, file_path, config);
  }

  // Builds a mock prefs file to test the parsing code.
  bool BuildPrefsFile(const CString& type,
                      const CString& config_url,
                      const CString& http_host,
                      const CString& http_port,
                      const CString& ssl_host,
                      const CString& ssl_port,
                      CString* file_path);

  scoped_ptr<FirefoxProxyDetector> detector_;

 private:
  DISALLOW_EVIL_CONSTRUCTORS(FirefoxProxyDetectorTest);
};

bool FirefoxProxyDetectorTest::BuildPrefsFile(const CString& type,
                                              const CString& config_url,
                                              const CString& http_host,
                                              const CString& http_port,
                                              const CString& ssl_host,
                                              const CString& ssl_port,
                                              CString* file_path) {
  CString temp_dir(app_util::GetTempDir());
  file_path->Format(_T("%somaha_test_%x.js"), temp_dir, ::GetTickCount());

  FILE* prefs_file = NULL;
  fopen_s(&prefs_file, CStringA(*file_path), "w");
  if (!prefs_file) {
    return false;
  }

  fprintf(prefs_file,
          "user_pref(\"network.proxy.type\", %s);\n",
          CStringA(type));
  fprintf(prefs_file,
          "user_pref(\"network.proxy.autoconfig_url\", \"%s\");\n",
          CStringA(config_url));
  fprintf(prefs_file,
          "user_pref(\"network.proxy.http\", \"%s\");\n",
          CStringA(http_host));
  fprintf(prefs_file,
          "user_pref(\"network.proxy.http_port\", %s);\n",
          CStringA(http_port));
  fprintf(prefs_file,
          "user_pref(\"network.proxy.ssl\", \"%s\");\n",
          CStringA(ssl_host));
  fprintf(prefs_file,
          "user_pref(\"network.proxy.ssl_port\", %s);\n",
          CStringA(ssl_port));

  fclose(prefs_file);

  return true;
}

TEST_F(FirefoxProxyDetectorTest, BuildProxyString) {
  CString http_host = _T("foo");
  CString http_port = _T("80");
  CString ssl_host;
  CString ssl_port;
  CString proxy;

  EXPECT_SUCCEEDED(BuildProxyString(http_host,
                                    http_port,
                                    ssl_host,
                                    ssl_port,
                                    &proxy));
  EXPECT_STREQ(proxy, _T("http=foo:80"));

  http_host = _T("foo");
  http_port = _T("80");
  ssl_host  = _T("bar");
  ssl_port  = _T("8080");

  EXPECT_SUCCEEDED(BuildProxyString(http_host,
                                    http_port,
                                    ssl_host,
                                    ssl_port,
                                    &proxy));
  EXPECT_STREQ(proxy, _T("http=foo:80;https=bar:8080"));

  http_host.Empty();
  http_port.Empty();
  ssl_host  = _T("bar");
  ssl_port  = _T("8080");

  EXPECT_SUCCEEDED(BuildProxyString(http_host,
                                    http_port,
                                    ssl_host,
                                    ssl_port,
                                    &proxy));
  EXPECT_STREQ(proxy, _T("https=bar:8080"));
}

TEST_F(FirefoxProxyDetectorTest, ParsePrefsLine) {
  CString type;
  CString config_url;
  CString http_host;
  CString http_port;
  CString ssl_host;
  CString ssl_port;

  // Parse "type".
  const char* ansi_line = "user_pref(\"network.proxy.type\", 4);";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(type, _T("4"));

  // Parse "config_url".
  ansi_line =
      "user_pref(\"network.proxy.autoconfig_url\", \"http://wpad/wpad.dat\");";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(config_url, _T("\"http://wpad/wpad.dat\""));

  // Parse "http_host".
  ansi_line = "user_pref(\"network.proxy.http\", \"127.0.0.1\");";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(http_host, _T("\"127.0.0.1\""));

  // Parse "http_port".
  ansi_line = "user_pref(\"network.proxy.http_port\", 8888);";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(http_port, _T("8888"));

  // Parse "ssl_host".
  ansi_line = "user_pref(\"network.proxy.ssl\", \"10.0.0.1\");";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(ssl_host, _T("\"10.0.0.1\""));

  // Parse "ssl_port".
  ansi_line = "user_pref(\"network.proxy.ssl_port\", 8080);";
  ParsePrefsLine(ansi_line,
                 &type,
                 &config_url,
                 &http_host,
                 &http_port,
                 &ssl_host,
                 &ssl_port);
  EXPECT_STREQ(ssl_port, _T("8080"));
}

TEST_F(FirefoxProxyDetectorTest, ParsePrefsFile) {
  // Direct connection
  CString prefs_file;
  bool res = BuildPrefsFile(_T("0"),
                            _T("http://foobar"),
                            _T("foo"),
                            _T("80"),
                            _T("bar"),
                            _T("8080"),
                            &prefs_file);
  ASSERT_TRUE(res);
  Config config;
  EXPECT_SUCCEEDED(ParsePrefsFile(_T(""), prefs_file, &config));
  EXPECT_FALSE(config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
  EXPECT_TRUE(::DeleteFile(prefs_file));

  // Named proxy.
  res = BuildPrefsFile(_T("1"),
                       _T("http://foobar"),
                       _T("foo"),
                       _T("80"),
                       _T("bar"),
                       _T("8080"),
                       &prefs_file);
  ASSERT_TRUE(res);
  config = Config();
  EXPECT_SUCCEEDED(ParsePrefsFile(_T(""), prefs_file, &config));
  EXPECT_FALSE(config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_STREQ(config.proxy, _T("http=foo:80;https=bar:8080"));
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
  EXPECT_TRUE(::DeleteFile(prefs_file));

  // Auto config url.
  res = BuildPrefsFile(_T("2"),
                       _T("http://foobar"),
                       _T("foo"),
                       _T("80"),
                       _T("bar"),
                       _T("8080"),
                       &prefs_file);
  ASSERT_TRUE(res);
  config = Config();
  EXPECT_SUCCEEDED(ParsePrefsFile(_T(""), prefs_file, &config));
  EXPECT_FALSE(config.auto_detect);
  EXPECT_STREQ(config.auto_config_url, _T("http://foobar"));
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
  EXPECT_TRUE(::DeleteFile(prefs_file));

  // Auto detect.
  res = BuildPrefsFile(_T("4"),
                       _T("http://foobar"),
                       _T("foo"),
                       _T("80"),
                       _T("bar"),
                       _T("8080"),
                       &prefs_file);
  ASSERT_TRUE(res);
  config = Config();
  EXPECT_SUCCEEDED(ParsePrefsFile(_T(""), prefs_file, &config));
  EXPECT_TRUE(config.auto_detect);
  EXPECT_TRUE(config.auto_config_url.IsEmpty());
  EXPECT_TRUE(config.proxy.IsEmpty());
  EXPECT_TRUE(config.proxy_bypass.IsEmpty());
  EXPECT_TRUE(::DeleteFile(prefs_file));
}

// Tries to detect the configuration if a profile is available.
TEST_F(FirefoxProxyDetectorTest, Detect) {
  CString name, path;
  if (FAILED(GetFirefoxDefaultProfile(&name, &path))) {
    return;
  }
  Config config;
  EXPECT_SUCCEEDED(detector_->Detect(&config));
}

}  // namespace omaha

