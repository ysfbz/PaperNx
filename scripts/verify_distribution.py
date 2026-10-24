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
parser.add_argument("--dist", type=Path, default=Path("dist/v0.9.2"))
parser.add_argument("--version", default="v0.9.2")
parser.add_argument("--licenses-root", type=Path, default=Path("third-party-licenses"))
parser.add_argument("--nro", type=Path, default=Path("PaperNX.nro"))
parser.add_argument("--elf", type=Path, default=Path("PaperNX.elf"))
parser.add_argument("--evidence", type=Path, default=Path("verification/package-integrity.json"))
args = parser.parse_args()

def assert_sha_sidecar(path: Path, filename: str, expected: str) -> None:
    lines = path.read_text(encoding="utf-8").strip().split()
    assert len(lines) == 2 and lines[1] == filename and lines[0] == expected, f"Invalid SHA-256 sidecar: {path}"

manifest = json.loads((args.dist / "manifest.json").read_text(encoding="utf-8"))
assert Path(manifest["name"]).name == manifest["name"], "Invalid archive filename"
assert manifest["version"].removeprefix("v") == args.version.removeprefix("v")
archive = args.dist / manifest["name"]
assert hashlib.sha256(archive.read_bytes()).hexdigest() == manifest["sha256"]
nro = (args.dist / "PaperNX.nro").read_bytes()
assert nro == args.nro.read_bytes()
assert hashlib.sha256(nro).hexdigest() == manifest["nro"]["sha256"]
receipt = manifest.get("build_receipt")
if receipt:
    identity = source_identity(Path.cwd())
    for stage in ("source_before", "source_after"):
        assert receipt[stage]["source_identity"] == identity["source_identity"], "Build receipt source identity differs from current source"
    assert manifest["source"]["source_identity"] == identity["source_identity"], "Package manifest source identity differs from current source"
    for filename, path in (("PaperNX.nro", args.nro), ("PaperNX.elf", args.elf)):
        assert hashlib.sha256(path.read_bytes()).hexdigest() == receipt["outputs"][filename]["sha256"], "Build output changed"
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
assert hashlib.sha256(lic_archive.read_bytes()).hexdigest() == manifest["licenses_archive"]["sha256"], "License archive hash differs from manifest"
with ZipFile(lic_archive) as lic_zip:
    assert lic_zip.testzip() is None
    assert "LICENSE" in lic_zip.namelist()
    assert "docs/LICENSES.md" in lic_zip.namelist()
    assert "docs/SOURCE_ATTRIBUTION.md" in lic_zip.namelist()
    assert lic_zip.read("docs/LICENSES.md") == Path("docs/LICENSES.md").read_bytes()
    assert lic_zip.read("docs/SOURCE_ATTRIBUTION.md") == Path("docs/SOURCE_ATTRIBUTION.md").read_bytes()
    notices = [n for n in lic_zip.namelist() if n.startswith("third-party-licenses/")]
    assert len(notices) >= 50
    expected_notices = {
        "third-party-licenses/" + path.relative_to(args.licenses_root).as_posix()
        for path in args.licenses_root.rglob("*") if path.is_file()
    }
    assert expected_notices.issubset(set(lic_zip.namelist())), "License archive omitted staged dependency notices"
    required_notice_paths = (
        "COPYING", "SOURCE.txt", "unarr/COPYING", "unarr/COPYING.GPLv3", "unarr/AUTHORS", "unarr/SOURCE.md"
    )
    assert all(any(name.endswith(required) for name in notices) for required in required_notice_paths), "Required dependency attribution missing"

assert_sha_sidecar(args.dist / "PaperNX.nro.sha256", "PaperNX.nro", manifest["nro"]["sha256"])
assert_sha_sidecar(args.dist / f"{manifest['name']}.sha256", manifest["name"], manifest["sha256"])
assert_sha_sidecar(args.dist / f"{lic_archive_name}.sha256", lic_archive_name, manifest["licenses_archive"]["sha256"])

result = {
    "result": "PASS", "zip_sha256": manifest["sha256"],
    "nro_sha256": manifest["nro"]["sha256"], "nacp_version": version,
    "license_files": len(notices), "zip_members": members,
    "elf": validate_elf(args.elf.read_bytes()), "hardware": "UNVERIFIED"
}
args.evidence.parent.mkdir(parents=True, exist_ok=True)
args.evidence.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2))
