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

#include "omaha/net/net_utils.h"
#include <iphlpapi.h>
#include "base/scoped_ptr.h"
#include "omaha/common/logging.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/utils.h"

namespace omaha {

bool IsMachineConnectedToNetwork() {
  // Get the table of information on all interfaces.
  DWORD table_size = 0;
  DWORD result = ::GetIfTable(NULL, &table_size, false);
  if (result != ERROR_INSUFFICIENT_BUFFER) {
    NET_LOG(LW, (_T("[GetIfTable failed to get the table size][%d]"), result));
    return true;
  }

  scoped_array<char> buffer(new char[table_size]);
  MIB_IFTABLE* mib_table = reinterpret_cast<MIB_IFTABLE*>(buffer.get());
  result = ::GetIfTable(mib_table, &table_size, false);
  if (result != NO_ERROR) {
    NET_LOG(LW, (_T("[GetIfTable failed][%d]"), result));
    return true;
  }

  // Scan the table looking for active connections.
  bool active_LAN = false;
  bool active_WAN = false;
  for (size_t i = 0; i < mib_table->dwNumEntries; ++i) {
    if (mib_table->table[i].dwType != MIB_IF_TYPE_LOOPBACK) {
      active_WAN = active_WAN ||
          mib_table->table[i].dwOperStatus == MIB_IF_OPER_STATUS_CONNECTED;
      active_LAN = active_LAN ||
          mib_table->table[i].dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL;
    }
  }

  return active_LAN || active_WAN;
}

CString BufferToPrintableString(const void* buffer, size_t length) {
  CString result;
  result.Preallocate(length);
  if (buffer) {
    for (size_t i = 0; i != length; ++i) {
      char ch = (static_cast<const char*>(buffer))[i];
      result.AppendChar((isprint(ch) || ch =='\r' || ch == '\n') ? ch : '.');
    }
  }
  return result;
}

CString VectorToPrintableString(const std::vector<uint8>& response) {
  CString str;
  if (!response.empty()) {
    str = BufferToPrintableString(&response.front(), response.size());
  }
  return str;
}

}  // namespace omaha

