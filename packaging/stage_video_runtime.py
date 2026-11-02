from pathlib import Path
import argparse
import json
import subprocess
import sys
from prepare_video_runtime import ARCHITECTURE, ARCHIVE_SHA256, VERSION, digest, verified_cache, verify_runtime


def stage_video_runtime(cache, payload):
    cache = Path(cache).resolve()
    payload = Path(payload).resolve()
    destination = payload / "video-runtime"
    if destination.exists():
        raise RuntimeError("Fresh video-runtime destination is required; preserve prior payloads")
    verified_cache(cache)
    payload.mkdir(parents=True, exist_ok=True)
    preparation = Path(__file__).with_name("prepare_video_runtime.py")
    subprocess.run([
        sys.executable, "-B", "-X", "utf8", str(preparation),
        "--offline", "--cache", str(cache), "--destination", str(destination),
    ], check=True)
    manifest = verify_runtime(destination)
    return {
        "engine": "libVLC", "version": VERSION, "architecture": ARCHITECTURE,
        "archiveSha256": ARCHIVE_SHA256, "runtimeDirectory": "video-runtime",
        "manifestSha256": digest(destination / "runtime-manifest.json"),
        "files": len(manifest["files"]), "bytes": sum(entry["bytes"] for entry in manifest["files"]),
        "peDependencies": manifest["peDependencies"], "licenses": manifest["licenses"],
        "sources": manifest["sources"], "preparationScriptSha256": digest(preparation),
        "offlineVerifiedCache": True,
    }


def main():
    repository = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description="Stage the pinned video runtime into a fresh payload without building the app or installer.")
    parser.add_argument("--cache", type=Path, default=repository / "dist/deps" / f"vlc-{VERSION}-{ARCHITECTURE}")
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    result = stage_video_runtime(arguments.cache, arguments.payload)
    if arguments.report:
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
