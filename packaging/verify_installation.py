"""Repeatable silent install/upgrade check in a fresh isolated install directory."""
from pathlib import Path
import subprocess,hashlib,json,uuid,winreg,os,configparser,sys,ctypes,time

root=Path(__file__).resolve().parent.parent
metadata=json.loads((root/'dist'/'release-build.json').read_text(encoding='utf-8'))
installer=Path(metadata['installer'])
if len(sys.argv)>1:
 installer=Path(sys.argv[1]).resolve()
 if hashlib.sha256(installer.read_bytes()).hexdigest()!=metadata['sha256']:
  raise SystemExit('Downloaded installer differs from final release build')
fixture=root/'dist'/('installation-test-'+uuid.uuid4().hex[:8])
install=fixture/'program'
profile=fixture/'documents'/'影聊'/'accounts'/'install-test@example.test'
profile.mkdir(parents=True)
for name,content in {'beebeep.dat':b'history-preservation-fixture','.shadow-account.ini':b'email=install-test@example.test','received/asset.psd':b'synthetic-material','peer-avatars.ini':b'avatar-preservation-fixture','conversation-preferences.ini':b'[fixture]\npinned=true\n'}.items():
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
try:
 previous=root/'dist'/'影聊-1.1.0-test-x86-安装包.exe'
 previous_installed=False
 if metadata['version']!='1.1.0' and previous.is_file():
  prior=subprocess.run([str(previous),'/S','/D='+str(install)],timeout=120)
  if prior.returncode:raise SystemExit('Previous-version installation failed')
  prior_exe=root/'dist'/'影聊-1.1.0-test-x86'/'ShadowChat.exe'
  if hash_file(install/'ShadowChat.exe')!=hash_file(prior_exe):raise SystemExit('Previous-version payload mismatch')
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
  after={str(p.relative_to(profile)):hash_file(p) for p in profile.rglob('*') if p.is_file()}
  if before!=after:raise SystemExit('External profile changed during install')
  if not (install/'shadowchat-recovery.ini').is_file():raise SystemExit('Shared recovery hash seed missing')
  config=configparser.ConfigParser();config.read(install/'shadowchat-recovery.ini',encoding='ascii')
  owner_key=(root/'delivery-private'/'负责人恢复密码.txt').read_text(encoding='utf-8').splitlines()[2]
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
report={'installer':str(installer),'installDirectory':str(install),'installAndOverwritePassed':True,'installedExecutableMatchesBuild':True,'allPayloadFilesMatchBuild':True,'externalFixtureDataUnchanged':True,'recoverySeedMatchesOwnerKey':True,'lockedExecutableInstallRejected':True,'sanitizedPathDependencyProbePassed':True,'silentUninstallPreservesExternalFixture':True,'reinstallAfterUninstallPassed':True,
 'previousVersionInstalledBeforeUpgrade':previous_installed,
 'scope':'Silent installer on this Windows host; synthetic fixture external to install folder. Dependency probe exits at the argument guard before Qt UI/account startup. No real account launched; not a clean Windows VM.'}
(fixture/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
(root/'dist'/'installation-verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
