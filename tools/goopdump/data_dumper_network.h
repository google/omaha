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

#ifndef OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_NETWORK_H__
#define OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_NETWORK_H__

#include <windows.h>
#include <atlstr.h>
#include "base/basictypes.h"

#include "omaha/tools/goopdump/data_dumper.h"

namespace omaha {

class DataDumperNetwork : public DataDumper {
 public:
  DataDumperNetwork();
  virtual ~DataDumperNetwork();

  virtual HRESULT Process(const DumpLog& dump_log,
                          const GoopdumpCmdLineArgs& args);

 private:
  void DumpNetworkConfig(const DumpLog& dump_log);

  DISALLOW_EVIL_CONSTRUCTORS(DataDumperNetwork);
};

}  // namespace omaha

#endif  // OMAHA_TOOLS_SRC_GOOPDUMP_DATA_DUMPER_NETWORK_H__

