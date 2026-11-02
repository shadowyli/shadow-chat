"""Prepare one private owner recovery key and a distributable salted hash seed.
Never replaces an existing owner key. The plaintext is never put in the installer.
"""
from pathlib import Path
import hashlib,secrets
folder=Path(__file__).resolve().parent.parent/'delivery-private'
folder.mkdir(exist_ok=True)
seed=folder/'shadowchat-recovery.ini'
key=folder/'负责人恢复密码.txt'
if seed.exists() or key.exists():
 print('Existing recovery configuration preserved.')
else:
 password=secrets.token_urlsafe(18)
 salt=secrets.token_bytes(16)
 digest=hashlib.pbkdf2_hmac('sha256',password.encode(),salt,100000,32).hex()
 seed.write_text(f'[General]\nsalt={salt.hex()}\npassword={digest}\n',encoding='ascii')
 key.write_text('影聊安全恢复密码\n\n'+password+'\n\n仅由负责人保管。安装包仅包含加盐哈希；切勿将本文件上传网站或公开仓库。\n',encoding='utf-8')
 print('Private owner key and salted installer seed prepared; plaintext not printed.')
