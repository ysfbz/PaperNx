#!/usr/bin/env python3
import hashlib
import json
import argparse
from pathlib import Path
from validate_branding import validate_icon
from artifact_validation import validate_assets, read_romfs, validate_elf, source_identity
from package_release import validate_nro

parser = argparse.ArgumentParser(description="Validate the real Switch ELF/NRO and embedded assets")
parser.add_argument("--version", default="0.9.1")
args = parser.parse_args()
assert __debug__, "Artifact checks require enabled assertions"
elf = Path("PaperNX.elf").read_bytes()
nro = Path("PaperNX.nro").read_bytes()
meta = validate_nro(nro)
assets = validate_assets(nro, meta["declared_size"])
icon_meta = validate_icon("assets/branding/PaperNX-icon-switch.jpg")
icon = assets["icon"]
assert nro[icon["offset"]:icon["offset"] + icon["size"]] == Path("assets/branding/PaperNX-icon-switch.jpg").read_bytes(), "Embedded icon differs"
nacp = assets["nacp"]
nacp_data = nro[nacp["offset"]:nacp["offset"] + nacp["size"]]
assert nacp_data[:512].split(b"\0")[0] == b"PaperNX"
assert nacp_data[512:768].split(b"\0")[0] == b"Yusuf Buzrul"
assert nacp_data[0x3060:0x3070].split(b"\0")[0].decode() == args.version.removeprefix("v"), "NACP version mismatch"
files = read_romfs(nro, assets["romfs"])
for name in ("NotoSans-Regular.ttf", "NotoSans-SemiBold.ttf", "NotoSerif-Regular.ttf", "OFL.txt"):
    assert files["fonts/" + name] == (Path("fonts") / name).read_bytes(), name
result = {"source": source_identity(Path.cwd()), "elf": validate_elf(elf), "nro": meta, "assets": assets, "icon": icon_meta, "embedded_icon_identical": True,
          "romfs_files": sorted(files), "nro_sha256": hashlib.sha256(nro).hexdigest(),
          "hardware": "UNVERIFIED"}
Path("verification").mkdir(exist_ok=True)
Path("verification/artifacts.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
