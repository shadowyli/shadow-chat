@echo off
setlocal
set "SCRIPT=%~dp0..\packaging\diagnose_startup.ps1"
if not exist "%SCRIPT%" (
  echo Missing startup diagnostic: %SCRIPT%
  exit /b 2
)
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "%SCRIPT%" %*
exit /b %errorlevel%
