"""Configure, build and test one explicit Windows release environment."""
from pathlib import Path
import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
import uuid

root = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(root/'packaging'))
from release_config import ENVIRONMENTS, channel, output_directory, sha256, version
from source_snapshot import capture, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('action', nargs='?', choices=('build', 'rebuild', 'test', 'configure'), default='build')
parser.add_argument('--environment', choices=ENVIRONMENTS, default='local')
parser.add_argument('--local-update-port', type=int, default=5173)
parser.add_argument('--tests', nargs='+')
args = parser.parse_args()
if not 1 <= args.local_update_port <= 65535:
    raise SystemExit('Local update port must be between 1 and 65535')
output = output_directory(args.environment)
output.mkdir(parents=True, exist_ok=True)
production = output/'production'
executable = production/'bin/ShadowChat.exe'
update_source = {
    'local': 'http://127.0.0.1:'+str(args.local_update_port)+'/updates/shadowchat.json',
    'dev': 'https://api.github.com/repos/shadowyli/shadow-chat/releases',
    'main': 'https://api.github.com/repos/shadowyli/shadow-chat/releases',
}[args.environment]
qmake_arguments = ['SHADOWCHAT_ENV='+args.environment, 'CONFIG+=no_batch']
if args.environment == 'local':
    qmake_arguments.append('SHADOWCHAT_LOCAL_UPDATE_PORT='+str(args.local_update_port))
configuration = {
    'version': version(), 'releaseEnvironment': args.environment,
    'channel': channel(args.environment), 'updateSource': update_source,
    'qmakeArguments': qmake_arguments,
}
environment = dict(os.environ)
environment['PYTHONDONTWRITEBYTECODE'] = '1'
environment['SHADOWCHAT_QMAKE_ARGS'] = ' '.join(qmake_arguments)
environment['SHADOWCHAT_REGRESSION_ROOT'] = (output/'regression').as_posix()
environment['SHADOWCHAT_PRODUCTION_ROOT'] = production.as_posix()
qmake = shutil.which('qmake')
nmake = shutil.which('nmake')
if not qmake or not nmake:
    raise SystemExit('Run through scripts/release-windows.cmd with Qt and x86 MSVC available')

def run(command, cwd=root):
    print('RUN '+subprocess.list2cmdline([str(value) for value in command]), flush=True)
    subprocess.run(command, cwd=cwd, env=environment, check=True)

def check_defines(makefile):
    text = makefile.read_text(encoding='utf-8')
    defines = next((line for line in text.splitlines() if line.startswith('DEFINES')), '')
    if args.environment == 'main':
        valid = 'SHADOWCHAT_BUILD_MAIN' in defines and 'SHADOWCHAT_BUILD_DEV' not in defines and 'SHADOWCHAT_BUILD_LOCAL' not in defines
    elif args.environment == 'dev':
        valid = 'SHADOWCHAT_BUILD_DEV' in defines and 'SHADOWCHAT_BUILD_MAIN' not in defines and 'SHADOWCHAT_BUILD_LOCAL' not in defines
    else:
        valid = 'SHADOWCHAT_BUILD_LOCAL' in defines and 'SHADOWCHAT_BUILD_MAIN' not in defines and 'SHADOWCHAT_BUILD_DEV' not in defines and 'SHADOWCHAT_LOCAL_UPDATE_PORT='+str(args.local_update_port) in defines
    if args.environment != 'local' and 'SHADOWCHAT_LOCAL_UPDATE_PORT' in defines:
        valid = False
    if not valid:
        raise SystemExit('qmake did not apply the selected release environment: '+defines)
    return defines

if args.action == 'configure':
    makefile = output/'Makefile.configuration-check'
    run([qmake, str(root/'src/src.pro'), '-o', str(makefile), *qmake_arguments], cwd=output)
    defines = check_defines(Path(str(makefile)+'.Release'))
    configuration['effectiveDefines'] = defines
    configuration['configuredOnly'] = True
    (output/'configured-parameters.json').write_text(json.dumps(configuration, indent=2), encoding='utf-8')
    print(json.dumps(configuration, indent=2))
    raise SystemExit(0)

active = root/'dist/active-build-configuration.json'
if args.action == 'test':
    recorded = json.loads((output/'build-configuration.json').read_text(encoding='utf-8'))
    for field in ['version', 'releaseEnvironment', 'channel', 'updateSource', 'qmakeArguments']:
        if recorded.get(field) != configuration[field]:
            raise SystemExit('Test configuration differs from production build: '+field)
    if sha256(executable) != recorded['sourceExecutableSha256']:
        raise SystemExit('Production executable changed since selected environment build')
    test_define = {
        'local': 'SHADOWCHAT_LOCAL_UPDATE_PORT='+str(args.local_update_port),
        'dev': 'SHADOWCHAT_BUILD_DEV',
        'main': 'SHADOWCHAT_BUILD_MAIN',
    }[args.environment]
    environment['SHADOWCHAT_QMAKE_ARGS'] += ' DEFINES+='+test_define
    if args.environment == 'local':
        environment['SHADOWCHAT_QMAKE_ARGS'] += ' DEFINES+=SHADOWCHAT_BUILD_LOCAL'
    environment['SHADOWCHAT_RELEASE_ENV'] = args.environment
    environment['SHADOWCHAT_RELEASE_CHANNEL'] = channel(args.environment)
    environment['SHADOWCHAT_UPDATE_SOURCE'] = update_source
    if args.tests:
        for name in args.tests:
            if not name.replace('_', '').isalpha() or not (root/'tests'/(name+'_test.pro')).is_file():
                raise SystemExit('Unknown selected regression project: '+name)
        environment['SHADOWCHAT_TEST_SELECTION'] = ' '.join(args.tests)
    else:
        environment.pop('SHADOWCHAT_TEST_SELECTION', None)
    tls_runtime = root/'dist/tls-runtime-x86'
    tls_record = json.loads((tls_runtime/'TLS-DEPENDENCIES.json').read_text(encoding='utf-8'))
    for name, expected in tls_record['runtimeSha256'].items():
        if sha256(tls_runtime/name) != expected:
            raise SystemExit('Test TLS runtime digest mismatch: '+name)
    environment['PATH'] = str(tls_runtime)+os.pathsep+environment['PATH']
    fixtures = [directory for directory in (root/'dist').glob('tls-fixture-*') if (directory/'server.pem').is_file() and (directory/'synthetic-server-key.pem').is_file()]
    if not fixtures:
        raise SystemExit('Synthetic TLS fixture is required for update network regression')
    environment['SHADOW_UPDATE_TLS_FIXTURE'] = str(max(fixtures, key=lambda directory: directory.stat().st_mtime))
    regression = output/'regression'
    test_record = output/'test-configuration.json'
    prior_test = json.loads(test_record.read_text(encoding='utf-8')) if test_record.exists() else None
    if prior_test != configuration and regression.exists():
        archived = output/('regression-before-'+uuid.uuid4().hex[:8])
        if regression.is_symlink() or regression.resolve().parent != output.resolve() or archived.resolve().parent != output.resolve():
            raise SystemExit('Unsafe regression output path')
        regression.rename(archived)
    test_record.write_text(json.dumps(configuration, indent=2), encoding='utf-8')
    run(['cmd.exe', '/d', '/c', str(root/'scripts/test-windows.cmd')])
    raise SystemExit(0)

previous = json.loads(active.read_text(encoding='utf-8')) if active.exists() else None
if args.action == 'rebuild' or previous != configuration:
    run([sys.executable, '-B', str(root/'scripts/isolate-build-output.py')])
    if production.exists():
        archived = output/('production-before-'+uuid.uuid4().hex[:8])
        if production.is_symlink() or production.resolve().parent != output.resolve():
            raise SystemExit('Unsafe production output path')
        production.rename(archived)
production.mkdir(exist_ok=True)
snapshot = output/'build-source-inputs.json'
snapshot_digest = capture(snapshot)
production_arguments = [*qmake_arguments, '-after', 'DESTDIR='+(production/'bin').as_posix(), 'OBJECTS_DIR='+(production/'objects').as_posix(), 'MOC_DIR='+(production/'generated').as_posix(), 'UI_DIR='+(production/'generated').as_posix(), 'RCC_DIR='+(production/'generated').as_posix()]
run([qmake, str(root/'src/src.pro'), '-o', 'Makefile', *production_arguments], cwd=production)
configuration['effectiveDefines'] = check_defines(production/'Makefile.Release')
run([nmake, '/nologo'], cwd=production)
identity = json.loads(subprocess.check_output([str(executable), '--build-info'], cwd=executable.parent, env=environment, text=True, encoding='utf-8'))
for field, expected in {'version': version(), 'environment': args.environment, 'channel': channel(args.environment), 'updateSource': update_source}.items():
    if identity.get(field) != expected:
        raise SystemExit('Actual executable build identity mismatch: '+field)
source_verification = verify(snapshot)
configuration.update({
    'builtAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'sourceExecutableSha256': sha256(executable),
    'productionExecutable': str(executable),
    'productionDirectory': str(production),
    'effectiveQmakeArguments': production_arguments,
    'sourceSnapshotSha256': snapshot_digest,
    'sourceInputVerification': source_verification,
    'qmake': {'path': qmake, 'sha256': sha256(qmake), 'version': subprocess.check_output([qmake, '-v'], text=True).strip()},
    'nmake': {'path': nmake, 'sha256': sha256(nmake)},
    'compilerPath': shutil.which('cl'),
    'compilerSha256': sha256(shutil.which('cl')),
    'msvcToolsVersion': environment.get('VCToolsVersion'),
    'windowsSdkVersion': environment.get('WindowsSDKVersion'),
    'binaryBuildIdentity': identity,
    'sourceBranch': subprocess.check_output(['git', 'rev-parse', '--abbrev-ref', 'HEAD'], cwd=root, text=True).strip(),
    'sourceCommitBase': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
    'sourceArchivePolicy': 'Built from frozen working tree inputs; primary agent attaches final committed source identity after review',
})
(output/'build-configuration.json').write_text(json.dumps(configuration, indent=2), encoding='utf-8')
active.write_text(json.dumps({key: configuration[key] for key in ['version', 'releaseEnvironment', 'channel', 'updateSource', 'qmakeArguments']}, indent=2), encoding='utf-8')
print(json.dumps(configuration, indent=2))
