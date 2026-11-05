from pathlib import Path
import argparse
import datetime
import hashlib
import json
import shutil
import subprocess
import zipfile

from release_config import ROOT, version
from source_snapshot import verify
from source_archive import select_source_files


def digest(path):
    hasher = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            hasher.update(block)
    return hasher.hexdigest()


parser = argparse.ArgumentParser(description='Archive a reviewed local working-tree candidate without publishing or committing.')
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--verification', type=Path, required=True)
parser.add_argument('--new-source-list', type=Path, required=True,
                    help='JSON array of explicitly reviewed new source paths; generated proof files are excluded')
arguments = parser.parse_args()
output = arguments.output.resolve()
if output.exists():
    raise SystemExit('Preserve existing candidate directory; select a fresh destination.')
release = ROOT/'dist/local'
receipt = json.loads((release/'release-build.json').read_text(encoding='utf-8'))
verification = json.loads(arguments.verification.read_text(encoding='utf-8'))
if verification.get('localRequiredChecksPassed') is not True:
    raise SystemExit('Required local validation has not passed.')
source_verification = verify(release/'build-source-inputs.json')
installer = Path(receipt['installer'])
if digest(installer) != receipt['sha256']:
    raise SystemExit('Installer differs from the validated receipt.')
approved_new = json.loads(arguments.new_source_list.read_text(encoding='utf-8'))
files, excluded_untracked = select_source_files(ROOT, approved_new)
output.mkdir(parents=True)
package_name = f'ShadowChat-{version()}-local-setup.exe'
shutil.copy2(installer, output/package_name)
source_name = f'ShadowChat-{version()}-source.zip'
source_digests = {}
with zipfile.ZipFile(output/source_name, 'w', zipfile.ZIP_DEFLATED) as archive:
    for name, path in files:
        source_digests[name] = digest(path)
        archive.write(path, 'shadowchat-'+version()+'/'+name)
with zipfile.ZipFile(output/source_name) as archive:
    if archive.testzip() is not None:
        raise SystemExit('Source ZIP CRC verification failed.')
for name in ['RELEASE-'+version()+'.md', 'QA-'+version()+'-REAL-3PC.md']:
    shutil.copy2(ROOT/'docs'/name, output/('release-notes.md' if name.startswith('RELEASE') else name))
for source in (ROOT/'scripts'/('qa-'+version())).glob('Collect-ShadowChatDiagnostics.*'):
    shutil.copy2(source, output/source.name)
shutil.copy2(arguments.verification, output/'verification.json')
patch = subprocess.check_output(['git', 'diff', '--binary', 'HEAD'], cwd=ROOT)
(output/'source-changes.patch').write_bytes(patch)
information = {
    'version': version(), 'channel': 'test', 'releaseEnvironment': 'local',
    'status': 'local-candidate-real-three-pc-acceptance-pending',
    'archivedAt': datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'baseCommit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    'sourceKind': 'reviewed-uncommitted-working-tree-snapshot',
    'sourceArchivePolicy': 'Tracked reviewed sources plus explicitly reviewed new sources only; unapproved local proof files are excluded.',
    'approvedNewSources': sorted(approved_new),
    'excludedUntrackedFileCount': excluded_untracked,
    'sourceFileSha256': source_digests,
    'sourceSnapshotVerification': source_verification,
    'patchSha256': digest(output/'source-changes.patch'),
    'installer': {'file': package_name, 'bytes': installer.stat().st_size, 'sha256': digest(installer)},
    'sourceArchive': {'file': source_name, 'sha256': digest(output/source_name)},
    'productionExeSha256': receipt['sourceExecutableSha256'],
    'signing': 'unsigned; SmartScreen reputation warning may remain',
    'realThreePcAcceptance': 'pending',
    'manualNativeScreenReview': verification.get('nativeScreenVisualAcceptance', 'pending'),
    'distributionGate': verification.get('distributionGate', 'hold-for-manual-review'),
    'devDeployment': 'pending-admin-session',
    'githubPublication': 'not-published',
}
(output/'build-info.json').write_text(json.dumps(information, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
checksums = ''.join(digest(path)+'  '+path.name+'\n' for path in sorted(output.iterdir()) if path.is_file())
(output/'SHA256SUMS.txt').write_text(checksums, encoding='utf-8')
print(output)
