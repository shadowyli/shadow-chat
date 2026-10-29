Unicode true

!include "MUI2.nsh"

!define APP_NAME "影聊"
!define APP_EXE "ShadowChat.exe"
!define APP_VERSION "1.1.1"
!define APP_PUBLISHER "shadowyli"
!define APP_WEB "https://flowx.gz/tools/shadowchat"
!define ROOT "${__FILEDIR__}\.."
!define PAYLOAD "${ROOT}\dist\影聊-1.1.1-test-x86"

Name "${APP_NAME} ${APP_VERSION} 测试版"
OutFile "${ROOT}\dist\影聊-1.1.1-test-x86-安装包.exe"
InstallDir "$LOCALAPPDATA\Programs\ShadowChat"
RequestExecutionLevel user
SetCompressor /SOLID lzma
SetCompressorDictSize 32
CRCCheck on
ManifestDPIAware true
BrandingText "影聊 · 局域网办公通讯"
ShowInstDetails nevershow
ShowUninstDetails nevershow

VIProductVersion "1.1.1.0"
VIAddVersionKey /LANG=2052 "ProductName" "影聊"
VIAddVersionKey /LANG=2052 "ProductVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "FileVersion" "${APP_VERSION}"
VIAddVersionKey /LANG=2052 "CompanyName" "${APP_PUBLISHER}"
VIAddVersionKey /LANG=2052 "FileDescription" "影聊安装程序"
VIAddVersionKey /LANG=2052 "LegalCopyright" "BeeBEEP contributors and shadowyli"

!define MUI_ABORTWARNING
!define MUI_ICON "${ROOT}\src\beebeep.ico"
!define MUI_UNICON "${ROOT}\src\beebeep.ico"
!define MUI_FINISHPAGE_RUN "$INSTDIR\${APP_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "启动影聊"
!define MUI_FINISHPAGE_LINK "查看影聊项目主页"
!define MUI_FINISHPAGE_LINK_LOCATION "${APP_WEB}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "SimpChinese"

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
    IfSilent cancel_install
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "请先从托盘退出影聊，再点击重试。账号、聊天记录和接收文件会保留。" IDRETRY check_running
  cancel_install:
    SetErrorLevel 2
    Abort
  ready_to_install:
  SetOutPath "$INSTDIR"
  File /r "${PAYLOAD}\*.*"

  WriteUninstaller "$INSTDIR\卸载影聊.exe"
  WriteRegStr HKCU "Software\shadowyli\ShadowChat" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "DisplayName" "影聊"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "DisplayVersion" "${APP_VERSION}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "Publisher" "${APP_PUBLISHER}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "URLInfoAbout" "${APP_WEB}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "DisplayIcon" "$INSTDIR\${APP_EXE}"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "UninstallString" '"$INSTDIR\卸载影聊.exe"'
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "NoModify" 1
  WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat" "NoRepair" 1

  CreateDirectory "$SMPROGRAMS\影聊"
  CreateShortcut "$SMPROGRAMS\影聊\影聊.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
  CreateShortcut "$SMPROGRAMS\影聊\卸载影聊.lnk" "$INSTDIR\卸载影聊.exe"
  CreateShortcut "$DESKTOP\影聊.lnk" "$INSTDIR\${APP_EXE}" "" "$INSTDIR\${APP_EXE}" 0
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  Delete "$DESKTOP\影聊.lnk"
  RMDir /r "$SMPROGRAMS\影聊"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat"
  DeleteRegKey HKCU "Software\shadowyli\ShadowChat"
  RMDir /r "$INSTDIR"
  MessageBox MB_OK|MB_ICONINFORMATION "影聊已卸载。你的个人资料和聊天记录仍保留在用户数据目录中，重新安装后可以继续使用。" /SD IDOK
SectionEnd
