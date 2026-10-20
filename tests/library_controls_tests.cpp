#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
    static void axis(PaperNxApp &app,int value,int which=SDL_CONTROLLER_AXIS_RIGHTY) {
        SDL_Event event{};event.type=SDL_CONTROLLERAXISMOTION;event.caxis.axis=Uint8(which);event.caxis.value=Sint16(value);
        bool running=true;app.handleEvent(event,running);assert(running);
        app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();
    }
    static void sameSticks(PaperNxApp &app) {
        // Compare actual navigation and reveal state from the same starting focus.
        auto state=std::tie(app.listScroll_,app.quickScroll_,app.catalogScroll_,app.settingsScroll_,app.tocScroll_,app.bookmarkScroll_,app.browserScroll_,app.scrollY_,app.currentPage_,app.pdfPanX_,app.pdfPanY_,app.selectedBookIdx_,app.quickFocus_,app.catalogFocus_,app.aaRow_,app.tocSelected_,app.bmSelected_,app.browserSelected_,app.choiceFocusedIndex_,app.choiceIndex_,app.swatchFocus_,app.readerToolbarFocusActive_);
        for(int direction:{-32767,32767})for(int coordinate:{0,1}) {
            const auto before=std::apply([](auto... value){return std::make_tuple(value...);},state);
            const auto focus=app.navigationFocus_;
            app.clearHeldControls();axis(app,direction,SDL_CONTROLLER_AXIS_LEFTX+coordinate);
            const auto left=std::apply([](auto... value){return std::make_tuple(value...);},state);
            app.clearHeldControls();state=before;app.navigationFocus_=focus;
            axis(app,direction,SDL_CONTROLLER_AXIS_RIGHTX+coordinate);
            assert(state==left);
            // A neutral axis stops immediately; a held axis waits for the same repeat delay.
            app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(state==left);
            axis(app,0,SDL_CONTROLLER_AXIS_RIGHTX+coordinate);assert(state==left);
            app.clearHeldControls();state=before;app.navigationFocus_=focus;
        }
    }
    static void snapshot(PaperNxApp &app,const char *name) {
        for(int i=0;i<20;++i){app.render();SDL_Delay(10);app.processCompletedThumbnails();}
        app.render();int w,h;app.getWindowSize(w,h);
        auto *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
        fs::create_directories("verification/library-controls");
        assert(SDL_SaveBMP(surface,(std::string("verification/library-controls/")+name+".bmp").c_str())==0);SDL_FreeSurface(surface);
    }
    static void rotatedChoices(PaperNxApp &app) {
        const auto original=app.appSettings_.get();app.openChoice(7,3);
        const int keys[]{SDLK_RIGHT,SDLK_DOWN,SDLK_LEFT,SDLK_UP};
        const int buttons[]{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_UP};
        const int expected[3][4]{{1,1,-1,-1},{-1,1,1,-1},{1,-1,-1,1}};
        for(int orientation=0;orientation<3;++orientation) {
            auto settings=original;settings.displayOrientation=orientation;app.appSettings_.put(settings);sameSticks(app);
            for(int control=0;control<4;++control)for(int direction=0;direction<4;++direction) {
                app.clearHeldControls();app.choiceFocusedIndex_=app.choiceIndex_=3;
                SDL_Event event{};bool running=true;
                if(control==0){event.type=SDL_KEYDOWN;event.key.keysym.sym=SDL_Keycode(keys[direction]);}
                else if(control==1){event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=Uint8(buttons[direction]);}
                else {event.type=SDL_CONTROLLERAXISMOTION;event.caxis.axis=Uint8((control==2?SDL_CONTROLLER_AXIS_LEFTX:SDL_CONTROLLER_AXIS_RIGHTX)+(direction%2));event.caxis.value=direction<2?32767:-32767;}
                app.handleEvent(event,running);assert(app.choiceFocusedIndex_==3+expected[orientation][direction]);
                assert(app.screen_==ScreenState::FileBrowser); // modal owns the action
            }
        }
        app.clearHeldControls();app.appSettings_.put(original);app.choiceKind_=-1;
        std::cout<<"PASS: rotated keyboard/D-pad/LS/RS modal navigation, four directions and no double rotation\n";
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(app.init());ui::language=1;
        const auto deadline=SDL_GetTicks()+6000;
        while(SDL_GetTicks()<deadline) {
            app.processCompletedThumbnails();
            if(app.catalog_.entries().size()>=app.books_.size())break;
            SDL_Delay(10);
        }
        assert(app.catalog_.entries().size()==app.books_.size());
        app.updateVisibleBooks();assert(app.seriesShelves_.size()==1 && app.visibleBooks_.size()==2);
        auto group=app.seriesShelves_.begin();const auto representative=group->first;
        assert(group->second.issues.size()==14 && group->second.title=="PaperNX Test Comics");
        int shelfIndex=0;while(app.visibleBooks_[shelfIndex].id!=representative)++shelfIndex;
        app.selectedBookIdx_=shelfIndex;snapshot(app,"series-home");
        app.openLibraryItem(shelfIndex);assert(!app.openSeriesKey_.empty() && app.visibleBooks_.size()==14);
        assert(app.catalog_.get(app.visibleBooks_.front().id).sourceSeriesIndex=="9");
        assert(app.catalog_.get(app.visibleBooks_[1].id).sourceSeriesIndex=="10");
        assert(app.catalog_.get(app.visibleBooks_.back().id).sourceSeriesIndex=="22");
        assert(app.maxUiScroll()>0);sameSticks(app);
        app.rebuildFocus();app.navigationFocus_.select("book.0");axis(app,32767);assert(app.selectedBookIdx_>0);
        auto position=app.listScroll_;axis(app,0);assert(app.listScroll_==position);
        app.listScroll_=0;snapshot(app,"series-issues");
        const Book issue=app.visibleBooks_[1];app.openLibraryItem(1);assert(app.doc_ && app.currentBookId_==issue.id);
        app.turnPage(1);app.cancelPageTurn();app.saveCurrentPosition();auto saved=app.progressStore_.get(issue.id);
        app.closeBook();assert(!app.openSeriesKey_.empty() && app.visibleBooks_.size()==14);
        app.openLibraryItem(1);assert(app.currentBookId_==issue.id && app.currentPage_==saved.page);app.closeBook();
        assert(app.closeSeriesShelf() && app.visibleBooks_.size()==2 && app.selectedBookIdx_==shelfIndex);
        app.searchQuery_="Issue";app.updateVisibleBooks();assert(app.seriesShelves_.empty() && app.visibleBooks_.size()==14);
        app.searchQuery_.clear();app.category_=Category::Recent;app.updateVisibleBooks();assert(app.seriesShelves_.empty());
        app.category_=Category::All;app.updateVisibleBooks();
        app.quickMenu_=true;app.quickScroll_=0;sameSticks(app);app.quickMenu_=false;
        app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);app.catalogScroll_=0;sameSticks(app);app.setCatalogPanel(PaperNxApp::CatalogPanel::None);
        app.openSettings(true,5);app.settingsScroll_=0;sameSticks(app);app.dismissSettings();
        app.openBook(issue);app.screen_=ScreenState::TocModal;
        app.tocEntries_.resize(30);app.tocScroll_=0;sameSticks(app);
        auto progress=app.progressStore_.get(issue.id);progress.bookmarks.resize(30);app.progressStore_.put(issue.id,progress);
        app.screen_=ScreenState::BookmarksModal;app.bookmarkScroll_=0;sameSticks(app);
        app.screen_=ScreenState::Reader;app.extraStyle_.readingMode=1;app.documentMode_=1;app.currentPage_=0;app.scrollY_=0;
        sameSticks(app);axis(app,32767);assert(app.scrollY_>0 || app.currentPage_>0);axis(app,0);
        app.closeBook();app.openBrowser();
        // Many entries make the browser scroll independently of library layout.
        app.browserEntries_.resize(30);app.browserScroll_=0;sameSticks(app);
        app.openChoice(7,0);sameSticks(app);
        axis(app,32767);const int selected=app.choiceFocusedIndex_;assert(selected==1);
        SDL_Delay(230);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.choiceFocusedIndex_==selected+1);
        axis(app,0);app.choiceKind_=-1;app.colorEditing_=0;app.swatchFocus_=5;sameSticks(app);app.colorEditing_=-1;
        rotatedChoices(app);
        app.clearHeldControls();
        std::cout<<"PASS: full background series indexing, issue order 9/10/22, shelf/back/search/history, persisted issue identity, identical LS/RS directions, deadzone/repeat and modal priority in library/menus/settings/browser/contents/bookmarks/continuous reader\n";
    }
};
}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-series-controls";fs::remove_all(root);fs::create_directories(root/"books");
    for(int issue=9;issue<=22;++issue)fs::copy_file("tests/fixtures/comics/series-"+std::to_string(issue)+".cbz",root/"books"/("Issue "+std::to_string(issue)+".cbz"));
    fs::copy_file("tests/fixtures/sample_doc.pdf",root/"books"/"standalone.pdf");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);
    fs::remove_all(root);TTF_Quit();SDL_Quit();
}
