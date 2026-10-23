#!/usr/bin/env python3
"""Fail the Switch build if the updater's bundled TLS trust anchors are absent or malformed."""
import re
import ssl
from pathlib import Path

bundle = Path("romfs/cacert.pem").read_bytes()
certificates = re.findall(rb"-----BEGIN CERTIFICATE-----.*?-----END CERTIFICATE-----", bundle, re.S)
if len(certificates) < 50 or any(not ssl.PEM_cert_to_DER_cert(cert.decode("ascii")) for cert in certificates):
    raise SystemExit("romfs/cacert.pem is missing or contains malformed CA certificates")
print(f"RomFS CA bundle: {len(certificates)} certificates, {len(bundle)} bytes")
