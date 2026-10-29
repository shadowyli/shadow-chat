"""Shared release version and environment rules for build and installer tools."""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parent.parent
ENVIRONMENTS = ('local', 'dev', 'main')


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
