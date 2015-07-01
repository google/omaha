:: This script must not rely on any external tools or PATH values.
@echo OFF

:: Let advanced users checkout the tools in just one P4 enlistment
if "%SETUP_ENV_LZMA%"=="done" goto :EOF
set  SETUP_ENV_LZMA=done

set INCLUDE=%INCLUDE%;%~dp0files\C\include
