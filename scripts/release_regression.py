"""Run native release regressions against the selected frozen local production build."""
from pathlib import Path
import argparse
import datetime
import json
import os
import re
import shutil
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT/'packaging'))
from release_config import artifact_name, sha256, version
from source_snapshot import verify


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tests', nargs='+', default=[
        'github_release', 'dev_preview', 'profile_editor_preview',
        'login_startup', 'onboarding_startup', 'login_email_domains',
    ])
    parser.add_argument('--scale-factors', nargs='+', choices=('1', '1.5', '2'), default=['1', '1.5', '2'])
    parser.add_argument('--runtime-dir', type=Path)
    parser.add_argument('--dev-preview-modes', nargs='+', choices=(
        'all', 'reply', 'local-send', 'history', 'welcome', 'corners',
        'startup-size', 'messagebox', 'sidebar',
    ), default=['all'])
    args = parser.parse_args()
    for name in args.tests:
        if not re.fullmatch(r'[a-z_]+', name) or not (ROOT/'tests'/(name+'_test.pro')).is_file():
            parser.error('Unknown release test project: '+name)
    qmake = shutil.which('qmake')
    nmake = shutil.which('nmake')
    if not qmake or not nmake:
        parser.error('Use an x86 MSVC command environment with the selected Qt SDK on PATH')
    output = ROOT/'dist/local'
    production = output/'production'
    configuration = json.loads((output/'build-configuration.json').read_text(encoding='utf-8'))
    identity = configuration.get('binaryBuildIdentity', {})
    for field, expected in {'version': version(), 'releaseEnvironment': 'local', 'channel': 'test'}.items():
        if configuration.get(field) != expected:
            raise SystemExit('Release regression build configuration mismatch: '+field)
    for field, expected in {
        'version': version(), 'environment': 'local', 'channel': 'test',
        'updateSource': configuration['updateSource'],
    }.items():
        if identity.get(field) != expected:
            raise SystemExit('Release regression binary identity mismatch: '+field)
    executable = production/'bin/ShadowChat.exe'
    if sha256(executable) != configuration['sourceExecutableSha256']:
        raise SystemExit('Production executable changed after its recorded build')
    if Path(configuration['qmake']['path']).resolve() != Path(qmake).resolve() or sha256(qmake) != configuration['qmake']['sha256']:
        raise SystemExit('Regression qmake differs from the production toolchain')
    if Path(configuration['nmake']['path']).resolve() != Path(nmake).resolve() or sha256(nmake) != configuration['nmake']['sha256']:
        raise SystemExit('Regression nmake differs from the production toolchain')
    snapshot = output/'build-source-inputs.json'
    if sha256(snapshot) != configuration['sourceSnapshotSha256']:
        raise SystemExit('Source snapshot differs from the selected production build')
    verify(snapshot)
    payload = args.runtime_dir.resolve() if args.runtime_dir else output/artifact_name('local')
    if sha256(payload/'ShadowChat.exe') != configuration['sourceExecutableSha256']:
        raise SystemExit('Stage the matching release payload before native regression')
    environment = dict(os.environ)
    environment['PATH'] = str(payload)+os.pathsep+environment['PATH']
    environment['QT_QPA_PLATFORM'] = 'windows'
    environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(payload/'platforms')
    environment['QT_PLUGIN_PATH'] = str(payload)
    directory = output/'release-tests'/('run-'+uuid.uuid4().hex[:8])
    directory.mkdir(parents=True)
    platforms = directory/'qt-platforms'
    platforms.mkdir()
    platform_inventory = {}
    for name in ('qwindows.dll', 'qoffscreen.dll', 'qminimal.dll'):
        source = Path(qmake).resolve().parent.parent/'plugins/platforms'/name
        shutil.copy2(source, platforms/name)
        platform_inventory[name] = sha256(source)
    environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(platforms)
    report = {
        'version': version(), 'releaseEnvironment': 'local', 'channel': 'test',
        'sourceExecutableSha256': configuration['sourceExecutableSha256'],
        'sourceSnapshotSha256': sha256(snapshot),
        'testPlatformSha256': platform_inventory,
        'startedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
        'passed': False, 'results': [],
        'scope': 'Selected final-object native tests with synthetic profiles; not real office network acceptance',
    }
    try:
        for name in args.tests:
            workspace = directory/name
            workspace.mkdir()
            log = workspace/'verification.log'
            scales = args.scale_factors if name in ('login_startup', 'onboarding_startup', 'login_email_domains', 'dev_preview') else ['1']
            modes = args.dev_preview_modes if name == 'dev_preview' else ['all']
            with log.open('wb') as stream:
                for command in (
                    [qmake, str(ROOT/'tests'/(name+'_test.pro')), 'SHADOWCHAT_PREVIEW_BUILD='+production.as_posix()],
                    [nmake, '/nologo'],
                ):
                    subprocess.run(command, cwd=workspace, env=environment, stdout=stream,
                                   stderr=subprocess.STDOUT, check=True, timeout=300, creationflags=0x08000000)
                target = workspace/(name+'_test.exe')
                if not target.is_file():
                    target = workspace/'release'/(name+'_test.exe')
                for scale in scales:
                    test_environment = dict(environment)
                    test_environment['QT_SCALE_FACTOR'] = scale
                    for mode in modes:
                        arguments = [] if mode == 'all' else ['--'+mode+'-only']
                        result = subprocess.run([str(target), *arguments], cwd=workspace, env=test_environment,
                                                stdout=stream, stderr=subprocess.STDOUT, timeout=90,
                                                creationflags=0x08000000)
                        report['results'].append({
                            'test': name, 'mode': mode, 'scale': scale, 'exitCode': result.returncode,
                            'passed': result.returncode == 0, 'log': str(log),
                        })
                        if result.returncode:
                            raise RuntimeError('Native regression failed: '+name+' '+mode+' at scale '+scale)
        verify(snapshot)
        report['passed'] = True
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        report['error'] = str(error)
    finally:
        record = directory/'verification.json'
        record.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        print(json.dumps({'passed': report['passed'], 'report': str(record)}, ensure_ascii=False))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
