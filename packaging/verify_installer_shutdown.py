from pathlib import Path
import argparse
import ctypes
import json
import os
import shutil
import subprocess
import tempfile
import time
import uuid

from release_config import ROOT, sha256
from verify_installation import host_snapshot, require_installer_isolation

parser = argparse.ArgumentParser()
parser.add_argument("--previous-payload", type=Path, required=True)
arguments = parser.parse_args()
receipt = json.loads((ROOT / "dist/local/release-build.json").read_text(encoding="utf8"))
installer = Path(receipt["installer"])
require_installer_isolation(installer)
assert sha256(installer) == receipt["sha256"]
namespace = uuid.uuid4().hex
fixture = Path(tempfile.gettempdir()) / ("ShadowChat-QA-" + namespace)
install = fixture / "program"
other = fixture / "other-program"
output = ROOT / "dist/installer-shutdown"
output.mkdir(exist_ok=True)
before_host = host_snapshot()
shutil.copytree(arguments.previous_payload, install)
shutil.copytree(arguments.previous_payload, other)
environment = dict(os.environ)
environment.pop("GH_TOKEN", None)
user = ctypes.WinDLL("user32", use_last_error=True)
user.GetWindowThreadProcessId.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint32)]
user.GetWindowThreadProcessId.restype = ctypes.c_uint32
window_callback = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
user.EnumWindows.argtypes = [window_callback, ctypes.c_void_p]
children = []
streams = []


def launch(directory, name):
    profile = fixture / ("profile-" + name)
    profile.mkdir()
    (profile / ".shadow-dev-preview").write_text("shadow-native-preview-synthetic-v1\n", encoding="utf8")
    stream = (output / (name + ".log")).open("wb")
    streams.append(stream)
    child = subprocess.Popen([str(directory / "ShadowChat.exe"), "--dev-preview", str(ROOT / "dev-preview"),
        "--profile-root", str(profile), "--dev-instance", str(uuid.uuid4()), "--dev-seed"],
        cwd=directory, env=environment, stdout=stream, stderr=stream, creationflags=0x08000000)
    children.append(child)
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if child.poll() is not None:
            raise RuntimeError(name + " synthetic application exited before installer test")
        windows = []
        def collect(window, unused):
            process = ctypes.c_uint32()
            user.GetWindowThreadProcessId(window, ctypes.byref(process))
            if process.value == child.pid:
                windows.append(window)
            return 1
        user.EnumWindows(window_callback(collect), None)
        if windows:
            time.sleep(2)
            if child.poll() is None:
                return child, profile
        time.sleep(0.2)
    raise RuntimeError(name + " synthetic application did not create a native window")


def run_install():
    result = subprocess.run([str(installer), "/S", "/SHADOWCHAT-QA=" + namespace],
                            env=environment, timeout=120, creationflags=0x08000000)
    if result.returncode:
        raise RuntimeError("Automatic-close installer failed: " + str(result.returncode))
    assert sha256(install / "ShadowChat.exe") == receipt["sourceExecutableSha256"]


try:
    existing, profile = launch(install, "previous")
    unrelated, other_profile = launch(other, "unrelated")
    run_install()
    assert existing.wait(timeout=15) == 0
    assert unrelated.poll() is None
    assert (profile / "preview-window.ini").is_file()
    current, current_profile = launch(install, "current")
    run_install()
    assert current.wait(timeout=15) == 0
    assert unrelated.poll() is None
    assert (current_profile / "preview-window.ini").is_file()
    assert before_host == host_snapshot()
    report = {"passed": True, "installerSha256": sha256(installer),
        "previousRunningApplicationExitedNormally": True, "currentRunningApplicationExitedNormally": True,
        "differentInstallationKeptRunning": True, "normalSessionSaveObserved": True,
        "productionRegistrationUntouched": True, "fixture": str(fixture),
        "scope": "Exact installer and real Qt binaries on isolated Win32 desktop with synthetic dev-preview accounts; no user account or office network used."}
    (output / "verification.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf8")
    print(json.dumps(report, ensure_ascii=False), flush=True)
finally:
    for child in children:
        if child.poll() is None:
            child.terminate()
            child.wait(timeout=10)
    for stream in streams:
        stream.close()
