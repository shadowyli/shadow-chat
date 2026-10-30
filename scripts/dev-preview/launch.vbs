Option Explicit
Dim shell, files, base, python, action, command
Set shell = CreateObject("WScript.Shell")
Set files = CreateObject("Scripting.FileSystemObject")
base = files.GetParentFolderName(WScript.ScriptFullName)
action = "start"
If WScript.Arguments.Count > 0 Then action = WScript.Arguments(0)
If action <> "start" And action <> "build" And action <> "stop" And action <> "watch" Then WScript.Quit 2
python = shell.ExpandEnvironmentStrings("%SHADOWCHAT_DEV_PYTHON%")
If python = "%SHADOWCHAT_DEV_PYTHON%" Then
  python = shell.ExpandEnvironmentStrings("%USERPROFILE%") & "\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\pythonw.exe"
End If
If Not files.FileExists(python) Then
  MsgBox "Set SHADOWCHAT_DEV_PYTHON to an existing Python 3 pythonw.exe. Preview logs: dist\dev-preview\logs", 48, "Native preview"
  WScript.Quit 1
End If
command = """" & python & """ -B -X utf8 """ & base & "\runner.py"" " & action
shell.Run command, 0, False
