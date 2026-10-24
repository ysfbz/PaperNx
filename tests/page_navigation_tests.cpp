#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    static void event(PaperNxApp &app,SDL_Event e){bool running=true;app.handleEvent(e,running);assert(running);}
    static void pad(PaperNxApp &app,int button){SDL_Event e{};e.type=SDL_CONTROLLERBUTTONDOWN;e.cbutton.button=Uint8(button);event(app,e);}
    static void touch(PaperNxApp &app,Uint32 type,float x,float y,SDL_FingerID finger=21) {
        int w,h;SDL_GetWindowSize(app.window_,&w,&h);DisplayTransform t{w,h,app.appSettings_.get().displayOrientation};
        const auto p=t.toPhysical(x,y);SDL_Event e{};e.type=type;e.tfinger.fingerId=finger;e.tfinger.x=p.first/w;e.tfinger.y=p.second/h;event(app,e);
    }
    static void tap(PaperNxApp &app,SDL_Rect r){const float x=r.x+r.w*.5f,y=r.y+r.h*.5f;touch(app,SDL_FINGERDOWN,x,y);touch(app,SDL_FINGERUP,x,y);}
    static void drag(PaperNxApp &app,float fraction,Uint32 type) {
        int w,h;app.getWindowSize(w,h);const auto r=app.readerTrack(w,h);touch(app,type,r.x+r.w*fraction,r.y+4);
    }
    static void waitPreview(PaperNxApp &app,int page,float y=0) {
        app.preparePagePreviews();const auto key=app.previewKey(page,app.previewOffset(page,y));
        const auto end=SDL_GetTicks()+8000;
        while(SDL_GetTicks()<end) {app.render();app.drawPagePreview(page,y,{0,0,160,220},app.palette());app.processPagePreviews();if(app.pagePreviewCache_.get(key))return;SDL_Delay(5);}
        std::cerr<<"Preview missing for page "<<page<<" at "<<y<<"\n";assert(false);
    }
    static void snapshot(PaperNxApp &app,const std::string &name) {
        for(int i=0;i<30;++i){app.render();SDL_Delay(5);app.processPagePreviews();}
        app.render();auto *target=SDL_GetRenderTarget(app.renderer_);if(app.appSettings_.get().displayOrientation && app.orientationTarget_)SDL_SetRenderTarget(app.renderer_,app.orientationTarget_);
        int w,h;SDL_GetRendererOutputSize(app.renderer_,&w,&h);auto surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
        fs::create_directories("verification/navigation");assert(SDL_SaveBMP(surface,("verification/navigation/"+name+".bmp").c_str())==0);SDL_FreeSurface(surface);SDL_SetRenderTarget(app.renderer_,target);
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(initReady(app));ui::language=1;
        auto book=[&](const char *name){return *std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==name;});};
        const auto comic=book("navigation.cbz"),epub=book("sample.epub"),pdf=book("sample.pdf");
        app.openBook(comic);assert(app.pageCount_==40);app.zoomPdf(2.5f);app.pdfPanY_=180;app.toolbarVisible_=true;app.render();
        const unsigned builds=app.comicTextureBuilds_;const auto history=app.navigationHistory_.size();
        drag(app,.2f,SDL_FINGERDOWN);assert(app.scrub_.active && app.currentPage_==0 && app.scrub_.page==8);
        for(int i=0;i<200;++i)drag(app,float(i%40)/39,SDL_FINGERMOTION);
        drag(app,.75f,SDL_FINGERMOTION);assert(app.currentPage_==0 && app.scrub_.page==29 && app.navigationHistory_.size()==history);
        assert(app.progressStore_.get(comic.id).page==0);waitPreview(app,29);snapshot(app,"scrub-landscape");assert(app.comicTextureBuilds_==builds);
        drag(app,.75f,SDL_FINGERUP);assert(!app.scrub_.active && app.currentPage_==29 && app.pdfZoom_==2.5f && app.pdfPanY_==0);
        assert(app.navigationHistory_.size()==history+1 && app.progressStore_.get(comic.id).page==29);app.jumpBack();assert(app.currentPage_==0);
        for(int orientation:{1,2}) {
            auto settings=app.appSettings_.get();settings.displayOrientation=orientation;app.appSettings_.put(settings);app.toolbarVisible_=true;
            drag(app,.5f,SDL_FINGERDOWN);assert(app.scrub_.active && app.currentPage_==0 && app.scrub_.page==20);
            waitPreview(app,20);snapshot(app,orientation==1?"scrub-portrait-cw":"scrub-portrait-ccw");
            drag(app,.5f,SDL_FINGERUP);assert(app.currentPage_==20);app.jumpBack();assert(app.currentPage_==0);
        }
        auto settings=app.appSettings_.get();settings.displayOrientation=0;app.appSettings_.put(settings);app.toolbarVisible_=true;
        drag(app,.8f,SDL_FINGERDOWN);const auto beforeCancel=app.navigationHistory_.size();pad(app,SDL_CONTROLLER_BUTTON_B);drag(app,.8f,SDL_FINGERUP);
        assert(app.currentPage_==0 && app.navigationHistory_.size()==beforeCancel && !app.scrub_.active);
        drag(app,.8f,SDL_FINGERDOWN);touch(app,SDL_FINGERDOWN,600,400,22);touch(app,SDL_FINGERUP,600,400,22);drag(app,.8f,SDL_FINGERUP);assert(app.currentPage_==0 && !app.scrub_.active);
        drag(app,.8f,SDL_FINGERDOWN);settings.displayOrientation=1;app.appSettings_.put(settings);app.tickPageNavigation();assert(!app.scrub_.active && app.currentPage_==0);
        settings.displayOrientation=0;app.appSettings_.put(settings);app.toolbarVisible_=true;
        drag(app,.8f,SDL_FINGERDOWN);SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;event(app,lost);assert(!app.scrub_.active && app.currentPage_==0);
        lost.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;event(app,lost);
        pad(app,SDL_CONTROLLER_BUTTON_LEFTSTICK);assert(app.scrub_.active && app.scrub_.controller);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.scrub_.page==1 && app.currentPage_==0);
        SDL_Event axis{};axis.type=SDL_CONTROLLERAXISMOTION;axis.caxis.axis=SDL_CONTROLLER_AXIS_RIGHTX;axis.caxis.value=32767;event(app,axis);assert(app.scrub_.page==2 && app.currentPage_==0);
        axis.caxis.value=0;event(app,axis);pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.currentPage_==2 && app.pdfZoom_==2.5f);
        app.navigationJump(0);app.toolbarVisible_=true;int footerW,footerH;app.getWindowSize(footerW,footerH);tap(app,app.readerPages(footerW,footerH));assert(app.tocTab_==1 && app.tocCount()==40);
        int w,h;app.getWindowSize(w,h);waitPreview(app,0);snapshot(app,"pages-grid");
        pad(app,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);assert(app.tocTab_==2 && app.tocCount()==0 && app.currentPage_==0);
        pad(app,SDL_CONTROLLER_BUTTON_LEFTSHOULDER);assert(app.tocTab_==1 && app.tocCount()==40);
        app.navigationFocus_.select("toc.0");pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.tocSelected_==1 && app.currentPage_==0);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_DOWN);assert(app.tocSelected_==1+app.tocColumns(w));
        pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.currentPage_==1+app.tocColumns(w));app.navigationJump(0);app.openContents(1);
        app.currentTheme_=5;app.extraStyle_.backgroundColor=0;app.queueStyleChange();waitPreview(app,0);snapshot(app,"pages-grid-night");
        settings.displayOrientation=1;app.appSettings_.put(settings);app.openContents(1);waitPreview(app,0);snapshot(app,"pages-portrait");
        settings.displayOrientation=0;app.appSettings_.put(settings);app.openContents(1);
        const auto region=app.tocContent(w,h);touch(app,SDL_FINGERDOWN,region.x+100,region.y+220);touch(app,SDL_FINGERMOTION,region.x+100,region.y+50);touch(app,SDL_FINGERUP,region.x+100,region.y+50);
        assert(app.tocScroll_>0 && app.currentPage_==0 && app.screen_==ScreenState::TocModal);
        app.listVelocity_=0;app.tocScroll_=float(20/app.tocColumns(w)*app.tocStep());tap(app,app.tocCell(20,w,h));assert(app.currentPage_==20 && app.screen_==ScreenState::Reader);
        app.toggleBookmark();app.navigationJump(21);app.toggleBookmark();app.openContents(2);assert(app.tocCount()==2 && app.tocItem(0).page==20 && app.tocItem(1).page==21);
        waitPreview(app,20);snapshot(app,"bookmarked-pages");tap(app,app.tocCell(0,w,h));assert(app.currentPage_==20);
        app.openContents(0);assert(app.tocCount()==2 && app.tocItem(0).title.find("Bölüm 1")!=std::string::npos);waitPreview(app,0);snapshot(app,"comic-chapters");app.activateContentsItem(1);assert(app.currentPage_==10);
        app.extraStyle_.readingMode=1;app.currentPage_=3;app.scrollY_=100;app.startReaderScrub(true);app.setScrubFraction(1);app.finishReaderScrub();assert(app.currentPage_==39 && std::abs(app.readingFraction()-1)<.001);
        app.jumpBack();assert(app.currentPage_==3 && std::abs(app.scrollY_-100)<1);
        // Mouse fallback reaches the same drag transaction.
        app.extraStyle_.readingMode=0;app.currentPage_=0;app.toolbarVisible_=true;app.sawFinger_=false;
        const auto track=app.readerTrack(w,h);SDL_Event mouse{};mouse.type=SDL_MOUSEBUTTONDOWN;mouse.button.button=SDL_BUTTON_LEFT;mouse.button.x=track.x;mouse.button.y=track.y;event(app,mouse);
        mouse.type=SDL_MOUSEMOTION;mouse.motion.x=track.x+track.w;mouse.motion.y=track.y;event(app,mouse);assert(app.scrub_.page==39 && app.currentPage_==0);
        mouse.type=SDL_MOUSEBUTTONUP;mouse.button.button=SDL_BUTTON_LEFT;mouse.button.x=track.x+track.w;mouse.button.y=track.y;event(app,mouse);assert(app.currentPage_==39);
        app.closeBook();app.openBook(comic);assert(app.currentPage_==39 && app.progressStore_.get(comic.id).bookmarks.size()==2);
        app.closeBook();app.openBook(book("manga.cbz"));assert(app.readingDirection()==-1);app.toolbarVisible_=true;
        drag(app,0,SDL_FINGERDOWN);assert(app.scrub_.page==app.pageCount_-1);drag(app,0,SDL_FINGERUP);assert(app.currentPage_==app.pageCount_-1);
        app.openContents(1);app.getWindowSize(w,h);assert(app.tocCell(0,w,h).x>app.tocCell(1,w,h).x);pad(app,SDL_CONTROLLER_BUTTON_B);
        app.closeBook();app.openBook(book("single.cbz"));app.toolbarVisible_=true;drag(app,1,SDL_FINGERDOWN);assert(app.scrub_.page==0);waitPreview(app,0);drag(app,1,SDL_FINGERUP);assert(app.currentPage_==0);
        app.closeBook();app.openBook(book("sample.cbr"));app.openContents();waitPreview(app,0);assert(app.tocTab_==1);app.activateContentsItem(1);assert(app.currentPage_==1);
        app.closeBook();app.openBook(epub);app.openContents(0);assert(app.readingDirection()==1 && app.tocCount()>=1);
        const auto title=app.tocItem(0).title;assert(!title.empty() && title.size()<256);
        waitPreview(app,app.tocItem(0).page);snapshot(app,"epub-chapters");
        app.closeBook();app.openBook(epub);
        app.extraStyle_.readingMode=1;app.reflowEpub(app.currentFontSize_);assert(app.continuous());app.currentPage_=0;app.scrollY_=0;
        app.openContents(0);
        waitPreview(app,0,0);snapshot(app,"continuous-chapter-preview");
        app.closeBook();app.openBook(pdf);app.zoomPdf(2);app.toolbarVisible_=true;drag(app,1,SDL_FINGERDOWN);waitPreview(app,app.pageCount_-1);drag(app,1,SDL_FINGERUP);assert(app.currentPage_==app.pageCount_-1 && app.pdfZoom_==2);
        // An old in-flight decode cannot populate a newly opened document's cache.
        app.openContents(1);app.preparePagePreviews();const auto oldKey=app.previewKey(0,0);app.pagePreviews_.request(0,0,true);
        app.closeBook();app.openBook(comic);app.openContents(1);waitPreview(app,0);assert(!app.pagePreviewCache_.get(oldKey));assert(app.pagePreviewCache_.currentBytes()<=16u*1024u*1024u);
        app.tocEntries_.clear();app.changeContentsTab(0);assert(app.tocCount()==0);snapshot(app,"no-chapters");app.changeContentsTab(1);assert(app.tocCount()==40);
        app.closeBook();app.openBook(book("broken.cbz"));app.openContents(1);app.preparePagePreviews();
        const auto failed=app.previewKey(1,0);const auto deadline=SDL_GetTicks()+5000;
        while(SDL_GetTicks()<deadline && !app.failedPreviews_.count(failed)){app.render();app.processPagePreviews();SDL_Delay(5);}
        assert(app.failedPreviews_.count(failed));snapshot(app,"damaged-preview");app.activateContentsItem(1);app.render();
        const auto damagedPageDeadline=SDL_GetTicks()+5000;
        while(app.pageRenderError_.empty() && SDL_GetTicks()<damagedPageDeadline){app.processComicPages();app.render();SDL_Delay(5);}
        assert(!app.pageRenderError_.empty());
        app.navigationJump(2);const auto recoveredPageDeadline=SDL_GetTicks()+5000;
        while(!app.pageCache_.get(app.comicTextureKey(2)) && SDL_GetTicks()<recoveredPageDeadline){app.processComicPages();app.render();SDL_Delay(5);}
        app.render();assert(app.pageCache_.get(app.comicTextureKey(2)) && app.pageRenderError_.empty());
        app.closeBook();
        std::cout<<"PASS: transactional scrubbing and preview cache, touch/mouse/controllers, both rotations, RTL, grid scroll/selection, chapter/bookmark jumps, comic chapters, continuous anchors, CBR/PDF/single-page and stale decode isolation\n";
    }
};
}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-page-navigation";fs::remove_all(root);fs::create_directories(root/"books");
    for(const auto &pair:std::vector<std::pair<std::string,std::string>>{{"comics/navigation.cbz","navigation.cbz"},{"comics/manga.cbz","manga.cbz"},{"comics/single-page.cbz","single.cbz"},{"comics/sample-rar5.cbr","sample.cbr"},{"comics/broken-page.cbz","broken.cbz"},{"sample_book.epub","sample.epub"},{"sample_doc.pdf","sample.pdf"}})fs::copy_file(fs::path("tests/fixtures")/pair.first,root/"books"/pair.second);
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();
}
