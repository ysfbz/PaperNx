#!/usr/bin/env python3
"""
tests/test_package.py
Comprehensive packaging test suite:
- Synthetic valid NRO0 generation & validation
- Rejection of invalid magic (desktop ELF, random binary)
- Rejection of segment boundary violations & overlapping segments
- Verification of SD card directory layout upon unzipping
- Integrity of SHA256 file and manifest.json
"""

import hashlib
import json
import os
import struct
import tempfile
from pathlib import Path
from zipfile import ZipFile

# Add repo root to import path
import sys
sys.path.insert(0, str(Path(__file__).parent.parent))

from scripts.package_release import validate_nro, package
from scripts.artifact_validation import validate_assets, validate_elf

if not __debug__:
    raise RuntimeError("Packaging tests require enabled assertions")

def make_synthetic_nro(magic=b"NRO0", total_size=0x1000, seg_offsets=((0x80, 0x400), (0x480, 0x200), (0x680, 0x100))):
    buf = bytearray(total_size)
    # 0x00: Entrypoint branch
    struct.pack_into("<I", buf, 0, 0x14000020)
    # 0x10: Magic
    buf[0x10:0x14] = magic
    # 0x14: Version (0)
    struct.pack_into("<I", buf, 0x14, 0)
    # 0x18: Declared size
    struct.pack_into("<I", buf, 0x18, total_size)
    # 0x1C: Flags (0)
    struct.pack_into("<I", buf, 0x1C, 0)
    # 0x20: Segments (.text, .rodata, .data)
    for i, (off, sz) in enumerate(seg_offsets):
        struct.pack_into("<II", buf, 0x20 + 8 * i, off, sz)
    asset = bytearray(0x38 + 0x4000 + 0x50)
    struct.pack_into("<4sI6Q", asset, 0, b"ASET", 0, 0, 0, 0x38, 0x4000, 0x4038, 0x50)
    struct.pack_into("<Q", asset, 0x4038, 0x50)
    return bytes(buf + asset)

def run_tests():
    print("[TEST] Running PaperNX packaging validation tests...")
    
    from scripts.validate_branding import validate_icon
    custom_icon = Path("assets/branding/PaperNX-icon-switch.jpg")
    assert validate_icon(custom_icon)["components"] == 3
    with tempfile.TemporaryDirectory() as invalid:
        p = Path(invalid) / "invalid.jpg"
        p.write_bytes(b"invalid")
        try:
            validate_icon(p)
            assert False, "Invalid custom branding accepted"
        except ValueError:
            pass

    # 1. Valid NRO0 validation
    valid_nro = make_synthetic_nro()
    meta = validate_nro(valid_nro)
    assert meta["declared_size"] == 0x1000
    assert len(meta["segments"]) == 3
    assert meta["segments"][0]["name"] == ".text"
    assert meta["segments"][0]["size"] == 0x400
    assert validate_assets(valid_nro, meta["declared_size"])["nacp"]["size"] == 0x4000
    for image in (b"not an ELF", b"\x7fELF\x02\x01" + bytes(64)):
        try:
            validate_elf(image)
            assert False, "Invalid target architecture accepted"
        except ValueError:
            pass
    invalid_assets = bytearray(valid_nro)
    struct.pack_into("<Q", invalid_assets, 0x1000 + 0x30, 0xffffffffffffffff)
    try:
        validate_assets(invalid_assets, 0x1000)
        assert False, "Oversized RomFS accepted"
    except ValueError:
        pass

    # 2. Rejection of invalid magic (e.g. standard ELF header \x7fELF)
    elf_nro = make_synthetic_nro(magic=b"\x7fELF")
    try:
        validate_nro(elf_nro)
        assert False, "Should have rejected non-NRO0 magic"
    except ValueError as e:
        assert "expected 'NRO0'" in str(e)

    # 3. Rejection of oversized segment beyond declared image size
    overflow_nro = make_synthetic_nro(total_size=0x1000, seg_offsets=((0x80, 0x2000), (0, 0), (0, 0)))
    try:
        validate_nro(overflow_nro)
        assert False, "Should have rejected segment overflow"
    except ValueError as e:
        assert "exceeds declared image size" in str(e)

    # 4. Rejection of overlapping segments
    overlap_nro = make_synthetic_nro(total_size=0x1000, seg_offsets=((0x80, 0x500), (0x400, 0x200), (0x700, 0x100)))
    try:
        validate_nro(overlap_nro)
        assert False, "Should have rejected overlapping segments"
    except ValueError as e:
        assert "overlaps" in str(e)

    # 5. Full Package and Unzip SD verification
    with tempfile.TemporaryDirectory() as tmpdir:
        tmppath = Path(tmpdir)
        nro_file = tmppath / "PaperNX.nro"
        nro_file.write_bytes(valid_nro)

        dist_dir = tmppath / "dist"
        staged_licenses = tmppath / "staged-licenses"
        staged_licenses.mkdir()
        (staged_licenses / "example-notice.txt").write_text("license test notice", encoding="utf-8")
        archive, digest = package(nro_file, dist_dir, version="v0.1.0-test", repo_root=Path.cwd(), licenses_root=staged_licenses)
        
        assert archive.is_file()
        assert (dist_dir / f"{archive.name}.sha256").is_file()
        assert (dist_dir / "manifest.json").is_file()

        # Check SHA256 integrity
        actual_sha = hashlib.sha256(archive.read_bytes()).hexdigest()
        assert actual_sha == digest

        # Verify Manifest
        manifest = json.loads((dist_dir / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["name"] == archive.name
        assert manifest["sha256"] == actual_sha
        assert manifest["verified_format"] == "NRO0"
        assert manifest["hardware_verified"] is False
        assert "switch/PaperNX/PaperNX.nro" in manifest["contents"]
        license_archive = dist_dir / "PaperNX-licenses-v0.1.0-test.zip"
        assert license_archive.is_file()
        assert (dist_dir / "PaperNX.nro.sha256").is_file()
        with ZipFile(license_archive) as lic_zip:
            assert lic_zip.read("docs/LICENSES.md") == (Path.cwd() / "docs/LICENSES.md").read_bytes()
            assert lic_zip.read("docs/SOURCE_ATTRIBUTION.md") == (Path.cwd() / "docs/SOURCE_ATTRIBUTION.md").read_bytes()
            assert lic_zip.read("third-party-licenses/example-notice.txt") == b"license test notice"

        # Unpack archive and verify SD structure
        extract_dir = tmppath / "extracted_sd"
        preserved = extract_dir / "switch" / "PaperNX"
        for folder, name in (("books", "personal.epub"), ("data", "progress.txt"), ("logs", "papernx.log")):
            (preserved / folder).mkdir(parents=True, exist_ok=True)
            (preserved / folder / name).write_text("personal data: keep")
        with ZipFile(archive, "r") as z:
            assert sorted(z.namelist()) == sorted(manifest["contents"])
            assert not any(n.startswith("switch/PaperNX/docs/") for n in z.namelist())
            assert not any(n.startswith("switch/PaperNX/data/") for n in z.namelist())
            assert not any(n.startswith("switch/PaperNX/licenses/") for n in z.namelist())
            assert not any(n.startswith(("switch/PaperNX/books/", "switch/PaperNX/logs/")) and not n.endswith("/") for n in z.namelist())
            z.extractall(extract_dir)
        for folder, name in (("books", "personal.epub"), ("data", "progress.txt"), ("logs", "papernx.log")):
            assert (preserved / folder / name).read_text() == "personal data: keep"

        base = extract_dir / "switch" / "PaperNX"
        assert base.is_dir()
        assert (base / "PaperNX.nro").is_file()
        assert (base / "books").is_dir()
        assert (base / "fonts").is_dir()
        assert (base / "fonts" / "NotoSans-Regular.ttf").is_file()
        assert (base / "fonts" / "NotoSerif-Regular.ttf").is_file()
        assert (base / "fonts" / "OFL.txt").is_file()

    print("[PASS] Packaging and SD extraction test suite passed successfully!")

if __name__ == "__main__":
    run_tests()
