"""Build and exercise startup Settings against a disposable HKCU subtree."""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'packaging'))
from verify_installation import host_snapshot

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output', type=Path, default=ROOT/'dist/qa-startup')
parser.add_argument('--qt-dir', type=Path, default=Path('E:/ShadowChat/Qt/5.15.2/msvc2019'))
parser.add_argument('--probe-invalid-command-repair', action='store_true')
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
qt = args.qt_dir.resolve()
msvc = sorted(Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC').iterdir())[-1]
sdk = Path('C:/Program Files (x86)/Windows Kits/10')
sdk_version = sorted((sdk/'Include').iterdir())[-1].name
environment = dict(os.environ)
environment['PATH'] = os.pathsep.join([str(qt/'bin'), str(msvc/'bin/Hostx64/x86'), str(sdk/'bin'/sdk_version/'x86'), environment['PATH']])
environment['INCLUDE'] = os.pathsep.join([str(msvc/'include')] + [str(sdk/'Include'/sdk_version/name) for name in ('ucrt','shared','um','winrt','cppwinrt')])
environment['LIB'] = os.pathsep.join([str(msvc/'lib/x86')] + [str(sdk/'Lib'/sdk_version/name/'x86') for name in ('ucrt','um')])
environment['QT_QPA_PLATFORM'] = 'windows'
environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(qt/'plugins/platforms')
environment['QT_LOGGING_TO_CONSOLE'] = '1'
environment['PYTHONDONTWRITEBYTECODE'] = '1'
commands = []

def run(command, directory, log, env=environment, timeout=1800, allow_failure=False):
    command = [str(item) for item in command]
    commands.append({'command': command, 'cwd': str(directory), 'log': str(log)})
    with log.open('wb') as stream:
        result = subprocess.run(command, cwd=directory, env=env, stdout=stream, stderr=stream, timeout=timeout)
    if result.returncode and not allow_failure:
        raise RuntimeError(f'Command failed ({result.returncode}), see {log}')
    return result.returncode

before = host_snapshot()
(output/'host-startup-before.json').write_text(json.dumps(before,ensure_ascii=False,indent=2),encoding='utf-8')
results = []
integration_probe = None
try:
    production = output/'production'
    production.mkdir(exist_ok=True)
    run([qt/'bin/qmake.exe',ROOT/'src/src.pro','SHADOWCHAT_ENV=local','CONFIG+=release','CONFIG-=debug_and_release','-after',
         'DESTDIR='+(production/'bin').as_posix(),'OBJECTS_DIR='+(production/'objects').as_posix(),
         'MOC_DIR='+(production/'generated').as_posix(),'UI_DIR='+(production/'generated').as_posix(),
         'RCC_DIR='+(production/'generated').as_posix(),'QMAKE_CXXFLAGS+=/MP4'],production,output/'qmake-production.log')
    run([msvc/'bin/Hostx64/x86/nmake.exe','/nologo'],production,output/'build-production.log')
    build = output/'preferences'
    build.mkdir(exist_ok=True)
    run([qt/'bin/qmake.exe',ROOT/'tests/startup_preferences_test.pro','SHADOWCHAT_PREVIEW_BUILD='+production.as_posix(),
         'DESTDIR='+build.as_posix()],build,output/'qmake-test.log')
    run([msvc/'bin/Hostx64/x86/nmake.exe','/nologo'],build,output/'build-test.log')
    for scale in ('1','1.5','2'):
        screenshots = output/('dpi-'+scale)
        screenshots.mkdir(exist_ok=True)
        test_environment = dict(environment,QT_SCALE_FACTOR=scale,SHADOW_STARTUP_SCREENSHOTS=str(screenshots))
        log = screenshots/'test.log'
        run([build/'startup_preferences_test.exe'],screenshots,log,test_environment,timeout=120)
        results.append({'scale':scale,'passed':True,'log':str(log),'screenshots':str(screenshots)})
    spaced = output/'app with spaces'
    spaced.mkdir(exist_ok=True)
    shutil.copy2(build/'startup_preferences_test.exe',spaced/'startup_preferences_test.exe')
    test_environment = dict(environment,QT_SCALE_FACTOR='1',SHADOW_STARTUP_SCREENSHOTS=str(spaced))
    run([spaced/'startup_preferences_test.exe'],spaced,spaced/'test.log',test_environment,timeout=120)
    results.append({'case':'actual executable path containing spaces','passed':True,'log':str(spaced/'test.log')})
    if args.probe_invalid_command_repair:
        screenshots = output/'integration-invalid-command'
        screenshots.mkdir(exist_ok=True)
        test_environment = dict(environment,QT_SCALE_FACTOR='1',SHADOW_STARTUP_SCREENSHOTS=str(screenshots))
        log = screenshots/'test.log'
        code = run([build/'startup_preferences_test.exe','--verify-invalid-command-repair'],screenshots,log,test_environment,timeout=120,allow_failure=True)
        integration_probe = {'passed':code==0,'exitCode':code,'log':str(log)}
finally:
    after = host_snapshot()
    (output/'host-startup-after.json').write_text(json.dumps(after,ensure_ascii=False,indent=2),encoding='utf-8')
    (output/'commands.json').write_text(json.dumps(commands,ensure_ascii=False,indent=2),encoding='utf-8')
    if before != after:
        raise RuntimeError('Real user Run, Startup, registration or shortcuts changed during QA')
report = {'passed':True,'tests':results,'hostRunAndStartupUnchanged':before==after,
          'productionObjects':str(production),'qt':'5.15.2','architecture':'MSVC x86',
          'invalidCommandRepairIntegrationProbe':integration_probe,
          'scope':'Native Qt Settings and actual GuiMain settings modal, synthetic account only; no Core::start/network/account login'}
(output/'preferences-result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(report,ensure_ascii=False,indent=2))
