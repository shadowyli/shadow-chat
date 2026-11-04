"""Exercise the final installer verifier with an actual current executable and synthetic recovery seed."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'packaging'))
from verify_installation import hash_file, host_snapshot, require_installer_isolation

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--previous-receipt', type=Path, required=True)
parser.add_argument('--previous-payload', type=Path, required=True)
args = parser.parse_args()
previous = json.loads(args.previous_receipt.read_text(encoding='utf-8'))
output = ROOT/'dist/qa-startup/verifier-contract'
output.mkdir(parents=True, exist_ok=True)
payload = output/'payload'
payload.mkdir(exist_ok=True)
qt = Path('E:/ShadowChat/Qt/5.15.2/msvc2019')
environment = dict(os.environ, PATH=str(qt/'bin')+os.pathsep+os.environ['PATH'])
source = ROOT/'dist/qa-startup/production/bin/ShadowChat.exe'
shutil.copy2(source, payload/'ShadowChat.exe')
identity = json.loads(subprocess.check_output([str(source),'--build-info'],env=environment,text=True,encoding='utf-8',timeout=30))
with (output/'deploy.log').open('wb') as stream:
    subprocess.run([str(qt/'bin/windeployqt.exe'),'--release','--no-compiler-runtime','--no-translations','--dir',str(payload),str(payload/'ShadowChat.exe')],env=environment,stdout=stream,stderr=stream,check=True)
crt = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Redist/MSVC/14.44.35112/x86/Microsoft.VC143.CRT')
for source_dll in crt.glob('*.dll'):
    shutil.copy2(source_dll,payload/source_dll.name)
icon_name = 'shadowchat-'+identity['version']+'.ico'
shutil.copy2(ROOT/'packaging/shadowchat.ico',payload/icon_name)
recovery = output/'synthetic-recovery'
recovery.mkdir(exist_ok=True)
password = 'synthetic-startup-qa-only-not-an-owner-key'
salt = bytes(range(16))
digest = hashlib.pbkdf2_hmac('sha256',password.encode(),salt,100000,32).hex()
(payload/'shadowchat-recovery.ini').write_text('[General]\nsalt='+salt.hex()+'\npassword='+digest+'\n',encoding='ascii')
(recovery/'负责人恢复密码.txt').write_text('Synthetic fixture\nDo not use\n'+password+'\n',encoding='utf-8')
installer = output/'verifier-fixture-installer.exe'
compiler = Path('E:/ShadowChat/beebeep-code-r1557/.tools/NSIS/makensis.exe')
with (output/'compile.log').open('wb') as stream:
    subprocess.run([str(compiler),'/INPUTCHARSET','UTF8','/DAPP_VERSION='+identity['version'],
                    '/DAPP_CHANNEL='+identity['channel'],'/DAPP_ENVIRONMENT='+identity['environment'],
                    '/DAPP_WEB=https://example.test/shadowchat','/DPAYLOAD='+str(payload),'/DOUTPUT='+str(installer),
                    str(ROOT/'packaging/shadowchat-installer.nsi')],stdout=stream,stderr=stream,check=True)
require_installer_isolation(installer)
try:
    require_installer_isolation(Path(previous['installer']))
except RuntimeError:
    pass
else:
    raise RuntimeError('Expected historical installer fixture without QA capability')
receipt = {
    'installer':str(installer),'sha256':hash_file(installer),'sourceExecutableSha256':hash_file(payload/'ShadowChat.exe'),
    'version':identity['version'],'channel':identity['channel'],'releaseEnvironment':identity['environment'],
    'updateSource':identity['updateSource'],'downloadPage':'https://example.test/shadowchat',
    'shortcutIcon':{'filename':icon_name,'sha256':hash_file(payload/icon_name)},
    'payloadInventory':{item.relative_to(payload).as_posix():{'sha256':hash_file(item),'size':item.stat().st_size} for item in payload.rglob('*') if item.is_file()},
    'scope':'Verifier self-test fixture with actual locally built executable, fresh Qt/CRT and synthetic recovery seed; not a release package.',
}
receipt_path = output/'fixture-receipt.json'
receipt_path.write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
command = [sys.executable,'-B',str(ROOT/'packaging/verify_installation.py'),'--receipt',str(receipt_path),
           '--previous-installer',previous['installer'],'--previous-receipt',str(args.previous_receipt.resolve()),
           '--previous-payload',str(args.previous_payload.resolve()),'--recovery-dir',str(recovery),'--output',str(output/'verification')]
(output/'command.json').write_text(json.dumps(command,ensure_ascii=False,indent=2),encoding='utf-8')
before = host_snapshot()
with (output/'verification-console.json').open('wb') as stream:
    subprocess.run(command,stdout=stream,stderr=stream,check=True,timeout=300)
assert host_snapshot() == before
print(json.dumps({'passed':True,'receipt':str(receipt_path),'result':str(output/'verification/result.json'),
                  'historicalUnsupportedInstallerRejectedBeforeExecution':True,
                  'scope':receipt['scope']},ensure_ascii=False,indent=2))
