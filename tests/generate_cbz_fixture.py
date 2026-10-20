#!/usr/bin/env python3
"""Synthetic two-page CBZ fixture; no copyrighted artwork or external packages."""
from pathlib import Path
import struct
import zipfile
import zlib

TARGET = Path(__file__).resolve().parent / "fixtures" / "generated"


def png(rgb: tuple[int, int, int], width: int = 32, height: int = 48) -> bytes:
    def chunk(kind: bytes, body: bytes) -> bytes:
        return (struct.pack(">I", len(body)) + kind + body
                + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    raw = (b"\x00" + bytes(rgb) * width) * height
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw))
            + chunk(b"IEND", b""))


def main() -> None:
    TARGET.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(TARGET / "two-pages.cbz", "w", compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr("pages/Page2.png", png((240, 40, 32)))
        z.writestr("pages/Page10.png", png((30, 60, 220)))
        z.writestr("ComicInfo.xml", "<ComicInfo><Title>Fixture</Title><Series>Test Series</Series><Number>2</Number></ComicInfo>")
    (TARGET / "corrupt.cbz").write_bytes(b"PK\x03\x04truncated-archive")


if __name__ == "__main__":
    main()
