from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'packaging'))
from source_archive import select_source_files


class SourceArchiveTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        subprocess.run(['git', 'init', '-q'], cwd=self.root, check=True)
        (self.root/'src').mkdir()
        (self.root/'src/main.cpp').write_text('int main() { return 0; }\n', encoding='utf-8')
        subprocess.run(['git', 'add', 'src/main.cpp'], cwd=self.root, check=True)

    def tearDown(self):
        self.temporary.cleanup()

    def test_generated_root_proof_is_excluded(self):
        (self.root/'actual-main-window.png').write_bytes(b'synthetic screenshot')
        files, excluded = select_source_files(self.root, [])
        self.assertEqual([name for name, path in files], ['src/main.cpp'])
        self.assertEqual(excluded, 1)

    def test_reviewed_new_source_is_included(self):
        (self.root/'src/new.cpp').write_text('int reviewed = 1;\n', encoding='utf-8')
        files, excluded = select_source_files(self.root, ['src/new.cpp'])
        self.assertEqual([name for name, path in files], ['src/main.cpp', 'src/new.cpp'])
        self.assertEqual(excluded, 0)

    def test_unreviewed_code_fails_closed(self):
        (self.root/'src/unreviewed.cpp').write_text('int unreviewed = 1;\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Unreviewed'):
            select_source_files(self.root, [])

    def test_new_private_source_is_rejected(self):
        (self.root/'delivery-private').mkdir()
        (self.root/'delivery-private/key.ini').write_text('synthetic only\n', encoding='utf-8')
        with self.assertRaises(ValueError):
            select_source_files(self.root, ['delivery-private/key.ini'])

    def test_unapproved_root_code_is_rejected(self):
        (self.root/'unexpected.py').write_text('pass\n', encoding='utf-8')
        with self.assertRaisesRegex(ValueError, 'Unreviewed'):
            select_source_files(self.root, [])

    def test_path_traversal_and_duplicates_are_rejected(self):
        for approved in [['../escape'], ['src/main.cpp', 'src/main.cpp']]:
            with self.assertRaises(ValueError):
                select_source_files(self.root, approved)

    def test_symlink_attribute_is_rejected(self):
        with patch.object(Path, 'is_symlink', return_value=True):
            with self.assertRaisesRegex(ValueError, 'Linked'):
                select_source_files(self.root, [])

    def test_resolved_directory_escape_is_rejected(self):
        source = self.root/'src/escape.cpp'
        source.write_text('synthetic only\n', encoding='utf-8')
        original = Path.resolve

        def resolve(candidate, *arguments, **keywords):
            if candidate == source:
                return self.root.parent/'outside-source.cpp'
            return original(candidate, *arguments, **keywords)

        with patch.object(Path, 'resolve', new=resolve):
            with self.assertRaisesRegex(ValueError, 'out-of-workspace'):
                select_source_files(self.root, ['src/escape.cpp'])


if __name__ == '__main__':
    unittest.main()
