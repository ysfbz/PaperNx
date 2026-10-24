#!/usr/bin/env python3
"""
scripts/package_release.py
Production-grade Nintendo Switch packaging and verification tool for PaperNX.
Validates NRO0 binary headers, segments, bundled font assets, licenses, and SD layout.
"""

import argparse
import hashlib
import json
import os
import struct
import sys
import re
import shutil
try:
    from .artifact_validation import validate_assets, source_identity
except ImportError:
    from artifact_validation import validate_assets, source_identity
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


def write_entry(archive, name, data):
    info = ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
    info.compress_type = ZIP_DEFLATED
    archive.writestr(info, data)

def validate_nro(image: bytes) -> dict:
    if len(image) < 0x80:
        raise ValueError(f"NRO candidate too small: {len(image)} bytes (minimum 128 bytes required)")
    
    magic = image[0x10:0x14]
    if magic != b"NRO0":
        raise ValueError(f"Invalid magic '{magic}'; expected 'NRO0' at offset 0x10. Desktop ELFs are not NROs!")

    version, declared_size = struct.unpack_from("<II", image, 0x14)
    if declared_size < 0x80 or declared_size > len(image):
        raise ValueError(f"Invalid declared NRO size {declared_size} (file size {len(image)})")

    # Segment headers: 3 pairs of (offset, size) for .text, .rodata, .data
    segments = []
    seg_names = [".text", ".rodata", ".data"]
    for i in range(3):
        offset, length = struct.unpack_from("<II", image, 0x20 + 8 * i)
        if offset > declared_size or length > declared_size - offset:
            raise ValueError(f"Segment {seg_names[i]} (offset {offset}, len {length}) exceeds declared image size {declared_size}")
        segments.append({"name": seg_names[i], "offset": offset, "size": length})

    if not any(s["size"] > 0 for s in segments):
        raise ValueError("NRO has no non-empty executable or data segments")

    spans = sorted((s["offset"], s["offset"] + s["size"]) for s in segments if s["size"])
    for left, right in zip(spans, spans[1:]):
        if left[1] > right[0]:
            raise ValueError("NRO segment overlaps another segment")

    return {
        "version": version,
        "declared_size": declared_size,
        "actual_size": len(image),
        "segments": segments
    }

def package(nro_path: Path, destination: Path, version: str, repo_root: Path = None, build_receipt: Path = None, licenses_root: Path = None) -> tuple[Path, str]:
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,63}", version):
        raise ValueError("Invalid release version")
    if not nro_path.is_file():
        raise FileNotFoundError(f"NRO file not found: {nro_path}")

    if repo_root is None:
        repo_root = Path.cwd()

    image = nro_path.read_bytes()
    receipt = None
    if build_receipt is not None:
        receipt = json.loads(build_receipt.read_text(encoding="utf-8"))
        identity = source_identity(repo_root)["implementation_sha256"]
        if any(receipt.get(key, {}).get("implementation_sha256") != identity
               for key in ("source_before", "source_after")):
            raise ValueError("Build receipt source differs from current implementation; rebuild")
        if receipt.get("outputs", {}).get("PaperNX.nro", {}).get("sha256") != hashlib.sha256(image).hexdigest():
            raise ValueError("NRO hash differs from verified build receipt")
    nro_meta = validate_nro(image)
    assets = validate_assets(image, nro_meta["declared_size"])

    destination.mkdir(parents=True, exist_ok=True)
    archive_name = f"PaperNX-switch-{version}.zip"
    archive_path = destination / archive_name

    fonts_dir = repo_root / "fonts"
    required_fonts = [
        "NotoSans-Regular.ttf", "NotoSans-SemiBold.ttf",
        "NotoSerif-Regular.ttf",
        "OFL.txt"
    ]

    for f in required_fonts:
        fpath = fonts_dir / f
        if not fpath.is_file():
            raise FileNotFoundError(f"Required bundled font asset missing: {fpath}")

    with ZipFile(archive_path, "w", ZIP_DEFLATED, compresslevel=9) as z:
        # Standard Minimal Switch SD Card directory structure:
        # sdmc:/switch/PaperNX/
        write_entry(z, "switch/PaperNX/books/", "")
        write_entry(z, "switch/PaperNX/fonts/", "")

        # Write NRO binary
        write_entry(z, "switch/PaperNX/PaperNX.nro", image)

        # Write Bundled Fonts & OFL
        for f in required_fonts:
            fpath = fonts_dir / f
            write_entry(z, f"switch/PaperNX/fonts/{f}", fpath.read_bytes())

        # Verify no path traversal in created archive
        for info in z.infolist():
            if info.filename.startswith("/") or ".." in info.filename:
                raise ValueError(f"Security error: dangerous relative path in archive '{info.filename}'")

    # Open-source compliance: package licenses and third-party notices into separate release asset
    licenses_archive_name = f"PaperNX-licenses-{version}.zip"
    licenses_archive_path = destination / licenses_archive_name
    with ZipFile(licenses_archive_path, "w", ZIP_DEFLATED, compresslevel=9) as lz:
        license_file = repo_root / "LICENSE"
        if license_file.is_file():
            write_entry(lz, "LICENSE", license_file.read_bytes())
        for relative in (Path("docs/LICENSES.md"), Path("docs/SOURCE_ATTRIBUTION.md")):
            notice = repo_root / relative
            if not notice.is_file():
                raise FileNotFoundError(f"Required project attribution is missing: {notice}")
            write_entry(lz, relative.as_posix(), notice.read_bytes())
        notices = Path(licenses_root) if licenses_root is not None else repo_root / "third-party-licenses"
        if notices.is_dir():
            for notice in sorted(notices.rglob("*")):
                if notice.is_file():
                    write_entry(lz, "third-party-licenses/" + notice.relative_to(notices).as_posix(), notice.read_bytes())
    lic_digest = hashlib.sha256(licenses_archive_path.read_bytes()).hexdigest()
    (destination / f"{licenses_archive_name}.sha256").write_text(f"{lic_digest}  {licenses_archive_name}\n", encoding="utf-8")

    with ZipFile(archive_path) as check:
        if check.testzip() is not None:
            raise ValueError("ZIP integrity validation failed")
        contents = check.namelist()
    if nro_path.resolve() != (destination / "PaperNX.nro").resolve():
        shutil.copyfile(nro_path, destination / "PaperNX.nro")
    archive_bytes = archive_path.read_bytes()
    digest = hashlib.sha256(archive_bytes).hexdigest()
    nro_digest = hashlib.sha256(image).hexdigest()

    # Write SHA256 checksum file
    sha_file = destination / f"{archive_name}.sha256"
    sha_file.write_text(f"{digest}  {archive_name}\n", encoding="utf-8")

    # Write detailed Manifest
    manifest_file = destination / "manifest.json"
    (destination / "PaperNX.nro.sha256").write_text(f"{nro_digest}  PaperNX.nro\n", encoding="utf-8")
    manifest_data = {
        "source": source_identity(repo_root),
        "build_receipt": receipt,
        "name": archive_name,
        "version": version,
        "sha256": digest,
        "licenses_archive": {
            "name": licenses_archive_name,
            "sha256": lic_digest
        },
        "nro": {
            "sha256": nro_digest,
            "declared_size": nro_meta["declared_size"],
            "actual_size": nro_meta["actual_size"],
            "segments": nro_meta["segments"],
            "assets": assets
        },
        "contents": contents,
        "sd_root_target": "sdmc:/",
        "verified_format": "NRO0",
        "hardware_verified": False
    }
    manifest_file.write_text(json.dumps(manifest_data, indent=2) + "\n", encoding="utf-8")

    return archive_path, digest

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Package PaperNX release ZIP for Nintendo Switch")
    parser.add_argument("nro", type=Path, help="Path to compiled PaperNX.nro")
    parser.add_argument("--destination", "--dist", type=Path, default=Path("dist/v0.9.2"), help="Output directory")
    parser.add_argument("--version", default="v0.9.2", help="Release version tag")
    parser.add_argument("--build-receipt", type=Path, help="Validate pre/post-build source and NRO hash against this receipt")
    parser.add_argument("--repo-root", type=Path, default=Path("."), help="Path to repository root")
    parser.add_argument("--licenses-root", type=Path, help="Staged third-party notices directory")

    args = parser.parse_args()
    try:
        archive, sha = package(args.nro, args.destination, args.version, args.repo_root, args.build_receipt, args.licenses_root)
        print(f"[OK] Release package created: {archive}")
        print(f"[OK] SHA256: {sha}")
    except Exception as ex:
        print(f"[ERROR] Packaging failed: {ex}", file=sys.stderr)
        sys.exit(1)
