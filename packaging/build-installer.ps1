param(
  [string]$MakensisPath = '',
  [ValidateSet('local','dev','main')][string]$Environment = 'local'
)

$ErrorActionPreference = 'Stop'
$builder = Join-Path $PSScriptRoot 'build_installer.py'
if ($MakensisPath) {
  & python -B $builder --environment $Environment --makensis $MakensisPath
} else {
  & python -B $builder --environment $Environment
}
if ($LASTEXITCODE -ne 0) {
  throw "Python installer build failed with exit code $LASTEXITCODE"
}
