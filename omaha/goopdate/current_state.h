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

// The current state of the App.

#ifndef OMAHA_GOOPDATE_CURRENT_STATE_H_
#define OMAHA_GOOPDATE_CURRENT_STATE_H_

#include <atlbase.h>
#include <atlcom.h>
#include <vector>
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "goopdate/omaha3_idl.h"
#include "omaha/base/atlregmapex.h"
#include "omaha/base/constants.h"
#include "omaha/base/marshal_by_value.h"
#include "omaha/goopdate/google_update_ps_resource.h"
#include "omaha/common/goopdate_utils.h"

namespace omaha {

class ATL_NO_VTABLE CurrentAppState
  : public CComObjectRootEx<CComObjectThreadModel>,
    public CComCoClass<CurrentAppState>,
    public IDispatchImpl<ICurrentState,
                         &__uuidof(ICurrentState),
                         &CAtlModule::m_libid,
                         kMajorTypeLibVersion,
                         kMinorTypeLibVersion>,
    public IPersistStreamInitImpl<CurrentAppState>,
    public MarshalByValue<CurrentAppState> {
 public:
  static HRESULT Create(LONG state_value,
                        const CString& available_version,
                        ULONGLONG bytes_downloaded,
                        ULONGLONG total_bytes_to_download,
                        LONG download_time_remaining_ms,
                        ULONGLONG next_retry_time,
                        LONG install_progress_percentage,
                        LONG install_time_remaining_ms,
                        bool is_canceled,
                        LONG error_code,
                        LONG extra_code1,
                        const CString& completion_message,
                        LONG installer_result_code,
                        LONG installer_result_extra_code1,
                        const CString& success_launch_cmd_line,
                        const CString& post_install_url,
                        PostInstallAction post_install_action,
                        CComObject<CurrentAppState>** current_state);
  CurrentAppState();

  static bool is_machine() {
    return goopdate_utils::IsRunningFromOfficialGoopdateDir(true);
  }

  static const CLSID& GetObjectCLSID() {
    return is_machine() ? __uuidof(CurrentStateMachineClass) :
                          __uuidof(CurrentStateUserClass);
  }

  DECLARE_REGISTRY_RESOURCEID_EX(IDR_INPROC_SERVER_RGS)

  #pragma warning(push)
  // C4640: construction of local static object is not thread-safe
  #pragma warning(disable : 4640)
  BEGIN_REGISTRY_MAP()
    REGMAP_ENTRY(_T("HKROOT"), is_machine() ? _T("HKLM") : _T("HKCU"))
    REGMAP_ENTRY(_T("CLSID"), GetObjectCLSID())
  END_REGISTRY_MAP()

  BEGIN_PROP_MAP(CurrentAppState)
    PROP_DATA_ENTRY("StateValue", state_value_, VT_I4)
    PROP_DATA_ENTRY("AvailableVersion", available_version_, VT_BSTR)
    PROP_DATA_ENTRY("BytesDownloaded", bytes_downloaded_, VT_UI8)
    PROP_DATA_ENTRY("TotalBytesToDownload", total_bytes_to_download_, VT_UI8)
    PROP_DATA_ENTRY("DownloadTimeRemainingMs", download_time_remaining_ms_,
                    VT_I4)
    PROP_DATA_ENTRY("NextRetryTime", next_retry_time_, VT_UI8)
    PROP_DATA_ENTRY("InstallProgressPercentage", install_progress_percentage_,
                    VT_I4)
    PROP_DATA_ENTRY("InstallTimeRemainingMs", install_time_remaining_ms_, VT_I4)
    PROP_DATA_ENTRY("IsCanceled", is_canceled_, VT_BOOL)
    PROP_DATA_ENTRY("ErrorCode", error_code_, VT_I4)
    PROP_DATA_ENTRY("ExtraCode1", extra_code1_, VT_I4)
    PROP_DATA_ENTRY("CompletionMessage", completion_message_, VT_BSTR)
    PROP_DATA_ENTRY("InstallerResultCode", installer_result_code_, VT_I4)
    PROP_DATA_ENTRY("InstallerResultExtraCode1", installer_result_extra_code1_,
                    VT_I4)
    PROP_DATA_ENTRY("PostInstallLaunchCommandLine",
                    post_install_launch_command_line_, VT_BSTR)
    PROP_DATA_ENTRY("PostInstallUrl", post_install_url_, VT_BSTR)
    PROP_DATA_ENTRY("PostInstallAction", post_install_action_, VT_I4)
  END_PROP_MAP()
  #pragma warning(pop)

  // ICurrentState.
  STDMETHOD(get_stateValue)(LONG* state_value);
  STDMETHOD(get_availableVersion)(BSTR* available_version);
  STDMETHOD(get_bytesDownloaded)(ULONG* bytes_downloaded);
  STDMETHOD(get_totalBytesToDownload)(ULONG* total_bytes_to_download);
  STDMETHOD(get_downloadTimeRemainingMs)(LONG* download_time_remaining_ms);
  STDMETHOD(get_nextRetryTime)(ULONGLONG* next_retry_time);
  STDMETHOD(get_installProgress)(LONG* install_progress_percentage);
  STDMETHOD(get_installTimeRemainingMs)(LONG* install_time_remaining_ms);
  STDMETHOD(get_isCanceled)(VARIANT_BOOL* is_canceled);
  STDMETHOD(get_errorCode)(LONG* error_code);
  STDMETHOD(get_extraCode1)(LONG* extra_code1);
  STDMETHOD(get_completionMessage)(BSTR* completion_message);
  STDMETHOD(get_installerResultCode)(LONG* installer_result_code);
  STDMETHOD(get_installerResultExtraCode1)(LONG* installer_result_extra_code1);
  STDMETHOD(get_postInstallLaunchCommandLine)(
      BSTR* post_install_launch_command_line);
  STDMETHOD(get_postInstallUrl)(BSTR* post_install_url);
  STDMETHOD(get_postInstallAction)(LONG* post_install_action);

 protected:
  virtual ~CurrentAppState();

  BEGIN_COM_MAP(CurrentAppState)
    COM_INTERFACE_ENTRY(ICurrentState)
    COM_INTERFACE_ENTRY(IDispatch)
    COM_INTERFACE_ENTRY(IMarshal)
  END_COM_MAP()

  BOOL m_bRequiresSave;

 private:
  LONG state_value_;
  CComBSTR available_version_;
  ULONGLONG bytes_downloaded_;
  ULONGLONG total_bytes_to_download_;
  LONG download_time_remaining_ms_;
  ULONGLONG next_retry_time_;
  LONG install_progress_percentage_;
  LONG install_time_remaining_ms_;
  VARIANT_BOOL is_canceled_;
  LONG error_code_;
  LONG extra_code1_;
  CComBSTR completion_message_;
  LONG installer_result_code_;
  LONG installer_result_extra_code1_;
  CComBSTR post_install_launch_command_line_;
  CComBSTR post_install_url_;
  LONG post_install_action_;

  DISALLOW_COPY_AND_ASSIGN(CurrentAppState);
};

}  // namespace omaha

#endif  // OMAHA_GOOPDATE_CURRENT_STATE_H_
