Unicode true

!include "MUI2.nsh"
!include "FileFunc.nsh"

Var SC_QA_ID
Var SC_QA_ROOT
Var SC_PREF_KEY
Var SC_RUN_KEY
Var SC_UNINSTALL_KEY
Var SC_DESKTOP
Var SC_PROGRAMS
Var SC_ORIGINAL_INSTDIR
Var SC_APP_ARGUMENTS
Var SC_QUIT_ATTEMPTED
Var SC_QUIT_REGISTERED
Var SC_QUIT_WAIT

!define APP_NAME "影聊"
!define APP_EXE "ShadowChat.exe"
!ifndef APP_VERSION
!error "APP_VERSION must be supplied by build_installer.py from src/core/Version.h"
!endif
!ifndef APP_CHANNEL
!error "APP_CHANNEL must be supplied by the selected build profile"
!endif
!ifndef APP_ENVIRONMENT
!error "APP_ENVIRONMENT must be supplied by the selected build profile"
!endif
!if "${APP_CHANNEL}" == "stable"
!define APP_LABEL "正式版"
!else
!define APP_LABEL "测试版"
!endif
!define APP_ICON "shadowchat-${APP_VERSION}.ico"
!define APP_PUBLISHER "shadowyli"
!ifndef APP_WEB
!error "APP_WEB must be supplied from the selected build update source"
!endif
!define ROOT "${__FILEDIR__}\.."
!ifndef PAYLOAD
!define PAYLOAD "${ROOT}\dist\${APP_ENVIRONMENT}\影聊-${APP_VERSION}-${APP_CHANNEL}-x86"
!endif
!ifndef OUTPUT
!define OUTPUT "${ROOT}\dist\${APP_ENVIRONMENT}\影聊-${APP_VERSION}-${APP_CHANNEL}-x86-安装包.exe"
!endif

Name "${APP_NAME} ${APP_VERSION} ${APP_LABEL} (${APP_ENVIRONMENT})"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\ShadowChat"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
CRCCheck on
ManifestDPIAware true
BrandingText "影聊 · 局域网办公通讯"
ShowInstDetails nevershow
ShowUninstDetails nevershow

VIProductVersion "${APP_VERSION}.0"
VIAddVersionKey /LANG=2052 "ProductName" "影聊"
VIAddVersionKey /LANG=2052 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=2052 "FileDescription" "影聊安装程序"
VIAddVersionKey /LANG=2052 "LegalCopyright" "BeeBEEP contributors and shadowyli"
VIAddVersionKey /LANG=2052 "ShadowChatIsolation" "1"

!define MUI_ABORTWARNING
!define MUI_ICON "${ROOT}\src\beebeep.ico"
!define MUI_UNICON "${ROOT}\src\beebeep.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_FUNCTION RunInstalledApplication
!define MUI_FINISHPAGE_RUN_TEXT "启动影聊"
!define MUI_FINISHPAGE_LINK "查看影聊项目主页"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_WEB}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

!macro RegistrationInitializer FUNCTION_NAME
Function ${FUNCTION_NAME}
  SetShellVarContext current
  SetRegView 32
  StrCpy $SC_PREF_KEY "Software\shadowyli\ShadowChat"
  StrCpy $SC_RUN_KEY "Software\Microsoft\Windows\CurrentVersion\Run"
  StrCpy $SC_UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat"
  StrCpy $SC_DESKTOP "$DESKTOP"
  StrCpy $SC_PROGRAMS "$SMPROGRAMS\影聊"
  StrCpy $SC_APP_ARGUMENTS ""
  StrCmp $SC_QA_ID "" registration_ready
  StrLen $0 $SC_QA_ID
  IntCmp $0 32 0 invalid_namespace invalid_namespace
  System::Call 'shlwapi::StrSpnW(w "$SC_QA_ID", w "0123456789abcdef")i.r0'
  IntCmp $0 32 0 invalid_namespace invalid_namespace
  StrCpy $SC_QA_ROOT "$TEMP\ShadowChat-QA-$SC_QA_ID"
  StrCpy $SC_PREF_KEY "Software\shadowyli\ShadowChat-QA\$SC_QA_ID\Preferences"
  StrCpy $SC_RUN_KEY "Software\shadowyli\ShadowChat-QA\$SC_QA_ID\Startup"
  StrCpy $SC_UNINSTALL_KEY "Software\shadowyli\ShadowChat-QA\$SC_QA_ID\Uninstall"
  StrCpy $SC_DESKTOP "$SC_QA_ROOT\shell\Desktop"
  StrCpy $SC_PROGRAMS "$SC_QA_ROOT\shell\Programs\影聊"
  StrCpy $SC_APP_ARGUMENTS '--profile-root "$SC_QA_ROOT\documents\影聊\accounts"'
  StrCpy $INSTDIR "$SC_QA_ROOT\program"
  Goto registration_ready
  invalid_namespace:
    SetErrorLevel 3
    Abort
  registration_ready:
FunctionEnd
!macroend

!insertmacro RegistrationInitializer InitializeRegistration
!insertmacro RegistrationInitializer un.InitializeRegistration

Function .onInit
  StrCpy $SC_QA_ID ""
  ${GetParameters} $0
  ClearErrors
  ${GetOptions} $0 "/SHADOWCHAT-QA=" $SC_QA_ID
  IfErrors normal_registration
  StrCmp $SC_QA_ID "" invalid_qa_argument
  Goto initialize_registration
  normal_registration:
    IfFileExists "$INSTDIR\shadowchat-qa.ini" invalid_qa_argument
    StrCpy $1 "$TEMP\ShadowChat-QA-"
    StrLen $2 $1
    StrCpy $3 $INSTDIR $2
    StrCmp $1 $3 invalid_qa_argument
  initialize_registration:
  Call InitializeRegistration
  Return
  invalid_qa_argument:
    SetErrorLevel 3
    Abort
FunctionEnd

Function un.onInit
  StrCpy $SC_ORIGINAL_INSTDIR "$INSTDIR"
  StrCpy $SC_QA_ID ""
  IfFileExists "$INSTDIR\shadowchat-qa.ini" read_uninstall_namespace
  StrCpy $1 "$TEMP\ShadowChat-QA-"
  StrLen $2 $1
  StrCpy $3 $INSTDIR $2
  StrCmp $1 $3 invalid_uninstall_namespace initialize_uninstaller
  read_uninstall_namespace:
  ReadINIStr $SC_QA_ID "$INSTDIR\shadowchat-qa.ini" "Isolation" "Namespace"
  StrCmp $SC_QA_ID "" invalid_uninstall_namespace
  initialize_uninstaller:
  Call un.InitializeRegistration
  StrCmp $SC_QA_ID "" uninstall_ready
  StrCmp $INSTDIR $SC_ORIGINAL_INSTDIR uninstall_ready
  invalid_uninstall_namespace:
    SetErrorLevel 3
    Abort
  uninstall_ready:
FunctionEnd

Function RunInstalledApplication
  StrCmp $SC_QA_ID "" 0 launch_complete
    Exec '"$INSTDIR\${APP_EXE}"'
  launch_complete:
FunctionEnd

Function RequestApplicationShutdown
  StrCpy $SC_QUIT_REGISTERED 0
  System::Call 'rstrtmgr::RmStartSession(*i .r1, i 0, w .r2) i.r0'
  StrCmp $0 0 0 shutdown_complete
  System::Call 'kernel32::CreateToolhelp32Snapshot(i 2, i 0) p.r3'
  StrCmp $3 -1 end_shutdown_session
  System::Alloc 556
  Pop $4
  System::Call '*$4(i 556)'
  System::Call 'kernel32::Process32FirstW(p r3, p r4) i.r0'
  enumerate_shutdown_process:
    StrCmp $0 0 close_shutdown_snapshot
    System::Call '*$4(i, i, i.r5)'
    System::Call 'kernel32::OpenProcess(i 0x1000, i 0, i r5) p.r6'
    StrCmp $6 0 next_shutdown_process
    StrCpy $8 ${NSIS_MAX_STRLEN}
    System::Call 'kernel32::QueryFullProcessImageNameW(p r6, i 0, w .r7, *i r8) i.r0'
    StrCmp $0 0 close_shutdown_process
    System::Call 'kernel32::lstrcmpiW(w r7, w "$INSTDIR\${APP_EXE}") i.r0'
    StrCmp $0 0 0 close_shutdown_process
    System::Call '*(i r5, l 0) p.r9'
    IntOp $7 $9 + 4
    System::Alloc 8
    Pop $8
    System::Call 'kernel32::GetProcessTimes(p r6, p r7, p r8, p r8, p r8) i.r0'
    System::Free $8
    StrCmp $0 0 free_shutdown_identity
    System::Call 'rstrtmgr::RmRegisterResources(i r1, i 0, p 0, i 1, p r9, i 0, p 0) i.r0'
    StrCmp $0 0 0 free_shutdown_identity
    StrCpy $SC_QUIT_REGISTERED 1
  free_shutdown_identity:
    System::Free $9
  close_shutdown_process:
    System::Call 'kernel32::CloseHandle(p r6)'
  next_shutdown_process:
    System::Call 'kernel32::Process32NextW(p r3, p r4) i.r0'
    Goto enumerate_shutdown_process
  close_shutdown_snapshot:
    System::Free $4
    System::Call 'kernel32::CloseHandle(p r3)'
    StrCmp $SC_QUIT_REGISTERED 1 0 end_shutdown_session
    DetailPrint "正在请求影聊正常保存并退出…"
    System::Call 'rstrtmgr::RmShutdown(i r1, i 0, p 0) i.r0'
  end_shutdown_session:
    System::Call 'rstrtmgr::RmEndSession(i r1)'
  shutdown_complete:
FunctionEnd

Section "影聊" SEC_MAIN
  SectionIn RO
  SetShellVarContext current
  check_running:
    IfFileExists "$INSTDIR\${APP_EXE}" 0 ready_to_install
    ClearErrors
    FileOpen $0 "$INSTDIR\${APP_EXE}" a
    IfErrors app_is_running
    FileClose $0
    Goto ready_to_install
  app_is_running:
    StrCmp $SC_QUIT_ATTEMPTED 1 show_running_prompt
    StrCpy $SC_QUIT_ATTEMPTED 1
    Call RequestApplicationShutdown
    StrCpy $SC_QUIT_WAIT 30
  wait_for_shutdown:
    Sleep 200
    ClearErrors
    FileOpen $0 "$INSTDIR\${APP_EXE}" a
    IfErrors shutdown_still_pending
    FileClose $0
    Goto ready_to_install
  shutdown_still_pending:
    IntOp $SC_QUIT_WAIT $SC_QUIT_WAIT - 1
    IntCmp $SC_QUIT_WAIT 0 show_running_prompt wait_for_shutdown wait_for_shutdown
  show_running_prompt:
    IfSilent cancel_install
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "影聊尚未正常退出，或安装文件正被其他程序占用。请等待传输完成或正常退出后重试。安装不会强制结束进程；账号、聊天记录和接收文件会保留。" IDRETRY retry_shutdown
    Goto cancel_install
  retry_shutdown:
    StrCpy $SC_QUIT_ATTEMPTED 0
    Goto check_running
  cancel_install:
    SetErrorLevel 2
    Abort
  ready_to_install:
  ClearErrors
  ReadRegDWORD $1 HKCU "$SC_PREF_KEY" "StartupEnabled"
  IfErrors startup_legacy startup_selected
  startup_legacy:
    ReadRegStr $2 HKCU "$SC_RUN_KEY" "ShadowChat"
    StrCmp $2 "" 0 startup_default
    ReadRegStr $2 HKCU "$SC_PREF_KEY" "InstallLocation"
    StrCmp $2 "" startup_default
    StrCpy $1 0
    Goto startup_selected
  startup_default:
    StrCpy $1 1
  startup_selected:
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}\*.*"

  WriteUninstaller "$INSTDIR\卸载影聊.exe"
  StrCmp $SC_QA_ID "" isolation_marker_complete
    WriteINIStr "$INSTDIR\shadowchat-qa.ini" "Isolation" "Namespace" "$SC_QA_ID"
  isolation_marker_complete:
  WriteRegStr HKCU "$SC_PREF_KEY" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "$SC_PREF_KEY" "StartupEnabled" $1
  StrCmp $1 0 startup_disabled
    WriteRegStr HKCU "$SC_RUN_KEY" "ShadowChat" '"$INSTDIR\${APP_EXE}"'
    Goto startup_complete
  startup_disabled:
    DeleteRegValue HKCU "$SC_RUN_KEY" "ShadowChat"
  startup_complete:
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "DisplayName" "影聊"
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "URLInfoAbout" "${APP_WEB}"
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "DisplayIcon" "$INSTDIR\${APP_ICON},0"
  WriteRegStr HKCU "$SC_UNINSTALL_KEY" "UninstallString" '"$INSTDIR\卸载影聊.exe"'
  WriteRegDWORD HKCU "$SC_UNINSTALL_KEY" "NoModify" 1
  WriteRegDWORD HKCU "$SC_UNINSTALL_KEY" "NoRepair" 1

  CreateDirectory "$SC_PROGRAMS"
  CreateDirectory "$SC_DESKTOP"
  CreateShortcut "$SC_PROGRAMS\影聊.lnk" "$INSTDIR\${APP_EXE}" "$SC_APP_ARGUMENTS" "$INSTDIR\${APP_ICON}" 0
  CreateShortcut "$SC_PROGRAMS\卸载影聊.lnk" "$INSTDIR\卸载影聊.exe" "" "$INSTDIR\${APP_ICON}" 0
  CreateShortcut "$SC_DESKTOP\影聊.lnk" "$INSTDIR\${APP_EXE}" "$SC_APP_ARGUMENTS" "$INSTDIR\${APP_ICON}" 0
  StrCmp $SC_QA_ID "" 0 shell_refresh_complete
    System::Call 'shell32::SHChangeNotify(i 0x00002000, i 0x1005, w "$SC_DESKTOP\影聊.lnk", p 0)'
    System::Call 'shell32::SHChangeNotify(i 0x00002000, i 0x1005, w "$SC_PROGRAMS\影聊.lnk", p 0)'
    System::Call 'shell32::SHChangeNotify(i 0x00002000, i 0x1005, w "$SC_PROGRAMS\卸载影聊.lnk", p 0)'
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0x1000, p 0, p 0)'
  shell_refresh_complete:
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  DeleteRegValue HKCU "$SC_RUN_KEY" "ShadowChat"
  Delete "$SC_DESKTOP\影聊.lnk"
  RMDir /r "$SC_PROGRAMS"
  DeleteRegKey HKCU "$SC_UNINSTALL_KEY"
  DeleteRegKey HKCU "$SC_PREF_KEY"
  RMDir /r "$INSTDIR"
  MessageBox MB_OK|MB_ICONINFORMATION "影聊已卸载，开机启动项已移除。你的个人资料和聊天记录仍保留在用户数据目录中，重新安装后可以继续使用。" /SD IDOK
SectionEnd
