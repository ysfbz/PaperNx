#!/usr/bin/env python3
"""Collect upstream license texts from the exact MuPDF source and Switch SDK."""
from pathlib import Path
import shutil
import sys

source = Path(sys.argv[1])
destination = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("third-party-licenses")
if not (source / "COPYING").is_file():
    raise SystemExit("MuPDF source/COPYING is required")
count = 0
for folder in (source, Path("/opt/devkitpro/portlibs/switch/licenses"), Path("/opt/devkitpro/libnx")):
    if not folder.is_dir():
        continue
    candidates = folder.rglob("*") if folder == source or folder.name == "licenses" else folder.glob("*")
    for path in sorted(candidates):
        if not path.is_file() or path.stat().st_size > 1024 * 1024:
            continue
        if folder == source and "build" in path.relative_to(folder).parts:
            continue
        if folder.name != "licenses" and not path.name.lower().startswith(("license", "copying", "copyright", "notice", "ofl")):
            continue
        label = "MuPDF-1.23.11" if folder == source else "Switch-portlibs" if folder.name == "licenses" else "libnx"
        target = destination / label / path.relative_to(folder)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(path, target)
        count += 1
(destination / "SOURCE.txt").write_text(
    "PaperNX source and build recipes: https://github.com/ysfbz/PaperNx\n"
    "MuPDF 1.23.11 source: https://mupdf.com/downloads/archive/mupdf-1.23.11-source.tar.gz\n"
    "MuPDF SHA256: 478f2a167feae2a291c8b8bc5205f2ce2f09f09b574a6eb0525bfad95a3cfe66\n"
    "unarr RAR decoder and local changes: https://github.com/ysfbz/PaperNx/tree/main/third_party/unarr\n"
    "Switch dependency recipes: https://github.com/devkitPro/pacman-packages\n"
    "Full AGPL corresponding source and any required dependency source must accompany public redistribution.\n",
    encoding="utf-8")
print(f"Collected {count} license/notice files")
