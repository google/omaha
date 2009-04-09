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
// logging.h
//
// Tracing and logging system.
// Allows filtering of the log messages based on logging categories and levels.

#ifndef OMAHA_COMMON_LOGGING_H__
#define OMAHA_COMMON_LOGGING_H__

#include "omaha/common/commontypes.h"
#include "omaha/common/synchronized.h"

#ifdef LOGGING

// Logging levels.
enum LogLevel {
  LEVEL_FATALERROR = -3,      // crashing fatal error
  LEVEL_ERROR      = -2,      // errors - recoverable but shouldn't happen
  LE               = -2,
  LEVEL_WARNING    = -1,      // warnings
  LW               = -1,
  L1               =  1,      // for aprox. 10 logs per run
  L2,                         // for aprox. 100 logs per run
  L3,                         // for aprox. 1,000 logs per run
  L4,                         // for aprox. 10,000 logs per run
  L5,                         // for aprox. 100,000 logs per run
  L6,                         // for > 1,000,000 logs per run

  // add above
  LEVEL_ALL                   // all errors
};

#endif

namespace omaha {

#define kDefaultLogFileName             kFilePrefix L".log"
#define kDefaultLogFileWide             1
#define kDefaultLogToFile               1
#define kDefaultShowTime                1
#define kDefaultAppendToFile            1

#ifdef _DEBUG
#define kDefaultMaxLogFileSize          0xFFFFFFFF  // 4GB
#define kDefaultLogToOutputDebug        1
#define kDefaultLogLevel                L3
#define kDefaultLoggingEnabled          1
#else
#define kDefaultMaxLogFileSize          10000000    // 10MB
#define kDefaultLogToOutputDebug        0
#define kDefaultLogLevel                L1
#define kDefaultLoggingEnabled          0
#endif

// Truncates the log file when the size of the log file is this many
// times over the MaxLogFileSize to prevent disk overfill.
#define kStopGapLogFileSizeFactor       10

// config file sections
#define kConfigSectionLoggingLevel      L"LoggingLevel"
#define kConfigSectionLoggingSettings   L"LoggingSettings"

// config file attributes
#define kConfigAttrEnableLogging        L"EnableLogging"
#define kConfigAttrShowTime             L"ShowTime"
#define kConfigAttrLogToFile            L"LogToFile"
#define kConfigAttrLogFilePath          L"LogFilePath"
#define kConfigAttrLogFileWide          L"LogFileWide"
#define kConfigAttrLogToOutputDebug     L"LogToOutputDebug"
#define kConfigAttrAppendToFile         L"AppendToFile"
#define kConfigAttrMaxLogFileSize       L"MaxLogFileSize"

#define kLoggingMutexName               kLockPrefix L"logging_mutex"
#define kMaxMutexWaitTimeMs             500

// Does not allow messages bigger than 1 MB.
#define kMaxLogMessageSize              (1024 * 1024)

#define kLogSettingsCheckInterval       (5 * kSecsTo100ns)

#define kStartOfLogMessage \
    L"********************* NEW LOG *********************"
#define kEndOfLogMessage   \
    L"********************* END LOG *********************"

// TODO(omaha): Allow these defaults to be overriden in the config file.
#define kMaxLevelToStoreInLogHistory L2
#define kMaxHistoryBufferSize 1024

#ifdef LOGGING

#define LC_LOG(cat, level, msg) \
  do {                                                     \
    omaha::Logging* logger = omaha::GetLogging();          \
    if (logger) {                                          \
      omaha::LoggingHelper(logger, cat, level,             \
        logger->IsCatLevelEnabled(cat, level)) msg;        \
    }                                                      \
  } while (0)

#define LC_LOG_OPT(cat, level, msg)   LC_LOG(cat, level, msg)

#else
#define LC_LOG(cat, level, msg)   ((void)0)
#endif

#ifdef _DEBUG
#define LC_LOG_DEBUG(cat, level, msg) LC_LOG(cat, level, msg)
#else
#define LC_LOG_DEBUG(cat, level, msg) ((void)0)
#endif

// Shortcuts for different logging categories - no need to specify the category.
#define CORE_LOG(x, y)         LC_LOG_DEBUG(omaha::LC_CORE, x, y)
#define NET_LOG(x, y)          LC_LOG_DEBUG(omaha::LC_NET, x, y)
#define PLUGIN_LOG(x, y)       LC_LOG_DEBUG(omaha::LC_PLUGIN, x, y)
#define SERVICE_LOG(x, y)      LC_LOG_DEBUG(omaha::LC_SERVICE, x, y)
#define SETUP_LOG(x, y)        LC_LOG_DEBUG(omaha::LC_SETUP, x, y)
#define SHELL_LOG(x, y)        LC_LOG_DEBUG(omaha::LC_SHELL, x, y)
#define UTIL_LOG(x, y)         LC_LOG_DEBUG(omaha::LC_UTIL, x, y)

#define OPT_LOG(x, y)          LC_LOG_OPT(omaha::LC_OPT, x, y)
#define REPORT_LOG(x, y)       LC_LOG_OPT(omaha::LC_REPORT, x, y)

#ifdef LOGGING

// Logging components.
// Maximum 32 categories unless mask is increased to 64 bits.
enum LogCategory {
  LC_LOGGING = 0,

  // ADD BELOW - AND REMEMBER:
  //   - add a line to the LogCategoryNames table in logging.cpp!!!
  //   - add to C:\GoogleUpdate.ini

  LC_UTIL,
  LC_SETUP,
  LC_SHELL,
  LC_CORE,
  LC_JS,
  LC_PLUGIN,
  LC_SERVICE,
  LC_OPT,
  LC_NET,
  LC_REPORT,

  // ADD ABOVE

  LC_MAX_CAT
};

#define kCatEnabledField      L"Enabled"
#define kCatLevelField        L"Level"

struct CategoryInfo {
  bool enabled;
  LogLevel log_level;
};

struct OutputInfo;

// The LogWriter - can decide whether to process message or not, then
// will process it.  Actually, the message is processed if either a) the
// individual LogWriter wants to process it or b) it is marked as processable
// by settings in config ini.
//
// Included LogWriters:
//   OutputDebugStringLogWriter - Logs to OutputDebugString() API
//   FileLogWriter - Logs to a file
//   OverrideConfigLogWriter - Overrides the level settings of a
//     particular category, uses another writer to actually do the writing.
//     Used, e.g., in installer to force SETUP_LOG messages to go to a file
//     tr_setup_log.info even if the there is no trconfig.ini file.
//
// Not included LogWriters:
//   StdLogWriter - Logs to stdout or stderr
//   SubmitToGoogleLogWriter - When done logging submits the log file to
//     Google's status-receiving server
class LogWriter {
 protected:
  LogWriter();
  virtual void Cleanup();
 public:
  virtual ~LogWriter();

  // Returns true if this Logging object wants to log even if the global
  // "enable logging" flag is off.  Useful for always creating a log, e.g., an
  // install log, even without a GoogleUpdate.ini.
  virtual bool WantsToLogRegardless() const;

  // Returns true if this Logging object wants to handle the message,
  // regardless of other settings.
  virtual bool IsCatLevelEnabled(LogCategory category, LogLevel level) const;

  virtual void OutputMessage(const OutputInfo* output_info);

  // Registers and unregisters this LogWriter with the Logging system.  When
  // registered, the Logging class assumes ownership.
  bool Register();
  bool Unregister();

 private:
  DISALLOW_EVIL_CONSTRUCTORS(LogWriter);
};

// A LogWriter that writes to a named file.
class FileLogWriter : public LogWriter {
 protected:
  FileLogWriter(const wchar_t* file_name, bool append);
  ~FileLogWriter();
  virtual void Cleanup();

 public:
  static FileLogWriter* Create(const wchar_t* file_name, bool append);
  virtual void OutputMessage(const OutputInfo* output_info);

 private:
  void Initialize();
  bool CreateLoggingMutex();
  bool CreateLoggingFile();
  bool ArchiveLoggingFile();
  bool TruncateLoggingFile();
  bool GetMutex();
  void ReleaseMutex();

  // Returns true if archiving of the log file is pending a computer restart.
  bool IsArchivePending();

  // Returns the first position of str inside of a MULTI_SZ of count characters
  // including the terminating zeros.
  static int FindFirstInMultiString(const wchar_t* multi_str,
                                    size_t count,
                                    const wchar_t* str);

  uint32 max_file_size_;
  bool initialized_;
  bool valid_;
  bool append_;
  bool log_file_wide_;
  CString log_file_mutex_name_;
  HANDLE log_file_mutex_;
  CString file_name_;
  HANDLE log_file_;
  CString proc_name_;

  friend class FileLogWriterTest;

  DISALLOW_EVIL_CONSTRUCTORS(FileLogWriter);
};

// A LogWriter that uses OutputDebugString() to write messages.
class OutputDebugStringLogWriter : public LogWriter {
 protected:
  OutputDebugStringLogWriter();
  ~OutputDebugStringLogWriter();
 public:
  static OutputDebugStringLogWriter* Create();
  virtual void OutputMessage(const OutputInfo* info);
 private:
  DISALLOW_EVIL_CONSTRUCTORS(OutputDebugStringLogWriter);
};

// A LogWriter that overrides the settings in trconfig.ini and sends messages
// to another LogWriter.  Takes ownership of the other LogWriter.
class OverrideConfigLogWriter : public LogWriter {
 protected:
  OverrideConfigLogWriter(LogCategory category, LogLevel level,
                          LogWriter* log_writer, bool force_logging_enabled);
  virtual void Cleanup();
 public:
  static OverrideConfigLogWriter* Create(LogCategory category, LogLevel level,
    LogWriter* log_writer, bool force_logging_enabled);
  virtual bool WantsToLogRegardless() const;
  virtual bool IsCatLevelEnabled(LogCategory category, LogLevel level) const;
  virtual void OutputMessage(const OutputInfo* output_info);
 private:
  LogCategory category_;
  LogLevel level_;
  LogWriter* log_writer_;
  bool force_logging_enabled_;
  DISALLOW_EVIL_CONSTRUCTORS(OverrideConfigLogWriter);
};

// The Logging class - Singleton class
// Fine-grain logging based on categories and levels.
// Can log to a file, stdout or debugger.
class Logging {
 public:
  // constructor
  Logging();

  // destructor
  ~Logging();

  // Enables/disables the logging mechanism.  Allows turning logging on/off
  // in mid-run.  Returns true for success (not for 'logging enabled').
  void EnableLogging();
  void DisableLogging();

  // Checks if logging is enabled - and updates logging settings from the
  // configuration file every kLogSettingsCheckInterval seconds
  bool IsLoggingEnabled();

  // Checks if logging is already enabled. It does not try to enable it.
  bool IsLoggingAlreadyEnabled() const;

  // Overrides the config file settings for showing the time stamps.
  void ForceShowTimestamp(bool force_show_time);

  // Checks if logging is enabled for a given category and level.
  DWORD IsCatLevelEnabled(LogCategory category, LogLevel level);
  LogLevel GetCatLevel(LogCategory category) const;

  // Logs a message.
  void LogMessage(LogCategory cat, LogLevel level, const wchar_t* fmt, ...);
  void LogMessageVA(LogCategory cat, LogLevel level, const wchar_t* fmt,
                    va_list args);

  // Retrieves log directory.
  CString GetLogDirectory() const;

  // Retrieves in-memory history buffer.
  CString GetHistory();

  // Returns the file path of the current GoogleUpdate.ini.
  CString GetCurrentConfigurationFilePath() const;

  const CString& proc_name() const { return proc_name_; }

  bool IsCategoryEnabledForBuffering(LogCategory cat);
 private:
  bool InternalInitialize();
  void InternalLogMessageMaskedVA(DWORD writer_mask,
                                  LogCategory cat,
                                  LogLevel level,
                                  CString* log_buffer,
                                  CString* prefix,
                                  const wchar_t* fmt,
                                  va_list args);

  friend class LoggingHelper;
  void LogMessageMaskedVA(DWORD writer_mask, LogCategory cat, LogLevel level,
                          const wchar_t* fmt, va_list args);

  // Stores log message in in-memory history buffer.
  void StoreInHistory(const OutputInfo* output_info);

  // Appends string to in-memory history buffer.
  void AppendToHistory(const wchar_t* msg);

  // Initializes the logging engine. Harmless to call multiple times.
  bool InitializeLogging();

  // Configures/unconfigures the log writers for the current settings.  That
  // is, given the current settings from GoogleUpdate.ini, either initializes
  // and registers the file-out and debug-out logwriters, or unregisters them.
  bool ConfigureLogging();
  void UnconfigureLogging();

  void UpdateCatAndLevel(const wchar_t* cat_name, LogCategory cat);
  void ReadLoggingSettings();

  // Returns the primary file path of the GoogleUpdate.ini.
  CString GetConfigurationFilePath() const;

  // Returns the alternate file path of the GoogleUpdate.ini.
  CString GetAltConfigurationFilePath() const;

 public:

  // Passes the messages along to other OutputMessage()
  void OutputMessage(DWORD writer_mask, LogCategory cat, LogLevel level,
                     const wchar_t* msg1, const wchar_t* msg2);

  // Broadcasts the message to each LogWriter.
  // It should be private but the function we want to be able to use this,
  // debugASSERT is extern "C" and thus can't be declared a friend of
  // Logging.
  void OutputMessage(DWORD writer_mask, const OutputInfo* output_info);

 private:

  CategoryInfo category_list_[LC_MAX_CAT];

  // Checks if logging is initialized.
  bool logging_initialized_;

  // Is logging in the process of initializing?
  bool is_initializing_;

  // The logging process name including the calling module.
  CString proc_name_;

  // Serializes changing logging init/uninit/enable/disable status.
  LLock lock_;

  // Bunch of settings from the config .ini file.
  bool logging_enabled_;     // Checks if logging is enabled.
  bool force_show_time_;
  bool show_time_;
  bool log_to_file_;
  CString log_file_name_;
  bool log_to_debug_out_;
  bool append_to_file_;

  // Signals the logging system is shutting down.
  bool logging_shutdown_;

  // Checkpoint time for dynamic category updates.
  time64 g_last_category_check_time;

  // The file path of the optional ini file which defines the logging
  // configuration.
  CString config_file_path_;

 private:
  bool InternalRegisterWriter(LogWriter* log_writer);
  bool InternalUnregisterWriter(LogWriter* log_writer);

 public:
  bool RegisterWriter(LogWriter* log_writer);
  bool UnregisterWriter(LogWriter* log_writer);
  enum { all_writers_mask = -1 };

 private:
  enum { max_writers = 15 };
  int num_writers_;
  LogWriter* writers_[max_writers];

  LogWriter* file_log_writer_;
  LogWriter* debug_out_writer_;

  friend class HistoryTest;

  DISALLOW_EVIL_CONSTRUCTORS(Logging);
};

// In order to make the logging macro LC_LOG work out we need to pass a
// parameter (the mask of loggers to write to) (*) to the actual logging
// method. However, the last parameter to the macro LC_LOG has its own
// parenthesis - it encloses multiple expressions (a format string and
// arguments). So this function object is used as an intermediary in order to
// hold the writer mask.
//
// (*) The mask needs to be transferred separately because we want to keep the
// LC_LOG structure of asking if the message is going to be logged before
// evaluating the arguments, and we can't store it in the singleton Logging
// object - wouldn't be thread-safe.

class LoggingHelper {
 public:
  LoggingHelper(Logging* logger, LogCategory cat,
                LogLevel level, DWORD writer_mask)
      : logger_(logger),
        category_(cat),
        level_(level),
        writer_mask_(writer_mask) {}

  void operator()(const wchar_t* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    logger_->LogMessageMaskedVA(writer_mask_, category_, level_, fmt, args);
    va_end(args);
  }

 private:
  Logging* logger_;
  DWORD writer_mask_;
  LogLevel level_;
  LogCategory category_;

  DISALLOW_EVIL_CONSTRUCTORS(LoggingHelper);
};

// Getter for the Logging singleton class.
Logging* GetLogging();

#endif  // LOGGING

}  // namespace omaha

#endif  // OMAHA_COMMON_LOGGING_H__
