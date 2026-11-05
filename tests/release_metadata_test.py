import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "packaging"))
from release_metadata import update_metadata


class ReleaseMetadataTests(unittest.TestCase):
    def setUp(self):
        self.receipt = {
            "version": "1.2.3", "releaseEnvironment": "local", "channel": "test",
            "sha256": "a" * 64, "size": 100,
            "productionBuild": {"binaryBuildIdentity": {"version": "1.2.3", "environment": "local", "channel": "test"}},
        }

    def test_old_client_fields_are_present(self):
        metadata = update_metadata(self.receipt, "b" * 40, "ShadowChat-1.2.3-local-setup.exe")
        self.assertEqual(metadata["product"], "ShadowChat")
        self.assertEqual(metadata["sourceTag"], "v1.2.3-test")
        self.assertEqual(metadata["installer"], {"file": "ShadowChat-1.2.3-local-setup.exe", "bytes": 100, "sha256": "a" * 64})

    def test_version_and_environment_are_not_guessed(self):
        receipt = copy.deepcopy(self.receipt)
        receipt["productionBuild"]["binaryBuildIdentity"]["version"] = "1.2.2"
        with self.assertRaises(ValueError):
            update_metadata(receipt, "b" * 40, "ShadowChat-1.2.3-local-setup.exe")

    def test_wrong_asset_name_is_rejected(self):
        with self.assertRaises(ValueError):
            update_metadata(self.receipt, "b" * 40, "setup.exe")

    def test_unsigned_is_not_a_checksum_bypass(self):
        for size, sha in [(0, "a" * 64), (True, "a" * 64), (100, "missing"), (2**31 + 1, "a" * 64)]:
            receipt = copy.deepcopy(self.receipt)
            receipt["size"] = size
            receipt["sha256"] = sha
            with self.assertRaises(ValueError):
                update_metadata(receipt, "b" * 40, "ShadowChat-1.2.3-local-setup.exe")

    def test_stable_channel_stays_separate(self):
        receipt = copy.deepcopy(self.receipt)
        receipt["releaseEnvironment"] = "main"
        with self.assertRaises(ValueError):
            update_metadata(receipt, "b" * 40, "ShadowChat-1.2.3-main-setup.exe")


if __name__ == "__main__":
    unittest.main()
