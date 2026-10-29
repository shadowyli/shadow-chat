"""Verify actual payload DLLs with a trusted local HTTPS fixture and rejection control."""
from pathlib import Path
import datetime
import hashlib
import http.server
import ipaddress
import json
import os
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID

root = Path(__file__).resolve().parent.parent
payload = Path(sys.argv[1]) if len(sys.argv) > 1 else root/'dist/影聊-1.1.1-test-x86'
fixture = Path(tempfile.mkdtemp(prefix='tls-fixture-', dir=root/'dist'))
runtime = fixture/'runtime'
runtime.mkdir()
files = {}
for source in payload.glob('*.dll'):
    shutil.copy2(source, runtime/source.name)
    files[source.name] = hashlib.sha256(source.read_bytes()).hexdigest()
shutil.copy2(root/'dist/tls-probe/tls_probe.exe', runtime/'tls_probe.exe')
now = datetime.datetime.now(datetime.timezone.utc)
ca_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
ca_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'ShadowChat synthetic TLS fixture CA')])
ca = x509.CertificateBuilder().subject_name(ca_name).issuer_name(ca_name).public_key(ca_key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1)).not_valid_after(now+datetime.timedelta(days=1)).add_extension(x509.BasicConstraints(ca=True, path_length=0), critical=True).sign(ca_key, hashes.SHA256())
server_key = rsa.generate_private_key(public_exponent=65537, key_size=2048)
server_name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, 'localhost')])
certificate = x509.CertificateBuilder().subject_name(server_name).issuer_name(ca_name).public_key(server_key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-datetime.timedelta(days=1)).not_valid_after(now+datetime.timedelta(days=1)).add_extension(x509.SubjectAlternativeName([x509.DNSName('localhost'), x509.IPAddress(ipaddress.ip_address('127.0.0.1'))]), critical=False).add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False).sign(ca_key, hashes.SHA256())
(fixture/'ca.pem').write_bytes(ca.public_bytes(serialization.Encoding.PEM))
(fixture/'server.pem').write_bytes(certificate.public_bytes(serialization.Encoding.PEM))
(fixture/'synthetic-server-key.pem').write_bytes(server_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL, serialization.NoEncryption()))

class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        body = b'{"fixture":"trusted-local-https"}'
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *arguments):
        pass

server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Handler)
context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
context.minimum_version = ssl.TLSVersion.TLSv1_2
context.load_cert_chain(str(fixture/'server.pem'), str(fixture/'synthetic-server-key.pem'))
server.socket = context.wrap_socket(server.socket, server_side=True)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
environment = dict(os.environ)
environment['PATH'] = str(Path(os.environ['SystemRoot'])/'System32')+os.pathsep+os.environ['SystemRoot']
for name in ['QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'OPENSSL_CONF', 'OPENSSL_MODULES']:
    environment.pop(name, None)
url = 'https://localhost:'+str(server.server_port)+'/fixture'
try:
    trusted = subprocess.run([str(runtime/'tls_probe.exe'), url, str(fixture/'ca.pem')], cwd=runtime, env=environment, capture_output=True, text=True, timeout=30)
    untrusted = subprocess.run([str(runtime/'tls_probe.exe'), url], cwd=runtime, env=environment, capture_output=True, text=True, timeout=30)
finally:
    server.shutdown()
    server.server_close()
positive = json.loads(trusted.stdout.strip().splitlines()[-1])
negative = json.loads(untrusted.stdout.strip().splitlines()[-1])
passed = trusted.returncode == 0 and positive['supportsSsl'] and positive['httpsSucceeded'] and positive['httpStatus'] == 200 and not positive['certificateErrors'] and untrusted.returncode != 0 and not negative['httpsSucceeded'] and bool(negative['certificateErrors'])
result = {
    'passed': passed, 'payload': str(payload), 'fixtureDirectory': str(fixture),
    'trustedHttps': positive, 'untrustedCertificateRejected': negative,
    'developerPathRemoved': True, 'payloadDllSha256': files,
    'scope': 'Exact payload DLL copies; synthetic local CA trusted only by this probe, no machine trust changes and no ignored certificate errors; fixture private key is never packaged',
}
for name, process in [('trusted', trusted), ('untrusted', untrusted)]:
    (fixture/(name+'.log')).write_text(process.stdout+process.stderr, encoding='utf-8')
(fixture/'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
(root/'dist/tls-verification.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps(result, indent=2))
if not passed:
    raise SystemExit(1)
