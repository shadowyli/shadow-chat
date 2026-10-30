@echo off
setlocal
if not defined SHADOWCHAT_VCVARS set "SHADOWCHAT_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined SHADOWCHAT_QT_DIR if exist "%~dp0..\..\Qt\5.15.2\msvc2019\bin\qmake.exe" set "SHADOWCHAT_QT_DIR=%~dp0..\..\Qt\5.15.2\msvc2019"
if not defined SHADOWCHAT_QT_DIR if exist "%~dp0..\..\..\Qt\5.15.2\msvc2019\bin\qmake.exe" set "SHADOWCHAT_QT_DIR=%~dp0..\..\..\Qt\5.15.2\msvc2019"
if not exist "%SHADOWCHAT_QT_DIR%\bin\qmake.exe" (
  echo Set SHADOWCHAT_QT_DIR to the Qt MSVC x86 SDK directory.
  exit /b 1
)
call "%SHADOWCHAT_VCVARS%"
if errorlevel 1 exit /b 1
set "PATH=%SHADOWCHAT_QT_DIR%\bin;%PATH%"
cd /d "%~dp0.."
python -B -X utf8 scripts\windows_build.py %*
exit /b %errorlevel%
