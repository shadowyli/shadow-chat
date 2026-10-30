"""Deploy from an installed Qt SDK; reuse only validated private recovery input."""
from pathlib import Path
import argparse
import configparser
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import uuid
from urllib.parse import urlparse, urlunparse
from release_config import ENVIRONMENTS, artifact_name, channel as release_channel, output_directory, qt_directory, version as release_version

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--qt-dir', type=Path, default=qt_directory())
parser.add_argument('--makensis', type=Path, default=Path(os.environ.get('SHADOWCHAT_MAKENSIS_PATH', str(root/'.tools/NSIS/makensis.exe'))))
parser.add_argument('--recovery-dir', type=Path, default=Path(os.environ.get('SHADOWCHAT_RECOVERY_DIR', str(root/'delivery-private'))))
parser.add_argument('--crt-dir', type=Path)
parser.add_argument('--tls-dir', type=Path, default=Path(os.environ.get('SHADOWCHAT_TLS_DIR', str(root/'dist/tls-runtime-x86'))))
parser.add_argument('--environment', choices=ENVIRONMENTS, default='local')
args = parser.parse_args()
version = release_version()
channel = release_channel(args.environment)
output = output_directory(args.environment)
output.mkdir(parents=True, exist_ok=True)
payload = output/artifact_name(args.environment)
installer = output/(artifact_name(args.environment)+'-安装包.exe')
build_configuration = json.loads((output/'build-configuration.json').read_text(encoding='utf-8'))
exe = Path(build_configuration['productionExecutable'])

def digest(filename):
    return hashlib.sha256(filename.read_bytes()).hexdigest()

def tool(filename, arguments):
    output = subprocess.check_output([str(filename), *arguments], text=True, encoding='utf-8').strip()
    if '--help' in arguments:
        output = re.search(r'Qt Deploy Tool [^\r\n]+', output).group(0)
    return {'path': str(filename), 'version': output, 'sha256': digest(filename)}

if not exe.is_file():
    raise SystemExit('Build ShadowChat.exe first')
if any(build_configuration.get(key) != value for key, value in {'version': version, 'releaseEnvironment': args.environment, 'channel': channel, 'sourceExecutableSha256': digest(exe)}.items()):
    raise SystemExit('Production executable does not match the selected environment build record')
if not build_configuration.get('updateSource'):
    raise SystemExit('Effective update source is missing from production build record')
update_source = urlparse(build_configuration['updateSource'])
download_page = urlunparse((update_source.scheme, update_source.netloc, '/tools/shadowchat', '', '', ''))
if args.environment != 'local':
    download_page = 'https://github.com/shadowyli/shadow-chat/releases'
identity = build_configuration.get('binaryBuildIdentity', {})
for field, expected_value in {'version': version, 'environment': args.environment, 'channel': channel, 'updateSource': build_configuration['updateSource']}.items():
    if identity.get(field) != expected_value:
        raise SystemExit('Verified executable identity does not match selected installer configuration: '+field)
if installer.exists():
    raise SystemExit('Installer already exists; preserve prior artifacts and select a new candidate directory before rebuilding')
seed = args.recovery_dir/'shadowchat-recovery.ini'
owner_file = args.recovery_dir/'负责人恢复密码.txt'
if not seed.is_file() or not owner_file.is_file():
    raise SystemExit('Existing recovery seed and private owner file required for local validation; no credentials generated')
config = configparser.ConfigParser()
config.read(seed, encoding='ascii')
if config.sections() != ['General'] or set(config['General']) != {'salt', 'password'}:
    raise SystemExit('Unexpected recovery seed structure')
if not re.fullmatch(r'[0-9a-f]{32}', config['General']['salt']) or not re.fullmatch(r'[0-9a-f]{64}', config['General']['password']):
    raise SystemExit('Recovery seed must contain only the expected salt and PBKDF2 hash')
owner_key = owner_file.read_text(encoding='utf-8').splitlines()[2].encode()
if not owner_key:
    raise SystemExit('Empty private recovery key rejected')
expected = hashlib.pbkdf2_hmac('sha256', owner_key, bytes.fromhex(config['General']['salt']), 100000, 32).hex()
if expected != config['General']['password']:
    raise SystemExit('Existing recovery seed failed owner-key validation')
qt_bin = args.qt_dir/'bin'
tools = {
    'qmake': tool(qt_bin/'qmake.exe', ['-query', 'QT_VERSION']),
    'windeployqt': tool(qt_bin/'windeployqt.exe', ['--help']),
    'nsis': tool(args.makensis, ['/VERSION']),
}
redist = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC')
crt_candidates = sorted(redist.glob('*/x86/Microsoft.VC143.CRT'), key=lambda item: tuple(int(number) for number in item.parent.parent.name.split('.')))
crt = args.crt_dir or (crt_candidates[-1] if crt_candidates else None)
if crt is None or not crt.is_dir():
    raise SystemExit('Microsoft x86 redistributable CRT missing')
tls = json.loads((args.tls_dir/'TLS-DEPENDENCIES.json').read_text(encoding='utf-8'))
tls_files = ['libssl-1_1.dll', 'libcrypto-1_1.dll', 'LICENSE.OpenSSL-1.1.1w.txt', 'TLS-DEPENDENCIES.json']
for name in tls_files[:2]:
    data = (args.tls_dir/name).read_bytes()
    pe_offset = int.from_bytes(data[0x3c:0x40], 'little')
    if data[pe_offset:pe_offset+4] != b'PE\0\0' or int.from_bytes(data[pe_offset+4:pe_offset+6], 'little') != 0x14c:
        raise SystemExit('TLS runtime is not Windows x86: '+name)
    if digest(args.tls_dir/name) != tls['runtimeSha256'][name]:
        raise SystemExit('TLS runtime differs from recorded build: '+name)
if digest(args.tls_dir/tls_files[2]) != tls['licenseSha256']:
    raise SystemExit('TLS license differs from recorded upstream license')
if payload.exists():
    payload.rename(payload.with_name(payload.name+'-previous-'+uuid.uuid4().hex[:8]))
payload.mkdir(parents=True)
shutil.copy2(exe, payload/exe.name)
deploy_command = [str(qt_bin/'windeployqt.exe'), '--release', '--no-compiler-runtime', '--no-translations', '--dir', str(payload), str(payload/exe.name)]
deploy_env = dict(os.environ)
deploy_env['PATH'] = str(qt_bin)+os.pathsep+deploy_env.get('PATH', '')
subprocess.run(deploy_command, check=True, cwd=root, env=deploy_env)
allowed_plugin_dirs = {'audio', 'bearer', 'iconengines', 'imageformats', 'mediaservice', 'platforms', 'playlistformats', 'printsupport', 'styles'}
for deployed in payload.rglob('*'):
    if deployed.is_symlink():
        raise SystemExit('Linked deployment file rejected')
    if deployed.is_file():
        relative = deployed.relative_to(payload)
        if relative.as_posix() != exe.name and (deployed.suffix.lower() != '.dll' or (len(relative.parts) > 1 and (len(relative.parts) != 2 or relative.parts[0] not in allowed_plugin_dirs))):
            raise SystemExit('Unexpected Qt deployment file: '+relative.as_posix())
for dll in crt.glob('*.dll'):
    shutil.copy2(dll, payload/dll.name)
for required in ['msvcp140.dll', 'vcruntime140.dll', 'platforms/qwindows.dll']:
    if not (payload/required).is_file():
        raise SystemExit('Required runtime missing: '+required)
public_files = [
    root/'packaging/beebeep.rc', root/'LICENSE.txt', root/'THIRD_PARTY_NOTICES.md',
    root/'packaging/使用说明.txt', root/'locale/beebeep_cn.qm', root/'misc/beep.wav',
]
for source in public_files:
    if source.name == '使用说明.txt':
        instructions = source.read_text(encoding='utf-8')
        for token, value in {'@APP_VERSION@': version, '@APP_CHANNEL@': channel, '@APP_ENVIRONMENT@': args.environment}.items():
            instructions = instructions.replace(token, value)
        (payload/source.name).write_text(instructions, encoding='utf-8')
    else:
        shutil.copy2(source, payload/source.name)
shortcut_icon = root/'packaging/shadowchat.ico'
if digest(shortcut_icon) != digest(root/'branding/generated/app.ico'):
    raise SystemExit('Shortcut ICO differs from approved generated branding')
shortcut_icon_name = f'shadowchat-{version}.ico'
shutil.copy2(shortcut_icon, payload/shortcut_icon_name)
for name in tls_files:
    shutil.copy2(args.tls_dir/name, payload/name)
shutil.copy2(seed, payload/seed.name)
inventory = {}
for source in sorted(payload.rglob('*')):
    if source.is_file():
        data = source.read_bytes()
        if owner_key in data or owner_key.decode().encode('utf-16le') in data:
            raise SystemExit('Plaintext recovery credential found in payload; packaging stopped')
        inventory[source.relative_to(payload).as_posix()] = {'size': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
subprocess.run([str(args.makensis), '/INPUTCHARSET', 'UTF8', '/DAPP_VERSION='+version, '/DAPP_CHANNEL='+channel, '/DAPP_ENVIRONMENT='+args.environment, '/DAPP_WEB='+download_page, str(root/'packaging/shadowchat-installer.nsi')], check=True, cwd=root)
result = {
    'version': version, 'channel': channel, 'releaseEnvironment': args.environment,
    'updateSource': build_configuration['updateSource'], 'downloadPage': download_page, 'installer': str(installer),
    'size': installer.stat().st_size, 'sha256': digest(installer),
    'sourceExecutableSha256': digest(exe),
    'appLocalCrt': {dll.name: digest(dll) for dll in crt.glob('*.dll')},
    'crtSource': str(crt), 'tools': tools, 'qtDeployCommand': deploy_command,
    'payloadInventory': inventory,
    'payloadPolicy': 'Fresh windeployqt deployment, fixed public resources, current executable, app-local CRT and verified x86 TLS runtime with license/provenance, validated existing salted recovery hash only; no old dist baseline',
    'plaintextRecoveryScanPassed': True, 'existingRecoverySeedValidated': True,
    'privateInputPolicy': 'Only existing shadowchat-recovery.ini is distributed; owner file is read locally to validate PBKDF2 and scan payload, never copied or printed',
    'environment': {'windows': platform.platform(), 'python': platform.python_version(), 'architecture': 'x86 application and CRT'},
    'tlsRuntime': tls,
    'shortcutIcon': {'filename': shortcut_icon_name, 'sha256': digest(shortcut_icon), 'source': 'packaging/shadowchat.ico'},
    'productionBuild': build_configuration,
}
(output/'release-build.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in result.items() if key != 'payloadInventory'}, ensure_ascii=False, indent=2))
