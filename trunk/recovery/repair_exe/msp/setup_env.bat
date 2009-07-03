:: This script must not rely on any external tools or PATH values.
@echo OFF

:: Creating the MSP requires the msimsp.exe from the 2003 R2 SDK
set PATH=%PATH%;%~dp0..\..\..\..\third_party\platformsdk_win_server_2003_r2_partial\Samples\SysMgmt\Msi\Patching
