"""Real visible Main process, atomic saves and restart in a separate owned namespace."""
from pathlib import Path
import ctypes
from ctypes import wintypes
import importlib.util
import json
import os
import shutil
import subprocess
import time
import uuid

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("preview_runner", ROOT / "scripts/dev-preview/runner.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)
original_output = runner.OUT
source = runner.read_json(original_output / "ledger/staged.json")
runner.OUT = original_output / "tests" / ("visible-" + uuid.uuid4().hex[:8])
runner.initialize()
config = runner.OUT / "config"
config.mkdir()
runtime = runner.OUT / "runtime" / source["generation"]
shutil.copytree(Path(source["executable"]).parent, runtime)
receipt = {**source, "executable": str(runtime / "ShadowChat.exe")}
runner.write_json(runner.OUT / "ledger/staged.json", receipt)
results = []


def atomic(filename, text):
    temporary = filename.with_name(filename.name + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    os.replace(temporary, filename)


def qss(font=21, color="#172033", padding=0):
    return f"QLabel#ShadowChatTitle {{font-size:{font}px;color:{color};padding:{padding}px;}}"


def status(predicate, timeout=12):
    deadline = time.monotonic() + timeout
    value = {}
    while time.monotonic() < deadline:
        filename = runner.OUT / "logs/preview-status.json"
        if filename.exists():
            value = runner.read_json(filename)
            if predicate(value):
                return value
        time.sleep(0.1)
    raise AssertionError("Native status condition timed out: " + json.dumps(value, ensure_ascii=False))


def record(name, value):
    results.append({"test": name, "passed": True, "nativeStatus": value})


def resize_owned_window(process_record, width, height):
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows.argtypes = [callback_type, wintypes.LPARAM]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user32.IsWindowVisible.argtypes = [wintypes.HWND]
    user32.MoveWindow.argtypes = [wintypes.HWND, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, wintypes.BOOL]
    handles = []

    @callback_type
    def visit(handle, unused):
        process_id = wintypes.DWORD()
        user32.GetWindowThreadProcessId(handle, ctypes.byref(process_id))
        if process_id.value == process_record["pid"] and user32.IsWindowVisible(handle):
            handles.append(handle)
        return True

    with runner.owned_process(process_record) as handle:
        assert handle
        user32.EnumWindows(visit, 0)
        assert handles, "No visible window owned by exact smoke process"
        assert user32.MoveWindow(handles[0], 120, 80, width, height, True)


atomic(config / "preview.qss", qss())
atomic(config / "layout.json", '{"windowWidth":1200,"windowHeight":800,"conversationListWidth":300}')
try:
    first = runner.launch(receipt, config)
    baseline = status(lambda value: value.get("pid") == first["pid"] and value.get("titleFontPx") == 21)
    assert "原生开发预览" in baseline["windowTitle"] and "本机预览" in baseline["developerTitle"]
    assert baseline["networkLabel"] == "本机预览 · 不连接办公网络"
    assert baseline["visibleChatMessageCount"] == 4 and baseline["visibleFixtureWelcomeCount"] == 1
    assert not baseline["visibleChatPlainHasRawMarkup"]
    assert baseline["visibleChatHasFixtureImage"] and baseline["visibleChatHasFixtureFileLink"] and baseline["fixtureImageLoaded"]
    record("real-Main-visible-normal-auth-synthetic", baseline)
    atomic(config / "preview.qss", qss(font=25))
    changed = status(lambda value: value.get("titleFontPx") == 25)
    record("atomic-font-21-to-25", changed)
    atomic(config / "preview.qss", qss(font=25, color="#196b45"))
    changed = status(lambda value: value.get("titleColor") == "#196b45")
    record("atomic-native-palette-color", changed)
    height = changed["titleHeight"]
    atomic(config / "preview.qss", qss(font=25, color="#196b45", padding=9))
    changed = status(lambda value: value.get("titleContentsX") == 9 and
                     value.get("titleHeight", 0) >= value.get("titleFontMetricsHeight", 0) + 18)
    record("atomic-padding-native-height", changed)
    revision = changed["revision"]
    atomic(config / "layout.json", '{"windowWidth":1280,"windowHeight":820,"conversationListWidth":330}')
    changed = status(lambda value: value.get("width") == 1280 and value.get("listWidth") == 330)
    record("atomic-layout-native-widths", changed)
    revision = changed["revision"]
    atomic(config / "preview.qss", "QLabel {")
    atomic(config / "layout.json", '{"windowWidth":200}')
    changed = status(lambda value: value.get("revision", 0) > revision and bool(value.get("diagnostic")))
    assert changed["width"] == 1280 and changed["titleFontPx"] == 25 and changed["titleColor"] == "#196b45"
    record("invalid-style-and-layout-last-usable", changed)
    atomic(config / "preview.qss", qss(font=25, color="#196b45", padding=9))
    atomic(config / "layout.json", '{"windowWidth":1280,"windowHeight":820,"conversationListWidth":330}')
    changed = status(lambda value: not value.get("diagnostic") and value.get("width") == 1280)
    resize_owned_window(first, 1180, 760)
    revision = changed["revision"]
    time.sleep(0.4)
    atomic(config / "preview.qss", qss(font=25, color="#196b45", padding=9))
    changed = status(lambda value: value.get("revision", 0) > revision and value.get("width") == 1180)
    time.sleep(1.1)
    runner.stop()
    second = runner.launch(receipt, config)
    restarted = status(lambda value: value.get("pid") == second["pid"] and value.get("width") == 1180)
    assert restarted["height"] == 760 and restarted["listWidth"] == 330
    assert restarted["titleFontPx"] == 25 and restarted["titleColor"] == "#196b45"
    assert restarted["visibleChatMessageCount"] == 4 and restarted["visibleFixtureWelcomeCount"] == 1
    assert not restarted["visibleChatPlainHasRawMarkup"] and restarted["fixtureImageLoaded"]
    record("full-process-restart-restores-user-geometry-and-qss", restarted)
    network = subprocess.check_output(["C:/Windows/System32/netstat.exe", "-ano"],
                                      creationflags=runner.CREATE_NO_WINDOW).decode("mbcs", errors="replace")
    sockets = [line.strip() for line in network.splitlines() if line.split() and line.split()[-1] == str(second["pid"])]
    assert sockets and all("127.0.0.1:" in line and "LISTENING" in line for line in sockets), sockets
    results.append({"test": "OS-sockets-loopback-only", "passed": True, "sockets": sockets})
finally:
    runner.stop()
    runner.write_json(original_output / "logs/visible-smoke-verification.json",
                      {"utc": runner.utc(), "namespace": str(runner.OUT), "results": results})
print(json.dumps({"passed": len(results), "namespace": str(runner.OUT)}, ensure_ascii=False))
