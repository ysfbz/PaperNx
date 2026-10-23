#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>
namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    static void touch(PaperNxApp &app,Uint32 type,float x,float y) {
        SDL_Event event{};event.type=type;event.tfinger.fingerId=63;event.tfinger.x=x;event.tfinger.y=y;
        bool running=true;app.handleEvent(event,running);assert(running);
    }
    static void pad(PaperNxApp &app,Uint8 button) {
        SDL_Event event{};event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=button;
        bool running=true;app.handleEvent(event,running);assert(running);
    }
    static void finish(PaperNxApp &app) {
        app.pageTurnAnimation_.started=SDL_GetTicks()-2000;app.tickPageTurn();
    }
    static std::vector<Uint32> pixels(PaperNxApp &app) {
        app.render();int w,h;app.getWindowSize(w,h);std::vector<Uint32> out(w*h);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,SDL_PIXELFORMAT_ARGB8888,out.data(),w*4)==0);return out;
    }
    static void screenshot(PaperNxApp &app,const std::string &name) {
        auto data=pixels(app);int w,h;app.getWindowSize(w,h);
        auto *image=SDL_CreateRGBSurfaceWithFormatFrom(data.data(),w,h,32,w*4,SDL_PIXELFORMAT_ARGB8888);assert(image);
        fs::create_directories("verification/reader-input");assert(SDL_SaveBMP(image,("verification/reader-input/"+name+".bmp").c_str())==0);SDL_FreeSurface(image);
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(initReady(app));ui::language=1;
        assert(!app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        for(int language:{0,1}) {
            ui::language=language;
            for(auto size:{std::pair<int,int>{1280,720},{720,1280}}) {
                const auto sheet=app.quickSheet(size.first,size.second);
                const auto pages=app.quickPageShortcut(sheet),orientation=app.quickOrientationShortcut(sheet);
                assert(ui::contains(sheet,pages.x,pages.y) && ui::contains(sheet,orientation.x+orientation.w-1,orientation.y+orientation.h-1));
                assert(!SDL_HasIntersection(&pages,&orientation));
                int height=0;app.typography_->measureText(app.typography_->buttonFont(),app.quickSpreadLabel(),nullptr,&height,pages.w-24);
                assert(height<=pages.h-8);
                app.typography_->measureText(app.typography_->buttonFont(),ui::label("Portrait / landscape"),nullptr,&height,orientation.w-24);
                assert(height<=orientation.h-8);
            }
        }
        ui::language=1;
        // Joy-Con + opens the library menu; D-pad and A reach its pinned page shortcut.
        touch(app,SDL_FINGERDOWN,1272.f/1280,40.f/720);touch(app,SDL_FINGERUP,1272.f/1280,40.f/720);assert(!app.quickMenu_);
        pad(app,SDL_CONTROLLER_BUTTON_START);assert(app.quickMenu_);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_UP);assert(app.navigationFocus_.active()=="quick.pages");
        pad(app,SDL_CONTROLLER_BUTTON_A);assert(!app.quickMenu_ && app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        assert(app.appSettings_.save());
        pad(app,SDL_CONTROLLER_BUTTON_START);pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        assert(app.navigationFocus_.active()=="quick.orientation");pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.choiceKind_==1);
        pad(app,SDL_CONTROLLER_BUTTON_B);assert(app.choiceKind_<0 && !app.quickMenu_);
        auto book=[&](const char *name){auto it=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==name;});assert(it!=app.books_.end());return *it;};
        // The whole persistent plus control owns its corners as well as centre.
        app.openBook(book("english.epub"));app.toolbarVisible_=false;
        touch(app,SDL_FINGERDOWN,1260.f/1280,68.f/720);assert(!app.pageTurnTouch_.tracking);
        touch(app,SDL_FINGERUP,1260.f/1280,68.f/720);assert(app.quickMenu_);app.quickMenu_=false;app.closeBook();
        // The 0.16 external menu geometry and scan/settings routes are restored.
        ui::LibraryLayout library(1280);assert(library.top==174 && library.left==32);
        assert(app.quickSheet(1280,720).x==804);
        app.openSettings(true,5);assert(app.aaRow_==0 && app.settingsRows()==17);app.dismissSettings();
        for(const char *name:{"english.epub","hebrew.epub"})for(bool landscape:{true,false}) {
            SDL_SetWindowSize(app.window_,landscape?1280:720,landscape?720:1280);
            app.openBook(book(name));app.reflowEpub(24);const int dir=app.readingDirection();
            for(float y:{.015f,.06f,.5f,.94f,.985f}) {
                app.cancelPageTurn();app.currentPage_=2;app.toolbarVisible_=false;
                const float start=dir>0?.86f:.14f,end=dir>0?.36f:.64f;
                touch(app,SDL_FINGERDOWN,start,y);touch(app,SDL_FINGERMOTION,end,y);
                assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.dragging);
                assert(app.currentPage_==2 && app.pageTurnAnimation_.interactive);
                const float held=app.pageTurnAnimation_.progress;app.tickPageTurn();assert(app.pageTurnAnimation_.progress==held);
                touch(app,SDL_FINGERUP,end,y);assert(app.pageTurnAnimation_.complete);finish(app);
                assert(app.currentPage_==(landscape?4:3));
            }
            // A visible header remains a button, rather than starting a curl.
            app.toolbarVisible_=true;const auto header=app.readerHeaderButton(1,landscape?1280:720);
            const float x=(header.x+28.f)/(landscape?1280:720),y=36.f/(landscape?720:1280);
            touch(app,SDL_FINGERDOWN,x,y);assert(!app.pageTurnTouch_.tracking);touch(app,SDL_FINGERUP,x,y);
            assert(app.screen_==ScreenState::TocModal);app.closeContents();app.closeBook();
        }
        SDL_SetWindowSize(app.window_,1280,720);
        auto preferences=app.appSettings_.get();preferences.comicLandscapeSpreads=true;app.appSettings_.put(preferences);assert(app.appSettings_.save());
        app.openBook(book("smart.cbz"));assert(app.comicSpreadActive());
        app.toolbarVisible_=false;app.setPdfView(0);app.currentPage_=0;app.render();
        // Warm both pages in the next group; turning must reuse these textures.
        app.prefetchPage_=0;app.prefetchStep_=0;
        for(int i=0;i<4;++i){app.prefetchDue_=0;app.idlePrefetch();}
        const auto warmDeadline=SDL_GetTicks()+10000;
        while(app.comicPages_.pending() && SDL_GetTicks()<warmDeadline){app.processComicPages();SDL_Delay(2);}
        app.processComicPages();assert(!app.comicPages_.pending());
        const auto decodes=app.comicTextureBuilds_;app.turnPage(1);
        assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.comicSheet);
        assert(app.comicTextureBuilds_==decodes);finish(app);
        // A touch in the empty outer margin is owned immediately, without
        // folding any background, and grips only after crossing actual paper.
        app.cancelPageTurn();app.currentPage_=0;app.setPdfView(0);app.toolbarVisible_=false;
        const auto bounds=app.comicTurnBounds(0);assert(bounds.x>0 && bounds.x+bounds.w<1280);
        const float start=(bounds.x+bounds.w+20.f)/1280,near=(bounds.x+bounds.w+5.f)/1280;
        touch(app,SDL_FINGERDOWN,start,.5f);touch(app,SDL_FINGERMOTION,near,.5f);
        assert(app.pageTurnTouch_.tracking && !app.pageTurnAnimation_.active);
        const float end=(bounds.x+bounds.w*.2f)/1280;
        touch(app,SDL_FINGERMOTION,end,.5f);
        assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.dragging && app.pageTurnAnimation_.comicSheet);
        assert(app.pageTurnAnimation_.viewport.y>=72 && app.pageTurnAnimation_.viewport.h<720);
        touch(app,SDL_FINGERUP,end,.5f);finish(app);assert(app.currentPage_==2);
        // A gesture wholly above the artwork consumes no page turn on release.
        app.currentPage_=0;touch(app,SDL_FINGERDOWN,.85f,.01f);touch(app,SDL_FINGERMOTION,.35f,.01f);
        assert(!app.pageTurnAnimation_.active);touch(app,SDL_FINGERUP,.35f,.01f);assert(app.currentPage_==0);
        // Zoomed spread keeps two pages and supports controller curls; a
        // horizontal outer-edge drag turns, while an interior drag still pans.
        app.zoomPdf(2,.5f,.5f,true);app.pdfPanY_=0;app.pdfPanX_=0;
        app.turnPage(1);assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.comicSheet);finish(app);
        app.currentPage_=0;app.pdfPanX_=0;touch(app,SDL_FINGERDOWN,.94f,.5f);touch(app,SDL_FINGERMOTION,.3f,.5f);
        assert(app.pageTurnAnimation_.dragging);touch(app,SDL_FINGERUP,.3f,.5f);finish(app);
        app.closeBook();app.openBook(book("paper-bounds.cbz"));app.toolbarVisible_=false;
        for(float zoom:{1.f,1.01f}) {
            app.setPdfView(0);app.currentPage_=0;if(zoom>1)app.zoomPdf(zoom,.5f,.5f,true);
            app.pdfPanX_=app.pdfPanY_=0;
            const auto paper=app.comicTurnBounds(0);
            if(paper.y<72 || paper.y+paper.h>676)std::cerr<<"Comic page bounds outside reader clip: zoom="<<zoom<<" y="<<paper.y<<" h="<<paper.h<<"\n";
            assert(paper.y>=72 && paper.y+paper.h<=676);
            auto frame=pixels(app);const auto bg=app.palette().bg;const auto background=(Uint32(bg.r)<<16)|(Uint32(bg.g)<<8)|bg.b;
            // Neither half of the spine may continue through empty space.
            int seam=640;if(zoom>1)seam=int(app.currentMargin_+(640-app.currentMargin_)*zoom);
            for(int y=73;y<paper.y-2;++y)for(int x=seam-1;x<=seam;++x)assert((frame[y*1280+x]&0xffffff)==background);
            for(int y=paper.y+paper.h+2;y<676;++y)for(int x=seam-1;x<=seam;++x)assert((frame[y*1280+x]&0xffffff)==background);
            screenshot(app,zoom==1?"fit-bounded-spine":"zoom-bounded-spine");
            app.turnPage(1);auto &turn=app.pageTurnAnimation_;assert(turn.spread && turn.comicSheet);
            assert(turn.viewport.y==paper.y && turn.viewport.h==paper.h && turn.viewport.w<640);
            // Draw at a held progress, proving that the real paper mesh and its
            // folio snapshots survive eviction and leave the outer chrome clear.
            app.pageCache_.clear();turn.dragging=true;
            for(float progress:{0.f,.25f,.5f,.75f,1.f}) {
                turn.progress=progress;frame=pixels(app);
                for(int y=0;y<6;++y)for(int x=0;x<1280;++x)assert((frame[y*1280+x]&0xffffff)==background);
                int footerInk=0;for(int y=676;y<720;++y)for(int x=0;x<1280;++x)footerInk+=(frame[y*1280+x]&0xffffff)!=background;
                assert(footerInk<1000); // Folios move as ink, without a folded fullscreen background strip.
                screenshot(app,std::string(zoom==1?"paper-":"zoom-paper-")+std::to_string(int(progress*100)));
            }
            turn.dragging=false;finish(app);
        }
        app.closeBook();std::cout<<"PASS: 0.16 menu rollback; live edge drags LTR/RTL portrait/landscape; comic margin contact, warm next pair and zoomed paging\n";
    }
};
}
int main(){assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
 auto root=fs::temp_directory_path()/"papernx-reader-input";fs::remove_all(root);fs::create_directories(root/"books");
 for(auto pair:std::vector<std::pair<std::string,std::string>>{{"sample_book.epub","english.epub"},{"hebrew_navigation.epub","hebrew.epub"},{"comics/smart-spreads.cbz","smart.cbz"},{"comics/navigation.cbz","paper-bounds.cbz"}})fs::copy_file(fs::path("tests/fixtures")/pair.first,root/"books"/pair.second);
 fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();}
