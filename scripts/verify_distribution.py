#!/usr/bin/env python3
"""Verify the actual local release ZIP against its NRO, ELF, manifest and fonts."""
import hashlib
import json
import argparse
from pathlib import Path
from zipfile import ZipFile

from artifact_validation import validate_assets, read_romfs, validate_elf, source_identity
from package_release import validate_nro

if not __debug__:
    raise RuntimeError("Artifact checks require enabled assertions")

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--dist", type=Path, default=Path("dist/v0.9.1"))
parser.add_argument("--version", default="v0.9.1")
args = parser.parse_args()
manifest = json.loads((args.dist / "manifest.json").read_text(encoding="utf-8"))
assert Path(manifest["name"]).name == manifest["name"], "Invalid archive filename"
assert manifest["version"].removeprefix("v") == args.version.removeprefix("v")
archive = args.dist / manifest["name"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == manifest["sha256"]
nro = (args.dist / "PaperNX.nro").read_bytes()
assert nro == Path("PaperNX.nro").read_bytes()
assert hashlib.sha256(nro).hexdigest() == manifest["nro"]["sha256"]
receipt = manifest.get("build_receipt")
if receipt:
    identity = source_identity(Path.cwd())["implementation_sha256"]
    for stage in ("source_before", "source_after"):
        assert receipt[stage]["implementation_sha256"] == identity, "Source changed since build"
    for filename in ("PaperNX.nro", "PaperNX.elf"):
        assert hashlib.sha256(Path(filename).read_bytes()).hexdigest() == receipt["outputs"][filename]["sha256"], "Build output changed"
assets = validate_assets(nro, validate_nro(nro)["declared_size"])
version = nro[assets["nacp"]["offset"] + 0x3060:assets["nacp"]["offset"] + 0x3070].split(b"\0")[0].decode()
assert version == args.version.removeprefix("v"), version
files = read_romfs(nro, assets["romfs"])
for name in ("NotoSans-Regular.ttf", "NotoSans-SemiBold.ttf", "NotoSerif-Regular.ttf", "OFL.txt"):
    assert files["fonts/" + name] == (Path("fonts") / name).read_bytes()
with ZipFile(archive) as zipped:
    assert zipped.testzip() is None
    assert zipped.read("switch/PaperNX/PaperNX.nro") == nro
    assert not any(name.startswith("switch/PaperNX/docs/") for name in zipped.namelist()), "SD package contains dev docs"
    assert not any(name.startswith("switch/PaperNX/data/") for name in zipped.namelist()), "SD package contains data/ folder"
    assert not any(name.startswith(("switch/PaperNX/books/", "switch/PaperNX/logs/")) and not name.endswith("/") for name in zipped.namelist()), "Package overwrites user data"
    for name in ("NotoSans-Regular.ttf", "NotoSans-SemiBold.ttf", "NotoSerif-Regular.ttf", "OFL.txt"):
        assert zipped.read("switch/PaperNX/fonts/" + name) == (Path("fonts") / name).read_bytes()
    members = len(zipped.namelist())

# Verify separate licenses archive
lic_archive_name = manifest.get("licenses_archive", {}).get("name", f"PaperNX-licenses-{args.version}.zip")
lic_archive = args.dist / lic_archive_name
assert lic_archive.is_file(), f"Licenses archive missing: {lic_archive}"
with ZipFile(lic_archive) as lic_zip:
    assert lic_zip.testzip() is None
    assert "LICENSE" in lic_zip.namelist()
    notices = [n for n in lic_zip.namelist() if n.startswith("third-party-licenses/")]
    assert len(notices) >= 50

result = {
    "result": "PASS", "zip_sha256": manifest["sha256"],
    "nro_sha256": manifest["nro"]["sha256"], "nacp_version": version,
    "license_files": len(notices), "zip_members": members,
    "elf": validate_elf(Path("PaperNX.elf").read_bytes()), "hardware": "UNVERIFIED"
}
Path("verification").mkdir(exist_ok=True)
Path("verification/package-integrity.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
