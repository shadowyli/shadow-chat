#define MyAppName "影聊"
#define MyAppEnglishName "ShadowChat"
#define MyAppVersion "1.0.0-test"
#define MyAppExeName "ShadowChat.exe"

[Setup]
AppId={{5F4EC9D0-629B-47A7-87C8-2C6AD32D0D5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} 1.0.0 测试版
AppPublisher=liying
AppPublisherURL=https://shadowyli.github.io/
AppSupportURL=https://github.com/shadowyli/shadowyli.github.io/issues
DefaultDirName={localappdata}\Programs\{#MyAppEnglishName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir=..\dist
OutputBaseFilename=影聊-1.0.0-test-x86-setup
SetupIconFile=..\packaging\shadowchat.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
LicenseFile=..\LICENSE.txt
VersionInfoVersion=1.0.0.0
VersionInfoDescription=影聊 Windows 安装程序
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion=1.0.0.0
VersionInfoCompany=liying

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\dist\影聊-1.0.0-test-x86\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动{#MyAppName}"; Flags: nowait postinstall skipifsilent
