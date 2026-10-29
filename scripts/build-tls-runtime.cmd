@echo off
setlocal
if not defined SHADOWCHAT_VCVARS set "SHADOWCHAT_VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat"
call "%SHADOWCHAT_VCVARS%"
if errorlevel 1 exit /b 1
cd /d "%~dp0.."
python -X utf8 packaging\prepare_tls_runtime.py
exit /b %errorlevel%
