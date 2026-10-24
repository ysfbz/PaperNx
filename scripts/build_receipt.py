#!/usr/bin/env python3
"""Capture/verify source before and after an isolated Switch build."""
import argparse
import hashlib
import json
from pathlib import Path
from artifact_validation import source_identity, validate_elf

parser = argparse.ArgumentParser()
parser.add_argument('action', choices=['start', 'finish'])
parser.add_argument('--platform', choices=['desktop', 'switch'], default='switch')
parser.add_argument('--binary', type=Path, help='desktop executable path relative to --root')
parser.add_argument('--elf', type=Path, default=Path('PaperNX.elf'), help='Switch ELF path relative to --root')
parser.add_argument('--nro', type=Path, default=Path('PaperNX.nro'), help='Switch NRO path relative to --root')
parser.add_argument('--root', type=Path, default=Path('.'))
parser.add_argument('--receipt', type=Path, default=Path('verification/v04-switch-build-receipt.json'))
args = parser.parse_args()
identity = source_identity(args.root)
if args.action == 'start':
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps({'platform': args.platform, 'source_before': identity}, indent=2) + '\n')
else:
    receipt = json.loads(args.receipt.read_text())
    if receipt.get('platform') != args.platform or receipt['source_before']['source_identity'] != identity['source_identity']:
        raise SystemExit('Source changed during build; rebuild before packaging')
    outputs = {}
    artifacts = [args.binary] if args.platform == 'desktop' else [args.elf, args.nro]
    if args.platform == 'desktop' and args.binary is None:
        raise SystemExit('--binary is required for a desktop receipt')
    for artifact in artifacts:
        name = 'PaperNX.elf' if artifact.suffix == '.elf' else 'PaperNX.nro' if artifact.suffix == '.nro' else artifact.name
        data = (args.root / artifact).read_bytes()
        outputs[name] = {'path': artifact.as_posix(), 'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)}
        if args.platform == 'switch' and name.endswith('.elf'):
            outputs[name].update(validate_elf(data))
    receipt.update(source_after=identity, outputs=outputs)
    args.receipt.write_text(json.dumps(receipt, indent=2) + '\n')
print(args.receipt)
