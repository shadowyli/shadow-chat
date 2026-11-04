[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$Installer,

  [string]$ExpectedInstallerSha256 = '',

  [string]$ObservedExecutable = '',

  [string]$OutputDirectory = '',

  [ValidateRange(3, 60)]
  [int]$ObserveSeconds = 12,

  [switch]$KeepSandbox
)

Set-StrictMode -Version 2
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class ShadowChatWindowProbe
{
    public sealed class WindowRecord
    {
        public long Handle { get; set; }
        public string Title { get; set; }
        public bool Visible { get; set; }
    }

    private delegate bool EnumWindowsProc(IntPtr window, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr parameter);

    [DllImport("user32.dll")]
    private static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);

    [DllImport("user32.dll")]
    private static extern bool IsWindowVisible(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowTextLength(IntPtr window);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetWindowText(IntPtr window, StringBuilder text, int count);

    public static WindowRecord[] ForProcess(int processId)
    {
        var records = new List<WindowRecord>();
        EnumWindows(delegate(IntPtr window, IntPtr parameter)
        {
            uint owner;
            GetWindowThreadProcessId(window, out owner);
            if (owner != (uint)processId)
                return true;

            int length = GetWindowTextLength(window);
            var title = new StringBuilder(length + 1);
            GetWindowText(window, title, title.Capacity);
            records.Add(new WindowRecord
            {
                Handle = window.ToInt64(),
                Title = title.ToString(),
                Visible = IsWindowVisible(window)
            });
            return true;
        }, IntPtr.Zero);
        return records.ToArray();
    }
}
'@

function Get-Sha256([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
  return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Protect-Text([string]$Value) {
  if ([string]::IsNullOrEmpty($Value)) { return $Value }
  $protected = $Value
  if (-not [string]::IsNullOrEmpty($env:USERPROFILE)) {
    $protected = $protected.Replace($env:USERPROFILE, '%USERPROFILE%')
  }
  if (-not [string]::IsNullOrEmpty($env:USERNAME)) {
    $protected = $protected.Replace($env:USERNAME, '[USER]')
  }
  if (-not [string]::IsNullOrEmpty($env:COMPUTERNAME)) {
    $protected = $protected.Replace($env:COMPUTERNAME, '[COMPUTER]')
  }
  return $protected
}

function Get-FileRecord([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return [ordered]@{ path = Protect-Text $Path; exists = $false }
  }
  $item = Get-Item -LiteralPath $Path
  $signature = Get-AuthenticodeSignature -FilePath $Path
  $zone = $null
  try { $zone = Get-Content -LiteralPath $Path -Stream Zone.Identifier -Raw -ErrorAction Stop } catch {}
  return [ordered]@{
    path = Protect-Text $item.FullName
    exists = $true
    bytes = [int64]$item.Length
    sha256 = Get-Sha256 $item.FullName
    fileVersion = $item.VersionInfo.FileVersion
    productVersion = $item.VersionInfo.ProductVersion
    signatureStatus = [string]$signature.Status
    signer = if ($signature.SignerCertificate) { $signature.SignerCertificate.Subject } else { $null }
    zoneIdentifier = Protect-Text $zone
  }
}

function Get-RegistryValue([Microsoft.Win32.RegistryView]$View, [string]$SubKey, [string]$Name) {
  $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser, $View)
  try {
    $key = $base.OpenSubKey($SubKey, $false)
    if (-not $key) { return $null }
    try { return $key.GetValue($Name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) }
    finally { $key.Dispose() }
  }
  finally { $base.Dispose() }
}

function Get-ShortcutRecord([string]$Path, $Shell) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
    return [ordered]@{ path = Protect-Text $Path; exists = $false }
  }
  $link = $Shell.CreateShortcut($Path)
  return [ordered]@{
    path = Protect-Text $Path
    exists = $true
    sha256 = Get-Sha256 $Path
    target = Protect-Text $link.TargetPath
    arguments = Protect-Text $link.Arguments
    workingDirectory = Protect-Text $link.WorkingDirectory
    icon = Protect-Text $link.IconLocation
  }
}

function Get-HostState {
  $shadowName = (-join @([char]0x5f71, [char]0x804a))
  $shortcutName = $shadowName + '.lnk'
  $shell = New-Object -ComObject WScript.Shell
  $desktop = [Environment]::GetFolderPath('Desktop')
  $programs = [Environment]::GetFolderPath('Programs')
  $shortcutPaths = @(
    (Join-Path $desktop $shortcutName),
    (Join-Path (Join-Path $programs $shadowName) $shortcutName)
  )
  return [ordered]@{
    run32 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry32) 'Software\Microsoft\Windows\CurrentVersion\Run' 'ShadowChat'))
    run64 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry64) 'Software\Microsoft\Windows\CurrentVersion\Run' 'ShadowChat'))
    install32 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry32) 'Software\shadowyli\ShadowChat' 'InstallLocation'))
    install64 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry64) 'Software\shadowyli\ShadowChat' 'InstallLocation'))
    uninstall32 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry32) 'Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat' 'UninstallString'))
    uninstall64 = Protect-Text ([string](Get-RegistryValue ([Microsoft.Win32.RegistryView]::Registry64) 'Software\Microsoft\Windows\CurrentVersion\Uninstall\ShadowChat' 'UninstallString'))
    shortcuts = @($shortcutPaths | ForEach-Object { Get-ShortcutRecord $_ $shell })
  }
}

function Get-OsRecord {
  $os = Get-CimInstance -ClassName Win32_OperatingSystem
  return [ordered]@{
    caption = $os.Caption
    version = $os.Version
    buildNumber = $os.BuildNumber
    architecture = $os.OSArchitecture
    processArchitecture = $env:PROCESSOR_ARCHITECTURE
    powershell = $PSVersionTable.PSVersion.ToString()
  }
}

function Invoke-Captured([string]$File, [string]$Arguments, [int]$TimeoutMilliseconds) {
  $start = New-Object System.Diagnostics.ProcessStartInfo
  $start.FileName = $File
  $start.Arguments = $Arguments
  $start.WorkingDirectory = Split-Path -Parent $File
  $start.UseShellExecute = $false
  $start.CreateNoWindow = $true
  $start.RedirectStandardOutput = $true
  $start.RedirectStandardError = $true
  $process = New-Object System.Diagnostics.Process
  $process.StartInfo = $start
  if (-not $process.Start()) { throw "Unable to start $File" }
  if (-not $process.WaitForExit($TimeoutMilliseconds)) {
    $process.Kill()
    $process.WaitForExit()
    return [ordered]@{ timedOut = $true; exitCode = $null; stdout = ''; stderr = '' }
  }
  return [ordered]@{
    timedOut = $false
    exitCode = $process.ExitCode
    stdout = Protect-Text $process.StandardOutput.ReadToEnd()
    stderr = Protect-Text $process.StandardError.ReadToEnd()
  }
}

function Get-ProcessModules($Process) {
  try {
    $Process.Refresh()
    return @($Process.Modules | ForEach-Object { Protect-Text $_.FileName } | Sort-Object -Unique)
  }
  catch { return @() }
}

function Get-RelevantEvents([datetime]$StartTime, [datetime]$EndTime, [string]$InstalledExe) {
  $logs = @(
    'Application',
    'Microsoft-Windows-Windows Defender/Operational',
    'Microsoft-Windows-AppLocker/EXE and DLL',
    'Microsoft-Windows-CodeIntegrity/Operational'
  )
  $records = @()
  foreach ($log in $logs) {
    try {
      $events = Get-WinEvent -FilterHashtable @{ LogName = $log; StartTime = $StartTime; EndTime = $EndTime } -ErrorAction Stop
      foreach ($event in $events) {
        $message = [string]$event.Message
        if ($message -notmatch 'ShadowChat\.exe' -and $message -notlike ('*' + $InstalledExe + '*')) { continue }
        $records += [ordered]@{
          log = $log
          time = $event.TimeCreated.ToUniversalTime().ToString('o')
          id = $event.Id
          provider = $event.ProviderName
          level = $event.LevelDisplayName
          message = Protect-Text $message
        }
      }
    }
    catch {
      if ($_.FullyQualifiedErrorId -like 'NoMatchingEventsFound*') { continue }
      $records += [ordered]@{ log = $log; unavailable = Protect-Text $_.Exception.Message }
    }
  }
  return @($records)
}

function Write-Report($Value, [string]$Path) {
  $json = $Value | ConvertTo-Json -Depth 12
  [System.IO.File]::WriteAllText($Path, $json, (New-Object System.Text.UTF8Encoding($false)))
}

$installerPath = (Resolve-Path -LiteralPath $Installer).Path
$namespace = [Guid]::NewGuid().ToString('N')
$sandboxRoot = Join-Path $env:TEMP ('ShadowChat-QA-' + $namespace)
$installDirectory = Join-Path $sandboxRoot 'program'
$profileRoot = Join-Path $sandboxRoot 'diagnostic-profile'
$defaultOutput = Join-Path (Split-Path -Parent $PSScriptRoot) ('dist\startup-diagnostics\' + (Get-Date -Format 'yyyyMMddTHHmmss') + '-' + $namespace.Substring(0, 8))
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) { $OutputDirectory = $defaultOutput }
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$reportPath = Join-Path $OutputDirectory 'startup-diagnostic.json'
$runtimeNames = @(
  'ShadowChat.exe', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'Qt5Network.dll',
  'platforms\qwindows.dll', 'vcruntime140.dll', 'msvcp140.dll',
  'libssl-1_1.dll', 'libcrypto-1_1.dll'
)
$report = [ordered]@{
  schema = 1
  startedAt = (Get-Date).ToUniversalTime().ToString('o')
  outputDirectory = Protect-Text $OutputDirectory
  qaNamespace = $namespace
  sandboxRoot = Protect-Text $sandboxRoot
  os = Get-OsRecord
  installer = Get-FileRecord $installerPath
  observedExecutable = if ([string]::IsNullOrWhiteSpace($ObservedExecutable)) { $null } else { Get-FileRecord ([System.IO.Path]::GetFullPath($ObservedExecutable)) }
  observedExecutableMatchesQaPayload = $null
  expectedInstallerSha256 = $ExpectedInstallerSha256.ToLowerInvariant()
  hostBefore = Get-HostState
  install = $null
  installedFiles = @()
  buildInfo = $null
  startup = $null
  securityEvents = @()
  cleanup = $null
  hostAfter = $null
  hostUnchanged = $false
  outcome = 'not-run'
  failure = $null
}

$launchProcess = $null
$launchStartedAt = $null
try {
  if ($ExpectedInstallerSha256 -and $report.installer.sha256 -ne $ExpectedInstallerSha256.ToLowerInvariant()) {
    throw 'Installer SHA-256 does not match the expected value.'
  }

  $installProcess = Start-Process -FilePath $installerPath -ArgumentList @('/S', ('/SHADOWCHAT-QA=' + $namespace)) -PassThru -Wait
  $report.install = [ordered]@{ exitCode = $installProcess.ExitCode; directory = Protect-Text $installDirectory }
  if ($installProcess.ExitCode -ne 0) { throw ('QA installer exited with code ' + $installProcess.ExitCode) }

  $marker = Join-Path $installDirectory 'shadowchat-qa.ini'
  $installedExe = Join-Path $installDirectory 'ShadowChat.exe'
  if (-not (Test-Path -LiteralPath $marker -PathType Leaf)) { throw 'Installer did not preserve the QA isolation marker.' }
  if (-not (Test-Path -LiteralPath $installedExe -PathType Leaf)) { throw 'Installer completed without ShadowChat.exe.' }
  $report.install.marker = Protect-Text (Get-Content -LiteralPath $marker -Raw)
  $report.installedFiles = @($runtimeNames | ForEach-Object { Get-FileRecord (Join-Path $installDirectory $_) })
  if ($report.observedExecutable -and $report.observedExecutable.exists) {
    $report.observedExecutableMatchesQaPayload = $report.observedExecutable.sha256 -eq (Get-Sha256 $installedExe)
  }

  $report.buildInfo = Invoke-Captured $installedExe '--build-info' 15000
  if ($report.buildInfo.timedOut -or $report.buildInfo.exitCode -ne 0) { throw 'Installed executable did not complete --build-info.' }

  New-Item -ItemType Directory -Path $profileRoot -Force | Out-Null
  $launchStartedAt = Get-Date
  $startInfo = New-Object System.Diagnostics.ProcessStartInfo
  $startInfo.FileName = $installedExe
  $startInfo.Arguments = '--profile-root "' + $profileRoot + '"'
  $startInfo.WorkingDirectory = $installDirectory
  $startInfo.UseShellExecute = $false
  $startInfo.CreateNoWindow = $false
  $launchProcess = New-Object System.Diagnostics.Process
  $launchProcess.StartInfo = $startInfo
  if (-not $launchProcess.Start()) { throw 'Unable to start installed ShadowChat.exe.' }

  $deadline = [DateTime]::UtcNow.AddSeconds($ObserveSeconds)
  $windows = @()
  while ([DateTime]::UtcNow -lt $deadline -and -not $launchProcess.HasExited) {
    Start-Sleep -Milliseconds 250
    $windows = @([ShadowChatWindowProbe]::ForProcess($launchProcess.Id))
    if (@($windows | Where-Object { $_.Visible }).Count -gt 0) { break }
  }
  $launchProcess.Refresh()
  $running = -not $launchProcess.HasExited
  $exitCode = if ($running) { $null } else { $launchProcess.ExitCode }
  $windows = @([ShadowChatWindowProbe]::ForProcess($launchProcess.Id))
  $visibleWindows = @($windows | Where-Object { $_.Visible })
  $report.startup = [ordered]@{
    command = @((Protect-Text $installedExe), '--profile-root', (Protect-Text $profileRoot))
    processId = $launchProcess.Id
    observeSeconds = $ObserveSeconds
    running = $running
    exitCode = $exitCode
    windows = @($windows | ForEach-Object { [ordered]@{ handle = $_.Handle; title = $_.Title; visible = $_.Visible } })
    modules = if ($running) { Get-ProcessModules $launchProcess } else { @() }
  }
  $report.securityEvents = Get-RelevantEvents $launchStartedAt (Get-Date) $installedExe

  $missingRuntime = @($report.installedFiles | Where-Object { -not $_.exists })
  if ($missingRuntime.Count -gt 0) { $report.outcome = 'payload-incomplete' }
  elseif ($visibleWindows.Count -gt 0) { $report.outcome = 'visible-window' }
  elseif ($running) { $report.outcome = 'running-without-window' }
  elseif ($report.securityEvents.Count -gt 0) { $report.outcome = 'security-or-crash-event' }
  else { $report.outcome = 'early-exit-without-event' }
}
catch {
  $report.failure = Protect-Text $_.Exception.ToString()
  if ($report.outcome -eq 'not-run') { $report.outcome = 'diagnostic-failed' }
}
finally {
  if ($launchProcess) {
    try {
      if (-not $launchProcess.HasExited) {
        $launchProcess.Kill()
        $launchProcess.WaitForExit(10000) | Out-Null
      }
    }
    catch {}
  }

  $cleanup = [ordered]@{ attempted = $false; exitCode = $null; sandboxKept = [bool]$KeepSandbox; sandboxRemoved = $false }
  if (-not $KeepSandbox) {
    $marker = Join-Path $installDirectory 'shadowchat-qa.ini'
    $uninstallName = (-join @([char]0x5378, [char]0x8f7d, [char]0x5f71, [char]0x804a)) + '.exe'
    $uninstaller = Join-Path $installDirectory $uninstallName
    $safePrefix = (Join-Path $env:TEMP ('ShadowChat-QA-' + $namespace))
    if ($installDirectory.StartsWith($safePrefix, [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $marker -PathType Leaf) -and
        (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
      try {
        $cleanup.attempted = $true
        $uninstallProcess = Start-Process -FilePath $uninstaller -ArgumentList @('/S', ('_?=' + $installDirectory)) -PassThru -Wait
        $cleanup.exitCode = $uninstallProcess.ExitCode
        $resolvedSandbox = [System.IO.Path]::GetFullPath($sandboxRoot).TrimEnd('\')
        $resolvedPrefix = [System.IO.Path]::GetFullPath((Join-Path $env:TEMP 'ShadowChat-QA-'))
        if ($uninstallProcess.ExitCode -eq 0 -and
            $resolvedSandbox.StartsWith($resolvedPrefix, [StringComparison]::OrdinalIgnoreCase) -and
            (Split-Path -Leaf $resolvedSandbox) -eq ('ShadowChat-QA-' + $namespace)) {
          Remove-Item -LiteralPath $resolvedSandbox -Recurse -Force
          $cleanup.sandboxRemoved = -not (Test-Path -LiteralPath $resolvedSandbox)
        }
      }
      catch { $cleanup.error = Protect-Text $_.Exception.Message }
    }
  }
  $report.cleanup = $cleanup
  $report.hostAfter = Get-HostState
  $beforeJson = $report.hostBefore | ConvertTo-Json -Depth 8 -Compress
  $afterJson = $report.hostAfter | ConvertTo-Json -Depth 8 -Compress
  $report.hostUnchanged = $beforeJson -eq $afterJson
  $report.completedAt = (Get-Date).ToUniversalTime().ToString('o')
  Write-Report $report $reportPath
}

Write-Output $reportPath
if ($report.failure) { exit 2 }
if (-not $report.hostUnchanged) { exit 3 }
if ($report.outcome -ne 'visible-window') { exit 4 }
exit 0
