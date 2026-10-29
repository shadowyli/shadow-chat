@echo off
rem Build the production objects first using build-windows.cmd.
setlocal
set QT_LOGGING_TO_CONSOLE=1
if not defined QT_QPA_PLATFORM set QT_QPA_PLATFORM=windows
set FAILED=0
cd /d "%~dp0..\tests"
for %%F in (update_*_test.pro) do call :run_update_project %%~nF
for %%T in (workspace_archive local_accounts transfer_queue transfer_failure group_policy message_delivery history_records history_retention avatar_refresh profile_editor dialog_consistency conversation_preferences sticker_store history_ui sticker_ui loopback_chat group_loopback) do (
  call :run_test %%T
)
exit /b %FAILED%

:run_update_project
set "UPDATE_PROJECT=%~1"
call :run_test %UPDATE_PROJECT:~0,-5%
exit /b 0

:run_test
echo ==== TEST %1 ====
qmake %1_test.pro -o Makefile.%1 "CONFIG+=no_batch" "DESTDIR=../dist/regression-bin" "OBJECTS_DIR=../dist/regression-objects/%1" "MOC_DIR=../dist/regression-objects/%1" "UI_DIR=../dist/regression-objects/%1"
if errorlevel 1 goto test_failed
nmake /nologo /f Makefile.%1
if errorlevel 1 goto test_failed
..\dist\regression-bin\%1_test.exe
if errorlevel 1 goto test_failed
echo ==== PASS %1 ====
exit /b 0

:test_failed
echo ==== FAIL %1 ====
set FAILED=1
exit /b 0
