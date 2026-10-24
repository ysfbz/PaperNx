#!/usr/bin/env python3
"""Expose one build-time source identity to CMake, GNU make and build receipts."""
import argparse
import json
from pathlib import Path
from artifact_validation import source_identity

parser = argparse.ArgumentParser()
parser.add_argument("--root", type=Path, default=Path.cwd())
parser.add_argument("--format", choices=("json", "make"), default="json")
args = parser.parse_args()
identity = source_identity(args.root)
if args.format == "json":
    print(json.dumps(identity, sort_keys=True))
else:
    commit = identity["git_revision"]
    dirty = identity["dirty"]
    dirty_label = "unknown" if dirty == "unknown" else str(dirty).lower()
    values = {
        "PAPERNX_SOURCE_COMMIT": commit,
        "PAPERNX_SOURCE_DIRTY": dirty_label,
        "PAPERNX_SOURCE_TREE_SHA256": identity["source_tree_sha256"],
        "PAPERNX_SOURCE_IDENTITY": identity["source_identity"],
    }
    print(" ".join(f'-D{name}=\\"{value}\\"' for name, value in values.items()))
