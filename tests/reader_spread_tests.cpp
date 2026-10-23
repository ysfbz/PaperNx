#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    struct Shot {int w,h;std::vector<Uint32> pixels;};
    static Shot read(PaperNxApp &app,SDL_Texture *texture) {
        auto *target=SDL_GetRenderTarget(app.renderer_);
        assert(SDL_SetRenderTarget(app.renderer_,texture)==0);
        Shot s{};SDL_GetRendererOutputSize(app.renderer_,&s.w,&s.h);s.pixels.resize(s.w*s.h);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,SDL_PIXELFORMAT_ARGB8888,s.pixels.data(),s.w*4)==0);
        SDL_SetRenderTarget(app.renderer_,target);return s;
    }
    static void save(const Shot &s,const std::string &name) {
        fs::create_directories("verification/spreads");
        auto *surface=SDL_CreateRGBSurfaceWithFormatFrom(const_cast<Uint32*>(s.pixels.data()),s.w,s.h,32,s.w*4,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(SDL_SaveBMP(surface,("verification/spreads/"+name+".bmp").c_str())==0);SDL_FreeSurface(surface);
    }
    static bool same(const Shot &a,const Shot &b,SDL_Rect area,int bx=0,int by=0) {
        for(int y=0;y<area.h;++y)for(int x=0;x<area.w;++x)
            if(a.pixels[(area.y+y)*a.w+area.x+x]!=b.pixels[(by+y)*b.w+bx+x])return false;
        return true;
    }
    static void finish(PaperNxApp &app) {
        app.pageTurnAnimation_.started=SDL_GetTicks()-2000;app.tickPageTurn();
    }
    static void waitComicTurn(PaperNxApp &app) {
        const auto deadline=SDL_GetTicks()+20000;
        while(app.comicPendingTurn_.active && SDL_GetTicks()<deadline){app.processComicPages();SDL_Delay(2);}
        app.processComicPages();assert(!app.comicPendingTurn_.active);
    }
    static void settleLayout(PaperNxApp &app) {
        for(int i=0;i<1000 && (app.layoutPending_ || app.layoutWorker_.joinable());++i) {
            app.processLayout();SDL_Delay(2);
        }
        app.processLayout();assert(!app.layoutPending_ && !app.layoutWorker_.joinable());
    }
    static Shot sheet(PaperNxApp &app,int page) {
        auto l=app.readerLayout();auto t=app.captureReaderSurface(l.leafWidth(),l.height,[&]{app.drawReaderLeaf(page,{0,0,l.leafWidth(),l.height});});assert(t);
        return read(app,t.get());
    }
    static Shot spread(PaperNxApp &app,int page) {
        auto l=app.readerLayout();auto t=app.captureReaderSurface(l.width,l.height,[&]{app.drawReaderSheets(page);});assert(t);
        return read(app,t.get());
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(initReady(app));ui::language=1;
        auto book=[&](const char *name){auto i=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==name;});assert(i!=app.books_.end());return *i;};
        assert(!app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        auto tapSettingsRow=[&](int tab,int row) {
            app.openSettings(false,tab);int w,h;app.getWindowSize(w,h);const auto layout=ui::SettingsLayout(w,h);
            app.settingsScroll_=std::clamp(float(row*64-(layout.content.h-56)),0.f,app.maxUiScroll());
            auto control=layout.control(row);control.y-=int(app.settingsScroll_);
            assert(ui::contains(layout.content,control.x+control.w/2,control.y+control.h/2));
            app.handleAaModalTap(control.x+control.w/2,control.y+control.h/2,w,h);
        };
        auto epub=book("english.epub");app.openBook(epub);app.reflowEpub(24);assert(app.pageCount_>2);app.currentPage_=0;
        const int originalPage=app.currentPage_;app.saveCurrentPosition();app.toggleBookmark();
        int chapter=-1,character=-1;double offset=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&chapter,&character,&offset));
        const auto originalMarks=app.progressStore_.get(app.currentBookId_).bookmarks.size();
        tapSettingsRow(1,4);assert(app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        settleLayout(app);int restoredChapter=-1,restoredCharacter=-1;double restoredOffset=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&restoredChapter,&restoredCharacter,&restoredOffset));
        assert(chapter==restoredChapter && character==restoredCharacter && std::abs(offset-restoredOffset)<.01);
        assert(app.progressStore_.get(app.currentBookId_).bookmarks.size()==originalMarks && app.currentPage_>=0);
        app.dismissSettings();app.currentPage_=originalPage;
        app.beginPageTurnTouch(.9f,.5f,77);assert(app.movePageTurnTouch(.4f,.5f,77));
        assert(app.pageTurnAnimation_.active && app.currentPage_==originalPage);
        int w,h;app.getWindowSize(w,h);app.quickMenu_=true;const auto shortcut=app.quickPageShortcut(app.quickSheet(w,h));
        app.handleQuickMenuTap(shortcut.x+shortcut.w/2,shortcut.y+shortcut.h/2,w,h);
        assert(!app.pageTurnAnimation_.active && !app.quickMenu_ && app.currentPage_==originalPage);
        assert(!app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        tapSettingsRow(1,4);assert(app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        settleLayout(app);app.dismissSettings();app.closeBook();
        auto pdf=book("sample.pdf");app.openBook(pdf);assert(app.readerLayout().spread);app.closeBook();
        auto comic=book("comic.cbz");app.openBook(comic);
        assert(app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads && !app.comicSpreadActive());
        tapSettingsRow(4,7);assert(app.appSettings_.get().comicLandscapeSpreads && app.appSettings_.get().bookLandscapeSpreads && app.comicSpreadActive());
        app.dismissSettings();app.pdfZoom_=2;app.pdfPanX_=app.pdfPanY_=0;const int comicPage=app.currentPage_;
        app.quickMenu_=true;const auto comicShortcut=app.quickPageShortcut(app.quickSheet(w,h));
        app.handleQuickMenuTap(comicShortcut.x+comicShortcut.w/2,comicShortcut.y+comicShortcut.h/2,w,h);
        assert(!app.appSettings_.get().comicLandscapeSpreads && app.appSettings_.get().bookLandscapeSpreads);
        assert(app.currentPage_==comicPage && app.pdfZoom_==2 && app.pdfPanX_==0 && app.pdfPanY_==0);
        auto spreadPrefs=app.appSettings_.get();spreadPrefs.comicLandscapeSpreads=true;app.appSettings_.put(spreadPrefs);
        SDL_SetWindowSize(app.window_,720,1280);assert(!app.comicSpreadActive() && !app.readerLayout().spread);
        SDL_SetWindowSize(app.window_,1280,720);assert(app.comicSpreadActive() && app.readerLayout().spread);
        app.extraStyle_.readingMode=1;assert(app.continuous() && !app.comicSpreadActive());app.extraStyle_.readingMode=0;app.closeBook();
        auto spreadDefaults=app.appSettings_.get();spreadDefaults.bookLandscapeSpreads=true;spreadDefaults.comicLandscapeSpreads=true;app.appSettings_.put(spreadDefaults);
        for(const char *name:{"english.epub","hebrew.epub","comic.cbz","comic.cbr","manga.cbz","single.cbz","sample.pdf","spread.pdf"}) {
            app.openBook(book(name));app.extraStyle_.readingMode=0;app.documentMode_=0;
            for(int theme:{0,5}) {
                app.currentTheme_=theme;app.extraStyle_.backgroundColor=theme==5?0:0xffffff;
                app.extraStyle_.textColor=theme==5?0xeae7df:0x242424;
                if(px_document_is_epub(app.doc_))app.reflowEpub(24);else app.pageCache_.clear();
                app.currentPage_=0;app.toolbarVisible_=false;
                const auto l=app.readerLayout();assert(l.width==1280 && l.height==720);
                assert(l.spread==(!app.comic() || app.comicSpreadPages(0).second==2));
                const int firstSide=app.readingDirection()<0?1:0;
                const auto first=sheet(app,0),rest=spread(app,0);
                // Both content and the folio occupy the correct physical leaf.
                assert(same(rest,first,l.leaf(firstSide)));
                const auto paper=app.palette().bg;const Uint32 bg=(Uint32(paper.r)<<16)|(Uint32(paper.g)<<8)|paper.b;
                int ink=0;for(int y=l.height-40;y<l.height-4;++y)for(int x=first.w/2-20;x<first.w/2+20;++x)ink+=(first.pixels[y*first.w+x]&0xffffff)!=bg;
                assert(ink>10);
                const auto prefix=std::string(name)+"-"+std::to_string(theme);
                save(rest,prefix+"-landscape");
                app.turnPage(-1);assert(app.currentPage_==0 && !app.pageTurnAnimation_.active);
                if(app.comic() && app.pageCount_>2 && app.comicSpreadPages(2).second!=2) {
                    app.turnPage(1);waitComicTurn(app);assert(app.currentPage_==2 && app.pageTurnAnimation_.active && !app.pageTurnAnimation_.spread);
                    finish(app);app.turnPage(-1);waitComicTurn(app);finish(app);assert(app.currentPage_==0);
                } else if(app.pageCount_>2) {
                    const auto before=rest;app.turnPage(1);if(app.comic())waitComicTurn(app);assert(app.currentPage_==2 && app.pageTurnAnimation_.spread);
                    auto &turn=app.pageTurnAnimation_;const int side=turn.direction>0?1:0;
                    auto front=read(app,turn.previous.get()),back=read(app,turn.back.get());
                    if(!app.comic())assert(same(before,front,l.leaf(side)));
                    else assert(turn.comicSheet && turn.viewport.y>=72 && turn.viewport.h<l.height && turn.viewport.w<l.width/2);
                    const auto after=spread(app,2);if(!app.comic())assert(same(after,back,l.leaf(1-side)));
                    // Clearing the LRU cannot destroy paper or its number mid-turn.
                    app.pageCache_.clear();
                    for(float p:{0.f,.15f,.35f,.55f,.8f,1.f}) {
                        auto target=app.captureReaderSurface(l.width,l.height,[&]{
                            PageCurlRenderer::renderSpread(app.renderer_,turn.previous.get(),turn.back.get(),turn.under.get(),turn.viewport,{0,0,l.width,l.height},p,turn.direction,.5f,0,app.comic()?&turn.oppositeViewport:nullptr,false,turn.paperBack.get());
                        });assert(target);const auto frame=read(app,target.get());
                        if(p==0 && !app.comic())assert(frame.pixels==before.pixels);
                        if(app.comic() && (p==0 || p==1)) {
                            // Comic snapshots now contain actual artwork only;
                            // any raster differences are limited to the fitted
                            // edge/gutter rounding, not fullscreen paper capture.
                            const auto &expected=p==0?before:after;int changed=0;
                            for(size_t i=0;i<frame.pixels.size();++i)changed+=frame.pixels[i]!=expected.pixels[i];
                            assert(changed<int(frame.pixels.size()/100));
                        }
                        if(p==1 && !app.comic() && frame.pixels!=after.pixels){save(frame,prefix+"-unexpected");save(after,prefix+"-expected");
                            int count=0;for(int y=0;y<frame.h;++y)for(int x=0;x<frame.w;++x)if(frame.pixels[y*frame.w+x]!=after.pixels[y*frame.w+x]){if(count++<5)std::cerr<<prefix<<" mismatch "<<x<<","<<y<<" "<<std::hex<<frame.pixels[y*frame.w+x]<<" "<<after.pixels[y*frame.w+x]<<std::dec<<"\n";}
                            std::cerr<<"Differing pixels: "<<count<<" pages="<<app.pageCount_<<" direction="<<turn.direction<<"\n";assert(false);}
                        save(frame,prefix+"-forward-"+std::to_string(int(p*100)));
                    }
                    finish(app);app.turnPage(-1);if(app.comic())waitComicTurn(app);assert(app.currentPage_==0 && app.pageTurnAnimation_.spread);finish(app);
                    // Queued controller turns advance spreads without skipping leaves.
                    if(app.pageCount_>4){app.turnPage(1);app.turnPage(1);if(app.comic())waitComicTurn(app);finish(app);if(app.comic())waitComicTurn(app);assert(app.currentPage_==4);finish(app);}
                    app.currentPage_=app.pageCount_-1;app.turnPage(1);assert(!app.pageTurnAnimation_.active);
                } else {app.turnPage(1);assert(app.currentPage_==0 && !app.pageTurnAnimation_.active);}
                if(app.pageCount_%2 && (!app.comic() || app.comicSpreadPages(app.pageCount_-1).second==2)) {
                    const auto last=spread(app,app.pageCount_-1);const int blankSide=1-firstSide;
                    const auto rect=l.leaf(blankSide);
                    for(int y=0;y<rect.h;++y)for(int x=rect.x;x<rect.x+rect.w;++x)assert((last.pixels[y*last.w+x]&0xffffff)==bg);
                }
                // Portrait is one leaf; folio pixels are in its owned snapshot.
                SDL_SetWindowSize(app.window_,720,1280);
                if(px_document_is_epub(app.doc_))app.reflowEpub(24);else app.pageCache_.clear();
                app.currentPage_=0;assert(!app.readerLayout().spread);
                const auto portrait=spread(app,0);save(portrait,prefix+"-portrait");
                if(app.pageCount_>1) {
                    app.turnPage(1);if(app.comic())waitComicTurn(app);assert(app.currentPage_==1 && !app.pageTurnAnimation_.spread);
                    auto captured=read(app,app.pageTurnAnimation_.previous.get());
                    if(!app.comic())assert(captured.pixels==portrait.pixels);
                    else {assert(app.pageTurnAnimation_.comicSheet && app.pageTurnAnimation_.viewport.h<1280);assert(app.pageTurnAnimation_.back && !app.pageTurnAnimation_.comicFolioCaption.empty());}
                    finish(app);
                }
                SDL_SetWindowSize(app.window_,1280,720);
                if(px_document_is_epub(app.doc_))app.reflowEpub(24);else app.pageCache_.clear();
            }
            app.closeBook();
        }
        app.openBook(book("smart.cbz"));app.currentPage_=0;app.toolbarVisible_=false;
        assert(app.readerLayout().spread && app.comicSpreadPages(0).second==2);
        const auto left=app.comicLeafArtwork(0,{0,0,640,720}),right=app.comicLeafArtwork(1,{640,0,640,720});
        assert(std::abs(left.x+left.w-639)<.01 && std::abs(right.x-641)<.01); // Real centre gutter, no doubled inner margins.
        app.render();const auto decodes=app.comicTextureBuilds_;
        save(read(app,nullptr),"smart-comic-fit");
        auto focal=[&]{return std::pair<float,float>{(.73f*1280-app.currentMargin_+app.pdfPanX_)/app.pdfZoom_,(.38f*720-72+app.pdfPanY_)/app.pdfZoom_};};
        const auto before=focal();app.zoomPdf(1.01f,.73f,.38f,true);app.render();
        assert(app.readerLayout().spread && app.comicTextureBuilds_==decodes); // No cold decode or single-page switch at the first pinch.
        const auto after=focal();assert(std::abs(before.first-after.first)<1 && std::abs(before.second-after.second)<1);
        app.zoomPdf(2,.73f,.38f,true);assert(app.readerLayout().spread && app.pdfPanX_>0 && app.pdfPanY_>0);
        app.render();save(read(app,nullptr),"smart-comic-zoom");
        const int x=app.pdfPanX_;app.returnComicToTop();assert(app.pdfPanY_==0 && app.pdfPanX_==x && app.pdfZoom_==2);
        app.pdfPanY_=140;bool running=true;SDL_Event key{};key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_y;app.handleEvent(key,running);
        assert(app.pdfPanY_==0 && app.progressStore_.get(app.currentBookId_).bookmarks.empty());
        app.pdfPanY_=140;const auto top=app.zoomTopRect(1280,720);
        for(Uint32 type:{SDL_FINGERDOWN,SDL_FINGERUP}) {SDL_Event event{};event.type=type;event.tfinger.fingerId=71;event.tfinger.x=(top.x+top.w/2)/1280.f;event.tfinger.y=(top.y+top.h/2)/720.f;app.handleEvent(event,running);}
        assert(app.pdfPanY_==0 && app.pdfZoom_==2);
        app.setPdfView(0);app.currentPage_=0;
        for(int page:{2,3,5,6}) {app.turnPage(1);waitComicTurn(app);if(app.pageTurnAnimation_.active)finish(app);assert(app.currentPage_==page);}
        assert(px_document_page_is_spread(app.doc_,5) && !app.readerLayout().spread);
        for(int page:{5,3,2,0}) {app.turnPage(-1);waitComicTurn(app);if(app.pageTurnAnimation_.active)finish(app);assert(app.currentPage_==page);}
        app.openSettings(false,5);
        for(int row:{8}) {
            const auto layout=ui::SettingsLayout(1280,720);
            app.settingsScroll_=std::clamp(float(row*64-(layout.content.h-56)),0.f,app.maxUiScroll());
            auto r=layout.control(row);r.y-=int(app.settingsScroll_);
            assert(ui::contains(layout.content,r.x+r.w/2,r.y+r.h/2));
            app.handleAaModalTap(r.x+r.w/2,r.y+r.h/2,1280,720);
        }
        assert(!app.appSettings_.get().expandedViewport && app.appSettings_.get().bookLandscapeSpreads);app.dismissSettings();
        auto singleComic=app.appSettings_.get();singleComic.comicLandscapeSpreads=false;app.appSettings_.put(singleComic);
        assert(!app.readerLayout().spread && app.readerLayout().contentHeight()==576);
        assert(app.buildEpubCss().find("@page")==std::string::npos);
        app.turnPage(1);waitComicTurn(app);if(app.pageTurnAnimation_.active)finish(app);assert(app.currentPage_==1);
        auto prefs=app.appSettings_.get();prefs.bookLandscapeSpreads=prefs.expandedViewport=true;app.appSettings_.put(prefs);app.closeBook();
        // Actual shoulder and keyboard events follow each newly opened book in landscape.
        for(const char *name:{"hebrew.epub","english.epub","hebrew.epub"}) {
            app.openBook(book(name));app.currentPage_=2;app.toolbarVisible_=false;
            assert(app.pageCount_>4);const int direction=app.readingDirection();
            for(bool controller:{true,false})for(bool right:{true,false}) {
                app.currentPage_=2;SDL_Event e{};bool running=true;
                if(controller){e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=right?SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:SDL_CONTROLLER_BUTTON_LEFTSHOULDER;}
                else {e.type=SDL_KEYDOWN;e.key.keysym.sym=right?SDLK_PAGEDOWN:SDLK_PAGEUP;}
                app.handleEvent(e,running);assert(app.currentPage_==2+(right?2:-2)*direction);finish(app);
            }
            SDL_SetWindowSize(app.window_,720,1280);app.reflowEpub(24);app.currentPage_=2;
            SDL_Event e{};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;bool running=true;
            app.handleEvent(e,running);assert(app.currentPage_==3);finish(app);
            SDL_SetWindowSize(app.window_,1280,720);app.closeBook();
        }
        // A live drag commits a spread, while returning the sheet cancels it.
        app.openBook(book("english.epub"));app.currentPage_=0;app.toolbarVisible_=false;
        app.beginPageTurnTouch(.9f,.5f,9);assert(app.movePageTurnTouch(.4f,.55f,9));
        assert(app.pageTurnAnimation_.targetPage==2 && app.currentPage_==0);
        assert(std::abs(app.pageTurnAnimation_.progress-.5f)<.001f); // Full-screen travel, not the previous half-speed mapping.
        app.releasePageTurnTouch(.4f,.55f,9);finish(app);assert(app.currentPage_==2);
        app.beginPageTurnTouch(.1f,.5f,9);assert(app.movePageTurnTouch(.6f,.5f,9));
        app.movePageTurnTouch(.11f,.5f,9);app.pageTurnTouch_.velocity=0;
        app.releasePageTurnTouch(.11f,.5f,9);assert(!app.pageTurnAnimation_.complete);finish(app);assert(app.currentPage_==2);
        // Rotation invalidates a held mesh without committing a different leaf.
        app.beginPageTurnTouch(.9f,.5f,9);assert(app.movePageTurnTouch(.4f,.5f,9));
        SDL_SetWindowSize(app.window_,720,1280);app.tickPageTurn();assert(!app.pageTurnAnimation_.active && app.currentPage_==2);
        app.closeBook();
        std::cout<<"PASS: LTR/RTL spreads; EPUB/PDF/CBZ/CBR; owned folios; all four curl faces; exact endpoints; blank final leaf; queued/interactive turns; portrait and rotation cancellation\n";
    }
};
}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-reader-spreads";fs::remove_all(root);fs::create_directories(root/"books");
    for(auto pair:std::vector<std::pair<std::string,std::string>>{{"sample_book.epub","english.epub"},{"hebrew_navigation.epub","hebrew.epub"},{"comics/navigation.cbz","comic.cbz"},{"comics/sample-rar5.cbr","comic.cbr"},{"comics/manga.cbz","manga.cbz"},{"comics/single-page.cbz","single.cbz"},{"sample_doc.pdf","sample.pdf"},{"spread_doc.pdf","spread.pdf"},{"comics/smart-spreads.cbz","smart.cbz"}})
        fs::copy_file(fs::path("tests/fixtures")/pair.first,root/"books"/pair.second);
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();
}
