// Actual SDL zero-controller and pad flows against disposable fixture copies.
static void v04CatalogWorkflow(const fs::path &parent){
    const auto root=parent/"v04-catalog";fs::create_directories(root/"books");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    fs::copy_file("tests/fixtures/sample_book.epub",root/"books"/"disposable.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf",root/"books"/"disposable.pdf");
    PaperNxApp app(root);assert(initReady(app));assert(app.visibleBooks_.size()==2);
    const auto selected=std::find_if(app.visibleBooks_.begin(),app.visibleBooks_.end(),[](const Book &b){return b.format==Format::Epub;});assert(selected!=app.visibleBooks_.end());const auto book=*selected;const int bookIndex=int(selected-app.visibleBooks_.begin());
    auto choose=[&](int id){const auto rows=app.catalogRows();auto it=std::find_if(rows.begin(),rows.end(),[&](const auto &r){return r.first==id;});assert(it!=rows.end());int width,height;app.getWindowSize(width,height);const auto panel=app.catalogPanelRect(width,height);const int top=app.catalogContentTop(panel),index=int(it-rows.begin());app.catalogScroll_=float(index*64);tap(app,float(panel.x+80)/width,float(top+28)/height);};
    // Touch hold enters actions before release and release never activates Open.
    int w,h;app.getWindowSize(w,h);ui::LibraryLayout layout(w);const float x=float(layout.left+(bookIndex%layout.columns)*layout.stepX+80)/w,y=float(layout.top+(bookIndex/layout.columns)*layout.stepY+100)/h;
    finger(app,SDL_FINGERDOWN,x,y);app.libraryHoldStart_=SDL_GetTicks()-600;app.tickLibraryHold();assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Actions);
    finger(app,SDL_FINGERUP,x,y);assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Actions&&!app.doc_);
    snapshot(app,"v04-book-context");
    choose(1);assert(app.progressStore_.get(book.id).favorite);
    // Setup 30% reading progress on book to verify the exact status transitions:
    {
        Progress p = app.progressStore_.get(book.id);
        p.pageCount = 100; p.page = 30; p.fraction = 0.30;
        app.progressStore_.put(book.id, p);
        assert(app.progressStore_.save());
    }

    // 1) 30% -> Okunmadı (choose 3) -> 0% progress & page 0
    choose(3);
    assert(app.catalog_.get(book.id).status==ReadingStatus::NotStarted);
    assert(app.catalog_.get(app.actionBook_.id).status==ReadingStatus::NotStarted);
    assert(app.progressStore_.get(book.id).fraction==0.0);
    assert(app.progressStore_.get(book.id).page==0);
    app.category_=Category::Unread;app.updateVisibleBooks();
    assert(std::any_of(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &candidate){return candidate.id==book.id;}));
    app.category_=Category::All;app.updateVisibleBooks();

    // 2) 30% -> Okunuyor (choose 2) -> retains 30% progress & page 30
    {
        Progress p = app.progressStore_.get(book.id);
        p.pageCount = 100; p.page = 30; p.fraction = 0.30;
        app.progressStore_.put(book.id, p);
        assert(app.progressStore_.save());
    }
    choose(2);
    assert(app.catalog_.get(book.id).status==ReadingStatus::Reading);
    assert(app.catalog_.get(app.actionBook_.id).status==ReadingStatus::Reading);
    assert(std::abs(app.progressStore_.get(book.id).fraction - 0.30) < 0.01);
    assert(app.progressStore_.get(book.id).page==30);
    app.category_=Category::ContinueReading;app.updateVisibleBooks();
    assert(std::any_of(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &candidate){return candidate.id==book.id;}));
    app.category_=Category::All;app.updateVisibleBooks();

    // 3) 30% -> Okundu / Tamamlandı (choose 4) -> 100% progress & page 99
    choose(4);
    assert(app.catalog_.get(book.id).status==ReadingStatus::Completed);
    assert(app.catalog_.get(app.actionBook_.id).status==ReadingStatus::Completed);
    assert(app.progressStore_.get(book.id).fraction==1.0);
    assert(app.progressStore_.get(book.id).page==99);
    app.category_=Category::Completed;app.updateVisibleBooks();
    assert(std::any_of(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &candidate){return candidate.id==book.id;}));
    app.category_=Category::All;app.updateVisibleBooks();

    // 4) 100% -> Yeniden okunuyor (choose 2) -> starts fresh read at 0%
    choose(2);
    assert(app.catalog_.get(book.id).status==ReadingStatus::Reading);
    assert(app.catalog_.get(app.actionBook_.id).status==ReadingStatus::Reading);
    assert(app.progressStore_.get(book.id).fraction==0.0);
    assert(app.progressStore_.get(book.id).page==0);
    app.category_=Category::ContinueReading;app.updateVisibleBooks();
    assert(std::any_of(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &candidate){return candidate.id==book.id;}));
    app.category_=Category::All;app.updateVisibleBooks();

    choose(6);assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Metadata);
    app.requestCatalogText(1,"Yaşar Kemal");assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Text);app.commitCatalogText();
    app.requestCatalogText(2,"Roman");app.commitCatalogText();assert(app.catalog_.get(book.id).author=="Yaşar Kemal");
    snapshot(app,"v04-metadata-editor");app.setCatalogPanel(PaperNxApp::CatalogPanel::Actions);choose(7);snapshot(app,"v04-book-details");
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Actions);choose(5);choose(-1);app.inputText_="Edebiyat";app.commitCatalogText();
    const auto shelf=app.catalog_.collections().begin()->first;assert(app.catalog_.addToCollection(book.id,shelf));assert(app.catalog_.save());snapshot(app,"v04-collections");
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);choose(1);assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Values);snapshot(app,"v04-authors");
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);choose(2);snapshot(app,"v04-genres");
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);choose(6);choose(2);assert(app.readingStats_.dailyGoal()==5);snapshot(app,"v04-reading-statistics");
    app.setCatalogPanel(PaperNxApp::CatalogPanel::None);app.enterJournal(false);assert(app.screen_==ScreenState::ReadingJournal);pad(app,SDL_CONTROLLER_BUTTON_B);assert(app.screen_==ScreenState::Library);
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Actions);choose(9);assert(fs::exists(book.path));snapshot(app,"v04-delete-confirmation");
    {const auto panel=app.catalogPanelRect(w,h);int top=app.catalogContentTop(panel);float dx=float(panel.x+80)/w,dy=float(top+64+28)/h;
        finger(app,SDL_FINGERUP,dx,dy,91);assert(fs::exists(book.path));
        finger(app,SDL_FINGERDOWN,dx,dy,91);finger(app,SDL_FINGERDOWN,dx,dy,92);finger(app,SDL_FINGERUP,dx,dy,91);finger(app,SDL_FINGERUP,dx,dy,92);assert(fs::exists(book.path));
        assert(app.catalogPanel_==PaperNxApp::CatalogPanel::DeleteConfirm);}

    pad(app,SDL_CONTROLLER_BUTTON_B);assert(fs::exists(book.path));app.setCatalogPanel(PaperNxApp::CatalogPanel::Actions);choose(8);assert(fs::exists(book.path));assert(app.visibleBooks_.size()==1);
    app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);choose(5);choose(0);assert(app.visibleBooks_.size()==2);
    app.setCatalogPanel(PaperNxApp::CatalogPanel::None);app.selectedBookIdx_=int(std::find_if(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &b){return b.id==book.id;})-app.visibleBooks_.begin());pad(app,SDL_CONTROLLER_BUTTON_X);assert(app.catalogPanel_==PaperNxApp::CatalogPanel::Actions);
    choose(9);choose(1);assert(!fs::exists(book.path));assert(app.visibleBooks_.size()==1);
    // Movement cancels hold and never opens a context panel.
    app.setCatalogPanel(PaperNxApp::CatalogPanel::None);finger(app,SDL_FINGERDOWN,x,y);finger(app,SDL_FINGERMOTION,x,y-.08f);app.libraryHoldStart_=SDL_GetTicks()-600;app.tickLibraryHold();assert(app.catalogPanel_==PaperNxApp::CatalogPanel::None);finger(app,SDL_FINGERUP,x,y-.08f);
    // Page Flip pins exact zoom/pan and returns after ordinary PDF navigation.
    auto pdf=std::find_if(app.books_.begin(),app.books_.end(),[](const Book &b){return b.format==Format::Pdf;});assert(pdf!=app.books_.end());app.openBook(*pdf);assert(app.doc_);
    app.zoomPdf(2);const int originalPage=app.currentPage_;const float originalZoom=app.pdfZoom_;app.pinPageFlip();app.turnPage(1);app.returnToPageFlip();assert(app.currentPage_==originalPage&&app.pdfZoom_==originalZoom&&!app.pageFlipPosition_);
    app.closeBook();
    for(const auto &size:std::vector<std::pair<int,int>>{{640,480},{1920,1080}}){SDL_SetWindowSize(app.window_,size.first,size.second);app.openBookActions(app.visibleBooks_[0]);snapshot(app,size.first==640?"v04-context-640":"v04-context-1080");}
}
