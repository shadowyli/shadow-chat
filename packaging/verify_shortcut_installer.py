"""Exercise installer shortcuts in a sandbox without changing real user shortcuts."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import uuid
import winreg
from windows_shortcuts import read_shortcuts, set_shortcut_icons, verify_shortcut_icons
from release_config import version as release_version

root = Path(__file__).resolve().parent.parent
fixture = root/'dist'/('shortcut-installer-test-'+uuid.uuid4().hex[:8])
fixture.mkdir(parents=True)
source = (root/'packaging/shadowchat-installer.nsi').read_text(encoding='utf-8')
version = release_version()
payload = fixture/'payload'
payload.mkdir()
shutil.copy2(root/'test/ShadowChat.exe', payload/'ShadowChat.exe')
icon_name = 'shadowchat-'+version+'.ico'
shutil.copy2(root/'packaging/shadowchat.ico', payload/icon_name)
icon_digest = hashlib.sha256((payload/icon_name).read_bytes()).hexdigest()
desktop = fixture/'desktop'
programs = fixture/'programs'
desktop.mkdir()
programs.mkdir()
install = fixture/'program'
registry_name = 'ShadowChatIconTest-'+fixture.name
source = source.replace('!define ROOT "${__FILEDIR__}\\.."', '!define ROOT "'+str(root)+'"')
source = source.replace('$DESKTOP', str(desktop)).replace('$SMPROGRAMS', str(programs))
source = source.replace('Software\\shadowyli\\ShadowChat', 'Software\\shadowyli\\'+registry_name)
source = source.replace('Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\ShadowChat', 'Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\'+registry_name)
script = fixture/'sandbox-installer.nsi'
script.write_text(source, encoding='utf-8')
installer = fixture/'sandbox-installer.exe'
command = [str(root/'.tools/NSIS/makensis.exe'), '/INPUTCHARSET', 'UTF8', '/DAPP_VERSION='+version, '/DAPP_CHANNEL=test', '/DAPP_ENVIRONMENT=local', '/DAPP_WEB=http://127.0.0.1:5173/tools/shadowchat', '/DPAYLOAD='+str(payload), '/DOUTPUT='+str(installer), str(script)]
subprocess.run(command, cwd=root, check=True, timeout=120)
paths = [desktop/'影聊.lnk', programs/'影聊/影聊.lnk', programs/'影聊/卸载影聊.lnk']
targets = [install/'ShadowChat.exe', install/'ShadowChat.exe', install/'卸载影聊.exe']
passes = []
for attempt in range(2):
    subprocess.run([str(installer), '/S', '/D='+str(install)], check=True, timeout=120)
    rows = verify_shortcut_icons(paths, targets, install/icon_name, icon_digest)
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, 'Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\'+registry_name, 0, winreg.KEY_READ|winreg.KEY_WOW64_32KEY) as key:
        display_icon = winreg.QueryValueEx(key, 'DisplayIcon')[0]
        if display_icon != str(install/icon_name)+',0':
            raise SystemExit('Sandbox DisplayIcon mismatch')
    passes.append({'attempt': attempt, 'shortcuts': rows, 'displayIcon': display_icon})
    if attempt == 0:
        for shortcut, target in zip(paths, targets):
            set_shortcut_icons([shortcut], target, target)
        if any(row['icon'].rsplit(',', 1)[0].lower().endswith('.ico') for row in read_shortcuts(paths)):
            raise SystemExit('Old EXE-icon shortcut fixture not seeded')
        executable_digest = hashlib.sha256((install/'ShadowChat.exe').read_bytes()).hexdigest()
        repair = subprocess.run([sys.executable, '-B', '-X', 'utf8', str(root/'packaging/repair_shortcut_icons.py'), '--target', str(install/'ShadowChat.exe'), '--expected-exe-sha256', executable_digest, '--version', version, *[str(path) for path in paths[:2]]], capture_output=True, text=True, encoding='utf-8', check=True, timeout=60)
        repair_report = json.loads(repair.stdout)
        if not repair_report['executableUnchanged'] or not repair_report['shellNotified']:
            raise SystemExit('Standalone shortcut repair did not preserve executable or refresh Shell')
        verify_shortcut_icons(paths[:2], targets[:2], install/icon_name, icon_digest)
subprocess.run([str(install/'卸载影聊.exe'), '/S', '_?='+str(install)], check=True, timeout=120)
if any(path.exists() for path in paths):
    raise SystemExit('Sandbox uninstall left shortcuts')
report = {
    'passed': True, 'fixtureDirectory': str(fixture), 'version': version,
    'icoSha256': icon_digest, 'checks': passes,
    'existingExeIconLinksReplaced': True, 'sandboxUninstallPassed': True,
    'standaloneIconRepairPassed': True,
    'realDesktopAndStartMenuUntouched': True,
    'scope': 'Diagnostic installer uses existing production executable only as an inert file; shell folder paths and registry keys redirected to sandbox; no target launched and not a release build',
}
(fixture/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
(root/'dist/shortcut-installer-verification.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
