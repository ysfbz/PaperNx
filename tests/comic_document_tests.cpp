#include "papernx/mupdf_bridge.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

static const std::string root="tests/fixtures/comics/";
static void expectColor(const PxImage &image,int r,int g,int b) {
    assert(image.rgb && image.width>0 && image.height>0);
    const auto *pixel=image.rgb+(image.height/4)*image.stride+(image.width/4)*3;
    assert(std::abs(int(pixel[0])-r)<8 && std::abs(int(pixel[1])-g)<8 && std::abs(int(pixel[2])-b)<8);
}
static void standard(const char *name) {
    char error[512]{};auto *doc=px_document_open((root+name).c_str(),600,720,22,"",error,sizeof error);
    if(!doc)std::cerr << name << ": " << error << '\n';
    assert(doc && px_document_is_comic(doc) && !px_document_is_epub(doc));
    assert(px_document_pages(doc)==3);
    char metadata[512]{};
    assert(px_document_metadata(doc,"info:Title",metadata,sizeof metadata)>0 && std::string(metadata)=="The Signal — Issue 7");
    assert(px_document_metadata(doc,"info:Author",metadata,sizeof metadata)>0 && std::string(metadata)=="Noa Test");
    assert(px_document_metadata(doc,"info:Series",metadata,sizeof metadata)>0 && std::string(metadata)=="PaperNX Test Comics");
    assert(px_document_metadata(doc,"info:SeriesIndex",metadata,sizeof metadata)>0 && std::string(metadata)=="7");
    const int colors[3][3]={{220,40,35},{30,185,80},{30,80,210}};
    for(int number:{2,0,1,2,0}) {
        float w=0,h=0;assert(px_document_page_size(doc,number,&w,&h) && w>0 && h>0);
        PxImage image{};assert(px_document_render(doc,number,240,320,&image,error,sizeof error));
        expectColor(image,colors[number][0],colors[number][1],colors[number][2]);px_image_free(&image);
    }
    PxImage cover{};assert(px_document_render_thumbnail(doc,150,210,&cover,error,sizeof error));
    expectColor(cover,30,185,80);px_image_free(&cover);
    PxTocEntry outline[8]{};assert(px_document_toc(doc,outline,8)==2);
    assert(std::string(outline[0].title)=="Arrival" && outline[0].page==1);
    PxImage tile{};assert(px_document_render_tile(doc,0,0,100,128,&tile,error,sizeof error));
    expectColor(tile,220,40,35);px_image_free(&tile);
    PxImage view{};assert(px_document_render_view(doc,0,2,5,10,200,320,&view,error,sizeof error));
    assert(view.width==200 && view.height==320);px_image_free(&view);
    const auto mark=px_document_make_bookmark(doc,2);px_document_close(doc);
    doc=px_document_open((root+name).c_str(),600,720,22,"",error,sizeof error);
    assert(doc && px_document_lookup_bookmark(doc,mark)==2);px_document_close(doc);
}
int main() {
    for(const char *name:{"sample.cbz","UPPER.CBZ","sample-rar4.cbr","sample-rar4-solid.cbr","sample-rar5.cbr","sample-rar5-solid.cbr","unsafe.cbz","zip-renamed.cbr","rar-renamed.cbz"})standard(name);
    char error[512]{};
    for(const char *name:{"empty.cbz","corrupt.cbz","corrupt.cbr","password.cbr","password.cbz","split-rar4.cbr","split-rar5.cbr"}) {
        auto *doc=px_document_open((root+name).c_str(),600,720,22,"",error,sizeof error);
        if(doc)std::cerr << "Unexpectedly opened " << name << "\n";
        assert(!doc && error[0]);
    }
    for(const char *name:{"unicode.cbz","huge-number.cbz","bad-info.cbz"}) {
        auto *doc=px_document_open((root+name).c_str(),600,720,22,"",error,sizeof error);
        if(!doc)std::cerr << name << ": " << error << "\n";
        assert(doc && px_document_pages(doc)>=2);px_document_close(doc);
    }
    auto *doc=px_document_open((root+"manga.cbz").c_str(),600,720,22,"",error,sizeof error);
    char direction[8]{};assert(doc && px_document_metadata(doc,"info:ReadingDirection",direction,sizeof direction)>0);
    assert(!strcmp(direction,"rtl"));px_document_close(doc);
    doc=px_document_open((root+"broken-page.cbz").c_str(),600,720,22,"",error,sizeof error);
    assert(doc);PxImage broken{};
    assert(!px_document_render(doc,1,200,320,&broken,error,sizeof error) && !broken.rgb);
    assert(px_document_render(doc,2,200,320,&broken,error,sizeof error));px_image_free(&broken);px_document_close(doc);
    doc=px_document_open((root+"mixed-images.cbz").c_str(),600,720,22,"",error,sizeof error);
    assert(doc && px_document_pages(doc)==3);
    PxImage image{};assert(px_document_render(doc,0,200,320,&image,error,sizeof error));expectColor(image,230,45,30);px_image_free(&image);
    assert(px_document_render(doc,1,200,320,&image,error,sizeof error));expectColor(image,30,190,80);px_image_free(&image);
    assert(px_document_render(doc,2,200,320,&image,error,sizeof error));expectColor(image,255,255,255);px_image_free(&image);px_document_close(doc);
    doc=px_document_open((root+"damaged-cover.cbz").c_str(),600,720,22,"",error,sizeof error);
    assert(doc && px_document_render_thumbnail(doc,150,210,&image,error,sizeof error));expectColor(image,220,40,35);px_image_free(&image);px_document_close(doc);
    for(const char *name:{"large-cover.cbz","large-cover.cbr","large-cover-rar4-solid.cbr"}) {
        doc=px_document_open_limited((root+name).c_str(),600,720,22,"",error,sizeof error,16u*1024u*1024u);
        assert(doc && !px_document_render_thumbnail(doc,150,210,&image,error,sizeof error));px_document_close(doc);
        doc=px_document_open_limited((root+name).c_str(),600,720,22,"",error,sizeof error,96u*1024u*1024u);
        if(!doc)std::cerr<<error<<"\n";
        assert(doc && px_document_render_thumbnail(doc,150,210,&image,error,sizeof error));expectColor(image,230,70,30);px_image_free(&image);px_document_close(doc);
    }
    assert(!px_document_open_limited((root+"sample.cbz").c_str(),600,720,22,"",error,sizeof error,1024));
    std::thread worker([]{for(int i=0;i<3;++i)standard("sample-rar4-solid.cbr");});
    for(int i=0;i<3;++i)standard("sample-rar4-solid.cbr");
    worker.join();
    std::cout << "[PASS] CBZ/RAR4/RAR5, solid/random access, natural ordering, ComicInfo/cover, WebP alpha, zoom/tiles, bookmarks, errors and concurrent budgets\n";
}
