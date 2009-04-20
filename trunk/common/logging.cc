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
//
// Tracing and logging system.
//
// The log output goes to "All Users/Application Data/Google/Update/Log/".
// The log configuration file is under
// "%Program Files%/C:\Program Files\Google\Common\Update".
// By default, the logging is on in debug modes and off in opt mode although in
// opt mode only the OPT_LOG statements write to the log.
//
// The log is open to all the users to write to. There is a known vulnerability
// where a DOS can be created by holding on to the logging mutex.
//
// In this module use of ASSERT & REPORT is banned.  This is to prevent any
// possible recursion issues between logging (logging.h) and
// asserting/reporting (debug.h).  Both are basement-level systems that need to
// work when almost nothing else works and interdependencies are best avoided.
// One unavoidable interdependency is that debugASSERT will send messages to
// the logger via Logger::OutputMessage - which then broadcasts it to each
// LogWriter's OutputMessage.  So these methods should be as simple as
// possible.  (Also, unlike asserting/reporting - this module will not avoid
// use of the heap, however the code executed from Logger::OutputMessage
// doesn't use the heap (for all the LogWriters in this file).)
//
// TODO(omaha): implement the minidump handling in terms of breakpad.
//              Log initialization if full of lazy init. Consider doing
//              eager init of log and its registered log writers when the
//              log is created and initialized.
//              Reimplement in terms of smart handles and locks.
//              Reimplement without dependency on any other compilation unit
//               that can call assert, verify, or the log itself.
//              Redo the history logging feature

#include "omaha/common/logging.h"

#include <excpt.h>  // Microsoft specific: structured exceptions.
#include <shlobj.h>
#include <shlwapi.h>
#include <string.h>
#include <atlpath.h>
#include <atlsecurity.h>
#include "base/basictypes.h"
#include "omaha/common/app_util.h"
#include "omaha/common/const_debug.h"
#include "omaha/common/constants.h"
#include "omaha/common/debug.h"
#include "omaha/common/file.h"
#include "omaha/common/path.h"
#include "omaha/common/string.h"
#include "omaha/common/time.h"
#include "omaha/common/utils.h"

namespace omaha {

// enforce ban on ASSERT/REPORT
#undef ASSERT
#undef REPORT

#ifdef LOGGING

#define kNumLockRetries (20)
#define kLockRetryDelayMs (50)

// Circular buffer to log history.
static wchar_t history_buffer[kMaxHistoryBufferSize];

// Index into the history buffer to begin writing at.
static int history_buffer_next_idx = 0;

// Indicates whether the history buffer has ever reached its full capacity.
// Once this boolean is true, if never becomes false.
static bool history_buffer_full = false;

// If you want to log anything else, add it to this structure.  This structure
// basically says that each logged message is composed of two parts, and they
// are output one after the other.  Intended to be used for a message "prefix"
// and the message itself (the prefix can contain the component name, the time,
// and any other logging system boilerplate, while the message is supplied by
// the component).  (This basically saves having to copy a variable length -
// possibly very large - message just to tack it onto the end of the message
// prefix.)
struct OutputInfo {
  LogCategory category;
  LogLevel level;
  const wchar_t* msg1;
  const wchar_t* msg2;

  OutputInfo(LogCategory cat, LogLevel log_level,
             const wchar_t* m1, const wchar_t* m2)
      : category(cat),
        level(log_level),
        msg1(m1),
        msg2(m2) {}
};

//
// Table of category names to categories.
//
#define LC_ENTRY(lc_value)  (L#lc_value), (lc_value)
struct {
  wchar_t* category_name;
  LogCategory category;
} static LogCategoryNames[] = {
  LC_ENTRY(LC_UTIL),
  LC_ENTRY(LC_SETUP),
  LC_ENTRY(LC_SHELL),
  LC_ENTRY(LC_CORE),
  LC_ENTRY(LC_JS),
  LC_ENTRY(LC_PLUGIN),
  LC_ENTRY(LC_SERVICE),
  LC_ENTRY(LC_OPT),
  LC_ENTRY(LC_NET),
};

static CString StripBackslashes(const CString& name) {
  int n = String_FindChar(name, L'\\');
  if (n == -1) {
    return name;
  } else {
    CString result;
    for (int i = 0; i < name.GetLength(); ++i) {
      if (name[i] != L'\\') {
        result += name[i];
      }
    }
    return result;
  }
}

static CString GetProcName() {
  CString proc_name(app_util::GetAppNameWithoutExtension());
  CString module_name(app_util::GetCurrentModuleNameWithoutExtension());
  CString result(proc_name);
  if (module_name.CompareNoCase(proc_name) != 0) {
    result += L":";
    result += module_name;
  }
  return result;
}

// Formats a line prefix with the current time min:sec:milisec if wanted,
// otherwise just the process:module.
static void FormatLinePrefix(bool show_time,
                             const wchar_t* proc_name,
                             CString& result) {
  if (show_time) {
    SYSTEMTIME system_time = {0};
    GetLocalTime(&system_time);
    result.Format(L"[%02d/%02d/%02d %02d:%02d:%02d.%03d]",
                  system_time.wMonth, system_time.wDay, system_time.wYear % 100,
                  system_time.wHour, system_time.wMinute, system_time.wSecond,
                  system_time.wMilliseconds);
  }
  result.AppendFormat(L"[%s][%u:%u]",
                      proc_name,
                      ::GetCurrentProcessId(),
                      ::GetCurrentThreadId());
}

static bool g_logging_valid = false;
static Logging g_logging;

// Singleton factory for the Logging object.
Logging* GetLogging() {
  return g_logging_valid ? &g_logging : NULL;
}

// Force the logging system to be initialized during elaboration of static
// constructors, while the program is still single-threaded.  This global
// static object will cause the static logger inside of GetLogging() to be
// constructed.  However, it might not be the first call to GetLogging() -
// another static object in another object file might get constructed first,
// and in its constructor call a logging API.  Just as long as it is done when
// the system is single threaded.  (Reason: because the Logger object has a
// LLock object which has a Win32 critical section which needs to be
// initialized - only once!)


Logging::Logging()
    : logging_initialized_(false),
      logging_enabled_(true),
      force_show_time_(false),
      show_time_(true),
      log_to_file_(true),
      log_to_debug_out_(true),
      append_to_file_(true),
      logging_shutdown_(false),
      num_writers_(0),
      file_log_writer_(NULL),
      debug_out_writer_(NULL),
      is_initializing_(false),
      log_file_name_(kDefaultLogFileName),
      config_file_path_(GetConfigurationFilePath()) {
  g_last_category_check_time = 0;
  for (int i = 0; i < max_writers; ++i) {
    writers_[i] = NULL;
  }
  proc_name_ = GetProcName();
  g_logging_valid = true;

  // Read initial settings from the config file.
  ReadLoggingSettings();
}

// TODO(omaha): why aren't we using a mutexscope and what if an the code
// throws? Will the lock be unlocked?
Logging::~Logging() {
  // Acquire the lock outside the try/except block so we'll always release it
  lock_.Lock();

  __try {
    // prevent further access to the system
    // necessary because the static destructors happen
    // in a non-deterministic order
    logging_shutdown_ = true;

    // Delete all registered LogWriters
    for (int i = 0; i < num_writers_; ++i) {
      if (writers_[i]) {
        delete writers_[i];
      }
    }

    logging_initialized_ = false;
  } __except(SehNoMinidump(GetExceptionCode(),
                           GetExceptionInformation(),
                           __FILE__,
                           __LINE__,
                           true)) {
    OutputDebugStringA("Unexpected exception in: " __FUNCTION__ "\r\n");
    logging_initialized_  = false;
  }

  g_logging_valid = false;
  lock_.Unlock();
}

void Logging::UpdateCatAndLevel(const wchar_t* cat_name, LogCategory cat) {
  if (cat_name == NULL) {
    return;
  }
  if (cat >= LC_MAX_CAT) {
    return;
  }
  int log_level = kDefaultLogLevel;
  CString config_file = GetCurrentConfigurationFilePath();
  if (!config_file.IsEmpty()) {
    log_level = GetPrivateProfileInt(kConfigSectionLoggingLevel,
                                     cat_name,
                                     kDefaultLogLevel,
                                     config_file);
  }
  category_list_[cat].enabled = (log_level != 0);
  category_list_[cat].log_level = static_cast<LogLevel>(log_level);
}

void Logging::ReadLoggingSettings() {
  CString config_file = GetCurrentConfigurationFilePath();
  if (!config_file.IsEmpty()) {
    logging_enabled_ = ::GetPrivateProfileInt(
        kConfigSectionLoggingSettings,
        kConfigAttrEnableLogging,
        kDefaultLoggingEnabled,
        config_file) == 0 ? false : true;

    show_time_ = ::GetPrivateProfileInt(
        kConfigSectionLoggingSettings,
        kConfigAttrShowTime,
        kDefaultShowTime,
        config_file) == 0 ? false : true;

    log_to_file_ = ::GetPrivateProfileInt(
        kConfigSectionLoggingSettings,
        kConfigAttrLogToFile,
        kDefaultLogToFile,
        config_file) == 0 ? false : true;

    log_to_debug_out_ = ::GetPrivateProfileInt(
        kConfigSectionLoggingSettings,
        kConfigAttrLogToOutputDebug,
        kDefaultLogToOutputDebug,
        config_file) == 0 ? false : true;

    append_to_file_ = ::GetPrivateProfileInt(
        kConfigSectionLoggingSettings,
        kConfigAttrAppendToFile,
        kDefaultAppendToFile,
        config_file) == 0 ? false : true;

    ::GetPrivateProfileString(kConfigSectionLoggingSettings,
                              kConfigAttrLogFilePath,
                              kDefaultLogFileName,
                              CStrBuf(log_file_name_, MAX_PATH),
                              MAX_PATH,
                              config_file);
  } else {
    logging_enabled_ = kDefaultLoggingEnabled;
    show_time_ = kDefaultShowTime;
    log_to_file_ = kDefaultLogToFile;
    log_to_debug_out_ = kDefaultLogToOutputDebug;
    append_to_file_ = kDefaultAppendToFile;
    log_file_name_ = kDefaultLogFileName;
  }

  if (force_show_time_) {
    show_time_ = true;
  }

  // The "default" category is always enabled.
  category_list_[LC_LOGGING].enabled = true;
  category_list_[LC_LOGGING].log_level = LEVEL_ALL;

  // Read each category from the ini file.
  for (size_t i = 0; i < arraysize(LogCategoryNames); ++i) {
    UpdateCatAndLevel(LogCategoryNames[i].category_name,
                      LogCategoryNames[i].category);
  }

  g_last_category_check_time = GetCurrent100NSTime();
}

CString Logging::GetDefaultLogDirectory() const {
  CString path;
  CStrBuf buf(path, MAX_PATH);
  HRESULT hr = ::SHGetFolderPath(NULL,
                                 CSIDL_COMMON_APPDATA,
                                 NULL,
                                 SHGFP_TYPE_CURRENT,
                                 buf);
  if (FAILED(hr)) {
    return L"";
  }
  if (!::PathAppend(buf, OMAHA_REL_LOG_DIR)) {
    return L"";
  }
  return path;
}

CString Logging::GetLogFilePath() const {
  if (log_file_name_.IsEmpty()) {
    return CString();
  }

  if (!ATLPath::IsRelative(log_file_name_)) {
    return log_file_name_;
  }

  CString path = GetDefaultLogDirectory();
  if (path.IsEmpty()) {
    return CString();
  }

  if (!::PathAppend(CStrBuf(path, MAX_PATH), log_file_name_)) {
    return CString();
  }

  return path;
}

// Configures/unconfigures the log writers for the current settings.
bool Logging::ConfigureLogging() {
  // Create the logging file.
  if (log_to_file_ && file_log_writer_ == NULL) {
    CString path = GetLogFilePath();
    if (path.IsEmpty()) {
      return false;
    }

    // Extract the final target directory which will not be what
    // GetDefaultLogDirectory() returns if log_file_name_ is an absolute path.
    CString log_file_dir = GetDirectoryFromPath(path);
    if (!File::Exists(log_file_dir)) {
      if (FAILED(CreateDir(log_file_dir, NULL))) {
        return false;
      }
    }
    file_log_writer_ = FileLogWriter::Create(path, append_to_file_);
    if (file_log_writer_ == NULL) {
      OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: ERROR - "
                                L"Cannot create log writer to %s",
                                proc_name_, path));
    }
  }
  if (log_to_file_ && file_log_writer_ != NULL) {
    InternalRegisterWriter(file_log_writer_);
  }
  if (log_to_debug_out_ && debug_out_writer_ == NULL) {
    debug_out_writer_ = OutputDebugStringLogWriter::Create();
    if (debug_out_writer_ == NULL) {
      OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: ERROR - "
                                L"Cannot create OutputDebugString log writer",
                                proc_name_));
    }
  }
  if (log_to_debug_out_ && debug_out_writer_ != NULL) {
    InternalRegisterWriter(debug_out_writer_);
  }
  return true;
}

void Logging::UnconfigureLogging() {
  if (file_log_writer_ != NULL) {
    InternalUnregisterWriter(file_log_writer_);
  }
  if (debug_out_writer_ != NULL) {
    InternalUnregisterWriter(debug_out_writer_);
  }
}

bool Logging::InternalInitialize() {
  __try {
    if (logging_shutdown_ == true) {
      OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: ERROR - "
                                L"Calling the logging system after "
                                L"it has been shut down \n",
                                GetProcName()));
      return false;
    }

    if (logging_initialized_ == true) {
      return true;
    }
    // If something called by this method is attempting to do logging,
    // just ignore it. The cost/benefit ratio is too high to do otherwise.
    if (is_initializing_) {
      return false;
    }
    is_initializing_ = true;

    // Read the initial settings from the config file.
    ReadLoggingSettings();

    // Initialize logging system if enabled at start.
    if (logging_enabled_) {
      logging_initialized_ = ConfigureLogging();
    }
  } __except(SehNoMinidump(GetExceptionCode(),
                           GetExceptionInformation(),
                           __FILE__,
                           __LINE__,
                           true)) {
    OutputDebugStringA("Unexpected exception in: " __FUNCTION__ "\r\n");
    logging_initialized_  = false;
    return false;
  }

  is_initializing_ = false;
  return true;
}

bool Logging::InitializeLogging() {
  // Double-checked locking idiom is broken, especially on multicore machines.
  // TODO(omaha): understand how this works and fix it.
  if (logging_shutdown_ == true) {
    OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: ERROR - Calling the logging "
                              L"system after it has been shut down \n",
                              GetProcName()));
    return false;
  }

  if (logging_initialized_ == true) {
    return true;
  }

  // Acquire the lock outside the try/except block so we'll always release it.
  __mutexScope(lock_);
  return InternalInitialize();
}

// Enables/disables the logging mechanism. Allows turning logging on/off
// in mid-run.
// TODO(omaha):  same comment as for the destructor.
void Logging::EnableLogging() {
  if (!InitializeLogging()) {
    return;
  }

  // Acquire the lock outside the try/except block so we'll always release it.
  lock_.Lock();

  __try {
    if (!logging_enabled_) {
      ConfigureLogging();
      logging_enabled_ = true;
    }
  } __except(SehNoMinidump(GetExceptionCode(),
                           GetExceptionInformation(),
                           __FILE__,
                           __LINE__,
                           true)) {
    OutputDebugStringA("Unexpected exception in: " __FUNCTION__ "\r\n");
    logging_enabled_  = false;
  }

  lock_.Unlock();
}

void Logging::DisableLogging() {
  if (!InitializeLogging()) {
    return;
  }

  // Acquire the lock outside the try/except block so we'll always release it.
  lock_.Lock();

  __try {
    if (logging_enabled_) {
      logging_enabled_ = false;
      UnconfigureLogging();
    }
  } __except(SehNoMinidump(GetExceptionCode(),
             GetExceptionInformation(),
             __FILE__,
             __LINE__,
             true)) {
    OutputDebugStringA("Unexpected exception in: " __FUNCTION__ "\r\n");
    logging_enabled_  = false;
  }

  lock_.Unlock();
}

// Checks if logging is enabled - and updates logging settings from the
// configuration file every kLogSettingsCheckInterval seconds.
bool Logging::IsLoggingEnabled() {
  if (!InitializeLogging()) {
    return false;
  }

  // Dynamic update - including reading a new value of logging_enabled_.
  bool prev_logging_enabled = logging_enabled_;
  if (GetCurrent100NSTime() >
      g_last_category_check_time + kLogSettingsCheckInterval) {
    ReadLoggingSettings();
  }

  // If enabled state has changed either enable or disable logging.
  if (prev_logging_enabled != logging_enabled_) {
    if (logging_enabled_) {
      EnableLogging();
    } else {
      DisableLogging();
    }
  }

  return logging_enabled_;
}

bool Logging::IsLoggingAlreadyEnabled() const {
  return logging_enabled_;
}

void Logging::ForceShowTimestamp(bool force_show_time) {
  force_show_time_ = show_time_ = force_show_time;
}

// Get category level
LogLevel Logging::GetCatLevel(LogCategory category) const {
  if (!IsLoggingAlreadyEnabled()) {
    return kDefaultLogLevel;
  }

  if (category >= LC_MAX_CAT) {
    return kDefaultLogLevel;
  }

  return category_list_[category].log_level;
}

// Check if logging is enabled for a given category and level
DWORD Logging::IsCatLevelEnabled(LogCategory category, LogLevel level) {
  if (!IsLoggingEnabled()) {
    return 0;
  }

  if (category >= LC_MAX_CAT) {
    return 0;
  }

  // If the config value is to log: then log to all writers.
  if (category_list_[category].enabled &&
      level <= category_list_[category].log_level) {
    return static_cast<DWORD>(all_writers_mask);
  }

  // Check each of the registered loggers to see if they want to override the
  // negative config value.
  DWORD mask = 0;
  for (int i = num_writers_ - 1; i >= 0; --i) {
    mask <<= 1;
    if (writers_[i] && writers_[i]->IsCatLevelEnabled(category, level)) {
      mask |= 1;
    }
  }

  return mask;
}

// TODO(omaha): For now this is hard coded and there is no way to override
// writing other log categories into the history. Add a store_in_history
// boolean into the CategoryInfo struct that allows reading from the config
// file. This will enable other log categories to get buffered in the history.
bool Logging::IsCategoryEnabledForBuffering(LogCategory cat) {
  return cat == LC_REPORT;
}

void Logging::LogMessage(LogCategory cat, LogLevel level,
                         const wchar_t* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  LogMessageVA(cat, level, fmt, args);
  va_end(args);
}

void Logging::LogMessageVA(LogCategory cat, LogLevel level,
                           const wchar_t* fmt, va_list args) {
  LogMessageMaskedVA(static_cast<DWORD>(all_writers_mask),
                     cat,
                     level,
                     fmt,
                     args);
}

void Logging::InternalLogMessageMaskedVA(DWORD writer_mask,
                                         LogCategory cat,
                                         LogLevel level,
                                         CString* log_buffer,
                                         CString* prefix,
                                         const wchar_t* fmt,
                                         va_list args) {
  __try {
    // Initial buffer size in characters.
    // It will adjust dynamically if the message is bigger.
    DWORD buffer_size = 512;

    // Count of chars / bytes written.
    int num_chars = 0;
    bool result = false;

    // Write the message in the buffer.
    // Dynamically adjust the size to hold the entire message.

    while ((num_chars = _vsnwprintf_s(
        log_buffer->GetBufferSetLength(buffer_size),
        buffer_size,
        _TRUNCATE,
        fmt,
        args)) == -1) {
      // Truncate if the message is too big.
      if (buffer_size >= kMaxLogMessageSize) {
        num_chars = buffer_size;
        break;
      }

      // Get a buffer that is big enough.
      buffer_size *= 2;
    }

    log_buffer->ReleaseBuffer(num_chars);

    FormatLinePrefix(show_time_, proc_name_, *prefix);

    // Log the message.
    OutputInfo info(cat, level, *prefix, *log_buffer);
    OutputMessage(writer_mask, &info);
  } __except(SehSendMinidump(GetExceptionCode(),
                             GetExceptionInformation(),
                             kMinsTo100ns)) {
    OutputDebugStringA("Unexpected exception in: " __FUNCTION__ "\r\n");
    OutputDebugString(fmt);
    OutputDebugString(L"\n\r");
  }
}

void Logging::LogMessageMaskedVA(DWORD writer_mask,
                                 LogCategory cat,
                                 LogLevel level,
                                 const wchar_t* fmt,
                                 va_list args) {
  if (!fmt) {
    return;
  }

  if (writer_mask == 0 && level > kMaxLevelToStoreInLogHistory) {
    return;
  }

#pragma warning(push)
// construction of local static object is not thread-safe
#pragma warning(disable : 4640)
  // TODO(omaha): make these class members.
  static CString log_buffer;    // The buffer for formatted log messages.
  static CString prefix;
#pragma warning(pop)

  int i = 0;
  while (++i <= kNumLockRetries) {
    if (lock_.Lock(0)) {
      InternalLogMessageMaskedVA(writer_mask, cat, level, &log_buffer,
                                 &prefix, fmt, args);
      lock_.Unlock();
      break;
    }

    Sleep(kLockRetryDelayMs);
  }

  if (i > kNumLockRetries) {
    OutputDebugStringA("LOG_SYSTEM: Couldn't acquire lock - ");
    OutputDebugString(fmt);
    OutputDebugString(L"\n\r");
  }
}

void Logging::OutputMessage(DWORD writer_mask, LogCategory cat, LogLevel level,
                            const wchar_t* msg1, const wchar_t* msg2) {
  OutputInfo info(cat, level, msg1, msg2);
  OutputMessage(writer_mask, &info);
}

// Store log message in in-memory history buffer.
void Logging::StoreInHistory(const OutputInfo* output_info) {
  AppendToHistory(output_info->msg1);
  AppendToHistory(output_info->msg2);
  AppendToHistory(L"\r\n");
}

// Append string to in-memory history buffer.
// history_buffer_next_idx points to the next index to write at,
// thus it should always be in (0 - kHistoryBufferEndIdx).
void Logging::AppendToHistory(const wchar_t* msg) {
  int msg_len = wcslen(msg);
  if (msg_len == 0) {
    return;
  }

  if (msg_len >= kMaxHistoryBufferSize) {
    // Write the first kMaxHistoryBufferSize chars.
    memcpy(history_buffer, msg, kMaxHistoryBufferSize * sizeof(TCHAR));
    history_buffer_next_idx = 0;
    history_buffer_full = true;
    return;
  }

  // Determine if the message fits into the portion of the buffer after
  // history_buffer_next_idx.
  if (msg_len + history_buffer_next_idx < kMaxHistoryBufferSize) {
    memcpy(history_buffer + history_buffer_next_idx, msg,
           msg_len * sizeof(TCHAR));
    history_buffer_next_idx += msg_len;
    return;
  }

  // Have to split the input message into the part that fits in
  // history_buffer_next_idx to kMaxHistoryBufferSize and the remaining message.
  int msg_first_part_len = kMaxHistoryBufferSize - history_buffer_next_idx;
  int msg_second_part_len = msg_len - msg_first_part_len;
  memcpy(history_buffer + history_buffer_next_idx,
         msg,
         msg_first_part_len * sizeof(TCHAR));

  history_buffer_full = true;
  history_buffer_next_idx = msg_second_part_len;
  if (msg_second_part_len) {
    memcpy(history_buffer,
           msg + msg_first_part_len,
           msg_second_part_len * sizeof(TCHAR));
  }
}

// Retrieve in-memory history buffer.
CString Logging::GetHistory() {
  CString history;

  if (history_buffer_full) {
    history.Append(history_buffer + history_buffer_next_idx,
                   kMaxHistoryBufferSize - history_buffer_next_idx);
  }
  history.Append(history_buffer, history_buffer_next_idx);

  // Reset the history buffer to the original state.
  history_buffer_next_idx = 0;
  history_buffer_full = false;
  memset(history_buffer, 0, kMaxHistoryBufferSize * sizeof(TCHAR));

  return history;
}

void Logging::OutputMessage(DWORD writer_mask,
                            const OutputInfo* output_info) {
  if (output_info->level <= kMaxLevelToStoreInLogHistory &&
      IsCategoryEnabledForBuffering(output_info->category)) {
    StoreInHistory(output_info);
  }

  for (int i = 0; i < num_writers_; ++i) {
    if (writer_mask & 1) {
      __try {
        if (logging_enabled_ || writers_[i]->WantsToLogRegardless()) {
          writers_[i]->OutputMessage(output_info);
        }
      }
      __except(SehNoMinidump(GetExceptionCode(),
                             GetExceptionInformation(),
                             __FILE__,
                             __LINE__,
                             true)) {
        // Just eat errors that happen from within the LogWriters.  This is
        // important so that if such an error happens when OutputMessage is
        // called from debugASSERT we don't go recursively into more
        // error handling ...
      }
    }
    writer_mask >>= 1;
  }
}

bool Logging::InternalRegisterWriter(LogWriter* log_writer) {
  if (num_writers_ >= max_writers) {
    return false;
  }
  writers_[num_writers_++] = log_writer;
  return true;
}

bool Logging::RegisterWriter(LogWriter* log_writer) {
  if (!InternalRegisterWriter(log_writer)) {
    return false;
  }
  if (log_writer->WantsToLogRegardless()) {
    EnableLogging();
  }
  return true;
}

bool Logging::InternalUnregisterWriter(LogWriter* log_writer) {
  bool result = false;
  for (int i = 0; i < num_writers_; ++i) {
    if (writers_[i] == log_writer) {
      // Replace this entry with last entry in array, then truncate.
      writers_[i] = writers_[--num_writers_];
      result = true;
      break;
    }
  }
  return result;
}

bool Logging::UnregisterWriter(LogWriter* log_writer) {
  if (!InternalUnregisterWriter(log_writer)) {
    return false;
  }
  if (num_writers_ == 0) {
    DisableLogging();
  }
  return true;
}

// The primary configuration file under %PROGRAMFILES%\Google\Update is
// removed on uninstall. This is midly inconvenient during development
// therefore a fallback location for the configuration file is desired.
CString Logging::GetCurrentConfigurationFilePath() const {
  if (!config_file_path_.IsEmpty() &&
      File::Exists(config_file_path_)) {
    return config_file_path_;
  } else {
    return L"";
  }
}

CString Logging::GetConfigurationFilePath() const {
  CString file_path;
  CString system_drive = GetEnvironmentVariableAsString(_T("SystemDrive"));
  if (!system_drive.IsEmpty()) {
    file_path = system_drive;
    file_path += L"\\";
  }
  return file_path + kFilePrefix L".ini";
}

LogWriter::LogWriter() {
}

LogWriter::~LogWriter() {
}

void LogWriter::Cleanup() {}

bool LogWriter::WantsToLogRegardless() const { return false; }

bool LogWriter::IsCatLevelEnabled(LogCategory, LogLevel) const {
  return false;
}

void LogWriter::OutputMessage(const OutputInfo*) { }

bool LogWriter::Register() {
  Logging* logger = GetLogging();
  if (logger) {
    return logger->RegisterWriter(this);
  } else {
    return false;
  }
}

bool LogWriter::Unregister() {
  Logging* logger = GetLogging();
  if (logger) {
    return logger->RegisterWriter(this);
  } else {
    return false;
  }
}

// FileLogWriter

FileLogWriter* FileLogWriter::Create(const wchar_t* file_name, bool append) {
  return new FileLogWriter(file_name, append);
}

FileLogWriter::FileLogWriter(const wchar_t* file_name, bool append)
    : initialized_(false),
      valid_(false),
      file_name_(file_name),
      log_file_mutex_(NULL),
      log_file_(NULL),
      append_(append),
      max_file_size_(kDefaultMaxLogFileSize),
      log_file_wide_(kDefaultLogFileWide) {
  Logging* logger = GetLogging();
  if (logger) {
    CString config_file_path = logger->GetCurrentConfigurationFilePath();
    if (!config_file_path.IsEmpty()) {
        max_file_size_ = ::GetPrivateProfileInt(
            kConfigSectionLoggingSettings,
            kConfigAttrMaxLogFileSize,
            kDefaultMaxLogFileSize,
            config_file_path);
        log_file_wide_ = ::GetPrivateProfileInt(
            kConfigSectionLoggingSettings,
            kConfigAttrLogFileWide,
            kDefaultLogFileWide,
            config_file_path) == 0 ? false : true;
    } else {
      max_file_size_ = kDefaultMaxLogFileSize;
      log_file_wide_ = kDefaultLogFileWide;
    }
    proc_name_ = logger->proc_name();
  }
}

FileLogWriter::~FileLogWriter() {
  // TODO(omaha): Figure out a way to pass the proc_name - and possibly
  // the show_time var - into here.
  Logging* logger = GetLogging();
  if (logger && logger->IsLoggingAlreadyEnabled()) {
    // OutputInfo info(LEVEL_WARNING, NULL, kEndOfLogMessage);
    // OutputMessage(&info);
  }
  Cleanup();
}

void FileLogWriter::Initialize() {
  if (initialized_) {
    return;
  }

  initialized_ = true;

  bool already_created = CreateLoggingMutex();
  if (!log_file_mutex_) {
    return;
  }

  if (already_created) {
    append_ = true;
  }

  if (!GetMutex()) {
    ::OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: "
                                L"Could not acquire logging mutex %s\n",
                                proc_name_,
                                log_file_mutex_name_));
    return;
  }

  CreateLoggingFile();
  if (!log_file_) {
    ::OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: "
                                L"Could not create logging file %s\n",
                                proc_name_,
                                file_name_));
    valid_ = false;
  }

  valid_ = true;
  ReleaseMutex();
}

void FileLogWriter::Cleanup() {
  if (log_file_) {
    ::CloseHandle(log_file_);
  }
  if (log_file_mutex_) {
    ::ReleaseMutex(log_file_mutex_);
    ::CloseHandle(log_file_mutex_);
  }
}

bool FileLogWriter::CreateLoggingMutex() {
  log_file_mutex_name_ = StripBackslashes(kLoggingMutexName L"_" + file_name_);
  // TODO(omaha): I don't see where this class is used, but I guess the
  // caller is always in the same context. We should use the default security
  // here.  If the caller can be in different contexts (System Service, Usermode
  // applications, etc), then we should revisit this code to give access only
  // to those who really need it.  What if a malicious piece a code decide
  // to get the mutex and lock it? If it happens, everytime you try to log
  // something, the thread would hang for 500ms and then fail to log the
  // message.
  CSecurityAttributes sa;
  GetEveryoneDaclSecurityAttributes(&sa, GENERIC_ALL);
  log_file_mutex_ = CreateMutexWithSyncAccess(log_file_mutex_name_, &sa);
  if (log_file_mutex_) {
    return ERROR_ALREADY_EXISTS == ::GetLastError();
  }
  return false;
}

bool FileLogWriter::CreateLoggingFile() {
  uint32 file_size(0);
  File::GetFileSizeUnopen(file_name_, &file_size);
  if (file_size > max_file_size_) {
    ArchiveLoggingFile();
  }
  log_file_ = ::CreateFile(file_name_,
                           GENERIC_WRITE,
                           FILE_SHARE_WRITE | FILE_SHARE_READ,
                           NULL,
                           append_ ? OPEN_ALWAYS : CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL,
                           NULL);
  if (log_file_ == INVALID_HANDLE_VALUE) {
    // The code in this file is written with the assumption that log_file_ is
    // NULL on creation errors. The easy fix is to set it to NULL here. The
    // long term fix should be implementing it in terms of a smart handle.
    log_file_ = NULL;
    return false;
  }

  // Allow users to read, write, and delete the log file.
  ACCESS_MASK mask = GENERIC_READ | GENERIC_WRITE | DELETE;
  CDacl dacl;
  if (dacl.AddAllowedAce(ATL::Sids::Users(), mask)) {
    AtlSetDacl(file_name_, SE_FILE_OBJECT, dacl);
  }

  // Insert a BOM in the newly created file.
  if (GetLastError() != ERROR_ALREADY_EXISTS && log_file_wide_) {
    DWORD num = 0;
    ::WriteFile(log_file_, &kUnicodeBom, sizeof(kUnicodeBom), &num, NULL);
  }
  return true;
}

bool FileLogWriter::TruncateLoggingFile() {
  DWORD share_mode = FILE_SHARE_WRITE;
  HANDLE log_file = ::CreateFile(file_name_,
                                 GENERIC_WRITE,
                                 FILE_SHARE_WRITE | FILE_SHARE_READ,
                                 NULL,
                                 TRUNCATE_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
  if (log_file_ == INVALID_HANDLE_VALUE) {
    return false;
  }

  // Insert a BOM in the newly created file.
  if (log_file_wide_) {
    DWORD num = 0;
    ::WriteFile(log_file, &kUnicodeBom, sizeof(kUnicodeBom), &num, NULL);
  }
  ::CloseHandle(log_file);
  return true;
}

bool FileLogWriter::ArchiveLoggingFile() {
  ::OutputDebugString(L"LOG_SYSTEM: trying to move log file to backup\n");
  CString backup_file_name = file_name_ + L".bak";
  HRESULT hr = File::Move(file_name_, backup_file_name, true);
  if (FAILED(hr)) {
    ::OutputDebugString(L"LOG_SYSTEM: failed to move log file to backup\n");

    // Trying to move the log file when loggers have it open returns
    // ERROR_SHARING_VIOLATION. Each call to MoveFileAfterReboot inserts the
    // file into PendingFileRenames list. Moving files at reboot requires the
    // user to be either the LocalSystem account or in the Administrators
    // group.
    if (!IsArchivePending()) {
      File::MoveAfterReboot(file_name_, backup_file_name);
    }
    return false;
  }
  return true;
}

bool FileLogWriter::IsArchivePending() {
  // We look at the PendingFileRenameOperations to see if our log file is
  // pending a rename. The list is a REG_MULTI_SZ, which is a sequence of
  // null-terminated strings, terminated by an empty string "\0".
  // The strings have the structure:
  // \??\file1\0!\??\file2\0\file3\0\0...\0\0. where file1 is to be renamed to
  // file2 and file3 is to be deleted.
  // It is valid for the PFR list to include an empty string in the middle
  // of the sequence.
  const wchar_t sub_key_name[] = L"SYSTEM\\CurrentControlSet\\Control\\"
                                 L"Session Manager";
  HKEY key = NULL;
  int res = ::RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                            sub_key_name,
                            0,
                            KEY_READ,
                            &key);
  if (res != ERROR_SUCCESS) {
    return false;
  }
  DWORD bytes = 0;
  DWORD type = REG_MULTI_SZ;
  res = ::RegQueryValueEx(key,
                          L"PendingFileRenameOperations",
                          0,
                          &type,
                          NULL,
                          &bytes);
  if (!(res == ERROR_SUCCESS && type == REG_MULTI_SZ)) {
    return false;
  }
  scoped_array<byte> buf(new byte[bytes]);
  memset(buf.get(), 0, bytes);
  res = ::RegQueryValueEx(key,
                          L"PendingFileRenameOperations",
                          0,
                          NULL,
                          buf.get(),
                          &bytes);
  if (res != ERROR_SUCCESS) {
    return false;
  }
  const wchar_t* multi_str = reinterpret_cast<const wchar_t*>(buf.get());
  size_t count = bytes / sizeof(*multi_str);
  const size_t kMaxRegistryValueLen = 1024 * 1024;  // 1MB
  if (!(count >= 2 &&
        count < kMaxRegistryValueLen &&
        multi_str[count - 2] == L'\0' &&
        multi_str[count - 1] == L'\0')) {
    return false;
  }
  // The file names in the PFR list are prefixed by \??\.
  CString file_name = L"\\??\\" + file_name_;
  return FindFirstInMultiString(multi_str, count, file_name) != -1;
}

int FileLogWriter::FindFirstInMultiString(const wchar_t* multi_str,
                                          size_t count,
                                          const wchar_t* str) {
  const wchar_t* p = multi_str;
  size_t i = 0;
  while (i < count) {
    p =  multi_str + i;
    if (lstrcmp(p, str) == 0) {
      return i;
    } else {
      size_t len = lstrlen(p);
      i += len + 1;
    }
  }
  return -1;
}

void FileLogWriter::OutputMessage(const OutputInfo* output_info) {
  if (!initialized_) {
    Initialize();
  }

  if (!valid_) {
    return;
  }

  // Acquire the mutex.
  if (!GetMutex()) {
    return;
  }

  // Move to end of file.
  DWORD pos = ::SetFilePointer(log_file_, 0, NULL, FILE_END);
  int64 stop_gap_file_size = kStopGapLogFileSizeFactor *
                             static_cast<int64>(max_file_size_);
  if (pos >= stop_gap_file_size) {
    if (!TruncateLoggingFile()) {
      // Logging stops until the log can be archived over since we do not
      // want to overfill the disk.
      return;
    }
  }
  pos = ::SetFilePointer(log_file_, 0, NULL, FILE_END);

  // Write the date, followed by a CRLF
  DWORD written_size = 0;
  if (output_info->msg1) {
    if (log_file_wide_) {
      ::WriteFile(log_file_, output_info->msg1,
                  lstrlen(output_info->msg1) * sizeof(wchar_t), &written_size,
                  NULL);
    } else {
      CStringA msg(WideToAnsiDirect(output_info->msg1));
      ::WriteFile(log_file_, msg.GetString(), msg.GetLength(), &written_size,
                  NULL);
    }
  }

  if (output_info->msg2) {
    if (log_file_wide_) {
      ::WriteFile(log_file_, output_info->msg2,
                  lstrlen(output_info->msg2) * sizeof(wchar_t), &written_size,
                  NULL);
    } else {
      CStringA msg(WideToAnsiDirect(output_info->msg2));
      ::WriteFile(log_file_, msg.GetString(), msg.GetLength(), &written_size,
                  NULL);
    }
  }

  if (log_file_wide_) {
    ::WriteFile(log_file_, L"\r\n", 2 * sizeof(wchar_t), &written_size, NULL);
  } else {
    ::WriteFile(log_file_, "\r\n", 2, &written_size, NULL);
  }

  ReleaseMutex();
}

bool FileLogWriter::GetMutex() {
  if (!log_file_mutex_) {
    return false;
  }

  DWORD res = ::WaitForSingleObject(log_file_mutex_, kMaxMutexWaitTimeMs);
  if (res != WAIT_OBJECT_0 && res != WAIT_ABANDONED) {
    ::OutputDebugString(SPRINTF(L"LOG_SYSTEM: [%s]: "
                                L"Could not acquire logging mutex %s\n",
                                proc_name_, log_file_mutex_name_));
    valid_ = false;
    return false;
  }

  return true;
}

void FileLogWriter::ReleaseMutex() {
  if (log_file_mutex_) {
    ::ReleaseMutex(log_file_mutex_);
  }
}

// OutputDebugStringLogWriter.
OutputDebugStringLogWriter* OutputDebugStringLogWriter::Create() {
  return new OutputDebugStringLogWriter();
}

OutputDebugStringLogWriter::OutputDebugStringLogWriter() {}

OutputDebugStringLogWriter::~OutputDebugStringLogWriter() {
  Logging* logger = GetLogging();
  if (logger && logger->IsLoggingAlreadyEnabled()) {
    // OutputInfo info(LEVEL_WARNING, NULL, kEndOfLogMessage);
    // OutputMessage(&info);
  }
  Cleanup();
}

void OutputDebugStringLogWriter::OutputMessage(const OutputInfo* output_info) {
  // Combine everything into one string so that messages coming from
  // multiple threads don't get interleaved.
  ::OutputDebugString(SPRINTF(L"%s%s\n", output_info->msg1, output_info->msg2));
}

// OverrideConfigLogWriter.
OverrideConfigLogWriter* OverrideConfigLogWriter::Create(LogCategory category,
    LogLevel level, LogWriter* log_writer, bool force_logging_enabled) {
  return new OverrideConfigLogWriter(category,
                                     level,
                                     log_writer,
                                     force_logging_enabled);
}

OverrideConfigLogWriter::OverrideConfigLogWriter(LogCategory category,
                                                 LogLevel level,
                                                 LogWriter* log_writer,
                                                 bool force_logging_enabled)
    : category_(category),
      level_(level),
      log_writer_(log_writer),
      force_logging_enabled_(force_logging_enabled) {}

void OverrideConfigLogWriter::Cleanup() {
  if (log_writer_) {
    delete log_writer_;
  }
}

bool OverrideConfigLogWriter::WantsToLogRegardless() const {
  return force_logging_enabled_;
}

bool OverrideConfigLogWriter::IsCatLevelEnabled(LogCategory category,
                                                LogLevel level) const {
  if (category != category_) {
    return false;
  }
  if (level > level_) {
    return false;
  }
  return true;
}

void OverrideConfigLogWriter::OutputMessage(const OutputInfo* output_info) {
  if (log_writer_) {
    log_writer_->OutputMessage(output_info);
  }
  return;
}

}  // namespace omaha

#endif  // LOGGING

