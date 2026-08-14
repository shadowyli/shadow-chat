param(
  [string]$MakensisPath = ''
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$payload = Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'dist') -Directory |
  Where-Object { $_.Name -match '1\.0\.0-test-x86$' } |
  Select-Object -First 1 -ExpandProperty FullName
$builtExecutable = Join-Path $repositoryRoot 'test\ShadowChat.exe'

if (-not $MakensisPath) {
  $compilerCandidates = @(
    (Join-Path $repositoryRoot '.tools\NSIS\makensis.exe'),
    (Join-Path $env:ProgramFiles 'NSIS\makensis.exe'),
    (Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe')
  )
  $MakensisPath = $compilerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if (-not (Test-Path -LiteralPath $builtExecutable)) {
  throw "Production executable not found: $builtExecutable"
}
if (-not $payload -or -not (Test-Path -LiteralPath $payload)) {
  throw "Installer payload folder ending in 1.0.0-test-x86 was not found."
}
if (-not (Test-Path -LiteralPath $MakensisPath)) {
  throw "NSIS compiler not found: $MakensisPath"
}

Copy-Item -LiteralPath $builtExecutable -Destination (Join-Path $payload 'ShadowChat.exe') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'beebeep.rc') -Destination (Join-Path $payload 'beebeep.rc') -Force
$instructions = Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.txt' | Select-Object -First 1
Copy-Item -LiteralPath $instructions.FullName -Destination (Join-Path $payload $instructions.Name) -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE.txt') -Destination (Join-Path $payload 'LICENSE.txt') -Force
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md') -Destination (Join-Path $payload 'THIRD_PARTY_NOTICES.md') -Force

& $MakensisPath '/INPUTCHARSET' 'UTF8' (Join-Path $PSScriptRoot 'shadowchat-installer.nsi')
if ($LASTEXITCODE -ne 0) {
  throw "NSIS failed with exit code $LASTEXITCODE"
}

Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'dist') -Filter '*.exe' |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 1
