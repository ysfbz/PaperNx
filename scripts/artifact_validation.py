"""Structural Switch artifact checks; these do not establish hardware compatibility."""
import struct
import hashlib
import re
import subprocess
from pathlib import Path


def source_identity(root):
    root = Path(root)
    entries = []
    for folder in ("src", "include", "third_party", "assets", "fonts", "romfs"):
        for path in sorted((root / folder).rglob("*")):
            if path.is_file():
                entries.append((path.relative_to(root).as_posix(), hashlib.sha256(path.read_bytes()).hexdigest()))
    for name in ("Makefile", "CMakeLists.txt", "Dockerfile", "scripts/build-mupdf-switch.sh",
                 "scripts/artifact_validation.py", "scripts/build_receipt.py", "scripts/source_identity.py"):
        path = root / name
        if path.is_file():
            entries.append((name, hashlib.sha256(path.read_bytes()).hexdigest()))
    digest = hashlib.sha256("".join(f"{name}\t{sha}\n" for name, sha in sorted(entries)).encode()).hexdigest()
    revision = "unknown"
    dirty = "unknown"
    safe_root = root.resolve().as_posix()
    try:
        git = ["git", "-c", f"safe.directory={safe_root}"]
        revision = subprocess.check_output(git + ["rev-parse", "--verify", "HEAD"], cwd=root, stderr=subprocess.DEVNULL, text=True).strip()
        if not re.fullmatch(r"[0-9a-fA-F]{40}|[0-9a-fA-F]{64}", revision):
            revision = "unknown"
            raise subprocess.CalledProcessError(1, git + ["rev-parse", "--verify", "HEAD"])
        status = subprocess.check_output(git + ["status", "--porcelain=v1", "--untracked-files=normal"], cwd=root, stderr=subprocess.DEVNULL, text=True)
        dirty = bool(status.strip())
    except (OSError, subprocess.CalledProcessError):
        pass
    identity = f"git:{revision}:dirty:{str(dirty).lower()}:tree:{digest}" if revision != "unknown" else f"archive-sha256:{digest}"
    return {"git_revision": revision, "dirty": dirty, "implementation_sha256": digest,
            "source_tree_sha256": digest, "source_identity": identity}


def validate_assets(image, declared_size):
    base = declared_size
    if base + 0x38 > len(image) or image[base:base + 4] != b"ASET":
        raise ValueError("NRO is missing its ASET header and RomFS/NACP assets")
    version = struct.unpack_from("<I", image, base + 4)[0]
    if version != 0:
        raise ValueError("Unsupported ASET version")
    assets = {}
    spans = []
    for i, name in enumerate(("icon", "nacp", "romfs")):
        offset, size = struct.unpack_from("<QQ", image, base + 8 + i * 16)
        if size:
            if offset < 0x38 or offset > len(image) - base or size > len(image) - base - offset:
                raise ValueError(f"Invalid {name} asset boundaries")
            spans.append((offset, offset + size))
        assets[name] = {"offset": base + offset, "size": size}
    for left, right in zip(sorted(spans), sorted(spans)[1:]):
        if left[1] > right[0]:
            raise ValueError("ASET assets overlap")
    if assets["nacp"]["size"] != 0x4000 or assets["romfs"]["size"] < 0x50:
        raise ValueError("NACP or RomFS is missing or truncated")
    pos = assets["romfs"]["offset"]
    if struct.unpack_from("<Q", image, pos)[0] != 0x50:
        raise ValueError("Invalid RomFS header")
    return assets


def read_romfs(image, asset):
    romfs = memoryview(image)[asset["offset"]:asset["offset"] + asset["size"]]
    header, dh_off, dh_size, dm_off, dm_size, fh_off, fh_size, fm_off, fm_size, data_off = struct.unpack_from("<10Q", romfs)
    if header != 0x50 or data_off > len(romfs):
        raise ValueError("Invalid RomFS layout")
    for offset, size in ((dh_off, dh_size), (dm_off, dm_size), (fh_off, fh_size), (fm_off, fm_size)):
        if offset > len(romfs) or size > len(romfs) - offset:
            raise ValueError("RomFS metadata outside image")
    files = {}
    seen_dirs, seen_files = set(), set()
    def directory(offset, parent, depth):
        if depth > 32 or offset in seen_dirs or offset + 24 > dm_size:
            raise ValueError("Invalid or cyclic RomFS directory")
        seen_dirs.add(offset)
        _, sibling, child_dir, child_file, _, length = struct.unpack_from("<6I", romfs, dm_off + offset)
        if length > dm_size - offset - 24:
            raise ValueError("Invalid RomFS directory name")
        name = bytes(romfs[dm_off + offset + 24:dm_off + offset + 24 + length]).decode("utf-8")
        if name in (".", "..") or "/" in name or "\\" in name:
            raise ValueError("Unsafe RomFS directory name")
        path = f"{parent}/{name}".strip("/")
        child = child_file
        while child != 0xffffffff:
            if child in seen_files or child + 32 > fm_size:
                raise ValueError("Invalid or cyclic RomFS file")
            seen_files.add(child)
            _, next_file, start, size, _, length = struct.unpack_from("<IIQQII", romfs, fm_off + child)
            if length > fm_size - child - 32 or start > len(romfs) - data_off or size > len(romfs) - data_off - start:
                raise ValueError("RomFS file outside image")
            filename = bytes(romfs[fm_off + child + 32:fm_off + child + 32 + length]).decode("utf-8")
            if not filename or filename in (".", "..") or "/" in filename or "\\" in filename:
                raise ValueError("Unsafe RomFS file name")
            files[f"{path}/{filename}".strip("/")] = bytes(romfs[data_off + start:data_off + start + size])
            child = next_file
        child = child_dir
        while child != 0xffffffff:
            child = directory(child, path, depth + 1)
        return sibling
    directory(0, "", 0)
    return files


def validate_elf(image):
    if len(image) < 64 or image[:6] != b"\x7fELF\x02\x01":
        raise ValueError("Expected 64-bit little-endian ELF")
    kind, machine = struct.unpack_from("<HH", image, 16)
    if machine != 183 or kind not in (2, 3):
        raise ValueError("ELF is not an AArch64 executable")
    return {"architecture": "AArch64", "machine": machine, "type": kind}
