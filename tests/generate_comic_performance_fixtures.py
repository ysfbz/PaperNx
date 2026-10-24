#!/usr/bin/env python3
"""Original 37-page ~43 MB RAR5/CBR fixture; no external archiver required.

Uses RAR5's stored-file method. Existing checked-in RAR4/RAR5 fixtures cover
compressed and solid methods. The large private ancillary PNG chunk exercises
archive IO at realistic book sizes without adding third-party comic artwork.
"""
import binascii
import random
import struct
import zlib
from pathlib import Path
from generate_comic_fixtures import png
ROOT=Path(__file__).parent/'fixtures'/'comics'
def vint(n):
    out=bytearray()
    while n>=128:out.append((n&127)|128);n>>=7
    return bytes(out)+bytes([n])
def block(kind,flags,body,data=b''):
    head=vint(kind)+vint(flags)+(vint(len(data)) if flags&2 else b'')+body
    head=vint(len(head))+head
    return struct.pack('<I',binascii.crc32(head)&0xffffffff)+head+data
def chunk(name,data):
    return struct.pack('>I',len(data))+name+data+struct.pack('>I',binascii.crc32(name+data)&0xffffffff)
def main():
    ROOT.mkdir(exist_ok=True,parents=True)
    target=ROOT/'performance-37.cbr'
    if target.exists():
        print(f'Preserving existing performance fixture: {target.name}, {target.stat().st_size:,} bytes')
        return
    with target.open('wb') as f:
        f.write(b'Rar!\x1a\x07\x01\x00');f.write(block(1,0,vint(0)))
        rng=random.Random(17202)
        original=png(2048,3072,(55,110,185))
        for i in range(37):
            image=original[:33]+chunk(b'raNd',rng.randbytes(1_150_000))+original[33:]
            name=f'page{i:03}.png'.encode()
            body=vint(4)+vint(len(image))+vint(0o100644)+struct.pack('<I',binascii.crc32(image)&0xffffffff)+vint(0)+vint(1)+vint(len(name))+name
            f.write(block(2,2,body,image))
        f.write(block(5,0,vint(0)))
    print(f'Generated original RAR5 fixture: {target.name}, 37 pages, {target.stat().st_size:,} bytes')
if __name__=='__main__':main()
