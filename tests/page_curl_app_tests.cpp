#define main papernx_application_entry
#include "../src/main.cpp"
#include "test_evidence.hpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    static void snapshot(PaperNxApp &app,const std::string &name) {
        app.render();
        SDL_Texture *target=SDL_GetRenderTarget(app.renderer_);
        if(app.orientationTarget_) SDL_SetRenderTarget(app.renderer_,app.orientationTarget_);
        int w,h;SDL_GetRendererOutputSize(app.renderer_,&w,&h);
        auto surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
        assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
        const auto folder=test_evidence::path("page-curl");fs::create_directories(folder);
        assert(SDL_SaveBMP(surface,(folder/(name+".bmp")).string().c_str())==0);
        SDL_FreeSurface(surface);SDL_SetRenderTarget(app.renderer_,target);
    }
    static void touch(PaperNxApp &app,Uint32 type,float x,float y,SDL_FingerID id=17) {
        SDL_Event event{};event.type=type;event.tfinger.fingerId=id;
        event.tfinger.x=x;event.tfinger.y=y;
        bool running=true;app.handleEvent(event,running);assert(running);
    }
    static void interactive(PaperNxApp &app) {
        app.cancelPageTurn();app.currentPage_=0;app.toolbarVisible_=false;
        touch(app,SDL_FINGERDOWN,.9f,.22f);
        assert(!app.pageTurnAnimation_.active);
        touch(app,SDL_FINGERMOTION,.65f,.26f);
        assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.dragging);
        assert(app.currentPage_==0 && app.pageTurnAnimation_.anchorY<.25f);
        const float held=app.pageTurnAnimation_.progress;
        app.pageTurnAnimation_.started=SDL_GetTicks()-2000;
        app.tickPageTurn();
        assert(app.pageTurnAnimation_.progress==held); // Holding never advances with time.
        touch(app,SDL_FINGERMOTION,.4f,.35f);
        assert(app.pageTurnAnimation_.progress>held && app.pageTurnAnimation_.dragY>0);
        app.render();
        touch(app,SDL_FINGERUP,.4f,.35f);
        assert(!app.pageTurnAnimation_.dragging && app.pageTurnAnimation_.complete && app.currentPage_==0);
        finish(app);assert(app.currentPage_==1);
        touch(app,SDL_FINGERDOWN,.12f,.8f);
        touch(app,SDL_FINGERMOTION,.7f,.67f);
        if(app.pageTurnAnimation_.direction!=-1 || app.pageTurnAnimation_.anchorY<=.8f)
            std::cerr<<"Reverse curl state: active="<<app.pageTurnAnimation_.active<<" dragging="<<app.pageTurnAnimation_.dragging
                <<" direction="<<app.pageTurnAnimation_.direction<<" anchor="<<app.pageTurnAnimation_.anchorY
                <<" page="<<app.currentPage_<<" readingDirection="<<app.readingDirection()<<" layoutPending="<<app.layoutPending_<<"\n";
        assert(app.pageTurnAnimation_.direction==-1 && app.pageTurnAnimation_.anchorY>=.8f);
        assert(app.pageTurnAnimation_.dragY<0);
        touch(app,SDL_FINGERUP,.7f,.67f);finish(app);assert(app.currentPage_==0);
        // Return the held sheet to its edge: cancellation never changes progress.
        touch(app,SDL_FINGERDOWN,.92f,.2f);
        touch(app,SDL_FINGERMOTION,.5f,.22f);
        touch(app,SDL_FINGERMOTION,.91f,.21f);
        touch(app,SDL_FINGERUP,.91f,.21f);
        assert(!app.pageTurnAnimation_.complete);finish(app);assert(app.currentPage_==0);
        touch(app,SDL_FINGERDOWN,.9f,.7f);
        touch(app,SDL_FINGERMOTION,.5f,.65f);
        touch(app,SDL_FINGERDOWN,.4f,.5f,18);
        assert(!app.pageTurnAnimation_.active && !app.pageTurnTouch_.tracking);
        touch(app,SDL_FINGERUP,.4f,.5f,18);touch(app,SDL_FINGERUP,.5f,.65f);
        assert(app.currentPage_==0);
        touch(app,SDL_FINGERDOWN,.15f,.4f);touch(app,SDL_FINGERMOTION,.8f,.4f);
        assert(!app.pageTurnAnimation_.active);touch(app,SDL_FINGERUP,.8f,.4f);
        assert(app.currentPage_==0);
        touch(app,SDL_FINGERDOWN,.5f,.3f);touch(app,SDL_FINGERMOTION,.49f,.7f);
        assert(!app.pageTurnAnimation_.active);touch(app,SDL_FINGERUP,.49f,.7f);
        // A short flick commits, whereas the same slow displacement cancels.
        touch(app,SDL_FINGERDOWN,.9f,.5f);
        app.pageTurnTouch_.lastTick=SDL_GetTicks()-16;
        touch(app,SDL_FINGERMOTION,.8f,.5f);
        touch(app,SDL_FINGERUP,.8f,.5f);
        assert(app.pageTurnAnimation_.complete);finish(app);assert(app.currentPage_==1);
        app.currentPage_=0;
        touch(app,SDL_FINGERDOWN,.9f,.5f);
        app.pageTurnTouch_.lastTick=SDL_GetTicks()-500;
        touch(app,SDL_FINGERMOTION,.8f,.5f);
        touch(app,SDL_FINGERUP,.8f,.5f);
        assert(!app.pageTurnAnimation_.complete);finish(app);assert(app.currentPage_==0);
    }
    static void portrait(PaperNxApp &app) {
        auto settings=app.appSettings_.get();settings.displayOrientation=1;app.appSettings_.put(settings);
        app.reflowEpub(22);app.currentPage_=0;app.toolbarVisible_=false;
        auto logicalTouch=[&](Uint32 type,float x,float y) {
            int w,h;SDL_GetWindowSize(app.window_,&w,&h);
            DisplayTransform transform{w,h,1};
            const auto point=transform.toPhysical(x*transform.logicalWidth(),y*transform.logicalHeight());
            touch(app,type,point.first/w,point.second/h);
        };
        for(int theme:{0,5}) {
            app.reflowEpub(22,theme);
            for(float grip:{.18f,.82f}) {
                const std::string name=std::string(theme==0?"light-":"night-")+(grip<.5f?"upper-":"lower-");
                logicalTouch(SDL_FINGERDOWN,.92f,grip);
                for(int frame=0;frame<=12;++frame) {
                    const float x=.92f-float(frame)*.075f;
                    const float y=grip+(grip<.5f?1.f:-1.f)*float(frame)*.012f;
                    logicalTouch(SDL_FINGERMOTION,x,y);
                    snapshot(app,name+std::to_string(frame));
                }
                assert(app.pageTurnAnimation_.dragging && app.currentPage_==0);
                logicalTouch(SDL_FINGERUP,.02f,grip+(grip<.5f?.144f:-.144f));
                assert(app.pageTurnAnimation_.complete);
                for(int frame=0;frame<6;++frame) {
                    app.pageTurnAnimation_.started=SDL_GetTicks()-frame*50;
                    snapshot(app,name+std::to_string(frame+13));
                }
                finish(app);assert(app.currentPage_==1);
                app.currentPage_=0;
            }
        }
        settings.displayOrientation=0;app.appSettings_.put(settings);app.reflowEpub(22,0);
    }
    static void preview(PaperNxApp &app, const char *direction) {
        const auto folder=test_evidence::path("page-curl");fs::create_directories(folder);
        for (int frame = 0; frame <= 10; ++frame) {
            app.pageTurnAnimation_.started = SDL_GetTicks() - frame * 40;
            app.render();
            int w, h; SDL_GetRendererOutputSize(app.renderer_, &w, &h);
            auto surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
            assert(surface);
            assert(SDL_RenderReadPixels(app.renderer_, nullptr, surface->format->format,
                                        surface->pixels, surface->pitch) == 0);
            auto path = folder/(std::string("epub-") + direction + "-" + std::to_string(frame) + ".bmp");
            assert(SDL_SaveBMP(surface, path.string().c_str()) == 0);
            SDL_FreeSurface(surface);
        }
    }
    static void finish(PaperNxApp &app) {
        assert(app.pageTurnAnimation_.active);
        app.pageTurnAnimation_.started = SDL_GetTicks() - PaperNxApp::PAGE_TURN_MS;
        app.tickPageTurn();
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root); assert(initReady(app));
        auto preferences=app.appSettings_.get();preferences.bookLandscapeSpreads=false;preferences.comicLandscapeSpreads=false;app.appSettings_.put(preferences);
        SDL_RendererInfo driver{};SDL_GetRendererInfo(app.renderer_,&driver);
        std::cout << "page curl render driver: " << driver.name << '\n';
        auto epub = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Epub; });
        app.openBook(epub);
        app.extraStyle_.readingMode = 0;
        app.documentMode_ = 0;
        app.reflowEpub(22);
        assert(app.pageCount_ >= 3);
        app.currentPage_ = 0;
        app.turnPage(-1); assert(!app.pageTurnAnimation_.active && app.currentPage_ == 0);
        app.turnPage(1);
        assert(app.currentPage_ == 1 && app.pageTurnAnimation_.active);
        assert(app.pageTurnAnimation_.previous && app.pageTurnAnimation_.next);
        preview(app, "forward");
        // Snapshots survive complete LRU eviction while rendering a curl frame.
        app.pageCache_.clear();
        app.pageTurnAnimation_.started = SDL_GetTicks() - 200;
        app.render();
        app.turnPage(1);
        assert(app.currentPage_ == 1 && app.pageTurnAnimation_.queued == 1);
        finish(app); assert(app.currentPage_ == 2 && app.pageTurnAnimation_.active);
        finish(app); assert(!app.pageTurnAnimation_.active);
        app.turnPage(-1); assert(app.pageTurnAnimation_.direction == -1);
        preview(app, "backward");
        finish(app); assert(app.currentPage_ == 1);
        app.turnPage(1); app.turnPage(-1); finish(app);
        assert(app.currentPage_ == 1 && app.pageTurnAnimation_.direction == -1);
        finish(app);
        app.turnPage(1); app.openSettings();
        assert(!app.pageTurnAnimation_.active && !app.pageTurnAnimation_.previous);
        app.dismissSettings();
        app.turnPage(-1); app.layoutPending_ = true; app.tickPageTurn();
        assert(!app.pageTurnAnimation_.active); app.layoutPending_ = false;
        app.documentMode_ = 1; app.turnPage(1); assert(!app.pageTurnAnimation_.active);
        app.documentMode_ = 0;
        app.turnPage(-1); app.appBackground_ = true; app.tickPageTurn();
        assert(!app.pageTurnAnimation_.active); app.appBackground_ = false;
        app.turnPage(1); app.closeBook(); assert(!app.pageTurnAnimation_.next);
        app.openBook(epub);app.extraStyle_.readingMode=0;app.documentMode_=0;
        app.reflowEpub(22);interactive(app);portrait(app);app.closeBook();

        auto pdf = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Pdf; });
        app.openBook(pdf); app.currentPage_ = 0; app.setPdfView(0);
        float firstW, firstH, nextW, nextH;
        assert(px_document_page_size(app.doc_, 0, &firstW, &firstH));
        assert(px_document_page_size(app.doc_, 1, &nextW, &nextH));
        assert(firstW != nextW && firstH != nextH);
        app.turnPage(1); assert(app.pageTurnAnimation_.active); finish(app);
        app.turnPage(-1); app.zoomPdf(2.f); assert(!app.pageTurnAnimation_.active);
        app.turnPage(1); assert(!app.pageTurnAnimation_.active);
        app.setPdfView(0); app.turnPage(-1); assert(app.pageTurnAnimation_.active);
        app.currentPage_ = app.pageCount_ - 1; app.tickPageTurn();
        assert(!app.pageTurnAnimation_.active);
        app.turnPage(1); assert(!app.pageTurnAnimation_.active);
        app.closeBook();

        // Test persisted pageCurlAnimation toggle (Test E)
        app.openBook(epub);
        assert(app.readingDirection()==1);
        app.currentPage_=0;
        auto prefs=app.appSettings_.get();
        assert(prefs.pageCurlAnimation);
        prefs.pageCurlAnimation=false;
        app.appSettings_.put(prefs);
        assert(!app.canCurlPage());
        app.turnPage(1);
        assert(!app.pageTurnAnimation_.active && app.currentPage_==1);
        prefs.pageCurlAnimation=true;
        app.appSettings_.put(prefs);
        assert(app.canCurlPage());
        app.turnPage(-1);
        assert(app.pageTurnAnimation_.active && app.currentPage_==0);
        finish(app);
        app.closeBook();
        app.openBook(pdf);assert(app.readingDirection()==1);app.closeBook();
    }
};
}

int main() {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)==0);
    assert(TTF_Init()==0);
    const auto root = fs::temp_directory_path() / "papernx-page-curl-regressions";
    fs::remove_all(root); fs::create_directories(root / "books");
    fs::copy_file("tests/fixtures/sample_book.epub", root / "books" / "sample.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf", root / "books" / "sample.pdf");
    // Keep xref offsets valid while changing only the second page's aspect ratio.
    const auto pdfPath = root / "books" / "sample.pdf";
    std::ifstream input(pdfPath, std::ios::binary);
    std::string pdf((std::istreambuf_iterator<char>(input)), {}); input.close();
    auto media = pdf.rfind("MediaBox [0 0 595 842]");
    assert(media != std::string::npos);
    pdf.replace(media, 22, "MediaBox [0 0 842 595]");
    std::ofstream(pdfPath, std::ios::binary).write(pdf.data(), pdf.size());
    fs::create_directory_symlink(fs::absolute("fonts"), root / "fonts");
    AppTest::run(root);
    fs::remove_all(root);TTF_Quit();SDL_Quit();
    std::cout << "[PASS] page curl EPUB/PDF, queue/reversal, cache eviction, cancellation and boundaries\n";
}
