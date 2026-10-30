"""Seed isolated local accounts for the actual installed application's normal login path."""
from pathlib import Path
import argparse
import hashlib
import json
import secrets

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--profile-root', type=Path, required=True)
parser.add_argument('--name', choices=('alpha', 'beta'), required=True)
parser.add_argument('--port', type=int, required=True)
args = parser.parse_args()
if not args.profile_root.is_absolute() or not 1024 <= args.port <= 65000:
    raise SystemExit('Use an absolute synthetic root and a nonprivileged port')
root = args.profile_root.resolve()
email = 'release-'+args.name+'@example.test'
profile = root/email
profile.mkdir(parents=True, exist_ok=False)
received = profile/'received'
received.mkdir()
salt = secrets.token_bytes(16)
password = secrets.token_urlsafe(24)
digest = hashlib.pbkdf2_hmac('sha256', password.encode(), salt, 100000, 32).hex()
(profile/'.shadow-account.ini').write_text('[General]\nemail='+email+'\nsalt='+salt.hex()+'\npassword='+digest+'\n', encoding='ascii')
(root/'session.ini').write_text('[General]\nautoLoginEmail='+email+'\nautoLoginEnabled=true\nstartupConfigured=true\n', encoding='ascii')
(profile/'beebeep.ini').write_text(
    '[Version]\nSettings=18\nQt=5.15\n'
    '[User]\nLocalName=Release '+args.name.title()+' Fixture\nLocalHash=release-'+args.name+'-isolated-112\nAskChangeUserAtStartup=false\nAskPasswordAtStartup=false\n'
    '[Gui]\nDownloadDirectory='+received.as_posix()+'\n'
    '[Misc]\nListenerPort='+str(args.port)+'\n'
    '[Network]\nLocalHostAddressForced=127.0.0.1\nBroadcastToLocalSubnet=false\nUseHiveProtocol=false\nUseMulticastDns=false\n'
    '[Tools]\nSearchForNewVersionAtStartup=false\n', encoding='utf-8')
(profile/'beehosts.ini').write_text('# Read only explicitly configured loopback fixture peers.\n', encoding='ascii')
print(json.dumps({'profileRoot': str(root), 'profile': str(profile), 'syntheticEmail': email, 'listenerPort': args.port, 'passwordPrinted': False, 'warning': 'Before launch, set hosts-only and disable multicast in the isolated installation RC; use only explicit 127.0.0.1 peers, never the user installation.'}, indent=2))
