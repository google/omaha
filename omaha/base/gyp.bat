@echo off
REM Copyright 2022 Google Inc. All Rights Reserved.
REM Launcher script for google3 gyp

set SETUP_ENV_PYTHON_27=
call %~dp0..\..\third_party\python_27\setup_env.bat

set PYTHONPATH=%~dp0..\..\..\google3\third_party\py

set GYP_DIR=%~dp0..\..\..\google3\third_party\gyp

set NUMBER_OF_PROCESSORS=1

@call %~dp0..\..\third_party\python_27\files\python.exe %GYP_DIR%\gyp_main.py --depth=%~dp0 %*