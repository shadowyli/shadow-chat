"""Repair only existing shortcuts whose exact application target and hashes were verified."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import uuid
from windows_shortcuts import read_shortcuts, refresh_shell, set_shortcut_icons, verify_shortcut_icons
from release_config import version

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--target', type=Path, required=True)
parser.add_argument('--expected-exe-sha256', required=True)
parser.add_argument('--version')
parser.add_argument('--icon', type=Path, default=root/'packaging/shadowchat.ico')
parser.add_argument('--expected-icon-sha256', default='416b5c634305f485606d9541ae0f8a44cffa5b2b1b6c833898b513e35e7ebc6f')
parser.add_argument('shortcuts', nargs='+', type=Path)
args = parser.parse_args()
if args.version is None:
    args.version = version()
if not args.version or any(character not in '0123456789.' for character in args.version):
    raise SystemExit('Invalid icon version')
target = args.target.resolve(strict=True)
if hashlib.sha256(target.read_bytes()).hexdigest() != args.expected_exe_sha256:
    raise SystemExit('Actual installed executable does not match verified release')
data = args.icon.read_bytes()
if hashlib.sha256(data).hexdigest() != args.expected_icon_sha256 or data[:4] != b'\0\0\1\0':
    raise SystemExit('Approved ICO digest or format mismatch')
before = read_shortcuts(args.shortcuts)
if any(Path(row['target']).resolve() != target for row in before):
    raise SystemExit('Existing shortcut points to another target; no changes made')
backup = root/'dist'/('shortcut-icon-repair-'+uuid.uuid4().hex[:8])
backup.mkdir(parents=True)
for number, shortcut in enumerate(args.shortcuts):
    shutil.copy2(shortcut, backup/(str(number)+'-'+shortcut.name))
destination = target.parent/('shadowchat-'+args.version+'.ico')
if destination.exists() and destination.read_bytes() != data:
    raise SystemExit('Versioned ICO already exists with different contents')
if not destination.exists():
    with destination.open('xb') as output:
        output.write(data)
try:
    set_shortcut_icons(args.shortcuts, target, destination)
    after = verify_shortcut_icons(args.shortcuts, [target]*len(args.shortcuts), destination, args.expected_icon_sha256)
    for original, updated in zip(before, after):
        for field in ['target', 'arguments', 'workingDirectory']:
            if original[field] != updated[field]:
                raise SystemExit('Shortcut field changed unexpectedly: '+field)
except BaseException:
    for number, shortcut in enumerate(args.shortcuts):
        shutil.copy2(backup/(str(number)+'-'+shortcut.name), shortcut)
    raise
finally:
    refresh_shell(args.shortcuts)
if hashlib.sha256(target.read_bytes()).hexdigest() != args.expected_exe_sha256:
    raise SystemExit('Installed executable changed during icon repair')
report = {'before': before, 'after': after, 'icon': str(destination), 'iconSha256': args.expected_icon_sha256, 'executableUnchanged': True, 'shellNotified': True, 'backupDirectory': str(backup), 'scope': 'IconLocation only; no executable launch, process termination or installer invocation'}
(backup/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print(json.dumps(report, ensure_ascii=False, indent=2))
