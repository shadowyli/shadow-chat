"""Verify the exact release installer through its explicit isolated QA namespace."""
from pathlib import Path
import argparse
import base64
import configparser
import ctypes
import hashlib
import json
import os
import shutil
import subprocess
import tempfile
import time
import uuid
import winreg

ROOT = Path(__file__).resolve().parent.parent
RUN_KEY = r'Software\Microsoft\Windows\CurrentVersion\Run'
PREFERENCE_KEY = r'Software\shadowyli\ShadowChat'
UNINSTALL_KEY = r'Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat'


def hash_file(filename):
    return hashlib.sha256(filename.read_bytes()).hexdigest()


def file_hashes(folder):
    return {str(item.relative_to(folder)): hash_file(item) for item in folder.rglob('*') if item.is_file()}


def read_shortcuts(paths):
    ole = ctypes.WinDLL('ole32')
    ole.CoInitializeEx.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
    ole.CoInitializeEx.restype = ctypes.c_long
    ole.CoCreateInstance.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    ole.CoCreateInstance.restype = ctypes.c_long
    initialized = ole.CoInitializeEx(None, 2)
    if initialized < 0 and initialized != -2147417850:
        raise OSError('COM initialization failed: '+str(initialized))

    def guid(value):
        return ctypes.create_string_buffer(uuid.UUID(value).bytes_le)

    def invoke(interface, index, types, *arguments):
        table = ctypes.cast(interface, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
        method = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, *types)(table[index])
        result = method(interface, *arguments)
        if result < 0:
            raise OSError('Read-only shortcut COM call failed: '+str(result))

    rows = []
    try:
        for filename in paths:
            link = ctypes.c_void_p()
            persistence = ctypes.c_void_p()
            result = ole.CoCreateInstance(guid('00021401-0000-0000-c000-000000000046'), None, 1,
                                          guid('000214f9-0000-0000-c000-000000000046'), ctypes.byref(link))
            if result < 0:
                raise OSError('ShellLink creation failed: '+str(result))
            try:
                invoke(link, 0, [ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)],
                       guid('0000010b-0000-0000-c000-000000000046'), ctypes.byref(persistence))
                invoke(persistence, 5, [ctypes.c_wchar_p, ctypes.c_uint32], str(filename), 0)
                target = ctypes.create_unicode_buffer(32768)
                icon = ctypes.create_unicode_buffer(32768)
                arguments = ctypes.create_unicode_buffer(32768)
                working = ctypes.create_unicode_buffer(32768)
                icon_index = ctypes.c_int()
                invoke(link, 3, [ctypes.c_wchar_p, ctypes.c_int, ctypes.c_void_p, ctypes.c_uint32], target, len(target), None, 4)
                invoke(link, 16, [ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(ctypes.c_int)], icon, len(icon), ctypes.byref(icon_index))
                invoke(link, 10, [ctypes.c_wchar_p, ctypes.c_int], arguments, len(arguments))
                invoke(link, 8, [ctypes.c_wchar_p, ctypes.c_int], working, len(working))
                rows.append({'path': str(filename), 'target': target.value, 'icon': icon.value+','+str(icon_index.value),
                             'arguments': arguments.value, 'workingDirectory': working.value})
            finally:
                if persistence:
                    invoke(persistence, 2, [])
                invoke(link, 2, [])
    finally:
        if initialized >= 0:
            ole.CoUninitialize()
    return rows


def verify_shortcut_icons(paths, targets, icon, expected_digest):
    if hash_file(icon) != expected_digest:
        raise RuntimeError('Installed shortcut ICO digest mismatch')
    rows = read_shortcuts(paths)
    for row, target in zip(rows, targets):
        if Path(row['target']).resolve() != target.resolve():
            raise RuntimeError('Shortcut target mismatch: '+row['path'])
        location, index = row['icon'].rsplit(',', 1)
        if Path(location.strip('"')).resolve() != icon.resolve() or index.strip() != '0':
            raise RuntimeError('Shortcut icon mismatch: '+row['path'])
    return rows


def require_installer_isolation(filename):
    version = ctypes.WinDLL('version', use_last_error=True)
    version.GetFileVersionInfoSizeW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_uint32)]
    version.GetFileVersionInfoSizeW.restype = ctypes.c_uint32
    version.GetFileVersionInfoW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
    version.VerQueryValueW.argtypes = [ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_uint32)]
    ignored = ctypes.c_uint32()
    size = version.GetFileVersionInfoSizeW(str(filename), ctypes.byref(ignored))
    buffer = ctypes.create_string_buffer(size)
    if not size or not version.GetFileVersionInfoW(str(filename), 0, size, buffer):
        raise RuntimeError('Installer lacks a readable QA capability resource; refusing to execute')
    value = ctypes.c_void_p()
    length = ctypes.c_uint32()
    query = r'\StringFileInfo\080404b0\ShadowChatIsolation'
    if not version.VerQueryValueW(buffer, query, ctypes.byref(value), ctypes.byref(length)) or ctypes.wstring_at(value) != '1':
        raise RuntimeError('Installer does not declare ShadowChatIsolation=1; refusing to execute')


def registry_snapshot(name, view):
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, name, 0, winreg.KEY_READ | view) as key:
            children, count, unused = winreg.QueryInfoKey(key)
            values = {}
            for index in range(count):
                value, data, kind = winreg.EnumValue(key, index)
                if isinstance(data, bytes):
                    data = {'base64': base64.b64encode(data).decode('ascii')}
                values[value] = {'data': data, 'type': kind}
            return {'values': values, 'children': {
                winreg.EnumKey(key, index): registry_snapshot(name+'\\'+winreg.EnumKey(key, index), view)
                for index in range(children)}}
    except FileNotFoundError:
        return None


def host_snapshot():
    result = {}
    for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
        for name in (RUN_KEY, PREFERENCE_KEY, UNINSTALL_KEY):
            result[str(view)+':'+name] = registry_snapshot(name, view)
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r'Software\Microsoft\Windows\CurrentVersion\Explorer\User Shell Folders') as key:
        desktop = Path(os.path.expandvars(winreg.QueryValueEx(key, 'Desktop')[0]))
        programs = Path(os.path.expandvars(winreg.QueryValueEx(key, 'Programs')[0]))
        startup = Path(os.path.expandvars(winreg.QueryValueEx(key, 'Startup')[0]))
    for label, folder in [('startup-user', startup),
                          ('startup-common', Path(os.environ['ProgramData'])/'Microsoft/Windows/Start Menu/Programs/Startup'),
                          ('shortcuts', programs/'影聊')]:
        result[label] = {'path': str(folder), 'exists': folder.exists(), 'files': file_hashes(folder)}
    shortcut = desktop/'影聊.lnk'
    result['desktopShortcut'] = hash_file(shortcut) if shortcut.exists() else None
    return result


def seed_data(fixture):
    data = fixture/'documents/影聊/accounts'
    profile = data/'install-test@example.test'
    profile.mkdir(parents=True)
    sticker_folder = profile/'stickers'
    sticker_folder.mkdir()
    (sticker_folder/'index.json').write_text('{"version":1,"items":[]}', encoding='utf-8')
    (sticker_folder/'preservation-fixture.gif').write_bytes(b'GIF89a-synthetic-preservation-only')
    (profile/'sticker-panel.ini').write_text('[General]\nrecent=synthetic-preservation-only\n', encoding='utf-8')
    fixtures = {
        'beebeep.dat': b'history-preservation-fixture',
        '.shadow-account.ini': b'[General]\nemail=install-test@example.test\nsalt=synthetic-only\npassword=synthetic-hash-only\n',
        'beebeep.ini': b'[Version]\nSettings=18\n[Gui]\nUseReturnToSendMessage=false\n',
        'beebeep.off': b'offline-message-fixture',
        'beehosts.ini': b'127.0.0.1\n',
        '.shadow-paths.ini': b'[General]\nfixture=true\n',
        'received/asset.psd': b'synthetic-material',
        'peer-avatars.ini': b'avatar-preservation-fixture',
        'conversation-preferences.ini': b'[fixture]\npinned=true\n',
        'message-delivery.json': b'{"synthetic-message":{"chat":"fixture","preview":"pending synthetic message","recipients":["peer@example.test"],"delivered":[]}}',
    }
    for name, content in fixtures.items():
        filename = profile/name
        filename.parent.mkdir(parents=True, exist_ok=True)
        filename.write_bytes(content)
    attachment = profile/'received/asset.psd'
    (profile/'pending-transfers.json').write_text(json.dumps([{
        'id': 'synthetic-transfer', 'batch': 'synthetic-batch', 'recipient': 'peer@example.test',
        'chat': 'fixture', 'source': attachment.as_posix(), 'group': False, 'state': 'pending',
        'completed': [], 'directories': [], 'files': [{'path': attachment.as_posix(),
        'size': str(attachment.stat().st_size), 'modified': str(attachment.stat().st_mtime_ns//1000000)}]
    }]), encoding='utf-8')
    (data/'session.ini').write_text('[General]\nautoLoginEmail=install-test@example.test\nautoLoginEnabled=false\nstartupConfigured=true\n', encoding='ascii')
    return data.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', nargs='?', type=Path)
    parser.add_argument('--receipt', type=Path, default=ROOT/'dist/local/release-build.json')
    parser.add_argument('--previous-installer', type=Path, required=True, help='Hash checked only; historical installers are never executed on this user')
    parser.add_argument('--previous-receipt', type=Path, required=True)
    parser.add_argument('--previous-payload', type=Path, required=True, help='Verified extracted previous payload; historical installation itself requires a disposable Windows VM')
    parser.add_argument('--recovery-dir', type=Path, default=Path(os.environ.get('SHADOWCHAT_RECOVERY_DIR', str(ROOT/'delivery-private'))))
    parser.add_argument('--output', type=Path, default=ROOT/'dist/qa-startup/installation')
    args = parser.parse_args()
    metadata = json.loads(args.receipt.read_text(encoding='utf-8'))
    installer = (args.installer or Path(metadata['installer'])).resolve()
    previous = args.previous_installer.resolve()
    previous_metadata = json.loads(args.previous_receipt.read_text(encoding='utf-8'))
    if hash_file(installer) != metadata['sha256']:
        raise SystemExit('Installer differs from final release build receipt')
    require_installer_isolation(installer)
    if hash_file(previous) != previous_metadata['sha256']:
        raise SystemExit('Previous installer does not match published receipt')
    for name, entry in previous_metadata['payloadInventory'].items():
        if hash_file(args.previous_payload/name) != entry['sha256']:
            raise SystemExit('Previous payload mismatch: '+name)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    namespace = uuid.uuid4().hex
    fixture = Path(tempfile.gettempdir())/('ShadowChat-QA-'+namespace)
    install = fixture/'program'
    data = seed_data(fixture)
    before_data = file_hashes(data)
    registry_root = r'Software\shadowyli\ShadowChat-QA'+'\\'+namespace
    keys = [registry_root+r'\Preferences', registry_root+r'\Uninstall']
    run_key = registry_root+r'\Startup'
    shortcuts = [fixture/'shell/Desktop/影聊.lnk', fixture/'shell/Programs/影聊/影聊.lnk', fixture/'shell/Programs/影聊/卸载影聊.lnk']
    startup_checks = []
    preservation_checks = []
    icon_checks = []
    commands = []
    before_host = host_snapshot()
    (output/'host-before.json').write_text(json.dumps(before_host, ensure_ascii=False, indent=2), encoding='utf-8')

    def execute(command, phase, timeout=120, expected=0):
        commands.append({'phase': phase, 'command': [str(value) for value in command]})
        with (output/(phase+'.log')).open('wb') as stream:
            result = subprocess.run(command, stdout=stream, stderr=stream, timeout=timeout)
        if result.returncode != expected:
            raise RuntimeError(f'{phase}: exit {result.returncode}, expected {expected}')

    def install_current(phase, expected=0):
        execute([str(installer), '/S', '/SHADOWCHAT-QA='+namespace], phase, expected=expected)
        if expected == 0 and not (install/'shadowchat-qa.ini').exists():
            raise RuntimeError('Installer did not persist QA isolation; do not run unsupported historical installers')

    def startup_value():
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, run_key) as key:
                return winreg.QueryValueEx(key, 'ShadowChat')[0]
        except FileNotFoundError:
            return None

    def startup_preference(enabled):
        with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, keys[0], 0, winreg.KEY_ALL_ACCESS | winreg.KEY_WOW64_32KEY) as key:
            winreg.SetValueEx(key, 'StartupEnabled', 0, winreg.REG_DWORD, int(enabled))
        with winreg.CreateKey(winreg.HKEY_CURRENT_USER, run_key) as key:
            if enabled:
                winreg.SetValueEx(key, 'ShadowChat', 0, winreg.REG_SZ, '"'+str(install/'ShadowChat.exe')+'"')
            else:
                try:
                    winreg.DeleteValue(key, 'ShadowChat')
                except FileNotFoundError:
                    pass

    def check_startup(enabled, phase):
        expected = '"'+str(install/'ShadowChat.exe')+'"' if enabled else None
        if startup_value() != expected:
            raise RuntimeError('Startup command mismatch: '+phase)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, keys[0], 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
            if winreg.QueryValueEx(key, 'StartupEnabled')[0] != int(enabled):
                raise RuntimeError('Startup preference mismatch: '+phase)
        startup_checks.append({'phase': phase, 'enabled': enabled, 'command': startup_value()})

    def check_data(phase):
        after = file_hashes(data)
        if after != before_data:
            raise RuntimeError('Synthetic account data changed: '+phase)
        preservation_checks.append({'phase': phase, 'sha256': after})

    def verify_payload():
        for name, entry in metadata['payloadInventory'].items():
            if hash_file(install/name) != entry['sha256']:
                raise RuntimeError('Installed payload mismatch: '+name)
        icon = install/metadata['shortcutIcon']['filename']
        rows = verify_shortcut_icons(shortcuts, [install/'ShadowChat.exe', install/'ShadowChat.exe', install/'卸载影聊.exe'], icon, metadata['shortcutIcon']['sha256'])
        expected_arguments = '--profile-root "'+str(fixture/'documents/影聊/accounts')+'"'
        if any(row['arguments'] != expected_arguments for row in rows[:2]):
            raise RuntimeError('QA application shortcuts do not force the synthetic profile root')
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, keys[1], 0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as key:
            for name, expected in {'DisplayVersion': metadata['version'], 'DisplayIcon': str(icon)+',0', 'URLInfoAbout': metadata['downloadPage']}.items():
                if winreg.QueryValueEx(key, name)[0] != expected:
                    raise RuntimeError('Isolated registration mismatch: '+name)
        icon_checks.append(rows)

    try:
        install_current('fresh')
        check_startup(True, 'fresh defaults enabled')
        verify_payload()
        check_data('fresh')
        startup_preference(False)
        for name in previous_metadata['payloadInventory']:
            target = install/name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(args.previous_payload/name, target)
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, keys[0], 0, winreg.KEY_SET_VALUE | winreg.KEY_WOW64_32KEY) as key:
            winreg.DeleteValue(key, 'StartupEnabled')
        for attempt in range(2):
            phase = 'upgrade-'+str(attempt)
            install_current(phase)
            check_startup(False, phase+' preserves disabled')
            verify_payload()
            check_data(phase)
        startup_preference(True)
        install_current('upgrade-enabled')
        check_startup(True, 'upgrade preserves enabled')
        check_data('upgrade enabled')
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.CreateFileW.argtypes = [ctypes.c_wchar_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_void_p]
        kernel.CreateFileW.restype = ctypes.c_void_p
        kernel.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel.CreateFileW(str(install/'ShadowChat.exe'), 0x80000000, 1, None, 3, 0, None)
        if handle == ctypes.c_void_p(-1).value:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            install_current('locked-executable', expected=2)
        finally:
            kernel.CloseHandle(handle)
        verify_payload()
        check_data('locked install rejected')
        runtime_environment = dict(os.environ, PATH=str(Path(os.environ['SystemRoot'])/'System32')+';'+os.environ['SystemRoot'])
        runtime_environment.pop('QT_PLUGIN_PATH', None)
        runtime_environment.pop('QT_QPA_PLATFORM_PLUGIN_PATH', None)
        identity = json.loads(subprocess.check_output([str(install/'ShadowChat.exe'), '--build-info'], cwd=install, env=runtime_environment, text=True, encoding='utf-8', timeout=30))
        for name, expected in {'version': metadata['version'], 'environment': metadata['releaseEnvironment'], 'channel': metadata['channel'], 'updateSource': metadata['updateSource']}.items():
            if identity.get(name) != expected:
                raise RuntimeError('Installed build identity mismatch: '+name)
        with (output/'dependency-probe.log').open('wb') as stream:
            probe = subprocess.run([str(install/'ShadowChat.exe'), '-platformpluginpath'], cwd=install, env=runtime_environment, stdout=stream, stderr=stream, timeout=30)
        if probe.returncode != 1:
            raise RuntimeError('Installed dependency/argument guard probe failed')
        configuration = configparser.ConfigParser()
        configuration.read(install/'shadowchat-recovery.ini', encoding='ascii')
        owner_key = (args.recovery_dir/'负责人恢复密码.txt').read_text(encoding='utf-8').splitlines()[2]
        expected = hashlib.pbkdf2_hmac('sha256', owner_key.encode(), bytes.fromhex(configuration['General']['salt']), 100000, 32).hex()
        if expected != configuration['General']['password']:
            raise RuntimeError('Recovery hash differs from private owner key')
        execute([str(install/'卸载影聊.exe'), '/S', '_?='+str(install)], 'uninstall')
        deadline = time.monotonic()+30
        while (install/'ShadowChat.exe').exists() and time.monotonic() < deadline:
            time.sleep(0.2)
        if (install/'ShadowChat.exe').exists() or startup_value() is not None or any(item.exists() for item in shortcuts):
            raise RuntimeError('Uninstall left payload, startup or shortcuts')
        if any(registry_snapshot(name, winreg.KEY_WOW64_32KEY) is not None for name in keys):
            raise RuntimeError('Uninstall left isolated preferences/registration')
        check_data('uninstall')
        install_current('reinstall')
        check_startup(True, 'reinstall defaults enabled')
        verify_payload()
        check_data('reinstall')
    finally:
        after_host = host_snapshot()
        (output/'host-after.json').write_text(json.dumps(after_host, ensure_ascii=False, indent=2), encoding='utf-8')
        (output/'commands.json').write_text(json.dumps(commands, ensure_ascii=False, indent=2), encoding='utf-8')
        (output/'data-preservation.json').write_text(json.dumps(preservation_checks, ensure_ascii=False, indent=2), encoding='utf-8')
        if before_host != after_host:
            raise RuntimeError('Production Run, Startup, registration or shortcuts changed; no restore writes are performed')
    report = {
        'passed': True, 'installer': str(installer), 'installerSha256': hash_file(installer),
        'sameReleaseInstallerBytes': True, 'namespace': namespace, 'fixture': str(fixture),
        'hostStartupRegistrationAndShortcutsUnchanged': True, 'startupVerification': startup_checks,
        'dataPreservationVerification': preservation_checks, 'shortcutIconVerification': icon_checks,
        'installedBuildIdentity': identity, 'previousVersion': previous_metadata['version'],
        'previousInstallerExecuted': False, 'previousVerifiedPayloadStaged': True,
        'scope': 'Exact release installer QA switch redirects only registration/shortcut destinations and forces a temporary install root. Install/upgrade/uninstall sections are shared. No real Run or Startup folder writes. Synthetic file hashes prove preservation, not application migration/readability. Historical installer execution and final UI integration require a disposable Windows VM.',
    }
    (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
