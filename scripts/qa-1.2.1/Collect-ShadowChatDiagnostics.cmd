@echo off
setlocal
set "SCRIPT_DIR=%~dp0"

echo ShadowChat 1.2.1 real three-PC diagnostic collector
echo This tool only reads system, network, process, port, version, and log information.
echo It does not change firewall, registry, network, startup settings, or user data.
echo.
set /p ROLE=Enter this PC role - A, B, or C: 

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%Collect-ShadowChatDiagnostics.ps1" -Role "%ROLE%"
set "EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%EXIT_CODE%"=="0" (
  echo Collection failed. Exit code: %EXIT_CODE%
) else (
  echo Collection succeeded. Submit the ZIP from the desktop ShadowChat-QA-1.2.1 folder.
)
pause
exit /b %EXIT_CODE%
