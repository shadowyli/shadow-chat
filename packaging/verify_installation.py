"""Repeatable silent install/upgrade check in a fresh isolated install directory."""
from pathlib import Path
import subprocess,hashlib,json,uuid,winreg,os,configparser,sys,ctypes,time,argparse
from windows_shortcuts import refresh_shell, verify_shortcut_icons

root=Path(__file__).resolve().parent.parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('installer',nargs='?',type=Path)
parser.add_argument('--receipt',type=Path,default=root/'dist/local/release-build.json')
parser.add_argument('--previous-installer',type=Path,required=True)
parser.add_argument('--previous-receipt',type=Path,required=True)
parser.add_argument('--recovery-dir',type=Path,default=Path(os.environ.get('SHADOWCHAT_RECOVERY_DIR',str(root/'delivery-private'))))
args=parser.parse_args()
metadata=json.loads(args.receipt.read_text(encoding='utf-8'))
installer=Path(metadata['installer'])
if args.installer:
 installer=args.installer.resolve()
if hashlib.sha256(installer.read_bytes()).hexdigest()!=metadata['sha256']:
 raise SystemExit('Installer differs from final release build receipt')
previous=args.previous_installer.resolve()
previous_receipt=json.loads(args.previous_receipt.read_text(encoding='utf-8'))
if not previous.is_file():
 raise SystemExit('Explicit previous-version installer is required for upgrade verification')
if hashlib.sha256(previous.read_bytes()).hexdigest()!=previous_receipt['sha256']:
 raise SystemExit('Previous installer does not match its recorded published receipt')
fixture=args.receipt.resolve().parent/('installation-test-'+uuid.uuid4().hex[:8])
install=fixture/'program'
profile=fixture/'documents'/'影聊'/'accounts'/'install-test@example.test'
profile.mkdir(parents=True)
for name,content in {'beebeep.dat':b'history-preservation-fixture','.shadow-account.ini':b'email=install-test@example.test','received/asset.psd':b'synthetic-material','peer-avatars.ini':b'avatar-preservation-fixture','conversation-preferences.ini':b'[fixture]\npinned=true\n','pending-transfers.json':b'{"schema":1,"fixture":"pending-file-and-message-queue","entries":[{"source":"received/asset.psd","recipient":"peer@example.test"}]}'}.items():
 p=profile/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(content)
def hash_file(p):return hashlib.sha256(p.read_bytes()).hexdigest()
before={str(p.relative_to(profile)):hash_file(p) for p in profile.rglob('*') if p.is_file()}
# NSIS registers a per-user install even with an isolated /D. Preserve the real
# user's pre-existing application registration and shortcuts around this test.
keys=[r'Software\shadowyli\ShadowChat',r'Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat']
registrations={}
for name in keys:
 try:
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,name,0,winreg.KEY_READ|winreg.KEY_WOW64_32KEY) as key:
   values=[];i=0
   while True:
    try: values.append(winreg.EnumValue(key,i));i+=1
    except OSError:break
   registrations[name]=values
 except FileNotFoundError:registrations[name]=None
with winreg.OpenKey(winreg.HKEY_CURRENT_USER,r'Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders') as key:
 desktop=Path(os.path.expandvars(winreg.QueryValueEx(key,'Desktop')[0]))
programs=Path(os.environ['APPDATA'])/'Microsoft'/'Windows'/'Start Menu'/'Programs'/'影聊'
shortcuts=[desktop/'影聊.lnk',programs/'影聊.lnk',programs/'卸载影聊.lnk']
saved_shortcuts={path:path.read_bytes() if path.exists() else None for path in shortcuts}
programs_existed=programs.exists()
icon_verifications=[]
def verify_icons():
 icon=install/metadata['shortcutIcon']['filename']
 rows=verify_shortcut_icons(shortcuts,[install/'ShadowChat.exe',install/'ShadowChat.exe',install/'卸载影聊.exe'],icon,metadata['shortcutIcon']['sha256'])
 with winreg.OpenKey(winreg.HKEY_CURRENT_USER,keys[1],0,winreg.KEY_READ|winreg.KEY_WOW64_32KEY) as key:
  location=winreg.QueryValueEx(key,'DisplayIcon')[0]
  if location!=str(icon)+',0':raise SystemExit('Installed DisplayIcon does not use the verified versioned ICO')
  if winreg.QueryValueEx(key,'URLInfoAbout')[0]!=metadata['downloadPage']:raise SystemExit('Installed download page does not match selected environment')
 icon_verifications.append({'shortcuts':rows,'displayIcon':location,'icoSha256':hash_file(icon)})
try:
 previous_installed=False
 if previous.is_file():
  prior=subprocess.run([str(previous),'/S','/D='+str(install)],timeout=120)
  if prior.returncode:raise SystemExit('Previous-version installation failed')
  if hash_file(install/'ShadowChat.exe')!=previous_receipt['sourceExecutableSha256']:raise SystemExit('Previous-version payload mismatch')
  previous_installed=True
 for attempt in range(2):
  result=subprocess.run([str(installer),'/S','/D='+str(install)],timeout=120)
  if result.returncode:raise SystemExit(f'Installer run {attempt} failed: {result.returncode}')
  if hash_file(install/'ShadowChat.exe')!=metadata['sourceExecutableSha256']:raise SystemExit('Installed executable differs from current source build')
  for name,digest in metadata['appLocalCrt'].items():
   if hash_file(install/name)!=digest:raise SystemExit('Installed app-local runtime mismatch: '+name)
  for name,entry in metadata['payloadInventory'].items():
   if hash_file(install/name)!=entry['sha256']:raise SystemExit('Installed payload mismatch: '+name)
  with winreg.OpenKey(winreg.HKEY_CURRENT_USER,keys[1],0,winreg.KEY_READ|winreg.KEY_WOW64_32KEY) as key:
   if winreg.QueryValueEx(key,'DisplayVersion')[0]!=metadata['version']:raise SystemExit('Installed version registration mismatch')
  verify_icons()
  after={str(p.relative_to(profile)):hash_file(p) for p in profile.rglob('*') if p.is_file()}
  if before!=after:raise SystemExit('External profile changed during install')
  if not (install/'shadowchat-recovery.ini').is_file():raise SystemExit('Shared recovery hash seed missing')
  config=configparser.ConfigParser();config.read(install/'shadowchat-recovery.ini',encoding='ascii')
  owner_key=(args.recovery_dir/'负责人恢复密码.txt').read_text(encoding='utf-8').splitlines()[2]
  expected=hashlib.pbkdf2_hmac('sha256',owner_key.encode(),bytes.fromhex(config['General']['salt']),100000,32).hex()
  if expected!=config['General']['password']:raise SystemExit('Recovery configuration does not match owner key')
 kernel=ctypes.WinDLL('kernel32',use_last_error=True)
 kernel.CreateFileW.argtypes=[ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
 kernel.CreateFileW.restype=ctypes.c_void_p
 kernel.CloseHandle.argtypes=[ctypes.c_void_p]
 handle=kernel.CreateFileW(str(install/'ShadowChat.exe'),0x80000000,1,None,3,0,None)
 if handle==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
 try:
  locked=subprocess.run([str(installer),'/S','/D='+str(install)],timeout=120)
  if locked.returncode!=2:raise SystemExit('Installer did not reject a locked executable')
 finally:kernel.CloseHandle(handle)
 if hash_file(install/'ShadowChat.exe')!=metadata['sourceExecutableSha256']:raise SystemExit('Locked install altered executable')
 runtime_env=dict(os.environ)
 runtime_env['PATH']=str(Path(os.environ['SystemRoot'])/'System32')+';'+os.environ['SystemRoot']
 runtime_env.pop('QT_PLUGIN_PATH',None)
 runtime_env.pop('QT_QPA_PLATFORM_PLUGIN_PATH',None)
 identity=json.loads(subprocess.check_output([str(install/'ShadowChat.exe'),'--build-info'],cwd=install,env=runtime_env,text=True,encoding='utf-8',timeout=30))
 for name,expected_value in {'version':metadata['version'],'environment':metadata['releaseEnvironment'],'channel':metadata['channel'],'updateSource':metadata['updateSource']}.items():
  if identity.get(name)!=expected_value:raise SystemExit('Installed executable build identity mismatch: '+name)
 with (fixture/'dependency-probe.log').open('wb') as output:
  probe=subprocess.run([str(install/'ShadowChat.exe'),'-platformpluginpath'],cwd=install,env=runtime_env,stdout=output,stderr=output,timeout=30)
 if probe.returncode!=1:raise SystemExit('Installed executable dependency/argument-guard probe failed')
 snapshot=fixture/'verified-installed-payload.json'
 snapshot.write_text(json.dumps({name:hash_file(install/name) for name in metadata['payloadInventory']},ensure_ascii=False,indent=2),encoding='utf-8')
 uninstall=subprocess.run([str(install/'卸载影聊.exe'),'/S','_?='+str(install)],timeout=120)
 if uninstall.returncode:raise SystemExit('Silent uninstall failed')
 deadline=time.monotonic()+30
 while (install/'ShadowChat.exe').exists() and time.monotonic()<deadline:time.sleep(0.2)
 if (install/'ShadowChat.exe').exists():raise SystemExit('Uninstaller left application executable')
 if before!={str(p.relative_to(profile)):hash_file(p) for p in profile.rglob('*') if p.is_file()}:raise SystemExit('Uninstall altered external data')
 result=subprocess.run([str(installer),'/S','/D='+str(install)],timeout=120)
 if result.returncode:raise SystemExit('Reinstall after uninstall failed')
 for name,entry in metadata['payloadInventory'].items():
  if hash_file(install/name)!=entry['sha256']:raise SystemExit('Reinstalled payload mismatch: '+name)
 verify_icons()
finally:
 for name,values in registrations.items():
  if values is None:
   try:winreg.DeleteKeyEx(winreg.HKEY_CURRENT_USER,name,winreg.KEY_WOW64_32KEY,0)
   except FileNotFoundError:pass
  else:
   with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER,name,0,winreg.KEY_ALL_ACCESS|winreg.KEY_WOW64_32KEY) as key:
    current=[];i=0
    while True:
     try:current.append(winreg.EnumValue(key,i)[0]);i+=1
     except OSError:break
    for value in current:winreg.DeleteValue(key,value)
    for value,data,kind in values:winreg.SetValueEx(key,value,0,kind,data)
 for path,data in saved_shortcuts.items():
  if data is None:path.unlink(missing_ok=True)
  else:path.write_bytes(data)
 if not programs_existed and programs.exists() and not any(programs.iterdir()):programs.rmdir()
 refresh_shell(shortcuts)
report={'installer':str(installer),'installDirectory':str(install),'installAndOverwritePassed':True,'installedExecutableMatchesBuild':True,'allPayloadFilesMatchBuild':True,'externalFixtureDataUnchanged':True,'recoverySeedMatchesOwnerKey':True,'lockedExecutableInstallRejected':True,'sanitizedPathDependencyProbePassed':True,'silentUninstallPreservesExternalFixture':True,'reinstallAfterUninstallPassed':True,
 'version':metadata['version'],'releaseEnvironment':metadata['releaseEnvironment'],'channel':metadata['channel'],'installerSha256':metadata['sha256'],'externalQueueFixtureUnchanged':True,
 'installedBuildIdentity':identity,
 'versionedShortcutIconsVerified':True,'shortcutIconVerification':icon_verifications,
 'previousVersionInstalledBeforeUpgrade':previous_installed,
 'previousVersion':previous_receipt['version'],'previousInstallerSha256':previous_receipt['sha256'],
 'scope':'Silent installer on this Windows host; synthetic fixture external to install folder. Dependency probe exits at the argument guard before Qt UI/account startup. No real account launched; not a clean Windows VM.'}
(fixture/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(args.receipt.resolve().parent/'installation-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
