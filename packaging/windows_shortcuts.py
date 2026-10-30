"""Read and update shortcut icons without launching their targets."""
from pathlib import Path
import base64
import ctypes
import hashlib
import json
import os
import subprocess


def powershell(script, values):
    executable = Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
    script = "[Console]::InputEncoding=[Text.UTF8Encoding]::new(); [Console]::OutputEncoding=[Text.UTF8Encoding]::new(); $ErrorActionPreference='Stop'; $inputData=([Console]::In.ReadToEnd()|ConvertFrom-Json); "+script
    encoded = base64.b64encode(script.encode('utf-16le')).decode('ascii')
    result = subprocess.run([str(executable), '-NoProfile', '-NonInteractive', '-EncodedCommand', encoded], input=json.dumps(values, ensure_ascii=False), capture_output=True, text=True, encoding='utf-8', timeout=30)
    if result.returncode:
        raise RuntimeError('Windows shortcut operation failed: '+result.stderr)
    return json.loads(result.stdout)


def read_shortcuts(paths):
    result = powershell("$shell=New-Object -ComObject WScript.Shell; $rows=@(); foreach($path in $inputData.paths){if(!(Test-Path -LiteralPath $path)){throw 'Expected shortcut does not exist'}; $link=$shell.CreateShortcut($path); $rows+=@{path=$path;target=$link.TargetPath;icon=$link.IconLocation;arguments=$link.Arguments;workingDirectory=$link.WorkingDirectory}}; ConvertTo-Json -InputObject @($rows) -Compress", {'paths': [str(path) for path in paths]})
    return result


def set_shortcut_icons(paths, target, icon):
    return powershell("$shell=New-Object -ComObject WScript.Shell; foreach($path in $inputData.paths){if(!(Test-Path -LiteralPath $path)){throw 'Shortcut disappeared before repair'}; $link=$shell.CreateShortcut($path); if(![String]::Equals([IO.Path]::GetFullPath($link.TargetPath),[IO.Path]::GetFullPath($inputData.target),[StringComparison]::OrdinalIgnoreCase)){throw 'Shortcut target changed before repair'}; $link.IconLocation=$inputData.icon+',0'; $link.Save()}; ConvertTo-Json -InputObject @{updated=$inputData.paths.Count} -Compress", {'paths': [str(path) for path in paths], 'target': str(target), 'icon': str(icon)})


def refresh_shell(paths):
    notify = ctypes.WinDLL('shell32', use_last_error=True).SHChangeNotify
    notify.argtypes = [ctypes.c_long, ctypes.c_uint, ctypes.c_void_p, ctypes.c_void_p]
    notify.restype = None
    for path in paths:
        text = ctypes.c_wchar_p(str(path))
        notify(0x2000, 0x1005, ctypes.cast(text, ctypes.c_void_p), None)
    notify(0x08000000, 0x1000, None, None)


def verify_shortcut_icons(paths, targets, icon, expected_digest):
    if hashlib.sha256(icon.read_bytes()).hexdigest() != expected_digest:
        raise SystemExit('Installed shortcut ICO digest mismatch')
    rows = read_shortcuts(paths)
    for row, target in zip(rows, targets):
        if Path(row['target']).resolve() != target.resolve():
            raise SystemExit('Shortcut target mismatch: '+row['path'])
        location, index = row['icon'].rsplit(',', 1)
        if Path(location.strip('"')).resolve() != icon.resolve() or index.strip() != '0':
            raise SystemExit('Shortcut IconLocation mismatch: '+row['path'])
    return rows
