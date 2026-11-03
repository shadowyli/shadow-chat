import argparse
import concurrent.futures
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import struct
import urllib.parse
import urllib.request
import zipfile


VERSION = "3.0.24"
ARCHITECTURE = "win32"
ARCHIVE_NAME = f"vlc-{VERSION}-{ARCHITECTURE}.zip"
BASE_URL = f"https://download.videolan.org/pub/videolan/vlc/{VERSION}/{ARCHITECTURE}/"
ARCHIVE_SHA256 = "8511356afd680817f3aea624c63032d0936f3d77b2175fb36c8fc16adf9744e8"
SOURCE_URL = f"https://download.videolan.org/pub/videolan/vlc/{VERSION}/vlc-{VERSION}.tar.xz"
SOURCE_SHA256 = "e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf"
LIBRARY_LICENSE_URL = f"https://raw.githubusercontent.com/videolan/vlc/{VERSION}/COPYING.LIB"
LIBRARY_LICENSE_SHA256 = "dc626520dcd53a22f727af3ee42c770e56c97a64fe3adb063799d8ab032fe551"
PLUGIN_GROUPS = {
    "access", "audio_filter", "audio_mixer", "audio_output", "codec", "demux",
    "keystore", "logger", "meta_engine", "misc", "packetizer", "spu", "stream_filter",
    "text_renderer", "video_chroma", "video_filter", "video_output", "visualization",
}


class OfficialRedirects(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, file_pointer, code, message, headers, new_url):
        parsed = urllib.parse.urlparse(new_url)
        if parsed.scheme != "https" or parsed.hostname != "download.videolan.org":
            raise RuntimeError("Refusing an archive redirect outside the official HTTPS host")
        return super().redirect_request(request, file_pointer, code, message, headers, new_url)


def request(url, headers=None, method=None):
    opener = urllib.request.build_opener(OfficialRedirects())
    return opener.open(urllib.request.Request(url, headers=headers or {}, method=method), timeout=60)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def verified_cache(cache):
    archive = cache / ARCHIVE_NAME
    verification_path = cache / "official-download-verification.json"
    license_path = cache / "COPYING.LIB"
    for path in (archive, verification_path, license_path):
        if not path.is_file() or path.is_symlink() or not path.resolve().is_relative_to(cache.resolve()):
            raise RuntimeError("Required verified video cache input is missing or linked: " + str(path))
    verification = json.loads(verification_path.read_text(encoding="utf-8"))
    identity = {
        "version": VERSION, "architecture": ARCHITECTURE,
        "archiveUrl": BASE_URL + ARCHIVE_NAME,
        "checksumUrl": BASE_URL + ARCHIVE_NAME + ".sha256",
        "sha256": ARCHIVE_SHA256, "verified": True,
        "checksumHttpStatus": 200, "archiveHttpStatus": 206,
        "archiveProbeRange": "bytes=0-63", "bytes": archive.stat().st_size,
    }
    if any(verification.get(key) != value for key, value in identity.items()):
        raise RuntimeError("Official video cache verification does not match the pinned release")
    if digest(archive) != ARCHIVE_SHA256:
        raise RuntimeError("Cached official video archive failed SHA-256 verification")
    if digest(license_path) != LIBRARY_LICENSE_SHA256:
        raise RuntimeError("Cached libVLC license failed SHA-256 verification")
    return archive, verification


def download_archive(cache, workers):
    cache.mkdir(parents=True, exist_ok=True)
    checksum_url = BASE_URL + ARCHIVE_NAME + ".sha256"
    with request(checksum_url) as response:
        checksum_status = response.status
        published = response.read().decode("ascii").strip()
    if published.split()[0].lower() != ARCHIVE_SHA256:
        raise RuntimeError("The official checksum no longer matches the pinned archive")
    archive = cache / ARCHIVE_NAME
    if not archive.is_file() or digest(archive) != ARCHIVE_SHA256:
        with request(BASE_URL + ARCHIVE_NAME, method="HEAD") as response:
            total = int(response.headers["Content-Length"])
        workers = max(1, min(workers, 8))
        chunk_size = (total + workers - 1) // workers

        def download_part(index):
            begin = index * chunk_size
            end = min(total - 1, begin + chunk_size - 1)
            destination = cache / f"{ARCHIVE_NAME}.part-{index}"
            if destination.is_file() and destination.stat().st_size == end - begin + 1:
                return destination
            with request(BASE_URL + ARCHIVE_NAME, {"Range": f"bytes={begin}-{end}"}) as response:
                if response.status != 206 or response.headers.get("Content-Range") != f"bytes {begin}-{end}/{total}":
                    raise RuntimeError("The official server did not honor the archive byte range")
                with destination.open("wb") as output:
                    shutil.copyfileobj(response, output, 1024 * 1024)
            if destination.stat().st_size != end - begin + 1:
                raise RuntimeError("Incomplete official archive range")
            return destination

        with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
            parts = list(pool.map(download_part, range(workers)))
        combined = cache / (ARCHIVE_NAME + ".verified-download")
        with combined.open("wb") as output:
            for part in parts:
                with part.open("rb") as source:
                    shutil.copyfileobj(source, output, 1024 * 1024)
        if digest(combined) != ARCHIVE_SHA256:
            raise RuntimeError("Downloaded archive failed the pinned and official SHA-256")
        combined.replace(archive)
        for part in parts:
            part.unlink()
    with request(BASE_URL + ARCHIVE_NAME, {"Range": "bytes=0-63"}) as response:
        http_status = response.status
        if http_status != 206:
            raise RuntimeError("The official archive probe did not return HTTP 206")
        with archive.open("rb") as stream:
            if response.read() != stream.read(64):
                raise RuntimeError("The official archive HTTP probe differs from the verified cache")
    verification = {
        "version": VERSION, "architecture": ARCHITECTURE,
        "archiveUrl": BASE_URL + ARCHIVE_NAME, "checksumUrl": checksum_url,
        "checksumHttpStatus": checksum_status, "archiveHttpStatus": http_status,
        "archiveProbeRange": "bytes=0-63",
        "bytes": archive.stat().st_size, "sha256": digest(archive),
        "verified": True,
    }
    (cache / "official-download-verification.json").write_text(
        json.dumps(verification, indent=2) + "\n", encoding="utf-8")
    return archive, verification


def is_x86_pe(data):
    if data[:2] != b"MZ" or len(data) < 64:
        return False
    offset = struct.unpack_from("<I", data, 60)[0]
    if offset + 6 > len(data):
        return False
    return data[offset:offset + 4] == b"PE\x00\x00" and struct.unpack_from("<H", data, offset + 4)[0] == 0x14C


def pe_imports(data):
    if not is_x86_pe(data):
        raise RuntimeError("Imports require a valid x86 PE")
    header = struct.unpack_from("<I", data, 60)[0]
    count = struct.unpack_from("<H", data, header + 6)[0]
    optional_size = struct.unpack_from("<H", data, header + 20)[0]
    optional = header + 24
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    sections = []
    for index in range(count):
        section = optional + optional_size + index * 40
        virtual_size, address, raw_size, raw_offset = struct.unpack_from("<IIII", data, section + 8)
        sections.append((address, max(virtual_size, raw_size), raw_offset))

    def offset(address):
        for start, length, raw in sections:
            if start <= address < start + length:
                return raw + address - start
        return address

    def name_at(address):
        begin = offset(address)
        end = data.index(b"\x00", begin)
        return data[begin:end].decode("ascii").lower()

    result = set()
    for directory, descriptor_size, name_field in ((1, 20, 12), (13, 32, 4)):
        directory_entry = optional + 96 + directory * 8
        address, length = struct.unpack_from("<II", data, directory_entry)
        if not address or not length:
            continue
        descriptor = offset(address)
        while descriptor + descriptor_size <= len(data) and any(data[descriptor:descriptor + descriptor_size]):
            name_address = struct.unpack_from("<I", data, descriptor + name_field)[0]
            if directory == 13 and not struct.unpack_from("<I", data, descriptor)[0] & 1:
                name_address -= image_base
            result.add(name_at(name_address))
            descriptor += descriptor_size
    return sorted(result)


def prepare_runtime(archive, destination, verification, offline=False):
    destination.mkdir(parents=True, exist_ok=True)
    files = []
    with zipfile.ZipFile(archive) as bundle:
        candidates = [entry for entry in bundle.infolist() if entry.filename.endswith("/libvlc.dll")]
        if len(candidates) != 1:
            raise RuntimeError("Cannot identify the unique libVLC root in the official archive")
        prefix = str(PurePosixPath(candidates[0].filename).parent) + "/"
        roots = {PurePosixPath(entry.filename).name.lower(): entry for entry in bundle.infolist()
                 if entry.filename.startswith(prefix) and "/" not in entry.filename[len(prefix):] and entry.filename.lower().endswith(".dll")}
        for entry in bundle.infolist():
            if entry.is_dir() or not entry.filename.startswith(prefix):
                continue
            relative = PurePosixPath(entry.filename[len(prefix):])
            if relative.is_absolute() or ".." in relative.parts or "\\" in str(relative) or ":" in str(relative):
                raise RuntimeError("Unsafe archive member")
            name = relative.name.lower()
            root_dll = len(relative.parts) == 1 and name in {"libvlc.dll", "libvlccore.dll"}
            license_file = len(relative.parts) == 1 and (
                name.startswith(("copying", "license")) or name in {"authors.txt", "thanks.txt", "readme.txt"})
            plugin = len(relative.parts) >= 3 and relative.parts[0] == "plugins" and relative.parts[1] in PLUGIN_GROUPS and name.endswith(".dll")
            if not (root_dll or license_file or plugin):
                continue
            data = bundle.read(entry)
            if (root_dll or plugin) and not is_x86_pe(data):
                raise RuntimeError(f"Runtime binary has the wrong architecture: {relative}")
            target = destination.joinpath(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            if target.is_file() and target.read_bytes() != data:
                raise RuntimeError(f"Refusing to overwrite a different runtime file: {target}")
            if not target.exists():
                target.write_bytes(data)
            files.append({"path": str(relative), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                          **({"imports": pe_imports(data)} if root_dll or plugin else {})})
        bundled = {entry["path"].lower() for entry in files}
        for entry in files:
            for dependency in entry.get("imports", []):
                if dependency in roots and dependency not in bundled:
                    data = bundle.read(roots[dependency])
                    if not is_x86_pe(data):
                        raise RuntimeError(f"Wrong architecture in required root dependency: {dependency}")
                    (destination / dependency).write_bytes(data)
                    files.append({"path": dependency, "bytes": len(data),
                                  "sha256": hashlib.sha256(data).hexdigest(), "imports": pe_imports(data)})
                    bundled.add(dependency)
    sources = {
        "archiveUrl": SOURCE_URL, "sha256": SOURCE_SHA256, "checksumUrl": SOURCE_URL + ".sha256",
        "libraryLicenseUrl": LIBRARY_LICENSE_URL,
        "thirdPartySourceIndex": "https://download.videolan.org/pub/videolan/contrib/",
    }
    if not offline:
        with request(SOURCE_URL + ".sha256") as response:
            if response.status != 200 or response.read().decode("ascii").split()[0] != SOURCE_SHA256:
                raise RuntimeError("Official corresponding source checksum did not match the fixed version")
    license_cache = archive.parent / "COPYING.LIB"
    if not license_cache.exists() or digest(license_cache) != LIBRARY_LICENSE_SHA256:
        if offline:
            raise RuntimeError("Verified cached libVLC license is required for offline staging")
        with request(LIBRARY_LICENSE_URL) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != LIBRARY_LICENSE_SHA256:
            raise RuntimeError("Official libVLC LGPL license checksum mismatch")
        license_cache.write_bytes(data)
    source_notice = (
        f"VideoLAN VLC/libVLC {VERSION}, official unmodified win32 binaries.\n"
        "Preserve COPYING.txt (VLC/plugin GPL terms), COPYING.LIB (libVLC LGPL 2.1 terms),\n"
        "AUTHORS.txt and all original notices. The application's existing GNU license remains applicable.\n"
        f"Corresponding VLC source: {SOURCE_URL}\nSHA-256: {SOURCE_SHA256}\n"
        f"Official source checksum: {SOURCE_URL}.sha256\n"
        f"libVLC license source: {LIBRARY_LICENSE_URL}\n"
        "Third-party source mirrors: https://download.videolan.org/pub/videolan/contrib/\n"
        "The VLC source archive's contrib/src build recipes identify the matching dependency sources.\n"
        "No standalone VLC player, Qt GUI plugin or browser plugin is required by this payload.\n"
    ).encode("utf-8")
    for name, data in (("COPYING.LIB", license_cache.read_bytes()), ("SOURCE-AND-LICENSES.txt", source_notice)):
        (destination / name).write_bytes(data)
        files.append({"path": name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    system_root = Path(os.environ.get("SystemRoot", "C:/Windows"))
    dependencies = {}
    bundled_names = {Path(entry["path"]).name.lower() for entry in files}
    for entry in files:
        for dependency in entry.get("imports", []):
            if dependency in bundled_names:
                dependencies[dependency] = "bundled"
            elif dependency.startswith(("api-ms-win-", "ext-ms-win-")):
                dependencies[dependency] = "windows-api-set"
            elif any((system_root / folder / dependency).is_file() for folder in ("SysWOW64", "System32")):
                dependencies[dependency] = "windows-system"
            else:
                raise RuntimeError(f"Required PE dependency is neither bundled nor a Windows system DLL: {dependency}")
    paths = {entry["path"] for entry in files}
    if not {"libvlc.dll", "libvlccore.dll"}.issubset(paths):
        raise RuntimeError("Required libVLC DLLs are missing")
    licenses = [entry["path"] for entry in files if entry["path"].lower().startswith(("copying", "license"))]
    if not licenses:
        raise RuntimeError("The official runtime license must be preserved")
    manifest = {
        "engine": "libVLC", **verification,
        "runtimeDirectory": "video-runtime",
        "pluginGroups": sorted(PLUGIN_GROUPS), "licenses": licenses, "sources": sources,
        "peDependencies": dict(sorted(dependencies.items())),
        "excluded": ["VLC standalone player", "Qt GUI plugins", "skins", "network discovery", "stream output"],
        "files": sorted(files, key=lambda entry: entry["path"]),
    }
    manifest_path = destination / "runtime-manifest.json"
    temporary_manifest = destination / "runtime-manifest.json.tmp"
    temporary_manifest.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary_manifest.replace(manifest_path)
    print(json.dumps({
        "destination": str(destination), "manifest": str(manifest_path),
        "version": VERSION, "archiveSha256": ARCHIVE_SHA256,
        "files": len(files), "bytes": sum(entry["bytes"] for entry in files),
        "licenses": licenses,
    }, ensure_ascii=False, indent=2))


def verify_runtime(destination):
    destination = destination.resolve()
    manifest_path = destination / "runtime-manifest.json"
    if not manifest_path.is_file() or manifest_path.is_symlink():
        raise RuntimeError("Video runtime manifest is missing or linked")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    identity = {
        "engine": "libVLC", "version": VERSION, "architecture": ARCHITECTURE,
        "sha256": ARCHIVE_SHA256, "verified": True, "runtimeDirectory": "video-runtime",
        "archiveUrl": BASE_URL + ARCHIVE_NAME,
    }
    if any(manifest.get(key) != value for key, value in identity.items()):
        raise RuntimeError("Video runtime manifest does not identify the pinned release")
    if manifest.get("sources", {}).get("sha256") != SOURCE_SHA256:
        raise RuntimeError("Video corresponding-source identity differs from the pinned release")
    entries = manifest.get("files")
    if not isinstance(entries, list) or not entries:
        raise RuntimeError("Video runtime inventory is empty")
    expected = {"runtime-manifest.json"}
    for entry in entries:
        relative = PurePosixPath(entry["path"])
        if relative.is_absolute() or ".." in relative.parts or "\\" in str(relative) or ":" in str(relative):
            raise RuntimeError("Unsafe video runtime inventory path")
        name = relative.as_posix()
        if name in expected:
            raise RuntimeError("Duplicate video runtime inventory path: " + name)
        expected.add(name)
        target = destination.joinpath(*relative.parts)
        if target.is_symlink() or not target.resolve().is_relative_to(destination) or not target.is_file():
            raise RuntimeError("Video runtime file is missing or linked: " + name)
        if target.stat().st_size != entry["bytes"] or digest(target) != entry["sha256"]:
            raise RuntimeError("Video runtime digest or size mismatch: " + name)
        if target.suffix.lower() == ".dll" and not is_x86_pe(target.read_bytes()):
            raise RuntimeError("Video runtime binary is not x86: " + name)
        if target.suffix.lower() == ".exe" or name.startswith("plugins/gui/") or target.name.lower().startswith("qt"):
            raise RuntimeError("External player or Qt GUI runtime is not allowed in video-runtime")
    actual = set()
    for path in destination.rglob("*"):
        if path.is_symlink() or not path.resolve().is_relative_to(destination):
            raise RuntimeError("Linked video runtime output is not allowed")
        if path.is_file():
            actual.add(path.relative_to(destination).as_posix())
    if actual != expected:
        raise RuntimeError("Video runtime contains missing or unexpected files")
    if not {"libvlc.dll", "libvlccore.dll", "COPYING.txt", "COPYING.LIB", "SOURCE-AND-LICENSES.txt"}.issubset(expected):
        raise RuntimeError("Video runtime DLLs or source/license notices are incomplete")
    if digest(destination / "COPYING.LIB") != LIBRARY_LICENSE_SHA256:
        raise RuntimeError("Video runtime libVLC license differs from the pinned license")
    return manifest


def main():
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Prepare the pinned official x86 libVLC runtime; never installs a player.")
    parser.add_argument("--cache", type=Path, default=repository / "dist" / "deps" / f"vlc-{VERSION}-{ARCHITECTURE}")
    parser.add_argument("--destination", type=Path, help="Runtime directory to populate; package it beside the executable as video-runtime/.")
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--offline", action="store_true", help="Stage only from the pinned archive and verified license already in cache.")
    arguments = parser.parse_args()
    cache = arguments.cache.resolve()
    destination = (arguments.destination or cache / "runtime").resolve()
    archive, verification = verified_cache(cache) if arguments.offline else download_archive(cache, arguments.workers)
    prepare_runtime(archive, destination, verification, offline=arguments.offline)
    verify_runtime(destination)


if __name__ == "__main__":
    main()
