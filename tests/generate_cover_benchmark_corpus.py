#!/usr/bin/env python3
"""Generate a deterministic, copyright-safe EPUB/PDF/CBZ workload; reuse only checked-in synthetic CBRs."""
import argparse
import hashlib
import json
import struct
import zlib
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RAR_FIXTURES = [
    ROOT / "tests/fixtures/comics/sample-rar4.cbr",
    ROOT / "tests/fixtures/comics/sample-rar4-solid.cbr",
    ROOT / "tests/fixtures/comics/sample-rar5.cbr",
    ROOT / "tests/fixtures/comics/sample-rar5-solid.cbr",
    ROOT / "tests/fixtures/comics/large-cover-rar4-solid.cbr",
]


def raw_rgb(width, height, seed):
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels.extend(((x * 3 + y + seed * 17) & 255,
                           (y * 5 + x // 2 + seed * 31) & 255,
                           ((x ^ y) + seed * 47) & 255))
    return bytes(pixels)


def png_rgb(width, height, seed):
    pixels = raw_rgb(width, height, seed)
    rows = b"".join(b"\0" + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows, 6)) + chunk(b"IEND", b""))


def cover(seed):
    if seed % 31 == 0:
        size = (640, 900)
    elif seed % 7 == 0:
        size = (320, 480)
    else:
        size = (72 + seed % 6 * 24, 100 + seed % 5 * 30)
    return size, png_rgb(*size, seed)


def make_epub(path, index, with_cover):
    title = f"Generated EPUB {index:04d} " + ("metadata-long-" * (index % 17))
    image_size, image = cover(index)
    with zipfile.ZipFile(path, "w") as archive:
        archive.writestr("mimetype", "application/epub+zip", compress_type=zipfile.ZIP_STORED)
        archive.writestr("META-INF/container.xml", '<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        image_item = '<item id="cover" href="cover.png" media-type="image/png"/>' if with_cover else ""
        cover_meta = '<meta name="cover" content="cover"/>' if with_cover else ""
        archive.writestr("OEBPS/content.opf", f'''<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="uid" xmlns:dc="http://purl.org/dc/elements/1.1/">
<metadata><dc:identifier id="uid">urn:papernx:benchmark:{index}</dc:identifier><dc:title>{title}</dc:title><dc:creator>PaperNX Generated Corpus</dc:creator>{cover_meta}</metadata>
<manifest><item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>{image_item}</manifest><spine><itemref idref="chapter"/></spine></package>''')
        archive.writestr("OEBPS/chapter.xhtml", f'''<?xml version="1.0" encoding="utf-8"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>{title}</title></head><body><h1>{title}</h1><p>Deterministic test document {index}; generated locally without published text or artwork.</p><p>{'reader-performance-' * (index % 29 + 1)}</p></body></html>''')
        if with_cover:
            archive.writestr("OEBPS/cover.png", image)


def pdf_escape(text):
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def make_pdf(path, index):
    title = f"Generated PDF {index:04d} " + ("metadata-long-" * (index % 19))
    (width, height), _ = cover(index + 3000)
    compressed = zlib.compress(raw_rgb(width, height, index + 3000), 6)
    content = b"q\n520 0 0 700 46 46 cm\n/Cover Do\nQ\n"
    objects = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /XObject << /Cover 5 0 R >> >> /Contents 4 0 R >>",
        b"<< /Length " + str(len(content)).encode() + b" >>\nstream\n" + content + b"endstream",
        b"<< /Type /XObject /Subtype /Image /Width " + str(width).encode() + b" /Height " + str(height).encode()
        + b" /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /FlateDecode /Length " + str(len(compressed)).encode()
        + b" >>\nstream\n" + compressed + b"\nendstream",
        b"<< /Title (" + pdf_escape(title).encode("ascii") + b") /Author (PaperNX Generated Corpus) >>",
    ]
    data = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for number, body in enumerate(objects, 1):
        offsets.append(len(data))
        data.extend(f"{number} 0 obj\n".encode() + body + b"\nendobj\n")
    xref = len(data)
    data.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        data.extend(f"{offset:010d} 00000 n \n".encode())
    data.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R /Info 6 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    path.write_bytes(data)


def make_cbz(path, index):
    title = f"Generated CBZ {index:04d} " + ("series-volume-" * (index % 13))
    pages = 1 + index % 5
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        archive.writestr("ComicInfo.xml", f"<ComicInfo><Title>{title}</Title><Series>PaperNX synthetic benchmark</Series><Number>{index}</Number><PageCount>{pages}</PageCount></ComicInfo>")
        for page in range(pages):
            _, image = cover(index * 7 + page)
            archive.writestr(f"pages/{page + 1:03d}.png", image)
        archive.writestr("notes/README.txt", f"Generated archive {index}; nested entries exercise archive indexing safely.\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--count", type=int, default=1000)
    args = parser.parse_args()
    if args.count < 100 or args.count > 1000:
        parser.error("count must be between 100 and 1000")
    if args.output.exists():
        parser.error("output already exists; choose a new path to preserve prior evidence")
    args.output.mkdir(parents=True)
    entries = []
    rar_index = 0
    for index in range(args.count):
        if index % 25 == 24:
            source = RAR_FIXTURES[rar_index % len(RAR_FIXTURES)]
            path = args.output / f"book-{index:04d}.cbr"
            path.write_bytes(source.read_bytes())
            rar_index += 1
        elif index % 3 == 0:
            path = args.output / f"book-{index:04d}.epub"
            make_epub(path, index, with_cover=index % 10 != 0)
        elif index % 3 == 1:
            path = args.output / f"book-{index:04d}.pdf"
            make_pdf(path, index)
        else:
            path = args.output / f"book-{index:04d}.cbz"
            make_cbz(path, index)
        content = path.read_bytes()
        entries.append({"name": path.name, "sha256": hashlib.sha256(content).hexdigest(), "bytes": len(content)})
    counts = {ext: sum(Path(item["name"]).suffix.lower() == ext for item in entries) for ext in (".epub", ".pdf", ".cbz", ".cbr")}
    unique = {ext: len({item["sha256"] for item in entries if Path(item["name"]).suffix.lower() == ext}) for ext in counts}
    (args.output / "manifest.json").write_text(json.dumps({
        "schema": "papernx-cover-benchmark-corpus-1", "label": f"generated-mixed-v1-{args.count}",
        "count": args.count, "countsByFormat": counts, "uniqueContentByFormat": unique,
        "generatedFormats": ["EPUB", "PDF", "CBZ"], "reusedSyntheticCbrFixtures": True,
        "reusedCbrFixtureCount": len(RAR_FIXTURES), "books": entries,
    }, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"label": f"generated-mixed-v1-{args.count}", "count": args.count,
                      "countsByFormat": counts, "uniqueContentByFormat": unique}))


if __name__ == "__main__":
    main()
