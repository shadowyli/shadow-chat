"""Preserve known generated production output before rebuilding and linking tests."""
from pathlib import Path
import uuid
import subprocess

root = Path(__file__).resolve().parent.parent
legacy = [
    item for item in (root/'src').iterdir()
    if item.is_file() and (
        (item.name.startswith(('moc_', 'qrc_')) and item.suffix in {'.cpp', '.h', '.obj'})
        or (item.name.startswith('ui_') and item.suffix == '.h')
        or item.name == '.qmake.stash'
        or item.suffix in {'.obj', '.res', '.pch', '.idb', '.ilk'}
    )
]
if legacy:
    destination = (root/'dist'/('legacy-generated-before-'+uuid.uuid4().hex[:8])).resolve()
    if destination.parent != (root/'dist').resolve():
        raise SystemExit('Unexpected legacy archive destination')
    for item in legacy:
        if item.is_symlink() or item.resolve().parent != (root/'src').resolve():
            raise SystemExit('Unexpected legacy output path')
        tracked = subprocess.run(['git', 'ls-files', '--error-unmatch', '--', str(item.relative_to(root))], cwd=root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if tracked.returncode == 0:
            raise SystemExit('Generated-looking file is tracked; manual review required: '+item.name)
    destination.mkdir()
    for item in legacy:
        item.rename(destination/item.name)
    print('Preserved legacy root generated files:', destination, 'count:', len(legacy))
output = (root/'src/release').resolve()
if output != root/'src/release' or output.is_symlink():
    raise SystemExit('Unexpected production output path')
if output.exists():
    for item in output.rglob('*'):
        if item.is_symlink():
            raise SystemExit('Linked build output rejected')
        if item.is_file() and not (item.suffix.lower() in {'.obj', '.res', '.pch', '.pdb', '.idb', '.ilk', '.exp', '.lib'} or item.name.startswith(('moc_', 'qrc_', 'ui_'))):
            raise SystemExit('Unrecognized file in generated output: '+item.name)
    destination = (root/'dist'/('production-objects-before-'+uuid.uuid4().hex[:8])).resolve()
    if destination.parent != (root/'dist').resolve():
        raise SystemExit('Unexpected archive destination')
    output.rename(destination)
    print('Preserved previous generated output:', destination)
output.mkdir(exist_ok=True)
