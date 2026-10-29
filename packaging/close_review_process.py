"""Close only the exact isolated review process launched by our helper."""
from pathlib import Path
import ctypes,json
from ctypes import wintypes
root=Path(__file__).resolve().parent.parent
record=root.parent/'delivery'/'installed-client-process.json'
if not record.exists():raise SystemExit(0)
data=json.loads(record.read_text(encoding='utf-8'))
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
kernel.QueryFullProcessImageNameW.argtypes=[wintypes.HANDLE,wintypes.DWORD,wintypes.LPWSTR,ctypes.POINTER(wintypes.DWORD)]
kernel.TerminateProcess.argtypes=[wintypes.HANDLE,wintypes.UINT]
kernel.CloseHandle.argtypes=[wintypes.HANDLE]
handle=kernel.OpenProcess(0x1000|0x0001,False,data['pid'])
if handle:
 try:
  buffer=ctypes.create_unicode_buffer(32768);size=wintypes.DWORD(len(buffer))
  if not kernel.QueryFullProcessImageNameW(handle,0,buffer,ctypes.byref(size)):raise SystemExit('Unable to validate review process')
  if Path(buffer.value).resolve()!=Path(data['executable']).resolve():raise SystemExit('PID no longer belongs to the isolated review client')
  kernel.TerminateProcess(handle,0)
 finally:kernel.CloseHandle(handle)
print('Isolated review process closed.')
