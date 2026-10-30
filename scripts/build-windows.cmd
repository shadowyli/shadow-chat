@echo off
rem Run from an x86 Visual Studio Developer Command Prompt with Qt 5.15 qmake on PATH.
setlocal
python -B -X utf8 "%~dp0windows_build.py" %*
exit /b %errorlevel%
