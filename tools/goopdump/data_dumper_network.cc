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

#include "omaha/tools/goopdump/data_dumper_network.h"
#include "omaha/common/vistautil.h"
#include "omaha/goopdate/goopdate_utils.h"
#include "omaha/net/network_config.h"
#include "omaha/tools/goopdump/data_dumper.h"
#include "omaha/tools/goopdump/dump_log.h"
#include "omaha/tools/goopdump/goopdump_cmd_line_parser.h"

namespace omaha {

DataDumperNetwork::DataDumperNetwork() {
}

DataDumperNetwork::~DataDumperNetwork() {
}

void DataDumperNetwork::DumpNetworkConfig(const DumpLog& dump_log) {
  NetworkConfig& network_config(NetworkConfig::Instance());
  HRESULT hr = network_config.Detect();
  if (FAILED(hr)) {
    dump_log.WriteLine(_T("Can't detect the network configuration."));
  }
  std::vector<Config> configs(network_config.GetConfigurations());
  dump_log.WriteLine(_T("Detected configurations:\r\n"));
  dump_log.WriteLine(NetworkConfig::ToString(configs));
}

HRESULT DataDumperNetwork::Process(const DumpLog& dump_log,
                                   const GoopdumpCmdLineArgs& args) {
  UNREFERENCED_PARAMETER(args);
  DumpHeader header(dump_log, _T("Network"));

  const bool is_machine = vista_util::IsUserAdmin();
  HRESULT hr = omaha::goopdate_utils::ConfigureNetwork(is_machine, false);
  if (FAILED(hr)) {
    dump_log.WriteLine(_T("Can't configure the network."));
  }

  DumpNetworkConfig(dump_log);
  return S_OK;
}

}  // namespace omaha

// Register the WinHttp client with the class factory for http clients.
#pragma comment(linker, "/INCLUDE:_kRegisterWinHttp")

