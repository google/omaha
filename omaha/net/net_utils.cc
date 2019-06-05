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
#include <intsafe.h>
#include <memory>

#include "omaha/base/const_addresses.h"
#include "omaha/base/logging.h"
#include "omaha/base/utils.h"

namespace omaha {

bool IsMachineConnectedToNetwork() {
  // Get the table of information on all interfaces.
  DWORD table_size = 0;
  DWORD result = ::GetIfTable(NULL, &table_size, false);
  if (result != ERROR_INSUFFICIENT_BUFFER) {
    NET_LOG(LW, (_T("[GetIfTable failed to get the table size][%d]"), result));
    return true;
  }

  std::unique_ptr<char[]> buffer(new char[table_size]);
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
  if (!buffer || length > INT_MAX) {
    return result;
  }
  result.Preallocate(static_cast<int>(length));
  for (size_t i = 0; i != length; ++i) {
    const char ch = (static_cast<const char*>(buffer))[i];
    // Workaround for _chvalidator bug in VC2010.
    const bool is_printable = !!isprint(static_cast<unsigned char>(ch));
    result.AppendChar((is_printable || ch =='\r' || ch == '\n') ? ch : '.');
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

bool IsHttpUrl(const CString& url) {
  return String_StartsWith(url, kHttpProto, true);
}

bool IsHttpsUrl(const CString& url) {
  return String_StartsWith(url, kHttpsProto, true);
}

CString MakeHttpUrl(const CString& url) {
  if (IsHttpUrl(url)) {
    return url;
  }
  CString http_url(RemoveInternetProtocolHeader(url));
  http_url.Insert(0, kHttpProto);
  return http_url;
}

CString MakeHttpsUrl(const CString& url) {
  if (IsHttpsUrl(url)) {
    return url;
  }
  CString https_url(RemoveInternetProtocolHeader(url));
  https_url.Insert(0, kHttpsProto);
  return https_url;
}

}  // namespace omaha

