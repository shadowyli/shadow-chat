"""Prepare a limited static deployment bundle for an already verified dev/main build."""
from pathlib import Path, PurePosixPath
import argparse
import json
import shutil
from urllib.parse import urlparse
from release_config import channel, sha256

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--environment', choices=('dev', 'main'), required=True)
parser.add_argument('--receipt', type=Path, required=True)
parser.add_argument('--manifest', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
receipt = json.loads(args.receipt.read_text(encoding='utf-8'))
manifest = json.loads(args.manifest.read_text(encoding='utf-8'))
for field, expected in {'releaseEnvironment': args.environment, 'channel': channel(args.environment)}.items():
    if receipt.get(field) != expected or manifest.get(field) != expected:
        raise SystemExit('Build receipt and manifest environment/channel must match deployment')
for field in ['version', 'sha256', 'size']:
    if receipt.get(field) != manifest.get(field):
        raise SystemExit('Manifest differs from verified installer receipt: '+field)
if manifest.get('mandatory', False):
    raise SystemExit('Mandatory update manifests are not permitted')
installer = Path(receipt['installer'])
if installer.stat().st_size != receipt['size'] or sha256(installer) != receipt['sha256']:
    raise SystemExit('Installer bytes differ from receipt')
source = urlparse(receipt['updateSource'])
download = urlparse(manifest['installerUrl'])
expected_host = {'dev': 'dev.flowx.gz', 'main': 'flowx.gz'}[args.environment]
if source.scheme != 'https' or source.hostname != expected_host or source.port not in (None, 443) or source.username or source.password or source.query or source.fragment:
    raise SystemExit('Production deployment requires configured HTTPS update source')
if download.scheme not in ('', 'https') or download.username or download.password or download.query or download.fragment or (download.netloc and download.netloc != source.netloc):
    raise SystemExit('Installer must use configured distribution host')
source_path = PurePosixPath(source.path.lstrip('/'))
download_path = PurePosixPath(download.path.lstrip('/'))
for relative in [source_path, download_path]:
    if relative.is_absolute() or '..' in relative.parts or not relative.suffix:
        raise SystemExit('Unsafe deployment-relative path')
if args.environment not in download_path.parts:
    raise SystemExit('Installer path must explicitly identify deployment environment')
args.output.mkdir(parents=True, exist_ok=False)
site = args.output/'site'
for relative, source_file in [(download_path, installer), (source_path, args.manifest)]:
    destination = site/str(relative)
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source_file, destination)
inventory = {str(file.relative_to(site)).replace('\\', '/'): {'size': file.stat().st_size, 'sha256': sha256(file)} for file in site.rglob('*') if file.is_file()}
(args.output/'deployment-files.json').write_text(json.dumps(inventory, indent=2), encoding='utf-8')
shutil.copy2(args.receipt, args.output/'release-build.json')
script = """param([Parameter(Mandatory=$true)][string]$SiteRoot, [switch]$MainApproved)
$ErrorActionPreference='Stop'
if ('ENVIRONMENT' -eq 'main' -and !$MainApproved) { throw 'Main requires explicit approval after dev acceptance.' }
$root=[IO.Path]::GetFullPath($SiteRoot).TrimEnd('\\')+'\\'
$files=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'deployment-files.json') -Raw | ConvertFrom-Json
foreach($entry in $files.PSObject.Properties) {
  $relative=$entry.Name.Replace('/','\\')
  $source=Join-Path (Join-Path $PSScriptRoot 'site') $relative
  $target=[IO.Path]::GetFullPath((Join-Path $root $relative))
  if (!$target.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw 'Path escapes site root' }
  if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value.sha256) { throw 'Bundle digest mismatch' }
  $parent=Split-Path -Parent $target
  [IO.Directory]::CreateDirectory($parent) | Out-Null
  if ([IO.Path]::GetExtension($target) -eq '.exe' -and (Test-Path -LiteralPath $target)) {
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value.sha256) { throw 'An existing versioned installer has different bytes; replacement is forbidden.' }
    continue
  }
  $temporary=$target+'.shadowchat-'+[Guid]::NewGuid().ToString('N')+'.tmp'
  Copy-Item -LiteralPath $source -Destination $temporary
  if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value.sha256) { throw 'Temporary deployed digest mismatch' }
  if(Test-Path -LiteralPath $target) {
    if ([IO.Path]::GetExtension($target) -eq '.exe') { throw 'Installer appeared while deploying; retry after verifying its hash.' }
    $backup=$target+'.before-shadowchat-'+(Get-Date -Format 'yyyyMMddHHmmssfff')
    [IO.File]::Replace($temporary,$target,$backup)
  } else {
    [IO.File]::Move($temporary,$target)
  }
  if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -ne $entry.Value.sha256) { throw 'Deployed digest mismatch' }
}
Write-Output 'Files copied. Verify public HTTPS manifest, installer download and certificate before acceptance.'
"""
(args.output/'deploy-limited.ps1').write_text(script.replace('ENVIRONMENT', args.environment), encoding='utf-8-sig')
(args.output/'README.txt').write_text('Status: PREPARED, NOT DEPLOYED. Run deploy-limited.ps1 only through an authorized server channel. Only the listed installer and manifest are copied; existing targets are backed up and the rest of the site is preserved. Verify HTTPS content type, certificate, environment, channel, size, hash and complete download. Main requires dev acceptance and separate user confirmation. Rebuilt main/stable artifacts require their own regression and installation verification; a dev/test receipt cannot be relabeled as stable.\n', encoding='utf-8')
print(json.dumps({'status': 'prepared-not-deployed', 'releaseEnvironment': args.environment, 'output': str(args.output), 'files': inventory}, indent=2))
