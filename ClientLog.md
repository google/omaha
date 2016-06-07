# Introduction #

Omaha can be configured to produce a log. This page provides instructions for enabling the log and locating it.

# Location #

The log file location depends on the OS.
  * **Windows XP**: `C:\Documents and Settings\All Users\Application Data\Google\Update\Log`
  * **Windows Vista** and **Windows 7**: `C:\ProgramData\Google\Update\Log`

# Enabling Logging on Production (opt-win) Builds #

opt-win builds, such as the Google Update builds released to the public, support a limited set of logging (`OPT_LOG` statements in the code), which is disabled by default. To enable logging on these builds, create a file called `C:\GoogleUpdate.ini` with the following contents.

```
[LoggingLevel]
LC_OPT=1

[LoggingSettings]
EnableLogging=1
```

# Enabling Logging on Debug Builds #
Non-opt builds (dbg-win and coverage-win) allow provide much more logging and have level 3 enabled for all categories by default. The default logging can be modified by specifying an override in `C:\GoogleUpdate.ini`. Below is an example that overrides the default.

```
[LoggingLevel]
LC_CORE=5
LC_NET=4
LC_PLUGIN=3
LC_SERVICE=3
LC_SETUP=3
LC_SHELL=3
LC_UTIL=3

LC_OPT=3
LC_REPORT=3

[LoggingSettings]
EnableLogging=1
LogFilePath="C:\foo\GoogleUpdate.log"
MaxLogFileSize=10000000

ShowTime=1
LogToFile=1
AppendToFile=1
LogToStdOut=0
LogToOutputDebug=1

[DebugSettings]
SkipServerReport=1
NoSendDumpToServer=1
NoSendStackToServer=1
```
# Log Size Limits #
Omaha tries to archive the log when the log size is greater than 10 MB. When the log is in use by more than one instance of Omaha the archiving operation will fail. However, there is a 100 MB limit to how big the log can be to prevent overfilling the hard drive. When this limit is reached the log file is cleared and the logging starts from the beginning.