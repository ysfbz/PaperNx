// Actual SDL application interaction and output regressions, no mocked screenshots.
static void v04UiWorkflow(const fs::path &parent) {
    FocusManager focus;
    focus.begin("library");focus.add("book",{0,60,100,100});focus.add("menu",{120,0,56,56});focus.add("disabled",{0,0,56,56},false);focus.finish();
    assert(focus.focused("book"));assert(focus.move(1,0) && focus.focused("menu"));
    focus.begin("modal");focus.add("cancel",{0,0,56,56});focus.add("confirm",{80,0,56,56});focus.finish();
    assert(focus.focused("cancel"));assert(focus.move(1,0) && focus.focused("confirm"));
    focus.begin("library");focus.add("book",{0,60,100,100});focus.add("menu",{120,0,56,56});focus.finish();assert(focus.focused("menu"));
    assert(focus.touch(20,100) && focus.focused("book"));
    const auto root=parent/"v04-ui";fs::create_directories(root/"books");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    fs::copy_file("tests/fixtures/sample_book.epub",root/"books"/"paper.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf",root/"books"/"pdf.pdf");
    {
        PaperNxApp app(root);assert(initReady(app));
        auto assertReaderChrome=[&](int variant,SDL_Color expected) {
            auto prefs=app.appSettings_.get();prefs.interfaceStyle=variant;prefs.interfaceAccent=7;app.appSettings_.put(prefs);
            app.toolbarVisible_=true;app.render();int w,h;app.getWindowSize(w,h);std::vector<Uint32> pixels(w*h);
            assert(SDL_RenderReadPixels(app.renderer_,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),w*4)==0);
            assert((pixels[36*w+20]&0xffffff)==((Uint32(expected.r)<<16)|(Uint32(expected.g)<<8)|expected.b));
            app.appSettings_.put(prefs);
        };
        auto testPreferences=app.appSettings_.get();testPreferences.bookLandscapeSpreads=false;testPreferences.comicLandscapeSpreads=false;app.appSettings_.put(testPreferences);
        app.openSettings(true);auto old=app.currentStyle();
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_DOWN);assert(app.aaRow_==1 && app.currentFontFamily_==old.fontFamily);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_UP);pad(app,SDL_CONTROLLER_BUTTON_A);assert(app.settingsEditing_);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.currentFontSize_==old.fontSize+2);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_LEFT);assert(app.currentFontSize_==old.fontSize);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);pad(app,SDL_CONTROLLER_BUTTON_B);assert(!app.settingsEditing_ && app.currentFontSize_==old.fontSize);
        pad(app,SDL_CONTROLLER_BUTTON_A);pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);pad(app,SDL_CONTROLLER_BUTTON_A);
        assert(app.currentFontSize_==old.fontSize+2 && !app.settingsEditing_);
        std::vector<double> pressedMs,actionMs,focusMs;
        auto elapsed=[](auto started){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();};
        app.render();
        int bw,bh;app.getWindowSize(bw,bh);auto tab=ui::SettingsLayout(bw,bh).tab(0);
        for(int i=0;i<32;++i) {
            const float x=float(tab.x+tab.w/2)/bw,y=float(tab.y+tab.h/2)/bh;
            auto started=std::chrono::steady_clock::now();finger(app,SDL_FINGERDOWN,x,y);app.render();pressedMs.push_back(elapsed(started));
            finger(app,SDL_FINGERUP,x,y);app.render();actionMs.push_back(elapsed(started));
            started=std::chrono::steady_clock::now();app.handleDirectional(0,i%2?1:-1);app.render();focusMs.push_back(elapsed(started));
        }
        auto percentile=[](std::vector<double> samples,double fraction){std::sort(samples.begin(),samples.end());return samples[std::min(samples.size()-1,std::size_t(fraction*samples.size()))];};
        const auto evidence=test_evidence::path("v04-ui-latency.json");fs::create_directories(evidence.parent_path());std::ofstream bench(evidence);
        bench<<"{\n  \"environment\": \"Docker SDL dummy software renderer; CPU event-to-present-return only; physical visible latency UNVERIFIED\",\n";
        auto metric=[&](const char *name,const std::vector<double>&samples,bool last){bench<<"  \""<<name<<"\": {\"p50_ms\":"<<percentile(samples,.5)<<",\"p95_ms\":"<<percentile(samples,.95)<<",\"worst_ms\":"<<*std::max_element(samples.begin(),samples.end())<<"}"<<(last?"\n":",\n");};
        metric("tap_to_pressed_present",pressedMs,false);metric("tap_to_action_present",actionMs,false);metric("focus_to_present",focusMs,true);bench<<"}\n";
        snapshot(app,"v04-settings-focus-landscape");
        app.editColor(1);pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.extraStyle_.backgroundColor==app.swatchColors_[app.swatchFocus_]);
        {auto panel=app.colorPanel(bw,bh);float sx=float(panel.x+panel.w-40)/bw,sy=float(panel.y+376)/bh;
            finger(app,SDL_FINGERUP,sx,sy);assert(app.colorEditing_>=0);
            finger(app,SDL_FINGERDOWN,sx,sy,81);finger(app,SDL_FINGERDOWN,sx,sy,82);finger(app,SDL_FINGERUP,sx,sy,81);finger(app,SDL_FINGERUP,sx,sy,82);assert(app.colorEditing_>=0);}
        snapshot(app,"v04-color-swatches");pad(app,SDL_CONTROLLER_BUTTON_B);assert(app.extraStyle_.backgroundColor==old.backgroundColor);
        closeSettings(app);
        auto epub=*std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Epub;});
        app.openBook(epub);app.currentTheme_=2;app.extraStyle_.backgroundColor=app.extraStyle_.textColor=-1;app.queueStyleChange();v03WaitLayout(app);
        app.openSettings();assert(app.palette().bg.r==g_palettes[2].bg.r);snapshot(app,"v04-sepia-epub-settings");closeSettings(app);
        app.currentTheme_=4;app.extraStyle_.backgroundColor=app.extraStyle_.textColor=-1;app.queueStyleChange();v03WaitLayout(app);
        app.screen_=ScreenState::TocModal;snapshot(app,"v04-dark-epub-toc");app.screen_=ScreenState::Reader;
        app.currentTheme_=5;app.queueStyleChange();v03WaitLayout(app);
        auto followHeader=readerChromePalette(app.presentedStyle(),false,0,0).header;
        assertReaderChrome(0,{30,33,48,255});assertReaderChrome(1,{237,240,249,255});assertReaderChrome(2,followHeader);
        app.quickMenu_=true;snapshot(app,"v04-oled-quick-menu");app.quickMenu_=false;
        app.closeBook();
        // One global theme picker controls reader, library, headers and settings.
        // Old separate application/chrome policy settings cannot override it.
        {
            auto legacy=app.appSettings_.get();legacy.chromePolicy=2;
            legacy.customUiTheme=4;legacy.uiTheme=0;app.appSettings_.put(legacy);
            assert(app.appSettings_.get().chromePolicy==0);
            app.openSettings(true,5);
            assert(app.settingsRows()==17);
            app.openChoice(7,0);
            assert(app.choiceOptions().size()==10);
            const char *namesTr[]={"Saf Kâğıt","Yumuşak Fildişi","Sıcak Krem","Doğal Kâğıt","Nostaljik Sepya","Keten","Serin Kâğıt","Kömür Gecesi","OLED Siyah","Yumuşak E-Mürekkep"};
            ui::language=0;auto labels=app.choiceOptions();
            for(int i=0;i<10;++i)assert(labels[i]==namesTr[i] && app.paperPresetLabel(i)==namesTr[i]);
            ui::language=1;labels=app.choiceOptions();
            for(int i=0;i<10;++i)assert(labels[i]==g_paperPresets[i].name);
            app.choiceIndex_=4;app.commitChoice(); // Vintage Sepia
            const auto defaults=app.progressStore_.get("PAPERNX_DEFAULTS");
            assert(defaults.theme==2 && defaults.backgroundColor==0xe8d6b3);
            assert(defaults.textColor==0x4b3828 && defaults.styleOverride==0);
            assert(app.globalPaperPresetIndex()==4);
            assert(app.palette().bg.r==0xe8 && app.palette().bg.g==0xd6);
            snapshot(app,"v04-unified-global-settings-sepia");
            closeSettings(app);
            assert(app.appSettings_.save());
            app.flushProgress();
        }
        // App-wide Follow Reading Theme must use persisted defaults, not stale Paper UI.
        {
            auto defaults=app.progressStore_.get("PAPERNX_DEFAULTS");
            defaults.theme=2;defaults.backgroundColor=0xe8d6b3;defaults.textColor=0x4b3828;
            app.progressStore_.put("PAPERNX_DEFAULTS",defaults);
            auto prefs=app.appSettings_.get();prefs.chromePolicy=0;app.appSettings_.put(prefs);
            assert(app.palette().bg.r==0xe8);
            auto html=generateEpubCss(defaults);
            assert(html.find("background-color: #e8d6b3")!=std::string::npos);
            defaults.publisherStyles=1;
            assert(generateEpubCss(defaults).find("papernx-reader-controlled")!=std::string::npos);
        }
        // Default D-pad pagination wins even with permanently visible reader tools.
        app.openBook(epub);
        {
            auto prefs=app.appSettings_.get();prefs.toolbar=1;app.appSettings_.put(prefs);
            app.currentPage_=0;
            pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.currentPage_==1);
            pad(app,SDL_CONTROLLER_BUTTON_DPAD_UP);assert(app.readerToolbarFocusActive_);
            const int beforeFocus=app.currentPage_;
            pad(app,SDL_CONTROLLER_BUTTON_DPAD_LEFT);assert(app.currentPage_==beforeFocus);
            pad(app,SDL_CONTROLLER_BUTTON_B);assert(!app.readerToolbarFocusActive_ && app.doc_);
            pad(app,SDL_CONTROLLER_BUTTON_DPAD_LEFT);assert(app.currentPage_==0);
            prefs.toolbar=0;app.appSettings_.put(prefs);
        }
        app.closeBook();
        // Long finger movement through the *full event chain* must scroll a choice list.
        {
            // Six choice rows fit within 480px; use a deliberately short viewport
            // to exercise overflow and real swipe routing through the parent modal.
            SDL_SetWindowSize(app.window_,640,320);
            app.openSettings(true);
            app.openChoice(7,0);
            int cw,ch;app.getWindowSize(cw,ch);
            auto region=app.choiceContentRect(cw,ch);
            const float cx=float(region.x+region.w/2)/cw,cy=float(region.y+region.h-24)/ch;
            finger(app,SDL_FINGERDOWN,cx,cy,91);
            finger(app,SDL_FINGERMOTION,cx,cy-.22f,91);
            assert(app.choiceScroll_>0.f);
            finger(app,SDL_FINGERUP,cx,cy-.22f,91);
            pad(app,SDL_CONTROLLER_BUTTON_B);
            assert(app.choiceKind_<0);
            closeSettings(app);
            SDL_SetWindowSize(app.window_,1280,720);
        }
        for(int orientation=1;orientation<=2;++orientation) {
            auto a=app.appSettings_.get();a.displayOrientation=orientation;app.appSettings_.put(a);app.persistAppSettings();
            int w,h;app.getWindowSize(w,h);assert(w==720 && h==1280);app.render();
            const char *name=orientation==1?"v04-portrait-cw-library":"v04-portrait-ccw-library";snapshot(app,name);
            // Tap physical point corresponding to logical +; assert inverse input mapping.
            DisplayTransform transform{1280,720,orientation};auto point=transform.toPhysical(float(w-48),40);
            tap(app,point.first/1280,point.second/720);assert(app.quickMenu_);app.quickMenu_=false;
            app.openBook(epub);assert(app.layoutWidth_<=720);app.render();
            snapshot(app,orientation==1?"v04-portrait-cw-epub":"v04-portrait-ccw-epub");app.closeBook();
            app.openBook(epub);app.extraStyle_.readingMode=1;app.reflowEpub(app.currentFontSize_);assert(app.continuous());
            auto physicalFinger=[&](Uint32 type,float x,float y,SDL_FingerID id=61){auto physical=transform.toPhysical(x*w,y*h);finger(app,type,physical.first/1280,physical.second/720,id);};
            float oldY=app.scrollY_;physicalFinger(SDL_FINGERDOWN,.5f,.7f);physicalFinger(SDL_FINGERMOTION,.5f,.4f);physicalFinger(SDL_FINGERUP,.5f,.4f);assert(app.scrollY_>oldY || app.currentPage_>0);
            snapshot(app,orientation==1?"v04-portrait-cw-scroll":"v04-portrait-ccw-scroll");app.closeBook();
            auto portraitPdf=*std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Pdf;});app.openBook(portraitPdf);app.zoomPdf(2);
            // Logical page-right is physical down after a clockwise display
            // rotation, and physical up after a counterclockwise rotation.
            const auto next=orientation==1?SDL_CONTROLLER_BUTTON_DPAD_DOWN:SDL_CONTROLLER_BUTTON_DPAD_UP;
            const auto previous=orientation==1?SDL_CONTROLLER_BUTTON_DPAD_UP:SDL_CONTROLLER_BUTTON_DPAD_DOWN;
            app.currentPage_=0;pad(app,next);assert(app.currentPage_==1);pad(app,previous);assert(app.currentPage_==0);
            physicalFinger(SDL_FINGERDOWN,.5f,.7f);physicalFinger(SDL_FINGERMOTION,.5f,.4f);physicalFinger(SDL_FINGERUP,.5f,.4f);assert(app.pdfPanY_>0);
            SDL_Event pinch{};pinch.type=SDL_MULTIGESTURE;pinch.mgesture.numFingers=2;pinch.mgesture.dDist=.05f;auto physical=transform.toPhysical(w*.25f,h*.7f);pinch.mgesture.x=physical.first/1280;pinch.mgesture.y=physical.second/720;bool running=true;app.handleEvent(pinch,running);assert(app.pdfZoom_>2);
            snapshot(app,orientation==1?"v04-portrait-cw-pdf":"v04-portrait-ccw-pdf");app.closeBook();
            assert(app.appSettings_.save());{PaperNxApp resumed(root);assert(initReady(resumed));assert(resumed.appSettings_.get().displayOrientation==orientation);int rw,rh;resumed.getWindowSize(rw,rh);assert(rw==720 && rh==1280);}

        }
        auto a=app.appSettings_.get();a.displayOrientation=0;app.appSettings_.put(a);app.persistAppSettings();
        auto pdf=*std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Pdf;});app.openBook(pdf);
        followHeader=readerChromePalette(app.presentedStyle(),false,0,0).header;
        assertReaderChrome(0,{30,33,48,255});assertReaderChrome(1,{237,240,249,255});assertReaderChrome(2,followHeader);
        // The PDF uses EPUB's Appearance palette; the former independent
        // PDF filter is legacy data and may not change reader chrome.
        const auto globalBg=app.palette().bg;
        app.openSettings();
        assert(app.aaTab_==2);
        settingsControl(app,2,false); // Charcoal Night via same preset grid as EPUB.
        assert(app.extraStyle_.backgroundColor==0x252522);
        assert(app.palette().bg.r==0x25 && app.palette().bg.g==0x25);
        app.toolbarVisible_=true;snapshot(app,"v04-dark-pdf-toolbar");
        snapshot(app,"v04-dark-pdf-settings");
        closeSettings(app);app.toolbarVisible_=false;
        app.openSettings();settingsTab(app,2);
        settingsControl(app,1,false); // Vintage Sepia.
        assert(app.extraStyle_.backgroundColor==0xe8d6b3);
        assert(app.palette().bg.r==0xe8 && app.palette().bg.g==0xd6);
        snapshot(app,"v04-sepia-pdf-canvas");
        closeSettings(app);
        app.closeBook();assert(app.palette().bg.r==globalBg.r && app.palette().bg.g==globalBg.g);
        app.openBook(pdf);
        assert(app.extraStyle_.styleOverride==1 && app.extraStyle_.backgroundColor==0xe8d6b3);
        assert(app.palette().bg.r==0xe8);
        app.openSettings();settingsTab(app,2);settingsControl(app,0,false); // Pure Paper.
        closeSettings(app);snapshot(app,"v04-original-pdf");
        app.zoomPdf(2);app.currentPage_=0;pad(app,SDL_CONTROLLER_BUTTON_DPAD_RIGHT);assert(app.currentPage_==1 && app.pdfZoom_==2);
        pad(app,SDL_CONTROLLER_BUTTON_DPAD_LEFT);assert(app.currentPage_==0 && app.pdfZoom_==2);
        finger(app,SDL_FINGERDOWN,.8f,.5f);finger(app,SDL_FINGERMOTION,.2f,.5f);finger(app,SDL_FINGERUP,.2f,.5f);assert(app.currentPage_==1);
        finger(app,SDL_FINGERDOWN,.2f,.5f);finger(app,SDL_FINGERMOTION,.8f,.5f);finger(app,SDL_FINGERUP,.8f,.5f);assert(app.currentPage_==0);
        finger(app,SDL_FINGERDOWN,.5f,.7f);app.pdfGestureStart_=SDL_GetTicks()-600;finger(app,SDL_FINGERMOTION,.3f,.7f);finger(app,SDL_FINGERUP,.3f,.7f);assert(app.currentPage_==0);
        app.closeBook();
        // The pinned orientation shortcut remains directly reachable by touch.
        {
            app.quickMenu_=true;int qw,qh;app.getWindowSize(qw,qh);
            auto sheet=app.quickSheet(qw,qh);
            const auto shortcut=app.quickOrientationShortcut(sheet);
            app.handleQuickMenuTap(shortcut.x+shortcut.w/2,shortcut.y+shortcut.h/2,qw,qh);
            assert(app.choiceKind_==1 && !app.quickMenu_);
            app.choiceIndex_=0;app.commitChoice();
        }
        // EPUB appearance is book-scoped; old raster and canvas remain visually
        // synchronized while a new MuPDF layout is being prepared.
        app.openBook(epub);
        const auto priorTheme=app.renderedStyle_.theme;
        const auto defaultsBefore=app.progressStore_.get("PAPERNX_DEFAULTS");
        app.currentTheme_=(priorTheme==4?2:4);
        app.extraStyle_.backgroundColor=-1;app.extraStyle_.textColor=-1;
        app.queueStyleChange();
        assert(app.extraStyle_.styleOverride==1);
        assert(app.palette().bg.r==g_palettes[priorTheme].bg.r);
        v03WaitLayout(app);
        assert(app.palette().bg.r==g_palettes[app.currentTheme_].bg.r);
        const int savedBookTheme=app.currentTheme_;
        app.closeBook();
        assert(app.progressStore_.get("PAPERNX_DEFAULTS").theme==defaultsBefore.theme);
        app.openBook(epub);
        assert(app.currentTheme_==savedBookTheme && app.extraStyle_.styleOverride==1);
        app.closeBook();
        // Opt-in book styling remains isolated when the global palette changes.
        app.openBook(epub);
        auto bookStyle=app.currentStyle();
        bookStyle.styleOverride=1;bookStyle.theme=5;
        bookStyle.backgroundColor=0x000000;bookStyle.textColor=0xeae7df;
        app.applyStyle(bookStyle);app.saveCurrentPosition();
        app.openSettings(true,5);
        app.applyGlobalPaperPreset(1); // Soft Ivory, globally
        assert(app.currentTheme_==5 && app.extraStyle_.styleOverride==1);
        assert(app.extraStyle_.backgroundColor==0x000000);
        app.dismissSettings();app.closeBook();
        assert(app.palette().bg.r==0xf7 && app.palette().bg.g==0xf3); // Library is Ivory
        app.openBook(epub);
        assert(app.currentTheme_==5 && app.extraStyle_.styleOverride==1);
        assert(app.extraStyle_.backgroundColor==0x000000); // Book stays OLED
        app.closeBook();
        for(auto size:{std::pair<int,int>{640,480},{1920,1080}}){SDL_SetWindowSize(app.window_,size.first,size.second);snapshot(app,size.first==640?"v04-library-640":"v04-library-1920");}
    }
    {AppSettingsStore store(root/"data"/"settings.txt");assert(store.load() && store.get().displayOrientation==0);}
}
