// Included inside AppTest: exercise actual native finger routing and rendered geometry.
static void v03RectTap(PaperNxApp &app, SDL_Rect r, float horizontal=.5f) {
    int w,h; app.getWindowSize(w,h);
    tap(app,float(r.x+int(r.w*horizontal))/w,float(r.y+r.h/2)/h);
}
static void v03Drag(PaperNxApp &app,float x,float from,float to) {
    finger(app,SDL_FINGERDOWN,x,from);
    for(int i=1;i<=4;++i) finger(app,SDL_FINGERMOTION,x,from+(to-from)*i/4);
    finger(app,SDL_FINGERUP,x,to);
}
static void v03Plus(PaperNxApp &app) {
    int w,h; app.getWindowSize(w,h); v03RectTap(app,{w-80,8,64,64}); assert(app.quickMenu_);
}
static void v03MenuItem(PaperNxApp &app,int index) {
    assert(app.quickMenu_); int w,h; app.getWindowSize(w,h);
    auto sheet=app.quickSheet(w,h);
    for(int attempt=0;attempt<8;++attempt) {
        int y=sheet.y+44+index*60+28-int(app.quickScroll_);
        if(y>=sheet.y+44 && y<sheet.y+sheet.h-4) {
            v03RectTap(app,{sheet.x+20,y-20,sheet.w-40,40}); return;
        }
        v03Drag(app,float(sheet.x+sheet.w/2)/w,float(sheet.y+sheet.h-24)/h,float(sheet.y+100)/h);
    }
    assert(false && "Quick action must be reachable using finger drag");
}
static void v03Tab(PaperNxApp &app,int tab) {
    int w,h; app.getWindowSize(w,h); v03RectTap(app,ui::SettingsLayout(w,h).tab(tab)); assert(app.aaTab_==tab);
}
static void v03Control(PaperNxApp &app,int row,bool right=true) {
    int w,h; app.getWindowSize(w,h); ui::SettingsLayout layout(w,h);
    for(int attempt=0;attempt<8;++attempt) {
        auto r=layout.control(row); r.y-=int(app.settingsScroll_);
        if(r.y>=layout.content.y && r.y+r.h<=layout.content.y+layout.content.h) {
            app.aaRow_=row;
            if(app.numericSetting())r=right?ui::StepperLayout(r).plus:ui::StepperLayout(r).minus;
            v03RectTap(app,r,right?.75f:.25f);
            if(app.choiceKind_>=0){
                const auto count=int(app.choiceOptions().size());assert(count>0);
                app.choiceFocusedIndex_=(app.choiceFocusedIndex_+1)%count;
                pad(app,SDL_CONTROLLER_BUTTON_A);
            }
            return;
        }
        float x=float(layout.content.x+layout.content.w/2)/w;
        float top=float(layout.content.y+12)/h,bottom=float(layout.content.y+layout.content.h-12)/h;
        v03Drag(app,x,r.y<layout.content.y?top:bottom,r.y<layout.content.y?bottom:top);
    }
    assert(false && "Settings control must be reachable using finger drag");
}
static void v03CloseSettings(PaperNxApp &app) {
    int w,h; app.getWindowSize(w,h); v03RectTap(app,ui::SettingsLayout(w,h).close);
    assert(app.screen_!=ScreenState::AaModal);
}
static void v03WaitLayout(PaperNxApp &app) {
    const Uint32 deadline=SDL_GetTicks()+15000;
    while(app.layoutPending_ || app.layoutWorker_.joinable()) {
        app.processLayout(); SDL_Delay(4);
        assert(SDL_GetTicks()<deadline && "Layout worker did not settle within 15 seconds");
    }
}
static void v03RevealSeriesIssue(PaperNxApp &app,const std::string &id) {
    std::optional<Book> representative;
    for(const auto &shelf:app.seriesShelves_)for(const auto &issue:shelf.second.issues)
        if(issue.id==id)representative=shelf.second.issues.front();
    if(representative)assert(app.openSeriesShelf(*representative));
}
static void v03OpenLibraryBook(PaperNxApp &app,const std::string &id) {
    v03RevealSeriesIssue(app,id);
    assert(app.screen_==ScreenState::Library); int w,h; app.getWindowSize(w,h);
    assert(app.viewMode_==ViewMode::Grid); ui::LibraryLayout layout(w);
    const auto found=std::find_if(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &b){return b.id==id;});
    assert(found!=app.visibleBooks_.end()); const int index=int(found-app.visibleBooks_.begin());
    for(int attempt=0;attempt<12;++attempt) {
        int y=layout.top+(index/layout.columns)*layout.stepY-int(app.listScroll_)+110;
        if(y>=layout.top+8 && y<h-36) {
            v03RectTap(app,{layout.left+(index%layout.columns)*layout.stepX,y-30,layout.cardWidth,60});
            assert(app.screen_==ScreenState::Reader && app.doc_ && app.currentBookId_==id); return;
        }
        v03Drag(app,.5f,y<layout.top?.45f:.85f,y<layout.top?.85f:.45f);
    }
    assert(false && "Book must remain touch-reachable after metadata changes its title ordering");
}
static void v03BackLibrary(PaperNxApp &app) {
    v03Plus(app); v03MenuItem(app,0); assert(app.screen_==ScreenState::Library && !app.doc_);
}
static void v03Workflow(const fs::path &parent) {
    const auto root=parent/"v03-workflow";
    fs::create_directories(root/"books");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    for(int i=0;i<16;++i)
        fs::copy_file("tests/fixtures/sample_book.epub",root/"books"/("book-"+std::to_string(i)+".epub"));
    std::string firstId,secondId; int bookBPage=0,globalThemeAfterSelection=0; float bookBY=0; std::size_t bookmarkCount=0;
    std::vector<double> menuMs,tabMs,cachedMs,coldMs;
    auto milliseconds=[](auto start) { return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(); };
    {
        PaperNxApp app(root); assert(initReady(app) && !app.controller_ && SDL_NumJoysticks()==0);
        firstId=app.visibleBooks_[0].id; secondId=app.visibleBooks_[1].id;
        // Already-read fixtures deliberately have conflicting legacy appearances.
        for(int i=0;i<2;++i) {
            Progress old; old.page=3+i; old.pageCount=40; old.fraction=.1; old.lastRead=1700000000;
            old.theme=i?5:0; old.favorite=true; old.bookmarks.push_back({2,"Önceden kayıtlı",1700000000,-1});
            app.progressStore_.put(i?secondId:firstId,old);
        }
        // Global settings and preview are reached through the visible plus action.
        v03Plus(app); snapshot(app,"v03-plus-menu"); v03MenuItem(app,0);
        assert(app.screen_==ScreenState::AaModal && app.globalSettings_ && !app.doc_);
        // Reopening settings from its own plus sheet must retain the library return route.
        v03Plus(app); v03MenuItem(app,0);
        assert(app.screen_==ScreenState::AaModal && app.settingsReturn_==ScreenState::Library);
        v03Tab(app,2); v03Control(app,1,false); assert(app.currentTheme_==2);
        v03Tab(app,0); v03Control(app,0); v03Control(app,0); assert(app.currentFontSize_==26);
        app.render(); snapshot(app,"v03-global-settings-preview"); v03CloseSettings(app);
        assert(app.progressStore_.get("PAPERNX_DEFAULTS").theme==2);
        // Copies of the same series now have one home card; scroll its issues.
        v03RevealSeriesIssue(app,firstId);
        // Library drag tracks before release and must not open a book.
        finger(app,SDL_FINGERDOWN,.5f,.82f); finger(app,SDL_FINGERMOTION,.5f,.5f);
        assert(app.listScroll_>100 && !app.doc_);
        finger(app,SDL_FINGERUP,.5f,.5f); v03Drag(app,.5f,.4f,.9f);
        assert(app.listScroll_==0 && !app.doc_);
        v03OpenLibraryBook(app,firstId); assert(app.currentTheme_==2 && app.currentFontSize_==26);
        for(int i=0;i<12;++i) {
            auto start=std::chrono::steady_clock::now(); v03Plus(app); app.render(); menuMs.push_back(milliseconds(start));
            // Tap outside the drawer; no underlying page action is allowed.
            const int page=app.currentPage_; tap(app,.05f,.5f);
            assert(!app.quickMenu_ && app.currentPage_==page);
        }
        v03BackLibrary(app); v03OpenLibraryBook(app,secondId);
        assert(app.currentBookId_==secondId && app.currentTheme_==2 && app.currentFontSize_==26);
        assert(app.progressStore_.get(secondId).favorite && app.progressStore_.get(secondId).bookmarks.size()==1);
        v03Plus(app); v03MenuItem(app,1);
        int w,h; app.getWindowSize(w,h); v03RectTap(app,ui::SettingsLayout(w,h).scope);
        assert(app.extraStyle_.styleOverride==1);
        v03Tab(app,2); v03Control(app,2,false); assert(app.currentTheme_==4);
        for(int i=0;i<18;++i) {
            auto start=std::chrono::steady_clock::now(); v03Tab(app,i%6); app.render(); tabMs.push_back(milliseconds(start));
        }
        v03Tab(app,3); v03Control(app,1); assert(app.extraStyle_.readingMode==1);
        // Disable inertia using its visible toggle to make exact position assertions deterministic.
        if(app.extraStyle_.scrollInertia) v03Control(app,3);
        v03WaitLayout(app); v03CloseSettings(app); assert(app.continuous());
        finger(app,SDL_FINGERDOWN,.5f,.75f); finger(app,SDL_FINGERMOTION,.5f,.4f);
        assert(app.scrollY_>100 && app.scrollTracking_ && !app.toolbarVisible_);
        finger(app,SDL_FINGERUP,.5f,.4f); assert(!app.toolbarVisible_ && app.scrollVelocity_==0);
        const auto y=app.scrollY_; const int page=app.currentPage_;
        finger(app,SDL_FINGERDOWN,.8f,.5f); finger(app,SDL_FINGERMOTION,.2f,.5f); finger(app,SDL_FINGERUP,.2f,.5f);
        assert(app.currentPage_==page && std::abs(app.scrollY_-y)<1 && !app.toolbarVisible_);
        // The action sheet captures gestures outside its bounds as well.
        v03Plus(app); v03Drag(app,.05f,.3f,.7f);
        assert(app.currentPage_==page && std::abs(app.scrollY_-y)<1);
        if(!app.quickMenu_) v03Plus(app);
        finger(app,SDL_FINGERDOWN,.05f,.5f); finger(app,SDL_FINGERMOTION,.3f,.5f); finger(app,SDL_FINGERUP,.3f,.5f);
        assert(app.currentPage_==page && std::abs(app.scrollY_-y)<1);
        if(!app.quickMenu_) v03Plus(app);
        v03MenuItem(app,2); assert(app.screen_==ScreenState::TocModal);
        app.getWindowSize(w,h); v03RectTap(app,app.tocCell(0,w,h));
        assert(app.screen_==ScreenState::Reader && !app.navigationHistory_.empty());
        v03Plus(app);
        auto actions=app.quickActions();
        auto jump=std::find(actions.begin(),actions.end(),"Önceki konuma dön"); assert(jump!=actions.end());
        v03MenuItem(app,int(jump-actions.begin()));
        assert(app.currentPage_==page && std::abs(app.scrollY_-y)<1);
        v03Plus(app); v03MenuItem(app,3); assert(app.screen_==ScreenState::BookmarksModal);
        auto beforeBookmarks=app.progressStore_.get(secondId).bookmarks.size();
        v03RectTap(app,{48,h-112,w-96,56});
        assert(app.progressStore_.get(secondId).bookmarks.size()==beforeBookmarks+1);
        // Delete the just-created row, preserving the original migrated bookmark.
        v03RectTap(app,{w-184,152+int(beforeBookmarks)*56,112,56});
        assert(app.progressStore_.get(secondId).bookmarks.size()==beforeBookmarks);
        v03RectTap(app,{48,152,w-250,56});
        assert(app.screen_==ScreenState::Reader && !app.navigationHistory_.empty());
        v03Plus(app); actions=app.quickActions();
        jump=std::find(actions.begin(),actions.end(),"Önceki konuma dön"); assert(jump!=actions.end());
        v03MenuItem(app,int(jump-actions.begin()));
        assert(app.currentPage_==page && std::abs(app.scrollY_-y)<1);
        v03Plus(app); v03MenuItem(app,1);
        const auto completed=app.completedLayouts_;
        v03Tab(app,0); v03Control(app,0); v03Control(app,0); v03Control(app,0);
        assert(app.currentFontSize_==32 && app.completedLayouts_==completed);
        int chapter=-1,character=-1; double offset=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&chapter,&character,&offset));
        const auto layoutStart=std::chrono::steady_clock::now(); v03WaitLayout(app);
        const double layoutMs=milliseconds(layoutStart);
        assert(app.completedLayouts_==completed+1 && app.renderedStyle_.fontSize==32);
        int chapterAfter=-1,characterAfter=-1; double offsetAfter=0;
        assert(px_document_anchor(app.doc_,app.currentPage_,app.scrollY_,&chapterAfter,&characterAfter,&offsetAfter));
        assert(chapterAfter==chapter && std::abs(characterAfter-character)<160);
        std::cout<<"BENCH v03_layout_batch_ms="<<layoutMs<<" anchor_character_delta="<<std::abs(characterAfter-character)<<"\n";
        // Failed preparation keeps the valid document and allows subsequent reading.
        auto book=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return b.id==secondId;});
        assert(book!=app.books_.end()); const auto originalPath=book->path; auto *validDoc=app.doc_;
        book->path=(root/"missing.epub").string(); v03Control(app,0); v03WaitLayout(app);
        assert(app.doc_==validDoc && !app.currentErrorMessage_.empty() && app.currentFontSize_==32);
        book->path=originalPath; app.currentErrorMessage_.clear();
        v03CloseSettings(app);
        // Restart must preserve the explicit override and existing progress metadata.
        app.saveCurrentPosition(); assert(app.flushProgress());
        bookBPage=app.currentPage_; bookBY=app.scrollY_; bookmarkCount=app.progressStore_.get(secondId).bookmarks.size();
        v03BackLibrary(app); v03OpenLibraryBook(app,firstId); assert(app.currentTheme_==2 && app.currentFontSize_==26);
        // Unified General Theme replaces the old independent application chrome.
        v03BackLibrary(app); v03Plus(app); v03MenuItem(app,1); assert(app.aaTab_==5);
        const int previousPreset=std::max(0,app.globalPaperPresetIndex());
        v03Control(app,0);
        const auto chosen=g_paperPresets[(previousPreset+1)%9];
        globalThemeAfterSelection=chosen.theme;
        auto globalStyle=app.progressStore_.get("PAPERNX_DEFAULTS");
        assert(globalStyle.theme==chosen.theme && globalStyle.backgroundColor==chosen.background);
        assert(app.appSettings_.get().uiTheme==chosen.theme && app.appSettings_.get().chromePolicy==0);
        assert(app.palette().bg.r==Uint8(chosen.background>>16));
        assert(app.appSettings_.get().language==1);
        v03Control(app,5); assert(app.appSettings_.get().language==0);
        v03CloseSettings(app); snapshot(app,"v03-professional-library");
    }
    {
        PaperNxApp app(root); assert(initReady(app) && !app.controller_);
        assert(app.appSettings_.get().uiTheme==globalThemeAfterSelection);
        assert(app.appSettings_.get().language==0);
        v03OpenLibraryBook(app,secondId);
        assert(app.currentTheme_==4 && app.extraStyle_.styleOverride==1 && app.continuous());
        assert(app.currentPage_==bookBPage && std::abs(app.scrollY_-bookBY)<1 && app.progressStore_.get(secondId).favorite);
        assert(app.progressStore_.get(secondId).bookmarks.size()==bookmarkCount);
        v03Plus(app); v03MenuItem(app,1);
        int w,h; app.getWindowSize(w,h); v03RectTap(app,ui::SettingsLayout(w,h).scope);
        assert(!app.extraStyle_.styleOverride && app.currentTheme_==globalThemeAfterSelection && app.currentFontSize_==26);
        v03WaitLayout(app); v03CloseSettings(app); v03BackLibrary(app);
        // Each geometry is the actual SDL application; lower settings rows remain touch reachable.
        for(const auto &size:{std::pair<int,int>{1920,1080},{640,480},{1280,720}}) {
            SDL_SetWindowSize(app.window_,size.first,size.second);
            v03Plus(app); v03MenuItem(app,0); v03Tab(app,0);
            const int indentation=app.extraStyle_.indentation;
            v03Control(app,5); assert(app.extraStyle_.indentation!=indentation);
            app.render(); if(size.first==640) snapshot(app,"v03-settings-640"); v03CloseSettings(app);
            assert(app.typography_->textCacheBytes()<=8u*1024u*1024u);
        }
        // Cold/cached page timing includes native texture/render work, without Swap/VSync hardware claims.
        v03OpenLibraryBook(app,firstId); assert(!app.continuous());
        tap(app,.5f,.5f); assert(app.toolbarVisible_); snapshot(app,"v03-reader-toolbar");
        for(int i=0;i<10;++i) {
            app.pageCache_.clear(); auto start=std::chrono::steady_clock::now(); app.render(); coldMs.push_back(milliseconds(start));
            start=std::chrono::steady_clock::now(); app.render(); cachedMs.push_back(milliseconds(start));
        }
        v03BackLibrary(app); v03Plus(app); v03MenuItem(app,8); assert(app.exitRequested_);
    }
    // Empty-library settings never require a MuPDF document.
    {
        const auto empty=root/"empty"; PaperNxApp app(empty); assert(initReady(app) && app.books_.empty());
        v03Plus(app); v03MenuItem(app,0); v03Tab(app,2); v03Control(app,1,false); app.render();
        assert(!app.doc_ && app.currentTheme_==2); v03CloseSettings(app);
    }
    auto p95=[](std::vector<double> values) { std::sort(values.begin(),values.end()); return values[std::min(values.size()-1,std::size_t(std::ceil(values.size()*.95)-1))]; };
    fs::create_directories("verification"); std::ofstream perf("verification/v03-ui-benchmark.json");
    perf<<"{\n  \"backend\": \"desktop SDL dummy software; CPU wall time, not physical Switch\",\n"
        <<"  \"menu_open_render_p95_ms\": "<<p95(menuMs)<<",\n  \"settings_tab_render_p95_ms\": "<<p95(tabMs)
        <<",\n  \"cold_page_render_p95_ms\": "<<p95(coldMs)<<",\n  \"cached_page_render_p95_ms\": "<<p95(cachedMs)
        <<",\n  \"physical_switch\": \"UNVERIFIED\"\n}\n";
    std::cout<<"[PASS] v0.3 native touch workflow, global/custom/restart, async batching/fallback, scrolling, small screens and exit\n";
}
