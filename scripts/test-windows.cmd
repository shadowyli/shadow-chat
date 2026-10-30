@echo off
rem Build the production objects first using build-windows.cmd.
setlocal
set QT_LOGGING_TO_CONSOLE=1
if not defined QT_QPA_PLATFORM set QT_QPA_PLATFORM=windows
set FAILED=0
if not defined SHADOWCHAT_QMAKE_ARGS (
  echo Production environment arguments are required. Run release-windows.cmd test --environment local, dev or main.
  exit /b 1
)
if not defined SHADOWCHAT_REGRESSION_ROOT exit /b 1
if not defined SHADOWCHAT_PRODUCTION_ROOT exit /b 1
set "REPOSITORY_ROOT=%~dp0.."
cd /d "%~dp0..\tests"
if defined SHADOWCHAT_TEST_SELECTION (
  for %%T in (%SHADOWCHAT_TEST_SELECTION%) do call :run_test %%T
) else (
  for %%F in (update_*_test.pro) do call :run_update_project %%~nF
  for %%T in (workspace_archive local_accounts transfer_queue transfer_failure group_policy message_delivery history_records history_retention avatar_refresh profile_editor dialog_consistency conversation_preferences sticker_store history_ui sticker_ui loopback_chat group_loopback) do call :run_test %%T
)
exit /b %FAILED%

:run_update_project
set "UPDATE_PROJECT=%~1"
call :run_test %UPDATE_PROJECT:~0,-5%
exit /b 0

:run_test
echo ==== TEST %1 ====
python -B "%REPOSITORY_ROOT%\packaging\prepare_regression_project.py" --name %1 --production-root "%SHADOWCHAT_PRODUCTION_ROOT%" --output-root "%SHADOWCHAT_REGRESSION_ROOT%"
if errorlevel 1 goto test_failed
pushd "%SHADOWCHAT_REGRESSION_ROOT%"
qmake "projects/%1_test.pro" -o Makefile.%1 %SHADOWCHAT_QMAKE_ARGS% "DESTDIR=%SHADOWCHAT_REGRESSION_ROOT%/bin" "OBJECTS_DIR=%SHADOWCHAT_REGRESSION_ROOT%/objects/%1" "MOC_DIR=%SHADOWCHAT_REGRESSION_ROOT%/objects/%1" "UI_DIR=%SHADOWCHAT_REGRESSION_ROOT%/objects/%1"
if errorlevel 1 goto build_failed
nmake /nologo /f Makefile.%1
if errorlevel 1 goto build_failed
popd
pushd "%SHADOWCHAT_REGRESSION_ROOT%"
if errorlevel 1 goto test_failed
if "%1"=="update_build" (
  "%SHADOWCHAT_REGRESSION_ROOT%\bin\%1_test.exe" "%SHADOWCHAT_RELEASE_ENV%" "%SHADOWCHAT_RELEASE_CHANNEL%" "%SHADOWCHAT_UPDATE_SOURCE%"
) else (
  "%SHADOWCHAT_REGRESSION_ROOT%\bin\%1_test.exe"
)
set "TEST_EXIT=%errorlevel%"
popd
echo ==== EXIT %1 %TEST_EXIT% ====
if not "%TEST_EXIT%"=="0" goto test_failed
echo ==== PASS %1 ====
exit /b 0

:build_failed
popd
:test_failed
echo ==== FAIL %1 ====
set FAILED=1
exit /b 0
