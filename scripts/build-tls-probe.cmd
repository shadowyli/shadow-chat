@echo off
setlocal
if not defined SHADOWCHAT_VCVARS set "SHADOWCHAT_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
if not defined SHADOWCHAT_QT_DIR set "SHADOWCHAT_QT_DIR=%~dp0..\..\Qt\5.15.2\msvc2019"
call "%SHADOWCHAT_VCVARS%"
if errorlevel 1 exit /b 1
set "PATH=%SHADOWCHAT_QT_DIR%\bin;%PATH%"
cd /d "%~dp0..\packaging"
qmake tls_probe.pro -o ..\dist\Makefile.tls-probe "DESTDIR=../dist/tls-probe" "OBJECTS_DIR=../dist/tls-probe-objects" "MOC_DIR=../dist/tls-probe-objects"
if errorlevel 1 exit /b 1
nmake /nologo /f ..\dist\Makefile.tls-probe
exit /b %errorlevel%
