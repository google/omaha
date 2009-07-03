// Copyright 2003-2009 Google Inc.
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

#ifndef OMAHA_COMMON_UTILS_H__
#define OMAHA_COMMON_UTILS_H__

#include <windows.h>
#include <accctrl.h>
#include <aclapi.h>
#include <sddl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <atlstr.h>
#include <atlsecurity.h>
#include <atlwin.h>
#include <memory.h>
#include <map>
#include <vector>
#include "base/basictypes.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/logging.h"
#include "omaha/common/reg_key.h"
#include "omaha/common/scoped_any.h"
#include "omaha/common/static_assert.h"
#include "omaha/common/user_info.h"

namespace omaha {

// Determines whether to run ClickOnce components. This constant is not defined
// in the SDK headers.
#ifndef URLACTION_MANAGED_UNSIGNED
#define URLACTION_MANAGED_UNSIGNED (0x00002004)
#endif

ULONGLONG VersionFromString(const CString& s);

CString StringFromVersion(ULONGLONG version);

// Gets current directory
CString GetCurrentDir();

// Creates a unique file name using a new guid. Does not check for
// presence of this file in the directory.
HRESULT GetNewFileNameInDirectory(const CString& dir, CString* file_name);

// Use of this method is unsafe. Be careful!
// Gets security attributes for a "everyone" (authenticated users) DACL.
void GetEveryoneDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                       ACCESS_MASK non_admin_access_mask);

// Get security attributes containing a DACL that grant the ACCESS_MASK access
// to admins and system.
void GetAdminDaclSecurityAttributes(CSecurityAttributes* sec_attr,
                                    ACCESS_MASK accessmask);

// Merges an Allowed ACE into a named object. If the ACE already exists in the
// DACL with the same permissions (or a superset) and the same ACE flags, the
// merge is skipped.
HRESULT AddAllowedAce(const TCHAR* object_name,
                      SE_OBJECT_TYPE object_type,
                      const CSid& sid,
                      ACCESS_MASK required_permissions,
                      uint8 required_ace_flags);

struct NamedObjectAttributes {
  CString name;
  CSecurityAttributes sa;
};

// For machine and local system, the prefix would be "Global\G{obj_name}".
// For user, the prefix would be "Global\G{user_sid}{obj_name}".
// For machine objects, returns a security attributes that gives permissions to
// both Admins and SYSTEM. This allows for cases where SYSTEM creates the named
// object first. The default DACL for SYSTEM will not allow Admins access.
void GetNamedObjectAttributes(const TCHAR* base_name,
                              bool is_machine,
                              NamedObjectAttributes* attr);

// Returns true if the current process is running as SYSTEM.
HRESULT IsSystemProcess(bool* is_system_process);

// Returns true if the user of the current process is Local System or it has an
// interactive session: console, terminal services, or fast user switching.
HRESULT IsUserLoggedOn(bool* is_logged_on);

// Returns true if URLACTION_MANAGED_UNSIGNED is disabled for the Internet zone
// for the current user.
bool IsClickOnceDisabled();

// Wrapper around ::GetProcAddress().
template <typename T>
bool GPA(HMODULE module, const char* function_name, T* function_pointer) {
  ASSERT1(module);
  ASSERT1(function_name);
  ASSERT1(function_pointer);

  *function_pointer = reinterpret_cast<T>(::GetProcAddress(module,
                                                           function_name));
  if (NULL == *function_pointer) {
    UTIL_LOG(LW, (_T("GetProcAddress failed [%s]"), CA2T(function_name)));
  }
  return NULL != *function_pointer;
}

#define GPA_WRAP(module,                                                    \
                 function,                                                  \
                 proto,                                                     \
                 call,                                                      \
                 calling_convention,                                        \
                 result_type,                                               \
                 result_error)                                              \
typedef result_type (calling_convention *function##_pointer) proto;         \
inline result_type function##Wrap proto {                                   \
  HINSTANCE module_instance = ::GetModuleHandle(_T(#module));               \
  ASSERT1(module_instance);                                                 \
  if (!module_instance) {                                                   \
    return result_error;                                                    \
  }                                                                         \
  function##_pointer fn = NULL;                                             \
  return GPA(module_instance, #function, &fn) ? (*fn) call : result_error;  \
}

GPA_WRAP(kernel32.dll,
         AttachConsole,
         (DWORD process_id),
         (process_id),
         WINAPI,
         BOOL,
         0);

// Private Object Namespaces for Vista and above. More information here:
// http://msdn2.microsoft.com/en-us/library/ms684295(VS.85).aspx
GPA_WRAP(kernel32.dll,
         CreateBoundaryDescriptorW,
         (LPCWSTR boundary_name, ULONG flags),
         (boundary_name, flags),
         WINAPI,
         HANDLE,
         NULL);
GPA_WRAP(kernel32.dll,
         AddSIDToBoundaryDescriptor,
         (HANDLE* boundary_descriptor, PSID required_sid),
         (boundary_descriptor, required_sid),
         WINAPI,
         BOOL,
         FALSE);
GPA_WRAP(kernel32.dll,
         CreatePrivateNamespaceW,
         (LPSECURITY_ATTRIBUTES private_namespace_attributes, LPVOID boundary_descriptor, LPCWSTR alias_prefix),  // NOLINT
         (private_namespace_attributes, boundary_descriptor, alias_prefix),
         WINAPI,
         HANDLE,
         NULL);
GPA_WRAP(kernel32.dll,
         OpenPrivateNamespaceW,
         (LPVOID boundary_descriptor, LPCWSTR alias_prefix),
         (boundary_descriptor, alias_prefix),
         WINAPI,
         HANDLE,
         NULL);

bool IsPrivateNamespaceAvailable();


HRESULT PinModuleIntoProcess(const CString& module_name);

// Creates a directory with default security.
//   S_OK:    Created directory
//   S_FALSE: Directory already existed
//   E_FAIL:  Couldn't create
HRESULT CreateDir(const TCHAR* dirname, LPSECURITY_ATTRIBUTES security_attr);

// Gets the path for the specified special folder.
HRESULT GetFolderPath(int csidl, CString* path);

// Returns true if this directory name is 'safe' for deletion:
//  - it doesn't contain ".."
//  - it doesn't specify a drive root
bool SafeDirectoryNameForDeletion(const TCHAR* dir_name);

// Deletes a directory.
HRESULT DeleteDirectory(const TCHAR* ir_name);

// Deletes all files in a directory.
HRESULT DeleteDirectoryFiles(const TCHAR* dir_name);

// Deletes all files in a directory matching a wildcard.
HRESULT DeleteWildcardFiles(const TCHAR* dir_name, const TCHAR* wildcard_name);

// Deletes either a file or directory before or after reboot.
HRESULT DeleteBeforeOrAfterReboot(const TCHAR* targetname);

// Gets the size of all files in a directory.
// Aborts counting if one of the maximum criteria is reached.
HRESULT SafeGetDirectorySize(const TCHAR* dir_name,
                             uint64* size,
                             HANDLE shutdown_event,
                             uint64 max_size,
                             int max_depth,
                             int max_file_count,
                             int max_running_time_ms);

// Gets size of all files in a directory.
HRESULT GetDirectorySize(const TCHAR* dir_name, uint64* size);

enum TimeCategory {
  PAST = 0,  // older than 40 years from now
  FUTURE,    // in the future by > 1 day
  PRESENT,   // neither ANCIENT nor FUTURE
};

TimeCategory GetTimeCategory(time64 t);

// Returns true if a given time is likely to be valid.
bool IsValidTime(time64 t);

// Gets a time64 value.
// If the value is greater than the
// max value, then SetValue will be called using the max_value.
//
//  Args:
//   full_key_name:   the reg keyto read to get the time value.
//   value_name:      the name for the reg key value to be read.
//                    (may be NULL to get the default value)
//   max_time:        the maximum value for the reg key.
//   value:           the time value read.  May not be set on failure.
//   limited_value:   true iff will be set if the value was
//                    changed (and resaved). (NULL is allowable.)
HRESULT GetLimitedTimeValue(const TCHAR* full_key_name,
                            const TCHAR* value_name,
                            time64 max_time,
                            time64* value,
                            bool* limited_value);

// Gets a time64 value trying reg keys successively if there is a
// failure in getting a value.
//
// Typically used when there is a user value and a default value if the
// user has none.
//
//  Args:
//   full_key_names:  a list of reg keys to try successively (starting
//                    with index 0) to read to get the time value.  The
//                    attempts stops as soon as there is a successful read or
//                    when all keys have been tried.
//   key_names_length: number of keys in full_key_names.
//   value_name:      the name for the reg key value to be read.
//                    (may be NULL to get the default value)
//   max_time:        the maximum value for the reg key.
//   value:           the time value read.  May not be set on failure.
//   limited_value:   true iff will be set if the value was
//                    changed (and resaved). (NULL is allowable.)
HRESULT GetLimitedTimeValues(const TCHAR* full_key_names[],
                             int key_names_length,
                             const TCHAR* value_name,
                             time64 max_time,
                             time64* value,
                             bool* limited_value);

// Convert milliseconds to a relative time in units of 100ns, suitable for use
// with waitable timers
LARGE_INTEGER MSto100NSRelative(DWORD ms);

// TODO(omaha): remove from public interface.
inline void WINAPI NullAPCFunc(ULONG_PTR) {}

// Forces rasman.dll load to avoid a crash in wininet.
void EnsureRasmanLoaded();

// Returns if the HRESULT argument is a COM error
// TODO(omaha): use an ANONYMOUS_VARIABLE to avoid the situation in which the
// macro gets called like RET_IF_FAILED(hr);
// For now, use a quick fix hr -> __hr. Leading underscore names are not to be
// used in application code.
#define RET_IF_FAILED(x)    \
    do {                    \
      HRESULT __hr(x);      \
      if (FAILED(__hr)) {   \
        return __hr;        \
      }                     \
    } while (false)

// return error if the first argument evaluates to false
#define RET_IF_FALSE(x, err)  \
  do {                        \
    if (!(x)) {               \
      return err;             \
    }                         \
  } while (false)

// return false if the HRESULT argument is a COM error
#define RET_FALSE_IF_FAILED(x)  \
  do {                          \
    if (FAILED(x)) {            \
      return false;             \
    }                           \
  } while (false)

// return true if the HRESULT argument is a COM error
#define RET_TRUE_IF_FAILED(x)   \
  do {                          \
    if (FAILED(x)) {            \
      return true;              \
    }                           \
  } while (false)

// return if the HRESULT argument evaluates to FAILED - but also assert
// if failed
#define RET_IF_FAILED_ASSERT(x, msg) \
    do {                             \
      HRESULT hr(x);                 \
      if (FAILED(hr)) {              \
        ASSERT(false, msg);          \
        return hr;                   \
      }                              \
    } while (false)


// return if the HRESULT argument evaluates to FAILED - but also log an error
// message if failed
#define RET_IF_FAILED_LOG(x, cat, msg) \
    do {                               \
      HRESULT hr(x);                   \
      if (FAILED(hr)) {                \
        LC_LOG(cat, LEVEL_ERROR, msg); \
        return hr;                     \
      }                                \
    } while (false)

// return if the HRESULT argument evaluates to FAILED - but also REPORT an error
// message if failed
#define RET_IF_FAILED_REPORT(x, msg, n) \
    do {                                \
      HRESULT hr(x);                    \
      if (FAILED(hr)) {                 \
        REPORT(false, R_ERROR, msg, n); \
        return hr;                      \
      }                                 \
    } while (false)

// Initializes a POD to zero.
// Using this function requires discipline. Don't use for types that have a
// v-table or virtual bases.
template <typename T>
inline void SetZero(T& p) {   // NOLINT
  // Guard against the easy mistake of
  //    foo(int *p) { SetZero(p); } instead of
  //                  SetZero(*p);
  // which it should be.
  STATIC_ASSERT(sizeof(p) != sizeof(void*));    // NOLINT

  // A POD (plain old data) object has one of these data types:
  // a fundamental type, union, struct, array,
  // or class--with no constructor. PODs don't have virtual functions or
  // virtual bases.

  // Test to see if the type has constructors.
  union CtorTest {
      T t;
      int i;
  };

  // TODO(omaha): There might be a way to test if the type has virtuals
  // For now, if we zero a type with virtuals by mistake, it is going to crash
  // predictable at run-time when the virtuals are called.

  memset(&p, 0, sizeof(T));
}

inline void SecureSetZero(CString* p) {
  ASSERT1(p);
  if (!p->IsEmpty()) {
    ::SecureZeroMemory(p->GetBufferSetLength(p->GetLength()),
                       p->GetLength() * sizeof(TCHAR));
    p->ReleaseBuffer();
  }
}

inline void SecureSetZero(CComVariant* p) {
  ASSERT1(p);
  ASSERT1(V_VT(p) == VT_BSTR);
  uint32 byte_len = ::SysStringByteLen(V_BSTR(p));
  if (byte_len > 0) {
    ::SecureZeroMemory(V_BSTR(p), byte_len);
  }
}

// CreateForegroundParentWindowForUAC creates a WS_POPUP | WS_VISIBLE with zero
// size, of the STATIC WNDCLASS. It uses the default running EXE module
// handle for creation.
//
// A visible centered foreground window is needed as the parent in Windows 7 and
// above, to allow the UAC prompt to come up in the foreground, centered.
// Otherwise, the elevation prompt will be minimized on the taskbar. A zero size
// window works. A plain vanilla WS_POPUP allows the window to be free of
// adornments. WS_EX_TOOLWINDOW prevents the task bar from showing the
// zero-sized window.
//
// Returns NULL on failure. Call ::GetLastError() to get extended error
// information on failure.
inline HWND CreateForegroundParentWindowForUAC() {
  CWindow foreground_parent;
  if (foreground_parent.Create(_T("STATIC"), NULL, NULL, NULL,
                               WS_POPUP | WS_VISIBLE, WS_EX_TOOLWINDOW)) {
    foreground_parent.CenterWindow(NULL);
    ::SetForegroundWindow(foreground_parent);
  }
  return foreground_parent.Detach();
}

// TODO(omaha): move the definition and ShellExecuteExEnsureParent function
// below into shell.h

#if (NTDDI_VERSION < NTDDI_WINXPSP1)
// This value is not defined in the header, but has no effect on older OSes.
#define SEE_MASK_NOZONECHECKS      0x00800000
#endif

// ShellExecuteExEnsureParent is a wrapper around ::ShellExecuteEx.
// It ensures that we always have a parent window. In elevation scenarios, we
// need to use the HWND property to be acknowledged as a foreground application
// on Windows Vista. Otherwise, the elevation prompt will appear minimized on
// the taskbar The UAC elevation mechanism uses the HWND as part of determining
// whether the elevation is a foreground elevation.
//
// A better place for this might be in the Process class. However, to
// reduce dependencies for the stub, placing this in utils.
//
// Return values:
//   ShellExecuteExEnsureParent returns TRUE on success, and FALSE on failure.
//   Call ::GetLastError() to get the extended error information on failure.
//
// Args:
//   shell_exec_info structure pointer, filled in, with an optional HWND.
//   The structure is not validated. If the HWND is NULL, it creates one.
bool ShellExecuteExEnsureParent(LPSHELLEXECUTEINFO shell_exec_info);

//
// Wait with a message loop.
// Returns true if the wait completed successfully and false in case of an
// error.
//

// Waits with a message loop until any of the synchronization objects is
// signaled. Return the index of the object that satisfied the wait.
bool WaitWithMessageLoopAny(const std::vector<HANDLE>& handles, uint32* pos);

// Waits with message loop until all the synchronization objects are signaled.
bool WaitWithMessageLoopAll(const std::vector<HANDLE>& handles);

// Waits with message loop until the synchronization object is signaled.
bool WaitWithMessageLoop(HANDLE h);

// Waits with message loop for a certain period of time
bool WaitWithMessageLoopTimed(DWORD ms);

//
// TODO(omaha): message handler classes should go in other module.
//
// Handles windows messages
class MessageHandlerInterface {
 public:
  virtual ~MessageHandlerInterface() {}
  // Does the translate/dispatch for one window message
  // msg is never NULL.
  virtual void Process(MSG* msg) = 0;
};

// The simplest working implementation of a message handler.
// It does TranslateMessage/DispatchMessage.
class BasicMessageHandler : public MessageHandlerInterface {
 public:
  BasicMessageHandler() {}
  virtual void Process(MSG* msg);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(BasicMessageHandler);
};

// An internal detail (used to handle messages
// and adjust the handles/cnt as needed).
class MessageHandlerInternalInterface {
 public:
  virtual ~MessageHandlerInternalInterface() {}
  // Does the translate/dispatch for one window message
  // msg is never NULL and may change the handles and cnt (which are
  // never NULL).
  virtual void Process(MSG* msg, const HANDLE** handles, uint32* cnt) = 0;
};

// The callback for MessageLoopInterface::RegisterWaitForSingleObject
class WaitCallbackInterface {
 public:
  virtual ~WaitCallbackInterface() {}
  // Return false from this method to terminate the message loop.
  virtual bool HandleSignaled(HANDLE handle) = 0;
};

// This is similar to a threadpool interface
// but this is done on the main message loop instead.
//
// Important: These method calls are *not* threadsafe and
// should only be done on the message loop thread.
class MessageLoopInterface {
 public:
  virtual ~MessageLoopInterface() {}
  // sets-up a callback for when the handle becomes signaled
  //   * the callback happens on the main thread.
  //   * the handle/callback is removed when
  //     the callback is made.
  //   * If the handle becomes invalid while it is being waited on,
  //     the message loop will exit.
  //   * If the handle has already been registered,
  //     then adding it again will essentially replace the
  //     previous callback with the new one.
  virtual bool RegisterWaitForSingleObject(HANDLE handle,
                                           WaitCallbackInterface* callback) = 0;

  // stops watching for the handle to be signaled
  virtual bool UnregisterWait(HANDLE handle) = 0;
};

// An implementation of the MessageLoopInterface
class MessageLoopWithWait : public MessageLoopInterface,
                            private MessageHandlerInternalInterface {
 public:
  MessageLoopWithWait();

  // This method needs to be called before the "Process()"
  // method or else "Process()" will crash.
  //
  // Args:
  //    message_handler: handles any messages that occur
  //       *Note* It is the callers responsibility to keep
  //       message_handler around while this class exists
  //       and the *caller* should free message_handler
  //       after that.
  void set_message_handler(MessageHandlerInterface* message_handler);

  // sets-up a callback for when the handle becomes signaled
  virtual bool RegisterWaitForSingleObject(HANDLE handle,
                                           WaitCallbackInterface* callback);

  // stops watching for the handle to be signaled
  virtual bool UnregisterWait(HANDLE handle);

  // The message loop and handle callback routine
  HRESULT Process();

 private:
  // Handles one messgae and adjus tthe handles and cnt as appropriate after
  // handling the message
  virtual void Process(MSG* msg, const HANDLE** handles, uint32* cnt);

  void RemoveHandleAt(uint32 pos);

  MessageHandlerInterface* message_handler_;

  // Handles that are checked for being signaled.
  std::vector<HANDLE> callback_handles_;

  // What to call when a handle is signaled.
  std::vector<WaitCallbackInterface*> callbacks_;
  DISALLOW_EVIL_CONSTRUCTORS(MessageLoopWithWait);
};

// Calls an entry point that may be exposed from a DLL
//   It is an error if the DLL is missing or can't be loaded
//   If the entry point is missing the error returned is
//   ERROR_INVALID_FUNCTION (as an HRESULT).
//   Otherwise, if the function is called successfully then
//   CallEntryPoint0 return S_OK and the result parameter is set to the
//   return value from the function call.
HRESULT CallEntryPoint0(const TCHAR* dll_path,
                        const char* function_name,
                        HRESULT* result);

// (Un)Registers a COM DLL with the system.  Returns S_FALSE if entry
// point missing (so that it you can call (Un)RegisterDll on any DLL
// without worrying whether the DLL is actually a COM server or not).
HRESULT RegisterDll(const TCHAR* dll_path);
HRESULT UnregisterDll(const TCHAR* dll_path);

// (Un)Registers a COM Local Server with the system.
HRESULT RegisterServer(const TCHAR* exe_path);
HRESULT UnregisterServer(const TCHAR* exe_path);
HRESULT RegisterOrUnregisterExe(const TCHAR* exe_path, const TCHAR* cmd_line);

// (Un)Registers a COM Service with the system.
HRESULT RegisterService(const TCHAR* exe_path);
HRESULT UnregisterService(const TCHAR* exe_path);

// Starts a service.
HRESULT RunService(const TCHAR* service_name);

// Read an entire file into a memory buffer. Use this function when you need
// exclusive access to the file.
// Returns MEM_E_INVALID_SIZE if the file size is larger than max_len (unless
// max_len == 0, in which case it is ignored)
HRESULT ReadEntireFile(const TCHAR* filepath,
                       uint32 max_len,
                       std::vector<byte>* buffer_out);

// Allows specifying a sharing mode such as FILE_SHARE_READ. Otherwise,
// this is identical to ReadEntireFile.
HRESULT ReadEntireFileShareMode(const TCHAR* filepath,
                                uint32 max_len,
                                DWORD share_mode,
                                std::vector<byte>* buffer_out);

// Writes an entire file from a memory buffer
HRESULT WriteEntireFile(const TCHAR * filepath,
                        const std::vector<byte>& buffer_in);

// Conversions between a byte stream and a std::string
HRESULT BufferToString(const std::vector<byte>& buffer_in, CStringA* str_out);
HRESULT BufferToString(const std::vector<byte>& buffer_in, CString* str_out);
HRESULT StringToBuffer(const CStringA& str_in, std::vector<byte>* buffer_out);
HRESULT StringToBuffer(const CString& str_in, std::vector<byte>* buffer_out);

// Splits a "full regkey name" into a key name part and a value name part.
// Handles "(default)" as a value name.  Treats a trailing "/" as "(default)".
HRESULT RegSplitKeyvalueName(const CString& keyvalue_name,
                             CString* key_name,
                             CString* value_name);

// Expands string with embedded special variables which are enclosed
// in '%' pair. For example, "%PROGRAMFILES%\Google" expands to
// "C:\Program Files\Google".
// If any of the embedded variable can not be expanded, we will leave it intact
// and return HRESULT_FROM_WIN32(ERROR_NOT_FOUND)
HRESULT ExpandEnvLikeStrings(const TCHAR* src,
                             const std::map<CString, CString>& keywords,
                             CString* dest);

// Returns true if the path represents a registry path.
bool IsRegistryPath(const TCHAR* path);

// Returns true if the path is a URL.
bool IsUrl(const TCHAR* path);

// Converts GUID to string.
CString GuidToString(const GUID& guid);

// Converts string to GUID.
GUID StringToGuid(const CString& str);

// Converts a variant containing a list of strings.
void VariantToStringList(VARIANT var, std::vector<CString>* list);

// Appends two registry key paths, takes care of extra separators in the
// beginning or end of the key.
CString AppendRegKeyPath(const CString& one, const CString& two);
CString AppendRegKeyPath(const CString& one,
                         const CString& two,
                         const CString& three);

// Returns the list of user keys that are present within the HKEY_USERS key
// the method only returns the keys of the users and takes care of,
// removing the well known sids. The returned values are the complete values
// from the root of the registry
HRESULT GetUserKeysFromHkeyUsers(std::vector<CString>* key_names);

// Use when a function should be able to
// be replaced with another implementation. Usually,
// this is done for testing code only.
//
// Typical usage:
//
//  typedef bool BoolPreferenceFunctionType();
//  CallInterceptor<BoolPreferenceFunctionType> should_send_stats_interceptor;
//  BoolPreferenceFunctionType* ReplaceShouldSendStatsFunction(
//      BoolPreferenceFunctionType* replacement) {
//    return should_send_stats_interceptor.ReplaceFunction(replacement);
//  }
template <typename R>
class CallInterceptor {
 public:
  CallInterceptor() {
    interceptor_ = NULL;
  }

  R* ReplaceFunction(R* replacement) {
    R* old = interceptor_;
    interceptor_ = replacement;
    return old;
  }

  R* interceptor() {
    return interceptor_;
  }

 private:
  R* interceptor_;
  DISALLOW_EVIL_CONSTRUCTORS(CallInterceptor);
};

// Gets a handle of the current process. The handle is a real handle
// and the caller must close it
HRESULT GetCurrentProcessHandle(HANDLE* handle);

// Helper class for an ATL module that registers a custom AccessPermission
// to allow local calls from interactive users and the system account.
// Derive from this class as well as CAtlModuleT (or a derivative).
// Override RegisterAppId() and UnregisterAppId(), and delegate to the
// corresponding functions in this class.
template <class T>
class LocalCallAccessPermissionHelper {
 public:
  HRESULT RegisterAppId() throw() {
    // Local call permissions allowed for Interactive Users and Local System
    static LPCTSTR ALLOW_LOCAL_CALL_SDDL =
        _T("O:BAG:BAD:(A;;0x3;;;IU)(A;;0x3;;;SY)");

    UTIL_LOG(L1, (_T("[LocalCallAccessPermissionHelper::RegisterAppId]")));

    // First call the base ATL module implementation, so the AppId is registered
    RET_IF_FAILED(T::UpdateRegistryAppId(TRUE));

    // Next, write the AccessPermission value
    RegKey key_app_id;
    RET_IF_FAILED(key_app_id.Open(HKEY_CLASSES_ROOT, _T("AppID"), KEY_WRITE));

    RegKey key;
    RET_IF_FAILED(key.Create(key_app_id.Key(), T::GetAppIdT()));
    CSecurityDesc sd;
    RET_IF_FALSE(sd.FromString(ALLOW_LOCAL_CALL_SDDL), HRESULTFromLastError());
    RET_IF_FAILED(key.SetValue(
        _T("AccessPermission"),
        reinterpret_cast<const byte*>(sd.GetPSECURITY_DESCRIPTOR()),
        sd.GetLength()));

    UTIL_LOG(L1, (_T("[LocalCallAccessPermissionHelper::RegisterAppId]")
                  _T("[succeeded]")));
    return S_OK;
  }

  HRESULT UnregisterAppId() throw() {
    // First remove the AccesPermission entry.
    RegKey key_app_id;
    RET_IF_FAILED(key_app_id.Open(HKEY_CLASSES_ROOT, _T("AppID"), KEY_WRITE));

    RegKey key;
    RET_IF_FAILED(key.Open(key_app_id.Key(), T::GetAppIdT(), KEY_WRITE));
    VERIFY1(SUCCEEDED(key.DeleteValue(_T("AccessPermission"))));

    // Now, call the base ATL module implementation to unregister the AppId
    RET_IF_FAILED(T::UpdateRegistryAppId(FALSE));

    UTIL_LOG(L1, (_T("[LocalCallAccessPermissionHelper::UnregisterAppId'")
                  _T("[succeeded]")));
    return S_OK;
  }
};

// Returns true if the argument is a guid.
inline bool IsGuid(const TCHAR* s) {
  if (!s) return false;
  GUID guid = {0};
  return SUCCEEDED(::CLSIDFromString(const_cast<TCHAR*>(s), &guid));
}

inline bool IsLocalSystemSid(const TCHAR* sid) {
  ASSERT1(sid);
  return _tcsicmp(sid, kLocalSystemSid) == 0;
}

// Fills a buffer with cryptographically random bytes.
bool GenRandom(void* buffer, size_t buffer_length);

// Deletes an object. The functor is useful in for_each algorithms.
struct DeleteFun {
  template <class T> void operator()(T ptr) { delete ptr; }
};

// Sets or clears the specified value in the Run key to the specified command.
HRESULT ConfigureRunAtStartup(const CString& root_key_name,
                              const CString& run_value_name,
                              const CString& command,
                              bool install);

// Cracks a command line and returns the program name, which is the first
// whitespace separated token.
HRESULT GetExePathFromCommandLine(const TCHAR* command_line,
                                  CString* exe_path);

// Waits for MSI to complete, if MSI is busy installing or uninstalling apps.
HRESULT WaitForMSIExecute(int timeout_ms);

// Returns the value of the specified environment variable.
CString GetEnvironmentVariableAsString(const TCHAR* name);

// Returns true if the OS is installing (e.g., at an OEM factory).
bool IsWindowsInstalling();

}  // namespace omaha

#endif  // OMAHA_COMMON_UTILS_H__

