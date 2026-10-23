#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    struct Shot {int w,h;std::vector<Uint32> pixels;};
    static Shot snapshot(PaperNxApp &app,const std::string &name) {
        app.render();auto *target=SDL_GetRenderTarget(app.renderer_);
        if(app.appSettings_.get().displayOrientation && app.orientationTarget_)SDL_SetRenderTarget(app.renderer_,app.orientationTarget_);
        Shot shot{};SDL_GetRendererOutputSize(app.renderer_,&shot.w,&shot.h);shot.pixels.resize(shot.w*shot.h);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,SDL_PIXELFORMAT_ARGB8888,shot.pixels.data(),shot.w*4)==0);
        auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(shot.pixels.data(),shot.w,shot.h,32,shot.w*4,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        fs::create_directories("verification/books");assert(SDL_SaveBMP(surface,("verification/books/"+name+".bmp").c_str())==0);
        SDL_FreeSurface(surface);SDL_SetRenderTarget(app.renderer_,target);return shot;
    }
    static bool equal(const Shot &a,const Shot &b,SDL_Rect r) {
        assert(a.w==b.w && a.h==b.h);
        for(int y=r.y;y<r.y+r.h;++y)for(int x=r.x;x<r.x+r.w;++x)if(a.pixels[y*a.w+x]!=b.pixels[y*b.w+x])return false;
        return true;
    }
    static void centerTap(PaperNxApp &app,int w,int h) {
        int physicalW,physicalH;SDL_GetWindowSize(app.window_,&physicalW,&physicalH);
        DisplayTransform transform{physicalW,physicalH,app.appSettings_.get().displayOrientation};
        const auto p=transform.toPhysical(w*.5f,h*.5f);bool running=true;
        for(auto type:{SDL_FINGERDOWN,SDL_FINGERUP}){SDL_Event e{};e.type=type;e.tfinger.fingerId=42;e.tfinger.x=p.first/physicalW;e.tfinger.y=p.second/physicalH;app.handleEvent(e,running);assert(running);}
    }
    static std::string menuCaption(PaperNxApp &app,int page,int total) {
        const bool spread=app.readerLayout().spread;
        const int first=spread?(page-1)/2*2+1:page,last=spread?std::min(total,first+1):page;
        return std::to_string(first)+(last>first?"–"+std::to_string(last):"")+" of "+std::to_string(total)+" • "+std::to_string(int(std::lround(100.0*last/total)))+"%";
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(initReady(app));ui::language=1;
        auto book=[&](const char *name){auto it=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==name;});assert(it!=app.books_.end());return *it;};
        app.openBook(book("hebrew.epub"));
        for(int orientation:{0,1,2})for(int theme:{0,5}) {
            auto settings=app.appSettings_.get();settings.displayOrientation=orientation;settings.toolbar=false;app.appSettings_.put(settings);
            app.currentTheme_=theme;app.extraStyle_.backgroundColor=theme==5?0:0xffffff;app.extraStyle_.textColor=theme==5?0xeae7df:0x242424;
            app.render();app.reflowEpub(app.currentFontSize_);app.currentPage_=0;app.scrollY_=0;app.toolbarVisible_=false;
            int w,h;app.getWindowSize(w,h);assert(app.readerPageNumber(w,h)==1);
            assert(app.readerFooterCaption(w,h)=="1");
            const int remaining=app.bookChapterPagesLeft(h);assert(remaining>=0);
            assert(remaining==app.tocEntries_.back().page-1); // Nested headings do not truncate the parent chapter.
            const auto prefix="orientation-"+std::to_string(orientation)+"-theme-"+std::to_string(theme);
            const auto hidden=snapshot(app,prefix+"-hidden");centerTap(app,w,h);assert(app.toolbarVisible_);
            const auto shown=snapshot(app,prefix+"-shown");
            const auto total=app.pageCount_;
            assert(app.readerFooterCaption(w,h)==menuCaption(app,1,total));
            // Only the empty chrome strips change when the menu opens.
            assert(equal(hidden,shown,{0,72,w,h-144}));
            assert(!equal(hidden,shown,{w/2-100,h-48,200,30}));
            assert(!equal(hidden,shown,{232,0,w-464,72}));
            centerTap(app,w,h);assert(!app.toolbarVisible_);
            assert(app.readerFooterCaption(w,h)=="1");
            assert(equal(hidden,snapshot(app,prefix+"-closed"),{0,0,w,h}));
            settings=app.appSettings_.get();settings.toolbar=true;app.appSettings_.put(settings);
            assert(app.readerFooterCaption(w,h).find(" of ")!=std::string::npos);
            settings.toolbar=false;app.appSettings_.put(settings);
            if(app.pageCount_>1){app.currentPage_=1;assert(app.readerPageNumber(w,h)==2);}
            app.currentPage_=app.pageCount_-1;app.toolbarVisible_=true;
            assert(app.readerFooterCaption(w,h)==menuCaption(app,total,total));app.toolbarVisible_=false;
            const auto saved=app.tocEntries_;app.tocEntries_.clear();app.currentPage_=0;
            int chapter=0,page=0;assert(px_document_get_location(app.doc_,0,&chapter,&page));
            assert(app.bookChapterPagesLeft(h)==std::max(0,px_document_chapter_page_count(app.doc_,chapter)-1));app.tocEntries_=saved;
        }
        app.extraStyle_.readingMode=1;app.reflowEpub(40);app.currentPage_=0;app.scrollY_=0;
        int w,h;app.getWindowSize(w,h);assert(app.continuous() && app.readerPageNumber(w,h)==1);
        int total=0;for(int page=0;page<app.pageCount_;++page)total+=std::max(1,int(std::ceil(app.segmentHeight(page)/(h-144))));
        assert(app.readerPageCount(w,h)==total && total>app.pageCount_);
        app.toolbarVisible_=true;assert(app.readerFooterCaption(w,h).find(" of "+std::to_string(total)+" • ")!=std::string::npos);app.toolbarVisible_=false;
        const int left=app.bookChapterPagesLeft(h);
        app.scrollY_=float(h-144);assert(app.segmentHeight(0)>h-144);assert(app.readerPageNumber(w,h)==2);
        assert(app.bookChapterPagesLeft(h)==std::max(0,left-1));
        const auto chapterStart=app.tocEntries_.back();app.currentPage_=chapterStart.page;app.scrollY_=chapterStart.y;
        assert(app.readerPageNumber(w,h)>2);assert(app.bookChapterPagesLeft(h)>=0);
        app.currentPage_=app.pageCount_-1;app.scrollY_=std::max(0.f,app.segmentHeight(app.currentPage_)-(h-144));app.toolbarVisible_=true;
        assert(app.readerFooterCaption(w,h)==menuCaption(app,total,total));
        app.closeBook();app.openBook(book("sample.pdf"));assert(app.readerPageNumber(w,h)==1);app.tocEntries_.clear();assert(app.bookChapterPagesLeft(h)==-1);
        app.toolbarVisible_=false;assert(app.readerFooterCaption(w,h)=="1");
        app.toolbarVisible_=true;assert(app.readerFooterCaption(w,h)==menuCaption(app,1,app.pageCount_));
        app.closeBook();
        for(const char *name:{"comic.cbz","comic.cbr"}) {
            app.openBook(book(name));assert(app.comic() && app.bookChapterPagesLeft(h)==-1);
            for(int mode:{0,1})for(int orientation:{0,1,2})for(int theme:{0,5}) {
                auto settings=app.appSettings_.get();settings.displayOrientation=orientation;settings.toolbar=false;app.appSettings_.put(settings);
                app.currentTheme_=theme;app.extraStyle_.backgroundColor=theme==5?0:0xffffff;app.extraStyle_.textColor=theme==5?0xeae7df:0x242424;
                app.extraStyle_.readingMode=mode;app.currentPage_=app.pageCount_/2;app.scrollY_=0;app.setPdfView(0);app.toolbarVisible_=false;
                app.hintUntil_=SDL_GetTicks()+8000;app.getWindowSize(w,h);
                const int page=app.currentPage_+1,total=app.pageCount_;const auto label=std::to_string(page);
                assert(app.readerFooterCaption(w,h)==label && app.readerPageCount(w,h)==total);
                const auto prefix=std::string(name)+"-mode-"+std::to_string(mode)+"-rotation-"+std::to_string(orientation)+"-theme-"+std::to_string(theme);
                const auto hidden=snapshot(app,prefix+"-hidden");
                assert(hidden.w==w && hidden.h==h);
                const auto bg=app.palette().bg;const Uint32 background=(Uint32(bg.r)<<16)|(Uint32(bg.g)<<8)|bg.b;bool ink=false;
                const int folioX=app.readerLayout().spread?w/4:w/2;
                for(int y=h-44;y<h-4;++y)for(int x=folioX-30;x<folioX+30;++x)ink|=(hidden.pixels[y*w+x]&0xffffff)!=background;
                assert(ink); // The actual hidden-toolbar comic frame contains the page numeral.
                centerTap(app,w,h);assert(app.toolbarVisible_);
                assert(app.readerFooterCaption(w,h)==menuCaption(app,page,total));
                const auto shown=snapshot(app,prefix+"-shown");assert(equal(hidden,shown,{0,72,w,h-144}));assert(!equal(hidden,shown,{w/2-100,h-48,200,30}));
                centerTap(app,w,h);assert(!app.toolbarVisible_ && app.readerFooterCaption(w,h)==label);
                assert(equal(hidden,snapshot(app,prefix+"-closed"),{0,0,w,h}));
                assert(app.readerHeaderButton(0,w).w==56 && app.readerTrack(w,h).y==h-62);
                app.currentPage_=app.pageCount_-1;app.toolbarVisible_=true;
                assert(app.readerFooterCaption(w,h)==menuCaption(app,total,total));
            }
            app.closeBook();
        }
        for(const char *name:{"manga.cbz","single.cbz"}){
            app.openBook(book(name));app.currentPage_=0;app.toolbarVisible_=false;assert(app.readerFooterCaption(w,h)=="1");
            app.toolbarVisible_=true;assert(app.readerFooterCaption(w,h)==menuCaption(app,1,app.pageCount_));
            if(app.pageCount_==1)assert(app.readerFooterCaption(w,h)=="1 of 1 • 100%");
            app.closeBook();
        }
        std::cout<<"PASS: shared book/CBZ/CBR footer, toolbar transitions, unchanged content pixels, all orientations/themes, continuous modes, manga, single-page and final-page progress\n";
    }
};
}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-book-chrome";fs::remove_all(root);fs::create_directories(root/"books");
    for(const auto &pair:std::vector<std::pair<std::string,std::string>>{{"hebrew_navigation.epub","hebrew.epub"},{"sample_doc.pdf","sample.pdf"},{"comics/navigation.cbz","comic.cbz"},{"comics/sample-rar5.cbr","comic.cbr"},{"comics/manga.cbz","manga.cbz"},{"comics/single-page.cbz","single.cbz"}})fs::copy_file(fs::path("tests/fixtures")/pair.first,root/"books"/pair.second);
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();
}
