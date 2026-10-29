"""Build the Qt 5.15 compatible x86 TLS runtime from verified upstream source."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

root = Path(__file__).resolve().parent.parent
tooling = root/'dist/tls-tooling'
tooling.mkdir(parents=True, exist_ok=True)
dependencies = {
    'openssl': {
        'url': 'https://github.com/openssl/openssl/releases/download/OpenSSL_1_1_1w/openssl-1.1.1w.tar.gz',
        'sha256': 'cf3098950cb4d853ad95c0841f1f9c6d3dc102dccfcacd521d93925208b76ac8',
        'archive': 'openssl-1.1.1w.tar.gz',
    },
    'perl': {
        'url': 'https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_54231_64bit/strawberry-perl-5.42.3.1-64bit-portable.zip',
        'sha256': '6a081a811781c30aca51dbc036afd93092af91e3297901f02c17043795a10690',
        'archive': 'strawberry-perl-portable.zip',
        'digestSource': 'https://strawberryperl.com/releases.json',
    },
}

def digest(filename):
    return hashlib.sha256(filename.read_bytes()).hexdigest()

def inside(directory, name):
    target = (directory/name).resolve()
    if directory.resolve() not in target.parents and target != directory.resolve():
        raise SystemExit('Archive path escapes tooling directory')
    return target

for dependency in dependencies.values():
    archive = tooling/dependency['archive']
    if not archive.exists():
        temporary = archive.with_suffix(archive.suffix+'.part')
        with urllib.request.urlopen(dependency['url'], timeout=60) as response, temporary.open('wb') as output:
            shutil.copyfileobj(response, output)
        if digest(temporary) != dependency['sha256']:
            raise SystemExit('Upstream dependency digest mismatch')
        temporary.replace(archive)
    if digest(archive) != dependency['sha256']:
        raise SystemExit('Existing dependency digest mismatch')
source = tooling/'openssl-1.1.1w'
if not source.exists():
    with tarfile.open(tooling/dependencies['openssl']['archive']) as archive:
        for member in archive.getmembers():
            inside(tooling, member.name)
            if member.issym() or member.islnk():
                raise SystemExit('Linked source archive entry rejected')
        archive.extractall(tooling)
perl_root = tooling/'native-perl'
perl = perl_root/'perl/bin/perl.exe'
if not perl.exists():
    with zipfile.ZipFile(tooling/dependencies['perl']['archive']) as archive:
        for member in archive.infolist():
            if member.filename.startswith('perl/'):
                inside(perl_root, member.filename)
                archive.extract(member, perl_root)
environment = dict(os.environ)
environment['PATH'] = str(perl.parent)+os.pathsep+environment.get('PATH', '')
configure = [str(perl), 'Configure', 'VC-WIN32', 'shared', 'no-asm', 'no-tests']
subprocess.run(configure, cwd=source, env=environment, check=True)
subprocess.run(['nmake', '/nologo', 'build_libs'], cwd=source, env=environment, check=True)
runtime = root/'dist/tls-runtime-x86'
runtime.mkdir(exist_ok=True)
files = {}
for name in ['libssl-1_1.dll', 'libcrypto-1_1.dll']:
    shutil.copy2(source/name, runtime/name)
    files[name] = digest(runtime/name)
license_name = 'LICENSE.OpenSSL-1.1.1w.txt'
shutil.copy2(source/'LICENSE', runtime/license_name)
result = {
    'version': 'OpenSSL 1.1.1w', 'target': 'VC-WIN32', 'architecture': 'x86',
    'dependencies': dependencies, 'configure': configure,
    'buildCommand': ['nmake', '/nologo', 'build_libs'],
    'runtimeSha256': files, 'licenseFile': license_name,
    'licenseSha256': digest(runtime/license_name),
    'policy': 'Compatibility runtime for Qt built against OpenSSL 1.1.1g; no system installation and no legacy 1.0 DLL reuse',
}
(runtime/'TLS-DEPENDENCIES.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
