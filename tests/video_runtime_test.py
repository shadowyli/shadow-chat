import contextlib
import hashlib
import importlib.util
import io
import json
from pathlib import Path, PurePosixPath
import struct
import sys
import tempfile
import unittest
from unittest import mock
import zipfile


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("video_runtime", ROOT / "packaging" / "prepare_video_runtime.py")
PREPARE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PREPARE)
sys.path.insert(0, str(ROOT / "packaging"))
STAGE_SPEC = importlib.util.spec_from_file_location("stage_video_runtime", ROOT / "packaging" / "stage_video_runtime.py")
STAGE = importlib.util.module_from_spec(STAGE_SPEC)
STAGE_SPEC.loader.exec_module(STAGE)
CACHE = ROOT / "dist" / "deps" / f"vlc-{PREPARE.VERSION}-{PREPARE.ARCHITECTURE}"
RUNTIME = CACHE / "runtime"


class VideoRuntimeTests(unittest.TestCase):
    def test_official_pin_and_every_runtime_file(self):
        self.assertEqual(PREPARE.digest(CACHE / PREPARE.ARCHIVE_NAME), PREPARE.ARCHIVE_SHA256)
        manifest = json.loads((RUNTIME / "runtime-manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(manifest["sha256"], PREPARE.ARCHIVE_SHA256)
        self.assertEqual(manifest["runtimeDirectory"], "video-runtime")
        self.assertEqual(manifest["version"], PREPARE.VERSION)
        self.assertGreater(len(manifest["files"]), 200)
        for entry in manifest["files"]:
            relative = PurePosixPath(entry["path"])
            self.assertNotIn("..", relative.parts)
            target = RUNTIME.joinpath(*relative.parts)
            self.assertEqual(target.stat().st_size, entry["bytes"])
            self.assertEqual(PREPARE.digest(target), entry["sha256"])
            if target.suffix.lower() == ".dll":
                self.assertTrue(PREPARE.is_x86_pe(target.read_bytes()), entry["path"])
        self.assertIn("COPYING.txt", manifest["licenses"])
        self.assertIn("GNU GENERAL PUBLIC LICENSE", (RUNTIME / "COPYING.txt").read_text(encoding="utf-8"))
        self.assertFalse((RUNTIME / "vlc.exe").exists())
        self.assertFalse((RUNTIME / "plugins" / "gui").exists())
        self.assertFalse(list(RUNTIME.glob("Qt*.dll")))

    def test_arbitrary_independent_destination(self):
        verification = json.loads((CACHE / "official-download-verification.json").read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory(prefix="shadow-video-runtime-") as folder:
            destination = Path(folder) / "independent-payload" / "video-runtime"
            with contextlib.redirect_stdout(io.StringIO()):
                PREPARE.prepare_runtime(CACHE / PREPARE.ARCHIVE_NAME, destination, verification, offline=True)
            manifest = json.loads((destination / "runtime-manifest.json").read_text(encoding="utf-8"))
            for entry in manifest["files"]:
                self.assertEqual(PREPARE.digest(destination / entry["path"]), entry["sha256"])
            self.assertTrue((destination / "libvlc.dll").is_file())
            self.assertTrue((destination / "libvlccore.dll").is_file())

    def test_refuses_overwriting_another_runtime(self):
        with tempfile.TemporaryDirectory(prefix="shadow-video-conflict-") as folder:
            destination = Path(folder)
            (destination / "libvlc.dll").write_bytes(b"unrelated-existing-file")
            with self.assertRaisesRegex(RuntimeError, "overwrite"):
                PREPARE.prepare_runtime(CACHE / PREPARE.ARCHIVE_NAME, destination, {})
            self.assertEqual((destination / "libvlc.dll").read_bytes(), b"unrelated-existing-file")

    def test_checksum_change_is_rejected_before_download(self):
        response = mock.MagicMock()
        response.__enter__.return_value = response
        response.status = 200
        response.read.return_value = b"0" * 64 + b"  archive.zip"
        with tempfile.TemporaryDirectory(prefix="shadow-video-checksum-") as folder:
            with mock.patch.object(PREPARE, "request", return_value=response):
                with self.assertRaisesRegex(RuntimeError, "pinned"):
                    PREPARE.download_archive(Path(folder), 1)

    def test_archive_traversal_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="shadow-video-archive-") as folder:
            root = Path(folder)
            archive = root / "unsafe.zip"
            data = bytearray(512)
            data[:2] = b"MZ"
            struct.pack_into("<I", data, 60, 80)
            data[80:84] = b"PE\x00\x00"
            struct.pack_into("<H", data, 84, 0x14C)
            with zipfile.ZipFile(archive, "w") as bundle:
                bundle.writestr("vlc-test/libvlc.dll", data)
                bundle.writestr("vlc-test/libvlccore.dll", data)
                bundle.writestr("vlc-test/../escaped.dll", b"bad")
            with self.assertRaisesRegex(RuntimeError, "Unsafe"):
                PREPARE.prepare_runtime(archive, root / "payload", {})
            self.assertFalse((root / "escaped.dll").exists())

    def test_truncated_binary_is_not_a_valid_runtime(self):
        self.assertFalse(PREPARE.is_x86_pe(b"short"))
        data = bytearray(64)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 60, 100000)
        self.assertFalse(PREPARE.is_x86_pe(data))

    def test_offline_staging_uses_verified_cache_and_rejects_extra_output(self):
        with tempfile.TemporaryDirectory(prefix="shadow-video-offline-") as folder:
            destination = Path(folder) / "video-runtime"
            with mock.patch.object(PREPARE, "request", side_effect=AssertionError("Offline staging used the network")):
                archive, verification = PREPARE.verified_cache(CACHE)
                with contextlib.redirect_stdout(io.StringIO()):
                    PREPARE.prepare_runtime(archive, destination, verification, offline=True)
                manifest = PREPARE.verify_runtime(destination)
            self.assertEqual(manifest["sha256"], PREPARE.ARCHIVE_SHA256)
            self.assertEqual(PREPARE.digest(destination / "COPYING.LIB"), PREPARE.LIBRARY_LICENSE_SHA256)
            (destination / "unexpected-debug-file.txt").write_text("synthetic-extra-output", encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "unexpected"):
                PREPARE.verify_runtime(destination)

    def test_invalid_cache_provenance_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix="shadow-video-cache-proof-") as folder:
            cache = Path(folder)
            (cache / PREPARE.ARCHIVE_NAME).write_bytes(b"synthetic-invalid-cache")
            (cache / "COPYING.LIB").write_bytes((CACHE / "COPYING.LIB").read_bytes())
            verification = json.loads((CACHE / "official-download-verification.json").read_text(encoding="utf-8"))
            verification["version"] = "0.0.0"
            (cache / "official-download-verification.json").write_text(json.dumps(verification), encoding="utf-8")
            with self.assertRaisesRegex(RuntimeError, "verification"):
                PREPARE.verified_cache(cache)

    def test_installer_stages_by_offline_prepare_script_and_preserves_prior_payload(self):
        with tempfile.TemporaryDirectory(prefix="shadow-video-installer-stage-") as folder:
            payload = Path(folder) / "fresh-payload"
            with mock.patch.object(STAGE.subprocess, "run", wraps=STAGE.subprocess.run) as invocation:
                receipt = STAGE.stage_video_runtime(CACHE, payload)
            command = invocation.call_args.args[0]
            self.assertIn("--offline", command)
            self.assertTrue(any(str(value).endswith("prepare_video_runtime.py") for value in command))
            self.assertEqual(receipt["archiveSha256"], PREPARE.ARCHIVE_SHA256)
            self.assertEqual(receipt["manifestSha256"], PREPARE.digest(payload / "video-runtime/runtime-manifest.json"))
            self.assertEqual(receipt["files"], len(PREPARE.verify_runtime(payload / "video-runtime")["files"]))
            with self.assertRaisesRegex(RuntimeError, "Fresh"):
                STAGE.stage_video_runtime(CACHE, payload)


if __name__ == "__main__":
    unittest.main(verbosity=2)
