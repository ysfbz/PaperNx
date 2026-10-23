#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include "papernx/switch_compat.h"
#include "papernx/focus.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#ifdef NDEBUG
#error Assertions must remain enabled
#endif

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    // Workflow assertions describe settled pages. Dedicated curl tests exercise
    // intermediate frames and queued input with the animation still running.
    static void settlePageTurn(PaperNxApp &app) {
        while (app.pageTurnAnimation_.active) {
            app.pageTurnAnimation_.started = SDL_GetTicks() - PaperNxApp::PAGE_TURN_MS;
            app.tickPageTurn();
        }
    }
    static void key(PaperNxApp &app, SDL_Keycode code) {
        SDL_Event e{}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
        bool running = true; app.handleEvent(e, running); assert(running);
        settlePageTurn(app);
    }
    static void pad(PaperNxApp &app, Uint8 button) {
        SDL_Event e{}; e.type = SDL_CONTROLLERBUTTONDOWN; e.cbutton.button = button;
        bool running = true; app.handleEvent(e, running); assert(running);
        settlePageTurn(app);
    }
    static void finger(PaperNxApp &app, Uint32 type, float x, float y, SDL_FingerID id = 41) {
        SDL_Event e{}; e.type = type; e.tfinger.fingerId = id; e.tfinger.x = x; e.tfinger.y = y;
        bool running = true; app.handleEvent(e, running); assert(running);
        if (type == SDL_FINGERUP) settlePageTurn(app);
    }
    static void tap(PaperNxApp &app, float x, float y) {
        finger(app, SDL_FINGERDOWN, x, y); finger(app, SDL_FINGERUP, x, y);
    }
    static void settingsControl(PaperNxApp &app,int row,bool right=true) {
        int w,h; app.getWindowSize(w,h); auto r=ui::SettingsLayout(w,h).control(row);
        app.aaRow_=row;
        if(app.numericSetting())r=right?ui::StepperLayout(r).plus:ui::StepperLayout(r).minus;
        tap(app,float(r.x+int(r.w*(right?.75:.25)))/w,float(r.y+28)/h);
        if(app.choiceKind_>=0){
            // The real choice sheet confirms focused (not stale selected) controls.
            // Keep the helper's historical next-option semantics for all fixtures.
            const auto count=int(app.choiceOptions().size());
            assert(count>0);
            app.choiceFocusedIndex_=(app.choiceFocusedIndex_+1)%count;
            pad(app,SDL_CONTROLLER_BUTTON_A);
        }
    }
    static void brightnessSlider(PaperNxApp &app) {
        const auto original=app.appSettings_.get();auto prefs=original;
        prefs.displayOrientation=1;prefs.softwareBrightness=100;prefs.systemBrightness=100;app.appSettings_.put(prefs);
        app.openSettings(true,2);int w,h,pw,ph;app.getWindowSize(w,h);SDL_GetWindowSize(app.window_,&pw,&ph);
        const DisplayTransform transform{pw,ph,1};const auto layout=ui::SettingsLayout(w,h);app.settingsScroll_=app.maxUiScroll();
        auto drag=[&](int row,int low,int high,int value,SDL_FingerID id) {
            auto control=layout.control(row);control.y-=int(app.settingsScroll_);const auto track=ui::settingsSliderTrack(control);
            const int y=control.y+control.h/2;const int x=track.x+(track.w-1)*(value-low)/(high-low);
            auto send=[&](Uint32 type,int px) {const auto point=transform.toPhysical(float(px),float(y));finger(app,type,point.first/pw,point.second/ph,id);};
            send(SDL_FINGERDOWN,track.x);send(SDL_FINGERMOTION,x);send(SDL_FINGERUP,x);
        };
        drag(8,50,100,80,91);drag(9,10,100,55,92);
        assert(app.appSettings_.get().softwareBrightness==80 && app.appSettings_.get().systemBrightness==55);
        assert(app.appSettings_.save());AppSettingsStore restored(app.root_/"data"/"settings.txt");
        assert(restored.load() && restored.get().softwareBrightness==80 && restored.get().systemBrightness==55);
        auto reset=layout.control(10);reset.y-=int(app.settingsScroll_);
        const auto point=transform.toPhysical(reset.x+reset.w/2,reset.y+reset.h/2);tap(app,point.first/pw,point.second/ph);
        assert(app.appSettings_.get().softwareBrightness==100 && app.appSettings_.get().systemBrightness==100);
        app.dismissSettings();prefs=app.appSettings_.get();prefs.displayOrientation=original.displayOrientation;app.appSettings_.put(prefs);app.appSettings_.save();
    }
    static void settingsTab(PaperNxApp &app,int tab) {
        int w,h; app.getWindowSize(w,h); auto r=ui::SettingsLayout(w,h).tab(tab);
        tap(app,float(r.x+r.w/2)/w,float(r.y+28)/h);
    }
    static void closeSettings(PaperNxApp &app) {
        int w,h; app.getWindowSize(w,h); auto r=ui::SettingsLayout(w,h).close;
        tap(app,float(r.x+r.w/2)/w,float(r.y+28)/h);
    }
    static void closeNavigation(PaperNxApp &app) { int w,h; app.getWindowSize(w,h); tap(app,float(w-128)/w,116.f/h); }
    #include "v03_app_workflow.hpp"
    #include "v03_stress_workflow.hpp"
    #include "v04_ui_workflow.hpp"
    #include "v04_catalog_workflow.hpp"
    static void snapshot(PaperNxApp &app, const char *name) {
        for (int i = 0; i < 15; ++i) { app.render(); SDL_Delay(10); app.processCompletedThumbnails(); }
        app.render(); int w, h; SDL_GetRendererOutputSize(app.renderer_, &w, &h);
        SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0);
        fs::create_directories("verification/screenshots");
        assert(SDL_SaveBMP(surface, (std::string("verification/screenshots/") + name + ".bmp").c_str()) == 0);
        SDL_FreeSurface(surface);
    }
    static void captureV03Screens(const fs::path &root) {
        PaperNxApp app(root); assert(initReady(app));
        snapshot(app,"v03-library-final");
        v03Plus(app); snapshot(app,"v03-plus-menu"); tap(app,.1f,.5f);
        v03Plus(app); v03MenuItem(app,0); snapshot(app,"v03-settings-final");
        settingsTab(app,3); snapshot(app,"v03-navigation-final");
        SDL_SetWindowSize(app.window_,640,480); settingsTab(app,0); v03Control(app,5);
        snapshot(app,"v03-settings-640"); closeSettings(app);
        SDL_SetWindowSize(app.window_,1280,720);
        auto epub=std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Epub;});
        assert(epub!=app.books_.end()); app.openBook(*epub); tap(app,.5f,.5f);
        snapshot(app,"v03-reader-toolbar");
        app.openSettings(); settingsTab(app,5); v03Control(app,5); snapshot(app,"v03-application-english");
    }
    static void captureJournalScreens(const fs::path &root) {
        // Deterministic test fixture, never bundled as a user journal.
        PaperNxApp app(root); assert(initReady(app));
        assert(!app.books_.empty());
        const auto book = app.books_.front();
        app.journal_.registerBookAlias(book.id, "", book.title, book.author);
        app.journal_.interaction(1000);
        for (std::uint64_t ms = 1000; ms <= 121000; ms += 1000) {
            app.journal_.tick(ms, 1792367880 + std::int64_t(ms / 1000),
                "2026-10-18", book.id, book.title, book.author, 0.1 + double(ms) / 10000000., true);
        }
        app.journal_.pause(122000, 1792368003);

        const auto sessions = app.journal_.sessions();
        assert(!sessions.empty());
        std::uint64_t sampleSessionId = sessions.front().id;

        struct TargetConfig {
            int w, h;
            int orientation; // 0=none, 1=CW, 2=CCW
            std::string suffix;
        };

        std::vector<TargetConfig> configs = {
            {1280, 720, 0, "1280"},
            {1280, 720, 1, "portrait-cw"},
            {1280, 720, 2, "portrait-ccw"},
            {1920, 1080, 0, "1920"},
            {640, 360, 0, "640"}
        };

        for (const auto &cfg : configs) {
            SDL_SetWindowSize(app.window_, cfg.w, cfg.h);
            auto prefs = app.appSettings_.get();
            prefs.displayOrientation = cfg.orientation;
            app.appSettings_.put(prefs);

            app.enterJournal(false);

            // 1. Overview
            app.journalUI_.setTab(0);
            snapshot(app, ("journal-overview-" + cfg.suffix).c_str());

            // 2. Calendar
            app.journalUI_.setTab(1);
            snapshot(app, ("journal-calendar-" + cfg.suffix).c_str());

            // 3. Books
            app.journalUI_.setTab(2);
            snapshot(app, ("journal-books-" + cfg.suffix).c_str());

            // 4. Book detail
            app.journalUI_.setDetailBookId(book.id);
            snapshot(app, ("journal-book-detail-" + cfg.suffix).c_str());
            app.journalUI_.setDetailBookId("");

            // 5. History
            app.journalUI_.setTab(3);
            snapshot(app, ("journal-history-" + cfg.suffix).c_str());

            // 6. Goals modal
            app.journalUI_.openGoals();
            snapshot(app, ("journal-goals-" + cfg.suffix).c_str());
            app.journalUI_.back();

            // 7. Delete confirmation modal
            app.journalUI_.openDeleteConfirm(sampleSessionId);
            snapshot(app, ("journal-delete-" + cfg.suffix).c_str());
            app.journalUI_.back();

            // 8. Reader mini panel
            app.openBook(book);
            app.journalUI_.showPreview();
            app.screen_ = ScreenState::JournalPreview;
            snapshot(app, ("journal-reader-preview-" + cfg.suffix).c_str());
            app.journalUI_.hidePreview();
            app.closeBook();
        }

        // Restore default landscape
        SDL_SetWindowSize(app.window_, 1280, 720);
        auto resetPrefs = app.appSettings_.get();
        resetPrefs.displayOrientation = 0;
        app.appSettings_.put(resetPrefs);
    }
    static void continuousReading(const fs::path &root) {
        PaperNxApp app(root); assert(initReady(app));
        v03Plus(app); v03MenuItem(app,0); assert(app.globalSettings_ && app.screen_==ScreenState::AaModal);
        settingsTab(app,3); settingsControl(app,1); assert(app.extraStyle_.readingMode==1);
        closeSettings(app); assert(app.screen_==ScreenState::Library);
        auto epub=*std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Epub;});
        app.openBook(epub); assert(app.continuous());
        assert(app.segmentHeight(0)>720);
        app.extraStyle_.scrollInertia=0;
        finger(app,SDL_FINGERDOWN,.5f,.7f);
        finger(app,SDL_FINGERMOTION,.5f,.4f);
        assert(app.scrollY_>200 && !app.toolbarVisible_); // Moves before release.
        finger(app,SDL_FINGERUP,.5f,.4f); assert(app.scrollVelocity_==0 && !app.toolbarVisible_);
        // Optional momentum advances and decays; a new touch stops it.
        app.extraStyle_.scrollInertia=1; app.scrollVelocity_=500;
        app.physicsTick_=SDL_GetTicks()-16; float before=app.scrollY_; app.tickScroll();
        assert(app.scrollY_>before && app.scrollVelocity_>0 && app.scrollVelocity_<500);
        finger(app,SDL_FINGERDOWN,.5f,.5f); assert(app.scrollVelocity_==0);
        finger(app,SDL_FINGERUP,.5f,.5f); app.toolbarVisible_=false;
        app.extraStyle_.scrollInertia=0;
        float y=app.scrollY_; int page=app.currentPage_;
        finger(app,SDL_FINGERDOWN,.8f,.5f); finger(app,SDL_FINGERMOTION,.2f,.5f); finger(app,SDL_FINGERUP,.2f,.5f);
        assert(app.currentPage_==page && app.scrollY_==y && !app.toolbarVisible_);
        app.reflowEpub(app.currentFontSize_,2); assert(std::abs(app.scrollY_-y)<1);
        app.reflowEpub(30);
        int ch=-1,character=-1; double offset=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&ch,&character,&offset));
        app.extraStyle_.readingMode=0; app.extraStyle_.modeOverride=1; app.reflowEpub(30);
        assert(!app.continuous());
        app.extraStyle_.readingMode=1; app.reflowEpub(30);
        int ch2=-1,c2=-1; double o2=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&ch2,&c2,&o2));
        assert(ch==ch2 && character==c2 && std::abs(offset-o2)<.01);
        // Chapter boundaries are contiguous and reversible, including a partial viewport.
        app.currentPage_=0; app.scrollY_=app.segmentHeight(0)-100;
        app.render(); snapshot(app,"epub-scroll-chapter-boundary");
        app.scrollBy(150); assert(app.currentPage_==1 && std::abs(app.scrollY_-50)<1);
        app.scrollBy(-150); assert(app.currentPage_==0 && std::abs(app.scrollY_-(app.segmentHeight(0)-100))<1);
        app.scrollBy(-app.segmentHeight(0)); assert(app.currentPage_==0 && app.scrollY_==0);
        // Traverse enough strips to force eviction; memory must plateau.
        for(int chapter=0;chapter<app.pageCount_;++chapter) {
            const float height=app.segmentHeight(chapter);
            for(float strip=0;strip<height;strip+=1024) {
                app.currentPage_=chapter; app.scrollY_=strip; app.render();
                assert(app.pageCache_.currentBytes()<=64*1024*1024);
            }
        }
        app.currentPage_=0; app.scrollY_=0;
        // Geometry fallback supports image-only chapters without text anchors.
        int geometryPage=-1; float geometryY=0;
        assert(px_document_restore_anchor(app.doc_,0,-1,.25,&geometryPage,&geometryY));
        assert(geometryPage==0 && std::abs(geometryY-app.segmentHeight(0)*.25)<1);
        app.scrollBy(333.25f); app.saveCurrentPosition(); assert(app.flushProgress());
        y=app.scrollY_; app.closeBook(); app.openBook(epub);
        assert(app.continuous() && std::abs(app.scrollY_-y)<1);
        // Measure actual event-to-render CPU wall time on the dummy desktop backend.
        std::vector<double> samples;
        finger(app,SDL_FINGERDOWN,.5f,.8f);
        for(int i=0;i<180;++i) {
            auto start=std::chrono::steady_clock::now();
            finger(app,SDL_FINGERMOTION,.5f,.8f-(i+1)*.003f); app.render();
            samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            assert(app.pageCache_.currentBytes()<=64*1024*1024);
        }
        finger(app,SDL_FINGERUP,.5f,.26f);
        std::sort(samples.begin(),samples.end());
        std::cout<<"[SCROLL DESKTOP dummy] motion+render ms p50="<<samples[90]<<" p95="<<samples[171]<<" max="<<samples.back()<<" cache_bytes="<<app.pageCache_.currentBytes()<<"\n";
        assert(samples[171]<100); // Broad desktop regression ceiling; never a Switch claim.
        // Independent book override and global default survive a new application instance.
        app.saveCurrentPosition(); assert(app.flushProgress());
        y=app.scrollY_; page=app.currentPage_;
        app.closeBook();
        {
            PaperNxApp reopened(root); assert(initReady(reopened)); reopened.openBook(epub);
            assert(reopened.continuous() && reopened.extraStyle_.modeOverride==1);
            assert(reopened.currentPage_==page && std::abs(reopened.scrollY_-y)<1);
        }
        Progress defaults; app.progressStore_.put("PAPERNX_DEFAULTS",defaults); app.flushProgress();
    }
    static void touchOnly(const fs::path &root) {
        const auto start = std::chrono::steady_clock::now();
        std::string epubId, pdfId;
        int savedPage;
        {
            PaperNxApp app(root); assert(initReady(app)); assert(!app.controller_);
            auto preferences=app.appSettings_.get();preferences.bookLandscapeSpreads=false;preferences.comicLandscapeSpreads=false;preferences.displayOrientation=0;app.appSettings_.put(preferences);assert(app.appSettings_.save());
            auto epub = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Epub; });
            // Open first library card using only finger events.
            tap(app, .06f, .30f); assert(app.doc_ && app.screen_ == ScreenState::Reader);
            if (!px_document_is_epub(app.doc_)) { app.closeBook(); app.openBook(epub); }
            epubId = app.currentBookId_;
            for (int i = 0; i < 10; ++i) tap(app, .75f, .5f);
            if(app.currentPage_!=10)std::cerr<<"Single-page touch regression reached page "<<app.currentPage_<<"\n";
            assert(app.currentPage_ == 10);
            tap(app, .25f, .5f); assert(app.currentPage_ == 9);
            finger(app, SDL_FINGERDOWN, .7f, .5f); SDL_Delay(900);
            finger(app, SDL_FINGERMOTION, .45f, .51f); finger(app, SDL_FINGERUP, .4f, .51f);
            assert(app.currentPage_ == 10); // Slow swipe used to be rejected.
            finger(app, SDL_FINGERDOWN, .25f, .5f); SDL_Delay(550); finger(app, SDL_FINGERUP, .27f, .51f);
            assert(app.currentPage_ == 9); // Long tap and ordinary finger drift.
            finger(app, SDL_FINGERDOWN, .3f, .5f); finger(app, SDL_FINGERUP, .65f, .5f);
            assert(app.currentPage_ == 8);
            // Two-finger input cannot also turn a page.
            finger(app, SDL_FINGERDOWN, .8f, .5f, 1); finger(app, SDL_FINGERDOWN, .7f, .5f, 2);
            finger(app, SDL_FINGERUP, .8f, .5f, 1); finger(app, SDL_FINGERUP, .7f, .5f, 2);
            assert(app.currentPage_ == 8);
            // Synthesized mouse event arriving after the native finger is ignored.
            SDL_Event mouse{}; mouse.type = SDL_MOUSEBUTTONDOWN; mouse.button.which = SDL_TOUCH_MOUSEID;
            mouse.button.button = SDL_BUTTON_LEFT; mouse.button.x = 1100; mouse.button.y = 350;
            bool running = true; app.handleEvent(mouse, running); mouse.type = SDL_MOUSEBUTTONUP; app.handleEvent(mouse, running);
            assert(app.currentPage_ == 8);
            // A mouse-only SDL backend remains usable, on release (not down).
            app.sawFinger_ = false;
            mouse.type = SDL_MOUSEBUTTONDOWN; app.handleEvent(mouse, running); assert(app.currentPage_ == 8);
            mouse.type = SDL_MOUSEBUTTONUP; app.handleEvent(mouse, running); assert(app.currentPage_ == 9);
            tap(app, .5f, .5f); assert(app.toolbarVisible_);
            tap(app, .89f, .04f); assert(app.screen_ == ScreenState::AaModal);
            int oldCh=-1,oldCharacter=-1; double oldOffset=0;
            assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&oldCh,&oldCharacter,&oldOffset));
            settingsControl(app,0); assert(app.currentFontSize_ == 24);
            settingsControl(app,1); assert(app.currentFontFamily_ == 1);
            settingsTab(app,1); assert(app.aaTab_ == 1);
            settingsControl(app,3); assert(app.extraStyle_.paragraphSpacing == 100);
            settingsControl(app,2); assert(app.extraStyle_.indentation == 0);
            settingsTab(app,2); assert(app.aaTab_ == 2);
            for (int i = 0; i < 6; ++i) { settingsControl(app,i/2,i%2==1); assert(app.currentTheme_ == i); }
            assert(app.commitColor("123456", 0)); assert(app.commitColor("#EEDDBB", 1));
            assert(!app.commitColor("GGGGGG", 0));
            assert(app.extraStyle_.textColor == 0x123456 && app.extraStyle_.backgroundColor == 0xeeddbb);
            v03WaitLayout(app);
            int newCh=-1,newCharacter=-1; double newOffset=0;
            assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&newCh,&newCharacter,&newOffset));
            assert(newCh==oldCh && std::abs(newCharacter-oldCharacter)<400);
            snapshot(app, "epub-settings");
            // Global defaults are explicit; current book retains an independent override.
            app.storeDefaults();
            closeSettings(app); assert(app.screen_ == ScreenState::Reader);
            savedPage = app.currentPage_;
            int width,height;app.getWindowSize(width,height);const auto bookmark=app.readerHeaderButton(2,width);
            tap(app,float(bookmark.x+bookmark.w/2)/width,float(bookmark.y+bookmark.h/2)/height);
            assert(app.screen_ == ScreenState::BookmarksModal);
            tap(app,.5f,636.f/720); assert(app.progressStore_.get(epubId).bookmarks.size() == 1);
            closeNavigation(app); assert(app.screen_ == ScreenState::Reader);
            tap(app, .04f, .04f); assert(app.screen_ == ScreenState::Library && !app.progressDirty_);
            app.category_ = Category::Completed; app.updateVisibleBooks();
            assert(app.libraryEmptyMessage() == "Henüz tamamladığınız bir kitap yok.");
            snapshot(app, "library-empty-completed");
            app.category_ = Category::All; app.updateVisibleBooks();
            app.books_[0].title = "İstanbul'da çok uzun bir kitap başlığı — Türkçe karakterler ve çok satırlı kitap kartı";
            app.updateVisibleBooks(); snapshot(app, "library");
            for (auto size : {std::pair<int,int>{960,540}, {1920,1080}, {640,480}}) {
                SDL_SetWindowSize(app.window_, size.first, size.second); app.render();
            }
            SDL_SetWindowSize(app.window_, 1280, 720);
            auto pdf = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Pdf; });
            app.openBook(pdf); pdfId = pdf.id;
            assert(app.currentFontFamily_ == 1); // New book inherits global defaults.
            tap(app, .5f, .5f); tap(app, .89f, .04f);
            // A PDF opens the very same Appearance tab as EPUB.
            assert(app.aaTab_==2);
            settingsControl(app,1,true); // Select "Cool Paper" in Appearance.
            assert(app.extraStyle_.backgroundColor==0xe9ebe8);
            assert(app.extraStyle_.styleOverride==1);
            const auto pdfChrome=app.palette();
            assert(pdfChrome.bg.r==0xe9 && pdfChrome.bg.g==0xeb && pdfChrome.bg.b==0xe8);
            settingsTab(app,4); // PDF-only page fit/zoom remains separate.
            settingsControl(app,1); assert(app.extraStyle_.pdfViewMode == 1 && app.pdfZoom_ > 1);
            snapshot(app, "pdf-shared-appearance-settings");
            closeSettings(app); tap(app, .5f, .5f); // Hide toolbar.
            const int page = app.currentPage_;
            finger(app, SDL_FINGERDOWN, .5f, .6f); finger(app, SDL_FINGERMOTION, .5f, .3f); finger(app, SDL_FINGERUP, .5f, .3f);
            assert(app.currentPage_ == page && app.pdfPanY_ > 0);
            app.handleAnalog(100000, 100000); int maxPan = app.pdfPanY_;
            app.handleAnalog(100000, 100000); assert(app.pdfPanY_ == maxPan);
            snapshot(app, "pdf-fit-width-filter");
            app.setPdfView(0); assert(app.pdfZoom_ == 1 && app.pdfPanY_ == 0);
            tap(app, .75f, .5f); assert(app.currentPage_ == 1);
            SDL_Event background{}; background.type = SDL_APP_WILLENTERBACKGROUND;
            app.handleEvent(background, running); assert(!app.progressDirty_);
            background.type = SDL_APP_DIDENTERFOREGROUND; app.handleEvent(background, running);
            app.closeBook();
        }
        {
            PaperNxApp restart(root); assert(initReady(restart));
            auto epub = *std::find_if(restart.books_.begin(), restart.books_.end(), [&](const Book &b) { return b.id == epubId; });
            restart.openBook(epub); assert(restart.currentPage_ == savedPage);
            assert(restart.extraStyle_.textColor == 0x123456 && restart.extraStyle_.backgroundColor == 0xeeddbb);
            assert(restart.currentTheme_ == 5 && restart.currentFontFamily_ == 1);
            assert(restart.progressStore_.get(epubId).bookmarks.size() == 1);
            auto pdf = *std::find_if(restart.books_.begin(), restart.books_.end(), [&](const Book &b) { return b.id == pdfId; });
            restart.openBook(pdf);
            assert(restart.currentPage_==1 && restart.extraStyle_.styleOverride==1);
            assert(restart.extraStyle_.backgroundColor==0xe9ebe8);
            assert(restart.palette().bg.r==0xe9 && restart.palette().bg.g==0xeb);
        }
        std::cout << "[PASS] Touch-only workflow elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() << "\n";
    }
    static void checkRestart(const fs::path &root) {
        PaperNxApp app(root); assert(initReady(app));
        assert(app.books_.size() == 3);
        auto epub = std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Epub; });
        assert(epub != app.books_.end() && epub->title.find("İstanbul") != std::string::npos);
        app.openBook(*epub);
        assert(app.currentTheme_ == 4 && app.currentFontFamily_ == 1);
        assert(app.progressStore_.get(app.currentBookId_).bookmarks.size() == 1);
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);
        assert(initReady(app));
        assert(app.books_.size() == 2);
        assert(!app.appSettings_.get().bookLandscapeSpreads && !app.appSettings_.get().comicLandscapeSpreads);
        pad(app,SDL_CONTROLLER_BUTTON_START);assert(app.quickMenu_);
        pad(app,SDL_CONTROLLER_BUTTON_START);assert(!app.quickMenu_ && app.screen_==ScreenState::Library);
        brightnessSlider(app);
        auto preferences=app.appSettings_.get();preferences.bookLandscapeSpreads=false;preferences.comicLandscapeSpreads=false;app.appSettings_.put(preferences);
        int first = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
        int second = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 21, 0);
        assert(first >= 0 && second >= 0);
        SDL_Event hotplug{}; hotplug.type = SDL_CONTROLLERDEVICEADDED; hotplug.cdevice.which = first;
        bool keepRunning = true; app.handleEvent(hotplug, keepRunning);
        assert(app.controller_);
        SDL_JoystickID primaryId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(app.controller_));
        hotplug.type = SDL_CONTROLLERDEVICEREMOVED; hotplug.cdevice.which = primaryId + 1000;
        app.handleEvent(hotplug, keepRunning);
        assert(app.controller_ && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(app.controller_)) == primaryId);
        assert(SDL_JoystickDetachVirtual(first) == 0);
        hotplug.cdevice.which = primaryId; app.handleEvent(hotplug, keepRunning);
        assert(app.controller_ && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(app.controller_)) != primaryId);
        SDL_JoystickID secondaryId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(app.controller_));
        assert(SDL_JoystickDetachVirtual(0) == 0);
        hotplug.cdevice.which = secondaryId; app.handleEvent(hotplug, keepRunning);
        assert(!app.controller_);
        app.render();
        app.toggleSearch();
        assert(app.searchActive_);
        SDL_Event text{}; text.type = SDL_TEXTINPUT;
        std::strcpy(text.text.text, "İıI iŞşĞğÜüÖöÇç");
        bool running = true;
        app.handleEvent(text, running);
        assert(app.searchQuery_ == "İıI iŞşĞğÜüÖöÇç");
        pad(app,SDL_CONTROLLER_BUTTON_START);
        assert(app.searchActive_ && !app.quickMenu_ && app.searchQuery_=="İıI iŞşĞğÜüÖöÇç");
        key(app, SDLK_BACKSPACE);
        assert(app.searchQuery_ == "İıI iŞşĞğÜüÖöÇ");
        key(app, SDLK_ESCAPE);
        assert(!app.searchActive_ && app.visibleBooks_.size() == 2);
        key(app, SDLK_MINUS); assert(app.viewMode_ == ViewMode::List);
        key(app, SDLK_MINUS); assert(app.viewMode_ == ViewMode::Grid);
        key(app, SDLK_f);
        assert(app.progressStore_.get(app.visibleBooks_[app.selectedBookIdx_].id).favorite);
        auto epub = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Epub; });
        app.openBook(epub); assert(app.doc_ && app.screen_ == ScreenState::Reader);
        assert(app.tocEntries_.size() == 2 && app.pageCount_ > 2);
        app.render();
        pad(app, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); assert(app.currentPage_ == 1);
        pad(app, SDL_CONTROLLER_BUTTON_Y);
        assert(app.progressStore_.get(epub.id).bookmarks.size() == 1);
        pad(app, SDL_CONTROLLER_BUTTON_X); assert(app.screen_ == ScreenState::TocModal);
        pad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        pad(app, SDL_CONTROLLER_BUTTON_A);
        assert(app.screen_ == ScreenState::Reader && app.currentPage_ > 1);
        const auto stableMark = px_document_make_bookmark(app.doc_, app.currentPage_);
        const int pageBeforeQuick=app.currentPage_;
        pad(app, SDL_CONTROLLER_BUTTON_START); assert(app.quickMenu_ && app.screen_ == ScreenState::Reader);
        pad(app, SDL_CONTROLLER_BUTTON_B); assert(!app.quickMenu_ && app.screen_==ScreenState::Reader && app.currentPage_==pageBeforeQuick);
        pad(app, SDL_CONTROLLER_BUTTON_START);assert(app.quickMenu_);pad(app,SDL_CONTROLLER_BUTTON_START);assert(!app.quickMenu_);
        app.openSettings();
        pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.settingsEditing_);
        pad(app,SDL_CONTROLLER_BUTTON_START);assert(app.settingsEditing_ && !app.quickMenu_);
        pad(app,SDL_CONTROLLER_BUTTON_B);assert(!app.settingsEditing_ && !app.quickMenu_);
        const auto original=app.currentStyle();
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_DOWN);assert(app.aaRow_==1 && app.currentFontFamily_==original.fontFamily);
        pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.choiceKind_==0);pad(app,SDL_CONTROLLER_BUTTON_DPAD_DOWN);pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.currentFontFamily_==(original.fontFamily+1)%3);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_UP);assert(app.aaRow_==0);
        pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.settingsEditing_);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.currentFontSize_==original.fontSize+2);
        pad(app,SDL_CONTROLLER_BUTTON_B);assert(!app.settingsEditing_ && app.currentFontSize_==original.fontSize);
        pad(app,SDL_CONTROLLER_BUTTON_A);pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);pad(app,SDL_CONTROLLER_BUTTON_A);
        assert(!app.settingsEditing_ && app.currentFontSize_==original.fontSize+2);
        pad(app,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);assert(app.aaTab_==1);
        assert(app.currentMargin_==original.margin && app.currentLineHeight_==original.lineHeight);
        int page = app.currentPage_;
        pad(app, SDL_CONTROLLER_BUTTON_B); assert(app.currentPage_ == page);
        app.currentFontFamily_ = 1;
        app.reflowEpub(30, 4);
        assert(app.currentPage_ == px_document_lookup_bookmark(app.doc_, stableMark));
        assert(app.pageCache_.count() == 0);
        app.render();
        assert(app.progressStore_.save());
        app.closeBook();
        app.openBook(epub);
        assert(app.currentTheme_ == 4 && app.currentFontSize_ == 30 && app.currentFontFamily_ == 1);
        pad(app, SDL_CONTROLLER_BUTTON_BACK); assert(app.screen_ == ScreenState::BookmarksModal);
        pad(app, SDL_CONTROLLER_BUTTON_A); assert(app.screen_ == ScreenState::Reader);
        auto pdf = *std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Pdf; });
        app.openBook(pdf); assert(app.doc_ && !px_document_is_epub(app.doc_));
        key(app, SDLK_EQUALS); assert(app.pdfZoom_ == 1.25f);
        SDL_Event axis{}; axis.type = SDL_CONTROLLERAXISMOTION;
        axis.caxis.axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT; axis.caxis.value = 30000;
        app.handleEvent(axis, running); assert(app.pdfZoom_ == 1.25f);
        app.controllerInputTick_ = SDL_GetTicks() - 16; app.tickControllerInput(); assert(app.pdfZoom_ > 1.25f && app.pdfZoom_ < 1.3f);
        axis.caxis.value = 0; app.handleEvent(axis, running);
        app.zoomPdf(5); app.handleAnalog(100000, 100000);
        assert(app.pdfPanX_ >= 0 && app.pdfPanY_ > 0);
        int oldPan = app.pdfPanY_;
        SDL_Event touch{}; touch.type = SDL_FINGERDOWN; touch.tfinger.fingerId = 1;
        touch.tfinger.x = 0.5f; touch.tfinger.y = 0.5f; app.handleEvent(touch, running);
        touch.type = SDL_FINGERMOTION; touch.tfinger.y = 0.6f; app.handleEvent(touch, running);
        assert(app.pdfPanY_ < oldPan);
        touch.type = SDL_FINGERUP; app.handleEvent(touch, running);
        pad(app, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        assert(app.currentPage_ == 1 && app.pdfZoom_==5 && app.pdfPanX_>=0 && app.pdfPanY_>=0);
        pad(app, SDL_CONTROLLER_BUTTON_RIGHTSTICK); assert(app.pdfZoom_ == 1);
        SDL_Event pinch{}; pinch.type = SDL_MULTIGESTURE; pinch.mgesture.numFingers = 2; pinch.mgesture.dDist = 0.1f;
        app.handleEvent(pinch, running); assert(app.pdfZoom_ > 1);
        pad(app, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        app.openSettings(false,4);
        settingsTab(app, 4);
        settingsControl(app, 3);
        assert(app.pdfZoom_ == 1.25f);
        pad(app, SDL_CONTROLLER_BUTTON_Y); assert(app.pdfZoom_ == 1);
        pad(app, SDL_CONTROLLER_BUTTON_B);
        app.render();
        app.closeBook();
        // Worker completion must release pending IDs and permit reload after eviction.
        app.getOrRenderThumbnail(epub);
        bool uploaded = false;
        for (int i = 0; i < 500; ++i) {
            SDL_Delay(10); app.processCompletedThumbnails();
            if (app.thumbCache_.get("thumb_" + epub.id)) { uploaded = true; break; }
        }
        assert(uploaded);
        { std::lock_guard<std::mutex> lock(app.queueMutex_); assert(!app.queuedThumbIds_.count(epub.id)); }
        app.thumbCache_.clear();
        app.getOrRenderThumbnail(epub);
        { std::lock_guard<std::mutex> lock(app.queueMutex_); assert(app.queuedThumbIds_.count(epub.id)); }
        app.openBrowser();
        pad(app,SDL_CONTROLLER_BUTTON_START);assert(app.quickMenu_ && app.screen_==ScreenState::FileBrowser);
        pad(app,SDL_CONTROLLER_BUTTON_B);assert(!app.quickMenu_ && app.screen_==ScreenState::FileBrowser);
        auto external = std::find_if(app.browserEntries_.begin(), app.browserEntries_.end(), [](const BrowserEntry &e) { return e.path.filename() == "external.pdf"; });
        assert(external != app.browserEntries_.end());
        app.browserSelected_ = static_cast<int>(external - app.browserEntries_.begin());
        app.handleBrowserSelect();
        assert(app.doc_ && app.books_.size() == 3);
        app.closeBook();
        assert(app.progressStore_.save());
        // A real failed periodic write must keep pending progress for retry.
        const auto blockedTemp = root / "data" / "progress.txt.tmp";
        fs::create_directory(blockedTemp);
        app.progressDirty_ = true;
        app.lastProgressSave_ = std::chrono::steady_clock::now() - std::chrono::seconds(4);
        SDL_Event quit{}; quit.type = SDL_QUIT; assert(SDL_PushEvent(&quit) == 1);
        app.run();
        assert(app.progressDirty_);
        fs::remove(blockedTemp);
        assert(app.progressStore_.save());
    }
};
}

static uint64_t hashImage(const PxImage &image) {
    uint64_t hash = 14695981039346656037ull;
    for (size_t i = 0; i < static_cast<size_t>(image.height) * image.stride; ++i)
        hash = (hash ^ image.rgb[i]) * 1099511628211ull;
    return hash;
}

int main() {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0);
    assert(TTF_Init() == 0);
    {
        const int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 4, 28, 0);
        assert(index >= 0);
        char guid[33] = {};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
        assert(SDL_GameControllerAddMapping(switchControllerMapping(guid).c_str()) >= 0);
        SDL_GameController *pad = SDL_GameControllerOpen(index);
        assert(pad);
        SDL_Joystick *joystick = SDL_GameControllerGetJoystick(pad);
        const std::pair<int, SDL_GameControllerButton> buttons[] = {
            {0, SDL_CONTROLLER_BUTTON_A}, {1, SDL_CONTROLLER_BUTTON_B},
            {2, SDL_CONTROLLER_BUTTON_X}, {3, SDL_CONTROLLER_BUTTON_Y},
            {6, SDL_CONTROLLER_BUTTON_LEFTSHOULDER}, {7, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
            {10, SDL_CONTROLLER_BUTTON_START}, {11, SDL_CONTROLLER_BUTTON_BACK},
            {12, SDL_CONTROLLER_BUTTON_DPAD_LEFT}, {13, SDL_CONTROLLER_BUTTON_DPAD_UP},
            {14, SDL_CONTROLLER_BUTTON_DPAD_RIGHT}, {15, SDL_CONTROLLER_BUTTON_DPAD_DOWN}};
        for (const auto &button : buttons) {
            assert(SDL_JoystickSetVirtualButton(joystick, button.first, SDL_PRESSED) == 0);
            SDL_GameControllerUpdate();
            assert(SDL_GameControllerGetButton(pad, button.second) == SDL_PRESSED);
            assert(SDL_JoystickSetVirtualButton(joystick, button.first, SDL_RELEASED) == 0);
            SDL_GameControllerUpdate();
        }
        for (int trigger = 8; trigger <= 9; ++trigger) {
            assert(SDL_JoystickSetVirtualButton(joystick, trigger, SDL_PRESSED) == 0);
            SDL_GameControllerUpdate();
            assert(SDL_GameControllerGetAxis(pad, trigger == 8 ? SDL_CONTROLLER_AXIS_TRIGGERLEFT : SDL_CONTROLLER_AXIS_TRIGGERRIGHT) == 32767);
        }
        SDL_GameControllerClose(pad);
        assert(SDL_JoystickDetachVirtual(index) == 0);
    }
    const auto root = fs::temp_directory_path() / "papernx-app-regressions";
    fs::remove_all(root); fs::create_directories(root / "books");
    fs::copy_file("tests/fixtures/sample_book.epub", root / "books" / "sample.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf", root / "books" / "sample.pdf");
    fs::create_directory_symlink(fs::absolute("fonts"), root / "fonts");
    fs::copy_file("tests/fixtures/sample_doc.pdf", root / "external.pdf");
    AppTest::run(root);
    AppTest::checkRestart(root);
    const auto touchRoot = root / "touch-only";
    fs::create_directories(touchRoot / "books");
    fs::copy_file("tests/fixtures/sample_book.epub", touchRoot / "books" / "a.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf", touchRoot / "books" / "b.pdf");
    fs::create_directory_symlink(fs::absolute("fonts"), touchRoot / "fonts");
    AppTest::touchOnly(touchRoot);
    SDL_Window *window = SDL_CreateWindow("cache", 0, 0, 128, 128, SDL_WINDOW_HIDDEN);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    assert(renderer);
    {
        PageCache cache(1024);
        auto texture = [&] { return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, 16, 16); };
        assert(cache.put("first", texture(), 16, 16));
        assert(cache.put("second", texture(), 16, 16));
        assert(!cache.get("first") && cache.currentBytes() == 1024);
        assert(!cache.put("oversized", SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, 32, 32), 32, 32));
        assert(cache.currentBytes() == 1024);
        assert(cache.put("second", texture(), 16, 16));
        assert(cache.count() == 1);
        auto serif = PageCache::makeEpubKey("doc", 0, 22, 0, 140, 40, 0, 1280, 720, 0);
        auto sans = PageCache::makeEpubKey("doc", 0, 22, 0, 140, 40, 0, 1280, 720, 1);
        assert(serif != sans);
    }
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
    char err[256] = {};
    assert(px_document_open_limited("tests/fixtures/sample_book.epub", 600, 600, 22, "", err, sizeof err, 1024) == nullptr);
    auto doc = px_document_open("tests/fixtures/sample_book.epub", 600, 600, 22, "", err, sizeof err);
    assert(doc);
    PxImage cover{};
    assert(px_document_render_thumbnail(doc, 96, 144, &cover, err, sizeof err));
    assert(cover.width == 96 && cover.height == 144);
    assert(cover.rgb[0] == 24 && cover.rgb[1] == 80 && cover.rgb[2] == 180);
    px_image_free(&cover);
    std::set<uint64_t> themeHashes;
    for (int theme = 0; theme < 6; ++theme) {
        const auto css = generateEpubCss(theme, 0, 22, 140, 20, 0);
        assert(px_document_relayout(doc, 600, 600, 22, css.c_str(), err, sizeof err));
        PxImage image{}; assert(px_document_render(doc, 0, 600, 600, &image, err, sizeof err));
        assert(image.rgb[0] == g_palettes[theme].bg.r && image.rgb[1] == g_palettes[theme].bg.g && image.rgb[2] == g_palettes[theme].bg.b);
        themeHashes.insert(hashImage(image));
        px_image_free(&image);
    }
    assert(themeHashes.size() == 6);
    // Exercise the reader's V4 CSS model, not just the legacy palette helper.
    for (int preset = 0; preset < 6; ++preset) {
        ReaderPreferences style; style.theme = preset; style.fontFamily = preset % 3;
        const auto css = generateEpubCss(style);
        const auto layoutStart = std::chrono::steady_clock::now();
        assert(px_document_relayout(doc, 600, 600, 22, css.c_str(), err, sizeof err));
        const auto renderStart = std::chrono::steady_clock::now();
        PxImage image{}; assert(px_document_render(doc, 0, 600, 600, &image, err, sizeof err));
        assert(image.rgb[0] == g_palettes[preset].bg.r && image.rgb[1] == g_palettes[preset].bg.g);
        std::cout << "BENCH preset=" << preset << " relayout_us=" << std::chrono::duration_cast<std::chrono::microseconds>(renderStart - layoutStart).count()
            << " render_us=" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - renderStart).count() << " viewport=600x600\n";
        px_image_free(&image);
    }
    std::set<uint64_t> fontHashes;
    for (int family = 0; family < 2; ++family) {
        ReaderPreferences style; style.fontFamily = family;
        const auto css = generateEpubCss(style);
        assert(px_document_relayout(doc, 600, 600, 22, css.c_str(), err, sizeof err));
        PxImage image{}; assert(px_document_render(doc, 0, 600, 600, &image, err, sizeof err));
        fontHashes.insert(hashImage(image)); px_image_free(&image);
    }
    assert(fontHashes.size() == 2);
    ReaderPreferences custom; custom.backgroundColor = 0xeeddbb; custom.textColor = 0x123456;
    auto customCss = generateEpubCss(custom);
    assert(px_document_relayout(doc, 600, 600, 22, customCss.c_str(), err, sizeof err));
    PxImage customImage{}; assert(px_document_render(doc, 0, 600, 600, &customImage, err, sizeof err));
    assert(customImage.rgb[0] == 0xee && customImage.rgb[1] == 0xdd && customImage.rgb[2] == 0xbb);
    const auto customHash = hashImage(customImage); px_image_free(&customImage);
    custom.textColor = 0x884422; customCss = generateEpubCss(custom);
    assert(px_document_relayout(doc, 600, 600, 22, customCss.c_str(), err, sizeof err));
    assert(px_document_render(doc, 0, 600, 600, &customImage, err, sizeof err));
    assert(hashImage(customImage) != customHash && customImage.rgb[0] == 0xee); px_image_free(&customImage);
    const int validPages = px_document_pages(doc);
    assert(!px_document_relayout(doc, 100000, 600, 22, customCss.c_str(), err, sizeof err));
    assert(px_document_pages(doc) == validPages);
    assert(px_document_render(doc, 0, 600, 600, &customImage, err, sizeof err)); px_image_free(&customImage);
    assert(readingContrast({255,255,255,255}, {255,255,255,255}) == 1);
    assert(readingContrast({255,255,255,255}, {0,0,0,255}) > 20);
    auto mark = px_document_make_bookmark(doc, px_document_pages(doc) - 1);
    px_document_close(doc);
    for (int i = 0; i < 20; ++i) {
        doc = px_document_open("tests/fixtures/sample_book.epub", 400 + i * 10, 500, 26, "", err, sizeof err);
        assert(doc && px_document_lookup_bookmark(doc, mark) == px_document_pages(doc) - 1);
        assert(px_document_lookup_bookmark(doc, 9876543210ll) == -1);
        px_document_close(doc);
    }
    struct tm epoch{}; epoch.tm_year = 70; epoch.tm_mday = 1;
    assert(papernx_timegm(&epoch) == 0);
    epoch.tm_year = 100; epoch.tm_mon = 1; epoch.tm_mday = 29;
    assert(papernx_timegm(&epoch) == 951782400);
    fs::remove_all(root);
    fs::create_directories(root / "books");
    fs::create_directory_symlink(fs::absolute("fonts"),root / "fonts");
    fs::copy("tests/fixtures/sample_book.epub",root / "books" / "scroll.epub");
    std::cout << ">> Starting v04UiWorkflow\n" << std::flush;
    AppTest::v04UiWorkflow(root);
    std::cout << ">> Starting v04CatalogWorkflow\n" << std::flush;
    AppTest::v04CatalogWorkflow(root);
    std::cout << ">> Starting continuousReading\n" << std::flush;
    AppTest::continuousReading(root);
    std::cout << ">> Starting captureJournalScreens\n" << std::flush;
    AppTest::captureJournalScreens(root);
    std::cout << ">> Starting captureV03Screens\n" << std::flush;
    AppTest::captureV03Screens(root);
    std::cout << ">> Starting v03Workflow\n" << std::flush;
    AppTest::v03Workflow(root);
    std::cout << ">> Starting v03StressWorkflow\n" << std::flush;
    AppTest::v03StressWorkflow(root);
    std::cout << ">> Finished all workflows\n" << std::flush;
    fs::remove_all(root);
    TTF_Quit(); SDL_Quit();
    std::cout << "[PASS] SDL input, worker reload, EPUB themes/cover/reopen, PDF controls, texture budget and UTC regressions\n";
}
