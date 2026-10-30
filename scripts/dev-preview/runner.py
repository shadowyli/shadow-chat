"""Own only dist/dev-preview: incremental native build, stage, launch and stop."""
from pathlib import Path
import argparse
import contextlib
import ctypes
from ctypes import wintypes
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "dist/dev-preview"
MARKER = "shadow-native-preview-synthetic-v1"
CREATE_NO_WINDOW = 0x08000000
KERNEL = ctypes.WinDLL("kernel32", use_last_error=True) if os.name == "nt" else None
if KERNEL:
    KERNEL.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    KERNEL.OpenProcess.restype = wintypes.HANDLE
    KERNEL.CloseHandle.argtypes = [wintypes.HANDLE]
    KERNEL.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    KERNEL.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
    KERNEL.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    KERNEL.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]


def utc():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def digest(filename):
    return hashlib.sha256(Path(filename).read_bytes()).hexdigest()


def managed(filename):
    filename = Path(filename)
    expected = ROOT.resolve() / "dist/dev-preview"
    resolved = filename.resolve()
    if resolved != expected and expected not in resolved.parents:
        raise RuntimeError("Refusing path outside independent preview output: " + str(filename))
    return filename


def write_json(filename, value):
    filename = managed(filename)
    filename.parent.mkdir(parents=True, exist_ok=True)
    temporary = filename.with_name(filename.name + "." + uuid.uuid4().hex + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")
    os.replace(temporary, filename)


def read_json(filename):
    return json.loads(Path(filename).read_text(encoding="utf-8-sig"))


def event(name, **fields):
    with managed(OUT / "logs/runner-events.jsonl").open("a", encoding="utf-8") as stream:
        stream.write(json.dumps({"utc": utc(), "event": name, **fields}, ensure_ascii=False) + "\n")


def initialize():
    managed(OUT).mkdir(parents=True, exist_ok=True)
    owner = OUT / "owner.json"
    identity = {"project": str(ROOT.resolve()), "marker": MARKER}
    if owner.exists() and read_json(owner) != identity:
        raise RuntimeError("Preview ownership marker belongs to another project")
    if not owner.exists() and any(OUT.iterdir()):
        raise RuntimeError("Refusing an unowned nonempty preview directory")
    write_json(owner, identity)
    for name in ("logs", "ledger", "control", "profile", "runtime", "tools"):
        managed(OUT / name).mkdir(exist_ok=True)
    marker = OUT / "profile/.shadow-dev-preview"
    if marker.exists() and marker.read_text(encoding="utf-8").strip() != MARKER:
        raise RuntimeError("Preview profile marker mismatch")
    marker.write_text(MARKER + "\n", encoding="utf-8")


def process_identity(handle):
    size = wintypes.DWORD(32768)
    buffer = ctypes.create_unicode_buffer(size.value)
    if not KERNEL.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
        raise ctypes.WinError(ctypes.get_last_error())
    values = [wintypes.FILETIME() for _ in range(4)]
    if not KERNEL.GetProcessTimes(handle, *(ctypes.byref(value) for value in values)):
        raise ctypes.WinError(ctypes.get_last_error())
    created = (values[0].dwHighDateTime << 32) | values[0].dwLowDateTime
    return {"executable": str(Path(buffer.value).resolve()), "createdFileTime": str(created)}


@contextlib.contextmanager
def owned_process(record, terminate=False):
    executable = managed(record["executable"])
    if executable.name != "ShadowChat.exe" or (OUT / "runtime").resolve() not in executable.resolve().parents:
        raise RuntimeError("Ledger does not identify a staged native preview executable")
    handle = KERNEL.OpenProcess(0x1000 | 0x100000 | (1 if terminate else 0), False, int(record["pid"]))
    if not handle:
        if ctypes.get_last_error() == 87:
            yield None
            return
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        actual = process_identity(handle)
        if os.path.normcase(actual["executable"]) != os.path.normcase(str(executable.resolve())) or actual["createdFileTime"] != record["createdFileTime"]:
            raise RuntimeError("PID reused or process path differs; refusing to touch process")
        if digest(executable) != record["sha256"]:
            raise RuntimeError("Staged executable changed since launch; refusing process action")
        yield handle
    finally:
        KERNEL.CloseHandle(handle)


def current():
    filename = OUT / "ledger/current.json"
    return read_json(filename) if filename.exists() else None


def stop():
    record = current()
    if not record or record.get("stoppedAt"):
        return
    with owned_process(record, terminate=True) as handle:
        if handle and KERNEL.WaitForSingleObject(handle, 0) != 0:
            managed(OUT / "control" / (record["instance"] + ".stop")).write_text(record["instance"], encoding="ascii")
            if KERNEL.WaitForSingleObject(handle, 6000) != 0:
                if not KERNEL.TerminateProcess(handle, 0):
                    raise ctypes.WinError(ctypes.get_last_error())
                KERNEL.WaitForSingleObject(handle, 3000)
                event("forced-stop", pid=record["pid"], executable=record["executable"])
            else:
                event("graceful-stop", pid=record["pid"], executable=record["executable"])
    record["stoppedAt"] = utc()
    write_json(OUT / "ledger/current.json", record)


def run_logged(command, cwd, log, env=None, timeout=None):
    with log.open("ab") as stream:
        stream.write(("\nRUN " + subprocess.list2cmdline([str(part) for part in command]) + "\n").encode("utf-8"))
        stream.flush()
        subprocess.run(command, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT,
                       check=True, creationflags=CREATE_NO_WINDOW, timeout=timeout)


def toolchain():
    configuration = read_json(ROOT / "dist/local/build-configuration.json")
    qmake = Path(configuration["qmake"]["path"]).resolve()
    nmake = Path(configuration["nmake"]["path"]).resolve()
    compiler = Path(configuration["compilerPath"]).resolve()
    if digest(qmake) != configuration["qmake"]["sha256"] or digest(nmake) != configuration["nmake"]["sha256"]:
        raise RuntimeError("Recorded production toolchain identity changed")
    sdk_root = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Windows Kits/10"
    sdk_version = sorted((sdk_root / "Include").iterdir())[-1].name
    msvc = compiler.parents[3]
    environment = dict(os.environ)
    environment["PATH"] = os.pathsep.join([str(qmake.parent), str(compiler.parent), str(sdk_root / "bin" / sdk_version / "x86"),
                                          str(Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32"), environment.get("PATH", "")])
    environment["INCLUDE"] = os.pathsep.join([str(msvc / "include")] + [str(sdk_root / "Include" / sdk_version / name) for name in ("ucrt", "shared", "um", "winrt", "cppwinrt")])
    environment["LIB"] = os.pathsep.join([str(msvc / "lib/x86")] + [str(sdk_root / "Lib" / sdk_version / name / "x86") for name in ("ucrt", "um")])
    return configuration, qmake, nmake, environment


def fingerprint():
    snapshot = hashlib.sha256()
    for base in (ROOT / "src", ROOT / "locale", ROOT / "scripts/dev-preview"):
        for filename in sorted(base.rglob("*")):
            if filename.is_file() and filename.suffix.lower() in (".cpp", ".h", ".cxx", ".c", ".pro", ".pri", ".qrc", ".ui", ".rc", ".py"):
                snapshot.update(str(filename.relative_to(ROOT)).encode("utf-8"))
                snapshot.update(filename.read_bytes())
    snapshot.update((ROOT / "beebeep.pri").read_bytes())
    return snapshot.hexdigest()


def verify():
    build()
    _, qmake, nmake, environment = toolchain()
    payload = ROOT / "dist/local/影聊-1.1.3-test-x86"
    environment["PATH"] = str(payload) + os.pathsep + environment["PATH"]
    platforms = managed(OUT / "tools/qt-platforms")
    platforms.mkdir(parents=True, exist_ok=True)
    for name in ("qoffscreen.dll", "qminimal.dll"):
        shutil.copy2(qmake.parent.parent / "plugins/platforms" / name, platforms / name)
    environment["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(platforms)
    results = []
    for name, project, extra in (
        ("native", "dev_preview_test.pro", []),
        ("main-gate", "dev_preview_gate_test.pro", ["PREVIEW_GATE_ENV=MAIN"]),
        ("dev-gate", "dev_preview_gate_test.pro", ["PREVIEW_GATE_ENV=DEV"]),
        ("restart-boundary", "dev_preview_restart_test.pro", []),
    ):
        directory = managed(OUT / "tests" / name)
        directory.mkdir(parents=True, exist_ok=True)
        log = OUT / "logs" / ("test-" + name + ".log")
        run_logged([str(qmake), str(ROOT / "tests" / project), "SHADOWCHAT_PREVIEW_BUILD=" +
                    str(OUT / "production").replace("\\", "/"), *extra], directory, log, environment)
        run_logged([str(nmake), "/nologo"], directory, log, environment)
        target = "dev_preview_test.exe" if name == "native" else "dev_preview_restart_test.exe" if name == "restart-boundary" else "dev_preview_gate_test.exe"
        executable = directory / target
        if not executable.exists():
            executable = directory / "release" / target
        command = [str(executable)]
        test_environment = dict(environment)
        if name == "restart-boundary":
            profile = directory / "synthetic"
            config = directory / "config"
            profile.mkdir(exist_ok=True)
            config.mkdir(exist_ok=True)
            (profile / ".shadow-dev-preview").write_text(MARKER, encoding="utf-8")
            (config / "preview.qss").write_text("", encoding="utf-8")
            (config / "layout.json").write_text("{}", encoding="utf-8")
            proof = directory / "restart-proof.json"
            test_environment["SHADOW_DEV_RESTART_PROOF"] = str(proof)
            command += ["--dev-preview", str(config), "--profile-root", str(profile),
                        "--dev-instance", str(uuid.uuid4()), "--dev-seed"]
        run_logged(command, directory, log, test_environment, timeout=90)
        if name == "restart-boundary":
            evidence = read_json(proof)
            time.sleep(0.3)
            if not evidence.get("coreStayedIsolatedAfterPropertyFalse") or not evidence.get("dynamicPropertyWasCleared") or proof.with_name(proof.name + ".unexpected-child").exists():
                raise RuntimeError("Actual Main restart isolation boundary failed")
            native_log = (directory / "logs/影聊.log").read_text(encoding="utf-8", errors="replace")
            if "restart with the native preview launcher. No detached child." not in native_log:
                raise RuntimeError("Dev account switch launcher instruction missing from actual Main log")
        results.append({"test": name, "passed": True, "log": str(log)})
    run_logged([sys.executable, "-B", str(ROOT / "tests/dev_preview_runner_test.py")], ROOT,
               OUT / "logs/test-runner.log", environment)
    write_json(OUT / "logs/verification.json", {"utc": utc(), "results": results, "runnerTests": True})


def build():
    configuration, qmake, nmake, environment = toolchain()
    production = ROOT / "dist/local/production"
    if configuration["version"] != "1.1.3" or configuration["releaseEnvironment"] != "local":
        raise RuntimeError("Expected verified 1.1.3 local production build")
    if digest(production / "bin/ShadowChat.exe") != configuration["sourceExecutableSha256"]:
        raise RuntimeError("Verified production executable identity changed")
    workspace = managed(OUT / "production")
    workspace.mkdir(exist_ok=True)
    if not (workspace / "reuse.json").exists():
        for name in ("objects", "generated"):
            shutil.copytree(production / name, managed(workspace / name), dirs_exist_ok=True, copy_function=shutil.copy2)
        write_json(workspace / "reuse.json", {"production": str(production), "sourceSha256": configuration["sourceExecutableSha256"], "utc": utc()})
    log = OUT / "logs/build.log"
    arguments = [str(qmake), str(ROOT / "src/src.pro"), "-o", "Makefile",
                 "SHADOWCHAT_ENV=local", "SHADOWCHAT_LOCAL_UPDATE_PORT=5173", "CONFIG+=no_batch", "-after"]
    arguments += [name + "=" + str(workspace / subdirectory).replace("\\", "/") for name, subdirectory in
                  (("DESTDIR", "bin"), ("OBJECTS_DIR", "objects"), ("MOC_DIR", "generated"), ("UI_DIR", "generated"), ("RCC_DIR", "generated"))]
    run_logged(arguments, workspace, log, environment)
    defines = next(line for line in (workspace / "Makefile.Release").read_text(encoding="utf-8").splitlines() if line.startswith("DEFINES"))
    if defines != configuration["effectiveDefines"]:
        raise RuntimeError("Independent build defines differ from reusable production objects")
    run_logged([str(nmake), "/nologo", "/f", "Makefile.Release"], workspace, log, environment)
    binary = workspace / "bin/ShadowChat.exe"
    identity = json.loads(subprocess.check_output([str(binary), "--build-info"], cwd=binary.parent, env=environment, creationflags=CREATE_NO_WINDOW))
    if identity.get("version") != "1.1.3" or identity.get("environment") != "local" or not identity.get("nativeDevPreview"):
        raise RuntimeError("New binary failed native local preview identity check")
    generation = uuid.uuid4().hex[:12]
    stage = managed(OUT / "runtime" / generation)
    stage.mkdir()
    payload = ROOT / "dist/local/影聊-1.1.3-test-x86"
    for filename in payload.iterdir():
        if filename.is_file() and filename.suffix.lower() in (".dll", ".qm", ".wav", ".ico"):
            shutil.copy2(filename, managed(stage / filename.name))
    for name in ("platforms", "imageformats", "iconengines", "styles", "audio", "mediaservice", "bearer", "playlistformats", "printsupport"):
        if (payload / name).exists():
            shutil.copytree(payload / name, managed(stage / name))
    shutil.copy2(binary, stage / "ShadowChat.exe")
    (stage / "beebeep.rc").write_text("[General]\nUseSettingsFileIni=true\nSaveDataInDocumentsFolder=false\nDataFolderPath=" +
                                     str(OUT / "profile").replace("\\", "/") + "\n", encoding="utf-8")
    receipt = {"utc": utc(), "executable": str(stage / "ShadowChat.exe"), "sha256": digest(stage / "ShadowChat.exe"),
               "buildIdentity": identity, "generation": generation, "productionReusedReadOnly": str(production)}
    write_json(OUT / "ledger/staged.json", receipt)
    event("staged", **receipt)
    return receipt


def launch(receipt=None, config=None):
    record = current()
    if record and not record.get("stoppedAt"):
        with owned_process(record) as handle:
            if handle and KERNEL.WaitForSingleObject(handle, 0) != 0:
                event("already-running", pid=record["pid"])
                return record
    receipt = receipt or read_json(OUT / "ledger/staged.json")
    executable = managed(receipt["executable"])
    if digest(executable) != receipt["sha256"]:
        raise RuntimeError("Staged executable identity differs from receipt")
    instance = str(uuid.uuid4())
    arguments = [str(executable), "--dev-preview", str(config or ROOT / "dev-preview"), "--profile-root", str(OUT / "profile"),
                 "--dev-instance", instance, "--dev-seed"]
    environment = dict(os.environ)
    for name in ("QT_QPA_PLATFORM", "QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "GAMMARAY_PROBE_INJECTOR"):
        environment.pop(name, None)
    environment["PATH"] = str(executable.parent) + os.pathsep + environment.get("PATH", "")
    with (OUT / "logs/process.log").open("ab") as log:
        process = subprocess.Popen(arguments, cwd=executable.parent, env=environment, stdout=log, stderr=log, creationflags=CREATE_NO_WINDOW)
    handle = KERNEL.OpenProcess(0x1000 | 0x100000, False, process.pid)
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        identity = process_identity(handle)
    finally:
        KERNEL.CloseHandle(handle)
    record = {**receipt, **identity, "pid": process.pid, "instance": instance, "arguments": arguments, "startedAt": utc()}
    record.pop("stoppedAt", None)
    write_json(OUT / "ledger" / (instance + ".json"), record)
    write_json(OUT / "ledger/current.json", record)
    ready = OUT / "control" / (instance + ".ready.json")
    for _ in range(80):
        if process.poll() is not None:
            raise RuntimeError("Preview exited before ready: " + str(process.returncode))
        if ready.exists():
            report = read_json(ready)
            if report["pid"] != process.pid or report["instance"] != instance or not report.get("visible"):
                raise RuntimeError("Ready receipt does not match visible launched preview")
            event("ready", pid=process.pid, executable=str(executable), instance=instance)
            return record
        time.sleep(0.25)
    raise RuntimeError("Preview ready timeout; inspect independent logs")


@contextlib.contextmanager
def lock():
    filename = managed(OUT / "runner.lock")
    stream = filename.open("a+b")
    acquired = False
    try:
        if os.name == "nt":
            import msvcrt
            if os.fstat(stream.fileno()).st_size == 0:
                stream.write(b"0")
                stream.flush()
            stream.seek(0)
            try:
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError:
                stream.close()
                yield False
                return
        acquired = True
        yield True
    finally:
        if not stream.closed:
            if os.name == "nt" and acquired:
                stream.seek(0)
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
            stream.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("start", "build", "stop", "status", "watch", "test"))
    args = parser.parse_args()
    initialize()
    if args.action == "status":
        print(json.dumps({"current": current(), "output": str(OUT)}, ensure_ascii=False, indent=2))
        return
    request = OUT / "control/build-request.json"
    stop_request = OUT / "control/runner-stop.json"
    initial_stop = stop_request.read_bytes() if stop_request.exists() else b""
    if args.action == "stop":
        write_json(stop_request, {"request": str(uuid.uuid4()), "utc": utc()})
        record = current()
        if record and not record.get("stoppedAt"):
            with owned_process(record):
                managed(OUT / "control" / (record["instance"] + ".stop")).write_text(record["instance"], encoding="ascii")
    if args.action in ("build", "watch"):
        write_json(request, {"request": str(uuid.uuid4()), "utc": utc()})
    with lock() as acquired:
        if not acquired:
            event("coalesced", action=args.action)
            return
        if args.action == "stop":
            stop()
            return
        if args.action == "test":
            verify()
            return
        if args.action == "start" and (OUT / "ledger/staged.json").exists():
            launch()
            return
        while True:
            if stop_request.exists() and stop_request.read_bytes() != initial_stop:
                stop()
                return
            requested = request.read_bytes() if request.exists() else b""
            before = fingerprint()
            try:
                receipt = build()
            except Exception as error:
                event("build-failed-old-runtime-retained", error=str(error))
                raise
            if before != fingerprint() or (request.exists() and request.read_bytes() != requested):
                event("build-again-after-coalesced-changes")
                time.sleep(0.35)
                continue
            if stop_request.exists() and stop_request.read_bytes() != initial_stop:
                stop()
                return
            old = current()
            stop()
            try:
                launch(receipt)
            except Exception:
                stop()
                if old:
                    launch(old)
                    write_json(OUT / "ledger/staged.json", old)
                raise
            if args.action != "watch":
                if request.exists() and request.read_bytes() != requested:
                    continue
                return
            while before == fingerprint():
                if stop_request.exists() and stop_request.read_bytes() != initial_stop:
                    stop()
                    return
                time.sleep(0.75)
            time.sleep(0.35)


if __name__ == "__main__":
    try:
        main()
    except Exception as failure:
        if (OUT / "logs").exists():
            event("error", error=str(failure))
            (OUT / "logs/last-error.txt").write_text(str(failure) + "\n", encoding="utf-8")
        if sys.stderr:
            print(str(failure), file=sys.stderr)
        sys.exit(1)
