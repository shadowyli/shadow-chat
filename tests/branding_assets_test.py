import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET

from PIL import Image, __version__ as pillow_version


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "src/images/shadowchat"
SOURCE = ASSETS / "shadowchat-unread-source.png"
ICON = ASSETS / "shadowchat-unread.ico"
MANIFEST = ASSETS / "shadowchat-unread-manifest.json"
SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
SOURCE_SHA256 = "62b0336451868dbc018a7b857ace7a39bad6971d88c24b5a5c3a5f87a67235f2"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def frame(size):
    with Image.open(SOURCE) as image:
        return image.convert("RGBA").resize((size, size), Image.Resampling.LANCZOS)


def generate():
    assert digest(SOURCE) == SOURCE_SHA256
    directory = bytearray(struct.pack("<HHH", 0, 1, len(SIZES)))
    payloads = []
    offset = 6 + 16 * len(SIZES)
    for size in SIZES:
        stream = io.BytesIO()
        frame(size).save(stream, "PNG", optimize=True, compress_level=9)
        payload = stream.getvalue()
        directory.extend(struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0,
                                     1, 32, len(payload), offset))
        payloads.append(payload)
        offset += len(payload)
    ICON.write_bytes(bytes(directory) + b"".join(payloads))
    manifest = {
        "source": "src/images/shadowchat/shadowchat-unread-source.png",
        "source_sha256": SOURCE_SHA256,
        "source_dimensions": [1080, 1080],
        "operations": "Byte-preserved source; direct RGBA LANCZOS resize from source for each ICO frame; lossless PNG encoding only; no crop, redraw or recolor",
        "pillow_version": pillow_version,
        "ico_sizes": list(SIZES),
        "ico_sha256": digest(ICON),
    }
    MANIFEST.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                        encoding="utf-8")


def verify():
    assert digest(SOURCE) == SOURCE_SHA256, "Owner source bytes changed"
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["ico_sha256"] == digest(ICON)
    with Image.open(SOURCE) as image:
        assert image.size == (1080, 1080) and image.mode == "RGBA"
    with Image.open(ICON) as icon:
        assert icon.ico.sizes() == {(size, size) for size in SIZES}
        for size in SIZES:
            actual = icon.ico.getimage((size, size)).convert("RGBA")
            assert actual.tobytes() == frame(size).tobytes(), size
    resources = ET.parse(ROOT / "src/beebeep.qrc").getroot()
    aliases = {entry.get("alias", entry.text): entry.text
               for entry in resources.iter("file")}
    for name in ("beebeep.png", "beebeep-tray.png"):
        assert aliases["images/" + name] == "images/shadowchat/shadowchat-mark-source.png"
    for name in ("beebeep-message.png", "beebeep-message-tray.png"):
        assert aliases["images/" + name] == "images/shadowchat/shadowchat-unread-source.png"
    assert aliases["images/shadowchat/shadowchat-app.ico"] == "beebeep.ico"
    assert aliases["images/shadowchat/shadowchat-unread.ico"] == "images/shadowchat/shadowchat-unread.ico"
    with Image.open(ROOT / "src/beebeep.ico") as normal:
        with Image.open(ASSETS / "shadowchat-mark-source.png") as mark:
            assert normal.ico.getimage((256, 256)).convert("RGBA").tobytes() == mark.convert("RGBA").tobytes()
    tray = (ROOT / "src/desktop/GuiSystemTray.cpp").read_text(encoding="utf-8")
    tray_header = (ROOT / "src/desktop/GuiSystemTray.h").read_text(encoding="utf-8")
    for forbidden in ("QTimer", "startTimer(", "timerEvent(", "FlashWindow", "blinkIcon", "flashIcon"):
        assert forbidden not in tray + tray_header, forbidden
    assert '"beebeep' not in tray, "Tray still explicitly requests old BeeBEEP artwork"
    print("PASS owner source SHA-256; 9 direct-resize ICO frames pixel-exact; normal red ICO; legacy QRC aliases; no tray blink timer")


def verify_executable(path):
    binary = path.read_bytes()
    pe_offset = struct.unpack_from("<I", binary, 0x3C)[0]
    assert binary[pe_offset:pe_offset + 4] == b"PE\0\0"
    section_count = struct.unpack_from("<H", binary, pe_offset + 6)[0]
    optional_size = struct.unpack_from("<H", binary, pe_offset + 20)[0]
    optional = pe_offset + 24
    magic = struct.unpack_from("<H", binary, optional)[0]
    directories = optional + (96 if magic == 0x10B else 112)
    resource_rva = struct.unpack_from("<I", binary, directories + 16)[0]
    section_table = optional + optional_size

    def file_offset(rva):
        for index in range(section_count):
            position = section_table + index * 40
            virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
                "<IIII", binary, position + 8)
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_offset + rva - virtual_address
        raise AssertionError("Resource RVA outside executable sections")

    resource_root = file_offset(resource_rva)
    resources = {}

    def walk(offset, ancestry=()):
        named, identifiers = struct.unpack_from("<HH", binary, resource_root + offset + 12)
        for index in range(named + identifiers):
            identity, child = struct.unpack_from("<II", binary, resource_root + offset + 16 + index * 8)
            key = ancestry + (identity,)
            if child & 0x80000000:
                walk(child & 0x7FFFFFFF, key)
            else:
                rva, length = struct.unpack_from("<II", binary, resource_root + child)
                position = file_offset(rva)
                resources[key] = binary[position:position + length]

    walk(0)
    normal = (ROOT / "src/beebeep.ico").read_bytes()
    count = struct.unpack_from("<H", normal, 4)[0]
    expected = set()
    for index in range(count):
        length, offset = struct.unpack_from("<II", normal, 6 + index * 16 + 8)
        expected.add(hashlib.sha256(normal[offset:offset + length]).digest())
    matching_groups = []
    for key, group in resources.items():
        if key[0] != 14:
            continue
        actual = set()
        frame_count = struct.unpack_from("<H", group, 4)[0]
        for index in range(frame_count):
            identity = struct.unpack_from("<H", group, 6 + index * 14 + 12)[0]
            actual.update(hashlib.sha256(payload).digest()
                          for ancestry, payload in resources.items()
                          if ancestry[0] == 3 and ancestry[1] == identity)
        if actual == expected:
            matching_groups.append(key)
    assert matching_groups, "Executable PE icon differs from the current red branding"
    print(f"PASS executable PE resources match all {count} red application ICO frames: {path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--generate", action="store_true")
    parser.add_argument("--executable", type=Path)
    arguments = parser.parse_args()
    if arguments.generate:
        generate()
    verify()
    if arguments.executable:
        verify_executable(arguments.executable)
