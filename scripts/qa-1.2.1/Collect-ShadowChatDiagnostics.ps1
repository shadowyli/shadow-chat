[CmdletBinding()]
param(
    [ValidateSet('A', 'B', 'C')]
    [string]$Role,

    [string]$OutputRoot = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'ShadowChat-QA-1.2.1'),

    [ValidateRange(1, 168)]
    [int]$EventLogHours = 24
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

if ([string]::IsNullOrWhiteSpace($Role)) {
    $Role = (Read-Host 'Enter this PC role: A, B, or C').Trim().ToUpperInvariant()
    if ($Role -notin @('A', 'B', 'C')) {
        throw 'Role must be A, B, or C.'
    }
}

$productNameCn = ([string][char]0x5F71) + ([string][char]0x804A)
$productNamePattern = 'ShadowChat|' + [regex]::Escape($productNameCn)
$appDataRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::ApplicationData)
$localAppDataRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::LocalApplicationData)
$programFilesRoot = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)
$programFilesX86Root = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$computerName = [Environment]::MachineName
$currentUserName = [Environment]::UserName
$safeComputerName = $computerName -replace '[^A-Za-z0-9._-]', '_'
$reportName = "ShadowChat-1.2.1-$Role-$safeComputerName-$timestamp"
$reportDirectory = Join-Path $OutputRoot $reportName
$zipPath = Join-Path $OutputRoot ($reportName + '.zip')
$logDirectory = Join-Path $reportDirectory 'shadowchat-logs'

New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null

function Write-Section {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $target = Join-Path $reportDirectory ($Name + '.txt')
    try {
        & $Action 2>&1 | Out-File -LiteralPath $target -Encoding utf8 -Width 4096
    }
    catch {
        @(
            "Collection failed: $Name"
            $_.Exception.ToString()
        ) | Out-File -LiteralPath $target -Encoding utf8 -Width 4096
    }
}

function Convert-ToSafeFileName {
    param([Parameter(Mandatory = $true)][string]$Value)

    return ($Value -replace '[^A-Za-z0-9._-]', '_')
}

function Get-ShadowChatProcesses {
    return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -ieq 'ShadowChat.exe' -or
        $_.ExecutablePath -like '*\ShadowChat.exe'
    })
}

function Get-InstalledShadowChatEntries {
    $registryRoots = @(
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
    )

    foreach ($registryRoot in $registryRoots) {
        Get-ItemProperty -Path $registryRoot -ErrorAction SilentlyContinue | Where-Object {
            $displayNameProperty = $_.PSObject.Properties['DisplayName']
            $null -ne $displayNameProperty -and $displayNameProperty.Value -match $productNamePattern
        } | Select-Object PSPath, DisplayName, DisplayVersion, Publisher, InstallDate, InstallLocation,
            DisplayIcon, UninstallString, QuietUninstallString
    }
}

function Get-ShadowChatShortcutEntries {
    $shell = New-Object -ComObject WScript.Shell
    $roots = @(
        [Environment]::GetFolderPath([Environment+SpecialFolder]::Desktop),
        [Environment]::GetFolderPath([Environment+SpecialFolder]::Programs)
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) -and (Test-Path -LiteralPath $_ -PathType Container) }

    foreach ($root in $roots) {
        foreach ($shortcut in Get-ChildItem -LiteralPath $root -Filter '*.lnk' -File -Recurse -ErrorAction SilentlyContinue) {
            try {
                $link = $shell.CreateShortcut($shortcut.FullName)
                if ($shortcut.Name -notmatch $productNamePattern -and $link.TargetPath -notmatch 'ShadowChat\.exe$') {
                    continue
                }
                [pscustomobject]@{
                    Shortcut = $shortcut.FullName
                    Target = $link.TargetPath
                    Arguments = $link.Arguments
                    WorkingDirectory = $link.WorkingDirectory
                    IconLocation = $link.IconLocation
                    ShortcutSHA256 = (Get-FileHash -LiteralPath $shortcut.FullName -Algorithm SHA256).Hash
                    TargetExists = Test-Path -LiteralPath $link.TargetPath -PathType Leaf
                    TargetSHA256 = if (Test-Path -LiteralPath $link.TargetPath -PathType Leaf) {
                        (Get-FileHash -LiteralPath $link.TargetPath -Algorithm SHA256).Hash
                    } else { $null }
                }
            }
            catch {
                [pscustomobject]@{ Shortcut = $shortcut.FullName; Error = $_.Exception.Message }
            }
        }
    }
}

function Get-ShadowChatExecutables {
    $paths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)

    foreach ($process in Get-ShadowChatProcesses) {
        if (-not [string]::IsNullOrWhiteSpace($process.ExecutablePath)) {
            [void]$paths.Add($process.ExecutablePath)
        }
    }

    $knownPaths = [System.Collections.Generic.List[string]]::new()
    if (-not [string]::IsNullOrWhiteSpace($localAppDataRoot)) {
        $knownPaths.Add((Join-Path $localAppDataRoot 'Programs\ShadowChat\ShadowChat.exe'))
    }
    foreach ($programRoot in @($programFilesRoot, $programFilesX86Root)) {
        if (-not [string]::IsNullOrWhiteSpace($programRoot)) {
            $knownPaths.Add((Join-Path $programRoot 'ShadowChat\ShadowChat.exe'))
        }
    }

    foreach ($knownPath in $knownPaths) {
        if (-not [string]::IsNullOrWhiteSpace($knownPath) -and (Test-Path -LiteralPath $knownPath -PathType Leaf)) {
            [void]$paths.Add($knownPath)
        }
    }

    foreach ($entry in Get-InstalledShadowChatEntries) {
        if (-not [string]::IsNullOrWhiteSpace($entry.InstallLocation)) {
            $candidate = Join-Path $entry.InstallLocation 'ShadowChat.exe'
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                [void]$paths.Add($candidate)
            }
        }
    }

    foreach ($shortcut in Get-ShadowChatShortcutEntries) {
        if ($shortcut.TargetExists -and -not [string]::IsNullOrWhiteSpace($shortcut.Target)) {
            [void]$paths.Add($shortcut.Target)
        }
    }

    return @($paths)
}

function Get-ShadowChatLogCandidates {
    $paths = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $knownDirectories = [System.Collections.Generic.List[string]]::new()
    foreach ($rootPath in @($appDataRoot, $localAppDataRoot)) {
        if ([string]::IsNullOrWhiteSpace($rootPath)) {
            continue
        }
        $knownDirectories.Add((Join-Path $rootPath (Join-Path 'shadowyli' $productNameCn)))
        $knownDirectories.Add((Join-Path $rootPath 'shadowyli\ShadowChat'))
        $knownDirectories.Add((Join-Path $rootPath $productNameCn))
    }

    $settingsRegistryPaths = @(
        ('HKCU:\Software\shadowyli\' + $productNameCn),
        'HKCU:\Software\shadowyli\ShadowChat'
    )

    foreach ($settingsPath in $settingsRegistryPaths) {
        $settings = Get-ItemProperty -LiteralPath $settingsPath -ErrorAction SilentlyContinue
        $logFolderProperty = if ($null -ne $settings) { $settings.PSObject.Properties['LogFolderPath'] } else { $null }
        if ($null -ne $logFolderProperty -and -not [string]::IsNullOrWhiteSpace($logFolderProperty.Value)) {
            $knownDirectories.Add([string]$logFolderProperty.Value)
        }
    }

    foreach ($directory in ($knownDirectories | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
            continue
        }

        foreach ($logFile in Get-ChildItem -LiteralPath $directory -File -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -match ('^(' + [regex]::Escape($productNameCn) + '|shadowchat|beebeep).*\.log$') -or
                $_.Extension -ieq '.log'
        }) {
            [void]$paths.Add($logFile.FullName)
        }
    }

    return @($paths)
}

$metadata = [ordered]@{
    CollectorVersion = '1.1'
    Role = $Role
    ComputerName = $computerName
    UserName = $currentUserName
    CollectedAtLocal = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz')
    CollectedAtUtc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    OutputDirectory = $reportDirectory
    ReadOnlyScope = 'System/network/process/port/install/version/event-log queries plus ShadowChat log copies only'
}
$metadata | ConvertTo-Json -Depth 4 | Out-File -LiteralPath (Join-Path $reportDirectory 'metadata.json') -Encoding utf8

Write-Section -Name '01-windows-version' -Action {
    Get-CimInstance Win32_OperatingSystem | Select-Object Caption, Version, BuildNumber, OSArchitecture,
        InstallDate, LastBootUpTime, LocalDateTime
    ''
    Get-CimInstance Win32_ComputerSystem | Select-Object Manufacturer, Model, SystemType, Domain,
        PartOfDomain, TotalPhysicalMemory
}

Write-Section -Name '02-network-adapters' -Action {
    Get-NetAdapter -IncludeHidden -ErrorAction SilentlyContinue | Sort-Object Status, Name | Format-Table -AutoSize Name,
        InterfaceDescription, Status, LinkSpeed, MacAddress, InterfaceIndex, HardwareInterface, Virtual
    ''
    Get-NetIPConfiguration -Detailed -ErrorAction SilentlyContinue | Format-List InterfaceAlias, InterfaceIndex,
        InterfaceDescription, NetProfile, IPv4Address, IPv6Address, IPv4DefaultGateway, IPv6DefaultGateway, DNSServer
    ''
    ipconfig.exe /all
    ''
    route.exe print
    ''
    'Current neighbor cache:'
    Get-NetNeighbor -ErrorAction SilentlyContinue | Sort-Object InterfaceIndex, IPAddress |
        Format-Table -AutoSize InterfaceIndex, IPAddress, LinkLayerAddress, State, Store
    ''
    arp.exe -a
}

Write-Section -Name '03-network-category' -Action {
    Get-NetConnectionProfile -ErrorAction SilentlyContinue | Format-List Name, InterfaceAlias, InterfaceIndex,
        NetworkCategory, IPv4Connectivity, IPv6Connectivity
}

Write-Section -Name '04-firewall-profiles' -Action {
    Get-NetFirewallProfile -ErrorAction SilentlyContinue | Format-List Name, Enabled, DefaultInboundAction,
        DefaultOutboundAction, AllowInboundRules, AllowLocalFirewallRules, NotifyOnListen, LogFileName,
        LogAllowed, LogBlocked, LogMaxSizeKilobytes
    ''
    netsh.exe advfirewall show allprofiles
}

Write-Section -Name '05-firewall-shadowchat-rules' -Action {
    $matchingRules = Get-NetFirewallRule -PolicyStore ActiveStore -ErrorAction SilentlyContinue | Where-Object {
        $_.DisplayName -match $productNamePattern -or $_.Description -match $productNamePattern
    }

    foreach ($rule in $matchingRules) {
        $application = $rule | Get-NetFirewallApplicationFilter -ErrorAction SilentlyContinue
        $port = $rule | Get-NetFirewallPortFilter -ErrorAction SilentlyContinue
        [pscustomobject]@{
            DisplayName = $rule.DisplayName
            Enabled = $rule.Enabled
            Direction = $rule.Direction
            Action = $rule.Action
            Profile = $rule.Profile
            Program = $application.Program
            Protocol = $port.Protocol
            LocalPort = $port.LocalPort
            RemotePort = $port.RemotePort
        }
    }
}

$shadowChatProcesses = @(Get-ShadowChatProcesses)
$shadowChatProcessIds = @($shadowChatProcesses | Select-Object -ExpandProperty ProcessId)

Write-Section -Name '06-shadowchat-processes' -Action {
    if ($shadowChatProcesses.Count -eq 0) {
        'ShadowChat.exe is not running.'
    }
    else {
        $shadowChatProcesses | Select-Object Name, ProcessId, ParentProcessId, ExecutablePath, CommandLine,
            CreationDate | Format-List
    }
}

Write-Section -Name '07-shadowchat-ports' -Action {
    $productPorts = @(6475, 6476, 36475)
    'Expected default ports: TCP 6475, TCP 6476, UDP 36475.'
    ''
    'TCP endpoints owned by ShadowChat or using default product ports:'
    Get-NetTCPConnection -ErrorAction SilentlyContinue | Where-Object {
        $_.OwningProcess -in $shadowChatProcessIds -or
        $_.LocalPort -in $productPorts -or
        $_.RemotePort -in $productPorts
    } | Sort-Object LocalPort, RemoteAddress | Format-Table -AutoSize State, LocalAddress, LocalPort,
        RemoteAddress, RemotePort, OwningProcess, AppliedSetting
    ''
    'UDP endpoints owned by ShadowChat or using default product ports:'
    Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object {
        $_.OwningProcess -in $shadowChatProcessIds -or $_.LocalPort -in $productPorts
    } | Sort-Object LocalPort, LocalAddress | Format-Table -AutoSize LocalAddress, LocalPort, OwningProcess
    ''
    'Targeted netstat rows:'
    netstat.exe -ano | Select-String -Pattern '(:6475\s|:6476\s|:36475\s|LISTENING)'
}

Write-Section -Name '08-installed-version' -Action {
    'Uninstall registry entries:'
    Get-InstalledShadowChatEntries | Format-List
    ''
    'Desktop and Start menu shortcuts:'
    Get-ShadowChatShortcutEntries | Format-List
    ''
    'Executable version and SHA-256:'
    $executables = @(Get-ShadowChatExecutables)
    if ($executables.Count -eq 0) {
        'No ShadowChat.exe was found in the running process, default install locations, or uninstall entries.'
    }
    foreach ($executable in $executables) {
        $item = Get-Item -LiteralPath $executable -ErrorAction SilentlyContinue
        $hash = Get-FileHash -LiteralPath $executable -Algorithm SHA256 -ErrorAction SilentlyContinue
        [pscustomobject]@{
            Path = $executable
            Length = $item.Length
            LastWriteTime = $item.LastWriteTime
            FileVersion = $item.VersionInfo.FileVersion
            ProductVersion = $item.VersionInfo.ProductVersion
            ProductName = $item.VersionInfo.ProductName
            CompanyName = $item.VersionInfo.CompanyName
            SHA256 = $hash.Hash
        } | Format-List
    }
}

Write-Section -Name '09-relevant-event-log' -Action {
    $startTime = (Get-Date).AddHours(-1 * $EventLogHours)
    Get-WinEvent -FilterHashtable @{ LogName = 'Application'; StartTime = $startTime } -ErrorAction SilentlyContinue |
        Where-Object {
            $_.ProviderName -match 'Application Error|Windows Error Reporting' -and
            $_.Message -match $productNamePattern
        } |
        Select-Object TimeCreated, Id, LevelDisplayName, ProviderName, Message |
        Format-List
}

$logManifest = [System.Collections.Generic.List[object]]::new()
foreach ($logPath in Get-ShadowChatLogCandidates) {
    try {
        $logItem = Get-Item -LiteralPath $logPath -ErrorAction Stop
        $destinationName = '{0}-{1}' -f (Convert-ToSafeFileName -Value $logItem.Directory.Name),
            (Convert-ToSafeFileName -Value $logItem.Name)
        $destinationPath = Join-Path $logDirectory $destinationName
        Copy-Item -LiteralPath $logPath -Destination $destinationPath -Force -ErrorAction Stop
        $logManifest.Add([pscustomobject]@{
            SourcePath = $logPath
            CopiedAs = $destinationName
            Length = $logItem.Length
            LastWriteTime = $logItem.LastWriteTime
            SHA256 = (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash
        })
    }
    catch {
        $logManifest.Add([pscustomobject]@{
            SourcePath = $logPath
            CopiedAs = $null
            Length = $null
            LastWriteTime = $null
            SHA256 = $null
            Error = $_.Exception.Message
        })
    }
}
$logManifest | ConvertTo-Json -Depth 4 | Out-File -LiteralPath (Join-Path $reportDirectory '10-shadowchat-log-manifest.json') -Encoding utf8

Write-Section -Name '11-discovery-events' -Action {
    $eventPattern = 'ShadowChat identity|non-ShadowChat|Searching users|Broadcaster (ignored|sends|will contact|has found)|New peer found|network interface|Network entry found|Local host address|Broadcast to IPv4|Unable to (send|bind|find)|ConnectionSocket rejected'
    $copiedLogs = @(Get-ChildItem -LiteralPath $logDirectory -File -ErrorAction SilentlyContinue)
    if ($copiedLogs.Count -eq 0) {
        'No ShadowChat log files were available for discovery-event extraction.'
    }
    foreach ($copiedLog in $copiedLogs) {
        "===== $($copiedLog.Name) ====="
        Select-String -LiteralPath $copiedLog.FullName -Pattern $eventPattern -CaseSensitive:$false -ErrorAction SilentlyContinue |
            Select-Object LineNumber, Line | Format-Table -Wrap -AutoSize
    }
}

$summaryLines = @(
    'ShadowChat 1.2.1 real three-PC diagnostic collection'
    "Role: $Role"
    "Computer: $computerName"
    "Collected: $((Get-Date).ToString('yyyy-MM-dd HH:mm:ss zzz'))"
    "ShadowChat process count: $($shadowChatProcesses.Count)"
    "Copied ShadowChat log count: $(@($logManifest | Where-Object { -not [string]::IsNullOrWhiteSpace($_.CopiedAs) }).Count)"
    'Default product ports checked: TCP 6475, TCP 6476, UDP 36475'
    'Identity gate expected: ShadowChat/1 in discovery and HELLO handshakes'
    ''
    'This collector did not change firewall, registry, network, startup settings, or ShadowChat user data.'
    'It only created this report directory/ZIP and copied matching ShadowChat log files for diagnosis.'
)
$summaryLines | Out-File -LiteralPath (Join-Path $reportDirectory 'SUMMARY.txt') -Encoding utf8

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -LiteralPath $reportDirectory -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host ''
Write-Host 'Collection completed.' -ForegroundColor Green
Write-Host "Directory: $reportDirectory"
Write-Host "Archive: $zipPath"
Write-Host 'Submit the ZIP together with the corresponding test screenshots.'

