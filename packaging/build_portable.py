"""Build a verified portable ZIP from the exact installer payload."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

from release_config import ENVIRONMENTS, artifact_name, output_directory


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--environment', choices=ENVIRONMENTS, default='local')
args = parser.parse_args()

output = output_directory(args.environment)
receipt_path = output/'release-build.json'
if not receipt_path.is_file():
    raise SystemExit('Build and verify the installer payload first')

receipt = json.loads(receipt_path.read_text(encoding='utf-8'))
payload = output/artifact_name(args.environment)
if not payload.is_dir():
    raise SystemExit('Installer payload directory is missing')


def digest(filename):
    hasher = hashlib.sha256()
    with filename.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            hasher.update(block)
    return hasher.hexdigest()


expected_inventory = receipt.get('payloadInventory')
if not isinstance(expected_inventory, dict) or not expected_inventory:
    raise SystemExit('Installer build receipt has no payload inventory')

actual_inventory = {}
for source in sorted(payload.rglob('*')):
    if source.is_symlink():
        raise SystemExit('Linked portable payload entry rejected')
    if source.is_file():
        relative = source.relative_to(payload).as_posix()
        actual_inventory[relative] = {'size': source.stat().st_size, 'sha256': digest(source)}

if actual_inventory != expected_inventory:
    raise SystemExit('Portable payload differs from the verified installer payload')

portable_name = f'ShadowChat-{receipt["version"]}-{args.environment}-portable.zip'
portable = output/portable_name
if portable.exists():
    raise SystemExit('Portable archive already exists; preserve prior artifacts before rebuilding')

top_level = f'ShadowChat-{receipt["version"]}-portable'
with zipfile.ZipFile(portable, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for relative in sorted(actual_inventory):
        source = payload/Path(relative)
        info = zipfile.ZipInfo(f'{top_level}/{relative}', date_time=(2020, 1, 1, 0, 0, 0))
        info.compress_type = zipfile.ZIP_DEFLATED
        info.external_attr = 0o100644 << 16
        with source.open('rb') as stream:
            archive.writestr(info, stream.read(), compresslevel=9)

result = {
    'version': receipt['version'],
    'channel': receipt['channel'],
    'releaseEnvironment': receipt['releaseEnvironment'],
    'portable': str(portable),
    'filename': portable.name,
    'size': portable.stat().st_size,
    'sha256': digest(portable),
    'sourceInstallerSha256': receipt['sha256'],
    'sourceExecutableSha256': receipt['sourceExecutableSha256'],
    'payloadInventory': actual_inventory,
    'policy': 'Portable ZIP contains the exact verified installer payload under one top-level directory; it does not install shortcuts, registry entries or auto-start settings.',
}
(output/'portable-build.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps({key: value for key, value in result.items() if key != 'payloadInventory'}, ensure_ascii=False, indent=2))
