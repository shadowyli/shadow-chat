param(
  [string]$MakensisPath = ''
)

$ErrorActionPreference = 'Stop'
$builder = Join-Path $PSScriptRoot 'build_installer.py'
if ($MakensisPath) {
  & python $builder --makensis $MakensisPath
} else {
  & python $builder
}
if ($LASTEXITCODE -ne 0) {
  throw "Python installer build failed with exit code $LASTEXITCODE"
}
