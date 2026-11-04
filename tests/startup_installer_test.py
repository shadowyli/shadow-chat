"""Compile the production NSIS script with synthetic payload, then exercise its isolation contract."""
from pathlib import Path
import ctypes
import json
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
import winreg

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'packaging'))
from verify_installation import file_hashes, hash_file, host_snapshot, require_installer_isolation, registry_snapshot, seed_data

output = ROOT/'dist/qa-startup/installer-contract'
output.mkdir(parents=True, exist_ok=True)
payload = output/'payload'
payload.mkdir(exist_ok=True)
(payload/'ShadowChat.exe').write_bytes(b'SYNTHETIC QA PAYLOAD - DO NOT EXECUTE\r\n')
shutil.copy2(ROOT/'packaging/shadowchat.ico', payload/'shadowchat-1.2.0.ico')
installer = output/'qa-installer.exe'
compiler = Path('E:/ShadowChat/beebeep-code-r1557/.tools/NSIS/makensis.exe')
commands = []
checks = []
before_host = host_snapshot()
(output/'host-before.json').write_text(json.dumps(before_host,ensure_ascii=False,indent=2),encoding='utf-8')
namespace = uuid.uuid4().hex
fixture = Path(tempfile.gettempdir())/('ShadowChat-QA-'+namespace)
install = fixture/'program'
key_root = r'Software\shadowyli\ShadowChat-QA'+'\\'+namespace
preference_key = key_root+r'\Preferences'
run_key = key_root+r'\Startup'
uninstall_key = key_root+r'\Uninstall'
data = seed_data(fixture)
before_data = file_hashes(data)

def execute(command, name, expected=0):
    command = [str(item) for item in command]
    commands.append({'name':name,'command':command})
    with (output/(name+'.log')).open('wb') as stream:
        result = subprocess.run(command,stdout=stream,stderr=stream,timeout=120)
    if result.returncode != expected:
        raise RuntimeError(f'{name}: exit {result.returncode}, expected {expected}')

def current(name, expected=0):
    execute([installer,'/S','/SHADOWCHAT-QA='+namespace],name,expected)

def set_preference(enabled):
    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER,preference_key,0,winreg.KEY_ALL_ACCESS|winreg.KEY_WOW64_32KEY) as key:
        winreg.SetValueEx(key,'StartupEnabled',0,winreg.REG_DWORD,int(enabled))

def set_command(enabled):
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER,run_key) as key:
        if enabled:
            winreg.SetValueEx(key,'ShadowChat',0,winreg.REG_SZ,'"'+str(install/'ShadowChat.exe')+'"')
        else:
            try:
                winreg.DeleteValue(key,'ShadowChat')
            except FileNotFoundError:
                pass

def check(enabled, name):
    snapshot = registry_snapshot(run_key,winreg.KEY_WOW64_32KEY)
    command = snapshot['values'].get('ShadowChat') if snapshot else None
    expected = {'data':'"'+str(install/'ShadowChat.exe')+'"','type':winreg.REG_SZ} if enabled else None
    assert command == expected, (name,command,expected)
    preference = registry_snapshot(preference_key,winreg.KEY_WOW64_32KEY)
    assert preference['values']['StartupEnabled']['data'] == int(enabled), name
    assert file_hashes(data) == before_data, name
    assert host_snapshot() == before_host, name
    assert hash_file(install/'ShadowChat.exe') == hash_file(payload/'ShadowChat.exe'), name
    assert (install/'shadowchat-qa.ini').is_file(), name
    checks.append({'name':name,'enabled':enabled,'productionStateUnchanged':True,'dataSha256':file_hashes(data)})

try:
    execute([compiler,'/INPUTCHARSET','UTF8','/DAPP_VERSION=1.2.0','/DAPP_CHANNEL=beta',
             '/DAPP_ENVIRONMENT=local','/DAPP_WEB=https://example.test/shadowchat',
             '/DPAYLOAD='+str(payload),'/DOUTPUT='+str(installer),ROOT/'packaging/shadowchat-installer.nsi'],'compile')
    require_installer_isolation(installer)
    try:
        require_installer_isolation(payload/'ShadowChat.exe')
    except RuntimeError:
        pass
    else:
        raise RuntimeError('An unsupported executable passed the preflight isolation capability check')
    for index, invalid in enumerate(('', 'not-hex', 'a'*31, 'a'*33, 'A'*32, '../'+'b'*29)):
        execute([installer,'/S','/SHADOWCHAT-QA='+invalid],'invalid-'+str(index),3)
        assert host_snapshot() == before_host
    current('fresh')
    check(True,'fresh defaults enabled')
    escaped = fixture/'escape-should-not-exist'
    execute([installer,'/S','/SHADOWCHAT-QA='+namespace,'/D='+str(escaped)],'qa-directory-forced')
    assert not escaped.exists()
    check(True,'QA switch forces the temporary install root even with /D')
    execute([installer,'/S','/D='+str(install)],'qa-directory-without-switch',3)
    marker = install/'shadowchat-qa.ini'
    saved_marker = marker.read_bytes()
    marker.unlink()
    execute([install/'卸载影聊.exe','/S','_?='+str(install)],'uninstall-missing-marker',3)
    marker.write_bytes(saved_marker)
    check(True,'missing isolation marker fails closed')
    marker.write_text('[Isolation]\nNamespace='+uuid.uuid4().hex+'\n',encoding='ascii')
    execute([install/'卸载影聊.exe','/S','_?='+str(install)],'uninstall-wrong-namespace',3)
    marker.write_bytes(saved_marker)
    check(True,'mismatched persisted namespace fails closed')
    set_preference(False)
    set_command(False)
    current('explicit-off')
    check(False,'explicit opt-out survives overwrite')
    current('explicit-off-repeat')
    check(False,'explicit opt-out survives repeated overwrite')
    set_command(True)
    current('off-with-stale-command')
    check(False,'explicit opt-out removes stale startup command')
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER,preference_key,0,winreg.KEY_SET_VALUE|winreg.KEY_WOW64_32KEY) as key:
        winreg.DeleteValue(key,'StartupEnabled')
    current('legacy-off')
    check(False,'legacy install without command remains disabled')
    set_preference(True)
    set_command(False)
    current('explicit-on')
    check(True,'explicit enabled repairs missing command')
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER,preference_key,0,winreg.KEY_SET_VALUE|winreg.KEY_WOW64_32KEY) as key:
        winreg.DeleteValue(key,'StartupEnabled')
    current('legacy-on')
    check(True,'legacy existing command remains enabled')
    kernel = ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.CreateFileW.argtypes = [ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
    kernel.CreateFileW.restype = ctypes.c_void_p
    kernel.CloseHandle.argtypes = [ctypes.c_void_p]
    handle = kernel.CreateFileW(str(install/'ShadowChat.exe'),0x80000000,1,None,3,0,None)
    if handle == ctypes.c_void_p(-1).value:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        current('locked',2)
    finally:
        kernel.CloseHandle(handle)
    check(True,'locked install rejected without changing preference or data')
    execute([install/'卸载影聊.exe','/S','_?='+str(install)],'uninstall')
    assert not (install/'ShadowChat.exe').exists()
    assert not (fixture/'shell/Desktop/影聊.lnk').exists()
    assert not (fixture/'shell/Programs/影聊').exists()
    assert registry_snapshot(preference_key,winreg.KEY_WOW64_32KEY) is None
    assert registry_snapshot(uninstall_key,winreg.KEY_WOW64_32KEY) is None
    remaining = registry_snapshot(run_key,winreg.KEY_WOW64_32KEY)
    assert not remaining or 'ShadowChat' not in remaining['values']
    assert file_hashes(data) == before_data
    assert host_snapshot() == before_host
    checks.append({'name':'uninstaller inherits namespace without command-line flag','dataSha256':file_hashes(data),'productionStateUnchanged':True})
    current('reinstall')
    check(True,'reinstall defaults enabled after preference cleanup')
    execute([install/'卸载影聊.exe','/S'],'uninstall-self-copy')
    deadline = time.monotonic()+30
    while (install/'ShadowChat.exe').exists() and time.monotonic() < deadline:
        time.sleep(0.1)
    assert not (install/'ShadowChat.exe').exists()
    assert registry_snapshot(preference_key,winreg.KEY_WOW64_32KEY) is None
    assert registry_snapshot(uninstall_key,winreg.KEY_WOW64_32KEY) is None
    assert file_hashes(data) == before_data
    assert host_snapshot() == before_host
    checks.append({'name':'normal self-copy uninstaller retains isolation namespace','productionStateUnchanged':True,'dataSha256':file_hashes(data)})
finally:
    after_host = host_snapshot()
    (output/'host-after.json').write_text(json.dumps(after_host,ensure_ascii=False,indent=2),encoding='utf-8')
    (output/'commands.json').write_text(json.dumps(commands,ensure_ascii=False,indent=2),encoding='utf-8')
    if before_host != after_host:
        raise RuntimeError('Production startup/registration/shortcuts changed; no restore writes performed')
report = {'passed':True,'namespace':namespace,'fixture':str(fixture),'installer':str(installer),
          'installerSha256':hash_file(installer),'nsisSourceSha256':hash_file(ROOT/'packaging/shadowchat-installer.nsi'),
          'invalidNamespaceCases':6,'hostStartupRegistrationAndShortcutsUnchanged':True,'checks':checks,
          'scope':'Current production NSIS source compiled once with a synthetic nonexecutable payload. Shared install/upgrade/uninstall logic exercised through explicit QA switch. No final release package or actual application migration claimed.'}
(output/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
