"""Capture only reviewed source inputs, never private delivery or build output."""
from pathlib import Path
import datetime
import hashlib
import json
import subprocess
from release_config import ROOT, version

SOURCE_PATHS = ['src', 'tests', 'locale', 'beebeep.pri', 'packaging', 'scripts', 'branding', 'LICENSE.txt', 'THIRD_PARTY_NOTICES.md']


def capture(destination):
    result = subprocess.check_output(['git', 'ls-files', '-z', '--cached', '--others', '--exclude-standard', '--', *SOURCE_PATHS], cwd=ROOT)
    files = {}
    deleted = []
    for name in sorted(set(result.decode('utf-8').split('\0'))-set([''])):
        if '__pycache__' in Path(name).parts or name.endswith('.pyc'):
            continue
        source = ROOT/name
        if not source.exists():
            deleted.append(name)
            continue
        if source.is_symlink():
            raise SystemExit('Linked source input is not supported: '+name)
        files[name] = hashlib.sha256(source.read_bytes()).hexdigest()
    snapshot = {'capturedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'version': version(), 'files': files, 'deleted': deleted}
    destination.write_text(json.dumps(snapshot, ensure_ascii=False, indent=2), encoding='utf-8')
    return hashlib.sha256(destination.read_bytes()).hexdigest()


def verify(snapshot):
    record = json.loads(Path(snapshot).read_text(encoding='utf-8'))
    changed = []
    changed.extend(name for name in record.get('deleted', []) if (ROOT/name).exists())
    for name, expected in record['files'].items():
        source = ROOT/name
        if not source.is_file() or hashlib.sha256(source.read_bytes()).hexdigest() != expected:
            changed.append(name)
    if changed:
        raise SystemExit('Source inputs changed after build capture: '+', '.join(changed))
    return {'unchanged': True, 'count': len(record['files']), 'snapshotSha256': hashlib.sha256(Path(snapshot).read_bytes()).hexdigest()}
