"""Process ownership and path/locking contracts; never operates installed clients."""
from pathlib import Path
import importlib.util
import json
import os
import subprocess
import sys
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("native_preview_runner", ROOT / "scripts/dev-preview/runner.py")
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class PreviewRunnerTests(unittest.TestCase):
    def test_outside_paths_rejected(self):
        for filename in (ROOT / "src/release/ShadowChat.exe", ROOT / "dist/local/production/bin/ShadowChat.exe",
                         ROOT.parent / "ShadowChat.exe", runner.OUT / "../other/ShadowChat.exe"):
            with self.assertRaises(RuntimeError):
                runner.managed(filename)

    def test_atomic_json(self):
        filename = runner.OUT / "tests" / ("atomic-" + uuid.uuid4().hex + ".json")
        runner.write_json(filename, {"fixture": "合成", "version": "1.1.2"})
        self.assertEqual(runner.read_json(filename), {"fixture": "合成", "version": "1.1.2"})
        self.assertFalse(list(filename.parent.glob(filename.name + "*.tmp")))

    def test_existing_preview_identity_is_exact(self):
        record = runner.current()
        if not record or record.get("stoppedAt"):
            self.skipTest("No live owned preview")
        with runner.owned_process(record) as handle:
            self.assertTrue(handle)
        for field, value in (("createdFileTime", "0"), ("sha256", "0" * 64)):
            wrong = {**record, field: value}
            with self.assertRaises(RuntimeError):
                with runner.owned_process(wrong):
                    self.fail("Incorrect process identity accepted")
        wrong = {**record, "executable": str(ROOT / "dist/local/production/bin/ShadowChat.exe")}
        with self.assertRaises(RuntimeError):
            with runner.owned_process(wrong):
                self.fail("Production process path accepted")

    def test_concurrent_runner_lock_is_exclusive(self):
        code = (
            "import importlib.util,sys;"
            "spec=importlib.util.spec_from_file_location('runner',sys.argv[1]);"
            "module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);"
            "context=module.lock();print(context.__enter__());context.__exit__(None,None,None)"
        )
        with runner.lock() as acquired:
            if not acquired:
                result = subprocess.check_output([sys.executable, "-B", "-c", code, str(ROOT / "scripts/dev-preview/runner.py")],
                                                 creationflags=runner.CREATE_NO_WINDOW, text=True)
            else:
                result = subprocess.check_output([sys.executable, "-B", "-c", code, str(ROOT / "scripts/dev-preview/runner.py")],
                                                 creationflags=runner.CREATE_NO_WINDOW, text=True)
        self.assertEqual(result.strip(), "False")


if __name__ == "__main__":
    runner.initialize()
    unittest.main()
