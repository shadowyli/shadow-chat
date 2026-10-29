@echo off
setlocal
if not defined SHADOWCHAT_VCVARS set "SHADOWCHAT_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined SHADOWCHAT_QT_DIR set "SHADOWCHAT_QT_DIR=%~dp0..\..\Qt\5.15.2\msvc2019"
call "%SHADOWCHAT_VCVARS%"
if errorlevel 1 exit /b 1
set "PATH=%SHADOWCHAT_QT_DIR%\bin;%PATH%"
cd /d "%~dp0.."
python -B -X utf8 scripts\windows_build.py %*
exit /b %errorlevel%
