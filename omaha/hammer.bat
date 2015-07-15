@echo off

rem -- Set all environment variables used by Hammer and Omaha. --

set GOROOT=%ProgramFiles(x86)%\go\files
set OMAHA_ATL_SERVER_DIR=c:\atl_server\files
set OMAHA_NET_DIR=%WINDIR%\Microsoft.NET\Framework\v2.0.50727
set OMAHA_NETFX_TOOLS_DIR=%ProgramFiles(x86)%\Microsoft SDKs\Windows\v8.1A\bin\NETFX 4.5.1 Tools
set OMAHA_PYTHON_DIR=C:\Python24
set OMAHA_WIX_DIR=%ProgramFiles(x86)%\WiX Toolset v3.8\bin
set OMAHA_WTL_DIR=C:\wtl\files
set PLATFORM_SDK_DIR=%ProgramFiles(x86)%\Windows Kits\8.1
set PYTHONPATH=%OMAHA_PYTHON_DIR%
set SCONS_DIR=C:\Python24\Lib\site-packages\scons-1.3.1
set SCT_DIR=C:\swtoolkit
set SIGNTOOL_SDK_DIR=%ProgramFiles(x86)%\Windows Kits\8.1\bin\x86
set VC12_0_DIR=%ProgramFiles(x86)%\Microsoft Visual Studio 12.0

setlocal

set PROXY_CLSID_TARGET=%~dp0proxy_clsids.txt
set CUSTOMIZATION_UT_TARGET=%~dp0common\omaha_customization_proxy_clsid.h

rem Force Hammer to use Python 2.4.  (The default of Python 2.6 exposes some
rem bugs in Scons 1.2, which we currently use.)
set PYTHON_TO_USE=python_24
call %SCT_DIR%\hammer.bat %*

if /i {%1} == {-c} (
  del /q /f "%PROXY_CLSID_TARGET%" 2> NUL
  del /q /f "%CUSTOMIZATION_UT_TARGET%" 2> NUL
)

