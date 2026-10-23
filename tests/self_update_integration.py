#!/usr/bin/env python3
"""Exercise the native updater against a local HTTPS server and valid NRO fixtures."""
import hashlib
import http.server
import json
import os
import pathlib
import shutil
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time


def make_nro(title="PaperNX", version="0.13.0"):
    declared = 0x80
    header = bytearray(declared)
    header[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", header, 0x18, declared)
    assets = bytearray(0x38)
    assets[:4] = b"ASET"
    struct.pack_into("<QQQQ", assets, 0x18, 0x38, 0x4000, 0x4038, 0x50)
    nacp = bytearray(0x4000)
    nacp[:len(title)] = title.encode("ascii")
    nacp[0x3060:0x3060 + len(version)] = version.encode("ascii")
    return bytes(header + assets + nacp + bytearray(0x50))


class UpdateServer(http.server.ThreadingHTTPServer):
    daemon_threads = True
    scenario = "success"
    current_nro = make_nro()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.0"

    def log_message(self, *_args):
        pass

    def do_GET(self):
        scenario = self.server.scenario
        tag = "v0.13.0"
        if scenario == "current":
            tag = "v0.12.0-reader.4"
        elif scenario == "downgrade":
            tag = "v0.11.0"

        payload = self.server.current_nro
        if scenario == "badapp":
            payload = make_nro("Not PaperNX")
        elif scenario == "badversion":
            payload = make_nro(version="0.12.0")

        if self.path == "/api":
            if scenario == "malformed":
                body = b"[{\"tag_name\":}"
            else:
                asset_url = f"https://localhost:{self.server.server_port}/download/{tag}/PaperNX.nro"
                assets = [{"name": "PaperNX.nro", "size": 256 * 1024 * 1024 + 1 if scenario == "oversized" else len(payload),
                           "browser_download_url": asset_url}]
                if scenario != "missing":
                    assets[0]["digest"] = "sha256:" + ("0" * 64 if scenario == "corrupt" else hashlib.sha256(payload).hexdigest())
                    checksum_url = f"https://localhost:{self.server.server_port}/download/{tag}/PaperNX.nro.sha256"
                    assets.append({"name": "PaperNX.nro.sha256", "browser_download_url": checksum_url})
                body = json.dumps([{"tag_name": tag, "name": "PaperNX test", "body": "Test notes",
                                    "draft": False, "prerelease": False, "assets": assets}]).encode()
            self.send_response(200)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if self.path.endswith("PaperNX.nro.sha256"):
            body = (hashlib.sha256(payload).hexdigest() + "  PaperNX.nro\n").encode()
        elif self.path.endswith("PaperNX.nro"):
            body = payload[:-64] if scenario == "truncated" else payload
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        try:
            if scenario == "cancel":
                for start in range(0, len(body), 128):
                    self.wfile.write(body[start:start + 128])
                    self.wfile.flush()
                    time.sleep(0.02)
            else:
                self.wfile.write(body)
        except OSError:
            pass


def main():
    binary = pathlib.Path(sys.argv[1]).resolve()
    openssl = shutil.which("openssl")
    if not binary.is_file() or not openssl:
        raise SystemExit("updater integration requires the built test executable and openssl")

    with tempfile.TemporaryDirectory(prefix="papernx-update-test-") as temporary:
        root = pathlib.Path(temporary)
        cert, key = root / "ca.pem", root / "server.key"
        subprocess.run([openssl, "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", str(key),
                        "-out", str(cert), "-days", "1", "-subj", "/CN=localhost",
                        "-addext", "subjectAltName=DNS:localhost",
                        "-addext", "basicConstraints=critical,CA:TRUE"],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

        server = UpdateServer(("127.0.0.1", 0), Handler)
        server.current_nro = make_nro()
        context = __import__("ssl").SSLContext(__import__("ssl").PROTOCOL_TLS_SERVER)
        context.load_cert_chain(str(cert), str(key))
        server.socket = context.wrap_socket(server.socket, server_side=True)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        base = f"https://localhost:{server.server_port}/"
        scenarios = ("current", "downgrade", "missing", "oversized", "success", "checksum-fallback",
                     "corrupt", "badapp", "badversion", "truncated", "cancel", "write-error",
                     "stale-part", "stale-part-error", "orphan-new", "resume-ready", "tampered", "missing-marker",
                     "malformed", "tls-reject", "http")
        try:
            for scenario in scenarios:
                server.scenario = scenario
                if scenario == "resume-ready":
                    executable = root / scenario / "custom folder" / "קורא.nro"
                    executable.parent.mkdir(parents=True)
                    payload = make_nro()
                    pathlib.Path(str(executable) + ".new").write_bytes(payload)
                    pathlib.Path(str(executable) + ".ready").write_text(
                        hashlib.sha256(payload).hexdigest() + "\n0.13.0\n" + str(len(payload)) + "\n",
                        encoding="ascii")
                if scenario == "checksum-fallback":
                    # Exercise the standalone checksum asset when GitHub omits digest metadata.
                    original = Handler.do_GET
                    def checksum_handler(self):
                        if self.path == "/api":
                            tag = "v0.13.0"
                            asset = f"https://localhost:{self.server.server_port}/download/{tag}/PaperNX.nro"
                            check = f"https://localhost:{self.server.server_port}/download/{tag}/PaperNX.nro.sha256"
                            body = json.dumps([{"tag_name": tag, "assets": [
                                {"name": "PaperNX.nro", "size": len(self.server.current_nro), "browser_download_url": asset},
                                {"name": "PaperNX.nro.sha256", "browser_download_url": check}]}]).encode()
                            self.send_response(200); self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
                        else:
                            original(self)
                    Handler.do_GET = checksum_handler
                try:
                    ca_arg = "" if scenario == "tls-reject" else str(cert)
                    scenario_base = "http://localhost/" if scenario == "http" else base
                    subprocess.run([str(binary), "--network", str(root / scenario), scenario_base, scenario, ca_arg],
                                   check=True, timeout=20)
                finally:
                    if scenario == "checksum-fallback":
                        Handler.do_GET = original
        finally:
            server.shutdown()
            thread.join(timeout=3)
            server.server_close()
    print(f"PASS: updater HTTPS integration ({len(scenarios)} scenarios)")


if __name__ == "__main__":
    main()
