param(
    [ValidateSet('A','B','C')][string]$Role,
    [string]$OutputRoot = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'ShadowChat-QA-1.2.2'),
    [string]$InstallerPath,
    [string]$LogRoot
)
$ErrorActionPreference = 'Stop'
if (-not $Role) { $Role = (Read-Host 'PC role: A, B or C').Trim().ToUpperInvariant() }
if ($Role -notin @('A','B','C')) { throw 'Role must be A, B or C.' }
$name = 'ShadowChat-1.2.2-{0}-{1}-{2}' -f $Role,$env:COMPUTERNAME,(Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$folder = Join-Path $OutputRoot $name
New-Item -ItemType Directory -Path $folder -Force | Out-Null
function Save-Section([string]$Name,[scriptblock]$Query) {
    try { & $Query | Out-String -Width 240 | Out-File -LiteralPath (Join-Path $folder ($Name+'.txt')) -Encoding utf8 }
    catch { $_.Exception.Message | Out-File -LiteralPath (Join-Path $folder ($Name+'.txt')) -Encoding utf8 }
}
Save-Section 'system' {
    Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber,OSArchitecture
    Get-Date -Format o
}
Save-Section 'network' {
    Get-NetAdapter | Select-Object Name,InterfaceDescription,Status,InterfaceIndex
    Get-NetIPConfiguration | Format-List InterfaceAlias,InterfaceIndex,IPv4Address,IPv4DefaultGateway
    Get-NetConnectionProfile | Select-Object Name,InterfaceAlias,NetworkCategory
    route.exe print
}
Save-Section 'firewall' {
    Get-NetFirewallProfile | Select-Object Name,Enabled,DefaultInboundAction,DefaultOutboundAction
    Get-NetFirewallRule -PolicyStore ActiveStore | Where-Object {
        $_.DisplayName -match 'ShadowChat|BeeBEEP|\u5f71\u804a'
    } | Select-Object DisplayName,Enabled,Direction,Action,Profile
}
$processes = @(Get-CimInstance Win32_Process -Filter "Name='ShadowChat.exe'")
Save-Section 'client-identity' {
    foreach ($client in $processes) {
        $path = $client.ExecutablePath
        [pscustomobject]@{
            ProcessId=$client.ProcessId
            Path=$path
            Version=if($path) { [System.Diagnostics.FileVersionInfo]::GetVersionInfo($path).ProductVersion } else { 'Access denied' }
            SHA256=if($path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash } else { 'Access denied' }
            Signature=if($path) { (Get-AuthenticodeSignature -LiteralPath $path).Status.ToString() } else { 'Unknown' }
        }
    }
}
Save-Section 'ports' {
    $ids = @($processes | ForEach-Object { $_.ProcessId })
    Get-NetTCPConnection | Where-Object { $_.OwningProcess -in $ids } |
        Select-Object State,LocalAddress,LocalPort,RemoteAddress,RemotePort,OwningProcess
    Get-NetUDPEndpoint | Where-Object { $_.OwningProcess -in $ids } |
        Select-Object LocalAddress,LocalPort,OwningProcess
}
Save-Section 'installer' {
    if ($InstallerPath) {
        Get-Item -LiteralPath $InstallerPath | Select-Object Name,Length,LastWriteTime
        Get-FileHash -LiteralPath $InstallerPath -Algorithm SHA256
        Get-AuthenticodeSignature -LiteralPath $InstallerPath | Select-Object Status,StatusMessage
    } else { 'Installer not specified. Pass -InstallerPath to collect its hash and signature.' }
}
Save-Section 'connection-events' {
    $roots = @($env:APPDATA,$env:LOCALAPPDATA,[Environment]::GetFolderPath('MyDocuments'))
    foreach ($client in $processes) {
        if ($client.ExecutablePath) { $roots += Split-Path -Parent $client.ExecutablePath }
    }
    $candidates = @()
    foreach ($root in ($roots | Select-Object -Unique)) {
        foreach ($relative in @('shadowchat.log','ShadowChat/shadowchat.log','shadowchat/shadowchat.log','BeeBEEP/beebeep.log')) {
            $candidate = Join-Path $root $relative
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { $candidates += $candidate }
        }
        Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'ShadowChat|\u5f71\u804a|BeeBEEP' } |
            ForEach-Object {
                Get-ChildItem -LiteralPath $_.FullName -Filter *.log -Recurse -ErrorAction SilentlyContinue |
                    ForEach-Object { $candidates += $_.FullName }
            }
    }
    if ($LogRoot -and (Test-Path -LiteralPath $LogRoot -PathType Container)) {
        Get-ChildItem -LiteralPath $LogRoot -Filter *.log -Recurse -ErrorAction SilentlyContinue |
            ForEach-Object { $candidates += $_.FullName }
    }
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        Get-Content -LiteralPath $candidate -Tail 5000 -ErrorAction SilentlyContinue |
            ForEach-Object {
                $match = [regex]::Match($_,'ShadowChatNetwork:\s+(?:discovery-invalid-size|discovery-identity-rejected|discovery-accepted|connection-requested|connection-policy-rejected-same-host|handshake-identity-rejected|handshake-connected|connection-error|send-identity-rejected|send-no-connection|send-failed|delivery-ack|group-config-deferred-capacity|group-config-deferred|group-config-invalid)\s+QHostAddress\("[0-9A-Fa-f:.%]+"\)(?:\s+\d+)+\s*$')
                if ($match.Success) {
                    $stamp = [regex]::Match($_,'\d{2}:\d{2}:\d{2}(?:[.,]\d+)?').Value
                    Write-Output ($stamp+' '+$match.Value.Trim())
                }
            }
    }
}
@(
    "Role: $Role"
    'Read-only: no firewall/network/registry/startup/user-data changes.'
    'No chat database, attachments, account configuration, passwords or raw logs are included.'
    'Only explicitly tagged ShadowChatNetwork connection events are exported.'
    'This report contains local IP addresses and computer identifiers. Share privately.'
    'A sees B/C does not prove B and C can connect. Complete all six directions separately.'
) | Out-File -LiteralPath (Join-Path $folder 'SUMMARY.txt') -Encoding utf8
Compress-Archive -LiteralPath $folder -DestinationPath ($folder+'.zip')
Write-Host ($folder+'.zip')
