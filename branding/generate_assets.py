"""Technical resizing/transcoding of the owner-supplied logo; no artwork edits.

Run from anywhere: python branding/generate_assets.py [source.png]
The optional input replaces the archived source; otherwise regenerate from it.
Requires Pillow. All outputs are deterministic for a given Pillow version.
"""
from pathlib import Path
import hashlib
import json
import shutil
import sys
from PIL import Image, __version__ as pillow_version

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "branding/source/logo.png"
OUT = ROOT / "branding/generated"

def main():
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    if len(sys.argv) > 1:
        incoming = Path(sys.argv[1]).resolve()
        if incoming != SOURCE.resolve():
            shutil.copyfile(incoming, SOURCE)
    with Image.open(SOURCE) as original:
        original.load()
        image = original.convert("RGBA")
        original_size = image.size
    # Preserve the entire supplied artwork, including its existing padding.
    # Non-square future sources are letterboxed, never cropped or stretched.
    def resized(size):
        target = Image.new("RGBA", (size,size), (0,0,0,0))
        scaled = image.copy()
        scaled.thumbnail((size,size), Image.Resampling.LANCZOS)
        target.alpha_composite(scaled, ((size-scaled.width)//2,(size-scaled.height)//2))
        return target
    png_sizes = (16,32,48,64,128,256,512)
    for size in png_sizes:
        resized(size).save(OUT/f"shadowchat-{size}.png", optimize=True, compress_level=9)
    resized(512).save(OUT/"shadowchat-512.webp", lossless=True, exact=True, method=6)
    icon_sizes = (16,20,24,32,40,48,64,128,256)
    resized(256).save(OUT/"app.ico", sizes=[(n,n) for n in icon_sizes],
                      append_images=[resized(n) for n in icon_sizes if n!=256])
    for destination in (ROOT/"src/beebeep.ico", ROOT/"packaging/shadowchat.ico"):
        shutil.copyfile(OUT/"app.ico",destination)
    # qmake's MSVC resource rule tracks .rc, but not its referenced .ico.
    # Refresh the resource timestamp so an incremental build embeds this icon.
    (ROOT/"src/beebeep.rc").touch()
    for destination in (ROOT/"src/images/beebeep.png",ROOT/"src/images/shadowchat/shadowchat-mark-source.png"):
        shutil.copyfile(OUT/"shadowchat-256.png",destination)
    files = {str(path.relative_to(ROOT)).replace("\\","/"):{"bytes":path.stat().st_size,"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
             for path in [SOURCE,*sorted(OUT.iterdir())] if path.name!="manifest.json"}
    manifest={"source_provenance":"Owner supplied E:/ShadowChat/logo.png, 2026-10-28",
              "source_dimensions":original_size,"operations":"RGBA conversion, preserve composition, LANCZOS resize, PNG/WebP/ICO encoding only",
              "pillow_version":pillow_version,"ico_sizes":icon_sizes,"files":files}
    (OUT/"manifest.json").write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    with Image.open(OUT/"app.ico") as icon:
        assert icon.ico.sizes()=={(n,n) for n in icon_sizes},icon.ico.sizes()
    for size in png_sizes:
        with Image.open(OUT/f"shadowchat-{size}.png") as png:
            assert png.size==(size,size)
            png.verify()
    with Image.open(OUT/"shadowchat-512.webp") as webp:
        assert webp.size==(512,512)
        assert webp.convert("RGBA").tobytes()==resized(512).tobytes()
    print("PASS source preserved; 9 ICO frames; 7 optimized PNGs; lossless WebP; client and installer aliases updated")

if __name__=="__main__":
    main()
