#!/usr/bin/env python3
"""Deterministic, original comic test pages and ZIP containers (stdlib only)."""
from pathlib import Path
import struct, zlib, zipfile

ROOT=Path(__file__).resolve().parent/'fixtures'/'comics'

def png(width,height,color):
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    rows=[]
    for y in range(height):
        row=bytearray()
        for x in range(width):
            border=x<8 or y<8 or x>=width-8 or y>=height-8
            gutter=height//2-4<=y<height//2+4
            row.extend((18,22,28) if border or gutter else color)
        rows.append(b'\0'+row)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b'')

def archive(path,entries):
    with zipfile.ZipFile(path,'w') as z:
        for name,data in entries:
            item=zipfile.ZipInfo(name,(2026,1,1,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(item,data)

INFO='''<?xml version="1.0" encoding="utf-8"?><ComicInfo><Title>The Signal — Issue 7</Title><Series>PaperNX Test Comics</Series><Number>7</Number><Writer>Noa Test</Writer><Genre>Science Fiction</Genre><Manga>No</Manga><Pages><Page Image="1" Type="FrontCover" Bookmark="Arrival"/><Page Image="2" Bookmark="The signal"/></Pages></ComicInfo>'''

def main():
    ROOT.mkdir(parents=True,exist_ok=True)
    pages=[('pages/Page10.png',png(360,240,(30,80,210))),('pages/Page2.png',png(180,300,(30,185,80))),('pages/Page1.PNG',png(200,320,(220,40,35)))]
    entries=pages+[('ComicInfo.xml',INFO.encode()),('__MACOSX/._cover.png',b'not an image'),('notes.txt',b'Original test fixtures, no third-party comics.')]
    archive(ROOT/'sample.cbz',entries)
    archive(ROOT/'UPPER.CBZ',entries)
    archive(ROOT/'manga.cbz',pages+[('ComicInfo.xml',INFO.replace('<Manga>No</Manga>','<Manga>YesAndRightToLeft</Manga>').encode())])
    archive(ROOT/'unsafe.cbz',entries+[('../outside.png',pages[0][1]),('/absolute.png',pages[0][1]),('.hidden/page.png',pages[0][1])])
    archive(ROOT/'empty.cbz',[('readme.txt',b'No image pages')])
    archive(ROOT/'broken-page.cbz',[(n,b'broken image' if 'Page2' in n else data) for n,data in pages])
    archive(ROOT/'damaged-cover.cbz',[(n,b'broken image' if 'Page2' in n else data) for n,data in pages]+[('ComicInfo.xml',INFO.encode())])
    archive(ROOT/'bad-info.cbz',pages+[('ComicInfo.xml',b'<ComicInfo><Title>')])
    archive(ROOT/'huge-number.cbz',[('p'+('9'*160)+'.png',pages[0][1]),('p2.png',pages[1][1]),('p1.png',pages[2][1])])
    archive(ROOT/'unicode.cbz',[('日本語/Sayfa2.PNG',pages[1][1]),('日本語/Sayfa1.PNG',pages[2][1])])
    archive(ROOT/'zip-renamed.cbr',entries)
    (ROOT/'corrupt.cbz').write_bytes(b'not a ZIP archive')
    (ROOT/'corrupt.cbr').write_bytes(b'Rar!\x1a\x07\x00broken archive')
    # PNG sources used to regenerate the checked-in compressed RAR fixtures.
    source=ROOT/'rar-source';source.mkdir(exist_ok=True)
    for name,data in pages:
        target=source/name;target.parent.mkdir(exist_ok=True);target.write_bytes(data)
    (source/'ComicInfo.xml').write_text(INFO,encoding='utf-8')
    assets=ROOT/'image-assets'
    if assets.exists():
        archive(ROOT/'mixed-images.cbz',[(f.name,f.read_bytes()) for f in sorted(assets.iterdir())])
    large=ROOT/'large-cover-source';large.mkdir(exist_ok=True)
    # A typical scan needs over 40 MiB decoded, even though the PNG compresses well.
    (large/'Page1.png').write_bytes(png(3000,4500,(230,70,30)))
    (large/'ComicInfo.xml').write_text(INFO.replace('Image="1"','Image="0"'),encoding='utf-8')
    archive(ROOT/'large-cover.cbz',[(f.name,f.read_bytes()) for f in sorted(large.iterdir())])
    for issue in range(9, 23):
        info = INFO.replace('Issue 7', 'Issue '+str(issue)).replace('<Number>7</Number>', '<Number>'+str(issue)+'</Number>')
        archive(ROOT / ('series-'+str(issue)+'.cbz'), pages+[('ComicInfo.xml', info.encode())])
    navigation_info = '<ComicInfo><Title>Test Çizgi Romanı</Title><Pages><Page Image="0" Bookmark="Bölüm 1 — Part 1"/><Page Image="10" Bookmark="Bölüm 2 &amp; English 123"/></Pages></ComicInfo>'
    images = [png(380,580,color) for color in [(30,170,155),(215,135,45),(115,70,185)]]
    archive(ROOT/'navigation.cbz', [('ComicInfo.xml', navigation_info.encode())]+[(f'Page{i+1:03}.png',images[i%3]) for i in range(40)])
    archive(ROOT/'single-page.cbz', [('Page1.png',images[0])])
    print('Generated comic ZIP fixtures:',ROOT)

if __name__=='__main__':main()
