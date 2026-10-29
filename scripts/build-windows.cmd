@echo off
rem Run from an x86 Visual Studio Developer Command Prompt with Qt 5.15 qmake on PATH.
setlocal
cd /d "%~dp0..\src"
if "%~1"=="rebuild" (
  python ..\scripts\isolate-build-output.py
  if errorlevel 1 exit /b 1
)
qmake src.pro -o Makefile "CONFIG+=no_batch"
if errorlevel 1 exit /b 1
if "%~1"=="rebuild" (
  nmake /nologo /A
) else (
  nmake /nologo
)
if errorlevel 1 exit /b 1
echo Built test\ShadowChat.exe
