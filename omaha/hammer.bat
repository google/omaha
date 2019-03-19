@echo off

:: Hammer does not need this variable but the unit
:: tests do.
set OMAHA_PSEXEC_DIR=%ProgramFiles(x86)%\pstools

setlocal

rem -- Set all environment variables used by Hammer and Omaha. --

:: VS2003/VC71 is 1310 (not supported by the current build).
:: VS2005/VC80 is 1400 (not supported by the current build).
:: VS2008/VC90 is 1500 (not supported by the current build).
:: VS2010/VC10 is 1600 (not supported by the current build).
:: VS2012/VC11 is 1700 (not supported by the current build).
:: VS2013/VC12 is 1800.
:: VS2015/VC14 is 1900.
:: VS2017/VC14.1 is 1910.

if "%VisualStudioVersion%"=="" goto error_no_vc
if "%VisualStudioVersion%"=="12.0" goto vc120
if "%VisualStudioVersion%"=="14.0" goto vc140
if "%VisualStudioVersion%"=="15.0" goto vc141
goto error_vc_not_supported

:vc120
set OMAHA_MSC_VER=1800
goto set_env_variables

:vc140
set OMAHA_MSC_VER=1900
goto set_env_variables

:vc141
set OMAHA_MSC_VER=1910
goto set_env_variables

:set_env_variables

:: Change these variables to match the local build environment.

:: Directory where the Go programming language toolchain is installed.
set GOROOT=C:\go

:: Directory where AtlServer files are.
set OMAHA_ATL_SERVER_DIR=c:\atl_server\files

:: This will depend on your OS. If this version of the .Net framework came with
:: the OS, then set it to the framework directory
:: (something like C:\Windows\Microsoft.NET\Framework\v2.0.50727).
:: Otherwise, set it to the directory where the .NET framework is installed.
set OMAHA_NET_DIR=%WINDIR%\Microsoft.NET\Framework\v2.0.50727

:: This directory is needed to find mage.exe tool, which is the .Net manifest
:: generating tool. This tool ships as part of the Windows SDK.
:: However, newer versions of mage.exe can't targer older versions of .Net
:: framework. If there is a need for the click-once application to run on older
:: versions of the .Net framework, then an older version of the Windows SDK
:: needs to be installed and this environment variable point to that directory.
set OMAHA_NETFX_TOOLS_DIR=%WindowsSDK_ExecutablePath_x86%

:: This directory is needed to find protoc.exe, which is the protocol buffer
:: compiler. From the release page https://github.com/google/protobuf/releases,
:: download the zip file protoc-$VERSION-win32.zip. It contains the protoc
:: binary. Unzip the contents under C:\protobuf.
set OMAHA_PROTOBUF_BIN_DIR=C:\protobuf\bin

:: This directory is needed to find the protocol buffer source files. From the
:: release page https://github.com/google/protobuf/releases, download the zip
:: file protobuf-cpp-$VERSION.zip. Unzip the "src" sub-directory contents to
:: C:\protobuf\src.
set OMAHA_PROTOBUF_SRC_DIR=C:\protobuf\src

:: Directory where Python (python.exe) is installed.
set OMAHA_PYTHON_DIR=C:\Python27

:: Directory in WiX where candle.exe and light.exe are installed.
set OMAHA_WIX_DIR=%ProgramFiles(x86)%\WiX Toolset v3.10\bin

:: Root directory of the WTL installation.
set OMAHA_WTL_DIR=C:\wtl\files

set OMAHA_PLATFORM_SDK_DIR=%WindowsSdkDir%\
set OMAHA_WINDOWS_SDK_10_0_VERSION=%WindowsSDKVersion:~0,-1%

:: Directory which includes the sign.exe tool for Authenticode signing.
set OMAHA_SIGNTOOL_SDK_DIR=%WindowsSdkDir%\bin\x86
set PYTHONPATH=%OMAHA_PYTHON_DIR%

:: Directory of Scons (http://www.scons.org/).
set SCONS_DIR=C:\Python27\Lib\site-packages\scons-1.3.1

:: Directory of the Google's Software Construction Toolkit.
set SCT_DIR=C:\swtoolkit

set PROXY_CLSID_TARGET=%~dp0proxy_clsids.txt
set CUSTOMIZATION_UT_TARGET=%~dp0common\omaha_customization_proxy_clsid.h

rem Force Hammer to use Python 2.7
set PYTHON_TO_USE=python_27
call "%SCT_DIR%\hammer.bat" %*

if /i {%1} == {-c} (
  del /q /f "%PROXY_CLSID_TARGET%" 2> NUL
  del /q /f "%CUSTOMIZATION_UT_TARGET%" 2> NUL
)

goto end

:error_no_vc
echo VisualStudioVersion variable is not set. Have you run vcvarsall.bat before running this script?
goto end

:error_vc_not_supported
echo Visual Studio version %VisualStudioVersion% is not supported.
goto end

:end
