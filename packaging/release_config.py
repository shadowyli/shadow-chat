"""Shared release version and environment rules for build and installer tools."""
from pathlib import Path
import hashlib
import os
import re

ROOT = Path(__file__).resolve().parent.parent
ENVIRONMENTS = ('local', 'dev', 'main')


def qt_directory():
    configured = os.environ.get('SHADOWCHAT_QT_DIR')
    if configured:
        return Path(configured)
    candidates = [parent/'Qt/5.15.2/msvc2019' for parent in ROOT.parents]
    return next((candidate for candidate in candidates if (candidate/'bin/qmake.exe').is_file()), candidates[0])


def version():
    text = (ROOT/'src/core/Version.h').read_text(encoding='utf-8')
    match = re.search(r'\bBEEBEEP_VERSION\[\]\s*=\s*"(\d+\.\d+\.\d+)"', text)
    if match is None:
        raise SystemExit('Unable to read release version from src/core/Version.h')
    return match.group(1)


def channel(environment):
    if environment not in ENVIRONMENTS:
        raise SystemExit('Unknown build environment: '+environment)
    return 'stable' if environment == 'main' else 'test'


def output_directory(environment):
    channel(environment)
    return ROOT/'dist'/environment


def artifact_name(environment):
    return '影聊-'+version()+'-'+channel(environment)+'-x86'


def sha256(filename):
    return hashlib.sha256(Path(filename).read_bytes()).hexdigest()
