// Included inside AppTest after v03_app_workflow.hpp.
static std::pair<int,int> v03InkBounds(const fs::path &path,bool publisher) {
    ReaderPreferences style; style.fontSize=26; style.publisherStyles=publisher;
    char error[256]{}; const auto css=generateEpubCss(style);
    auto *doc=px_document_open_limited(path.string().c_str(),600,480,26,css.c_str(),error,sizeof error,64u*1024u*1024u);
    assert(doc); PxImage image{}; assert(px_document_render(doc,0,600,480,&image,error,sizeof error));
    int minX=image.width,maxX=-1,minY=image.height,maxY=-1;
    for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
        const auto *pixel=image.rgb+y*image.stride+x*3;
        if(pixel[0]<150 && pixel[1]<150 && pixel[2]<150) {
            minX=std::min(minX,x); maxX=std::max(maxX,x); minY=std::min(minY,y); maxY=std::max(maxY,y);
        }
    }
    px_image_free(&image); px_document_close(doc); assert(maxX>=minX && maxY>=minY);
    return {maxX-minX+1,maxY-minY+1};
}
static void v03PositionBookmarks(const fs::path &fixtures) {
    ReaderPreferences style; style.fontSize=26;
    auto css=generateEpubCss(style); char error[256]{};
    auto *doc=px_document_open_limited((fixtures/"large.epub").string().c_str(),1200,0,26,css.c_str(),error,sizeof error,64u*1024u*1024u);
    assert(doc);
    const auto a=px_document_make_position_bookmark(doc,0,500.25f);
    const auto b=px_document_make_position_bookmark(doc,0,1100.75f);
    assert(a>=INT64_C(0x6000000000000000) && a!=b);
    int page=-1; float y=0;
    assert(px_document_lookup_position_bookmark(doc,a,&page,&y) && page==0 && std::abs(y-500.25f)<1);
    assert(px_document_lookup_position_bookmark(doc,b,&page,&y) && page==0 && std::abs(y-1100.75f)<1);
    assert(px_document_lookup_bookmark(doc,b)==0);
    int chapter=-1,character=-1; double offset=0;
    assert(px_document_anchor(doc,page,y,&chapter,&character,&offset) && character>=0);
    style.fontSize=32; css=generateEpubCss(style);
    assert(px_document_relayout(doc,1000,0,32,css.c_str(),error,sizeof error));
    assert(px_document_lookup_position_bookmark(doc,b,&page,&y));
    int restoredChapter=-1,restoredCharacter=-1; double restoredOffset=0;
    assert(px_document_anchor(doc,page,y,&restoredChapter,&restoredCharacter,&restoredOffset));
    assert(restoredChapter==chapter && std::abs(restoredCharacter-character)<160);
    const float themedY=y; style.theme=2; css=generateEpubCss(style);
    assert(px_document_relayout(doc,1000,0,32,css.c_str(),error,sizeof error));
    assert(px_document_lookup_position_bookmark(doc,b,&page,&y) && std::abs(y-themedY)<1);
    px_document_close(doc);
    // Packed position survives a new context and both continuous and paginated layouts.
    doc=px_document_open_limited((fixtures/"large.epub").string().c_str(),1000,0,32,css.c_str(),error,sizeof error,64u*1024u*1024u);
    assert(doc && px_document_lookup_position_bookmark(doc,b,&page,&y) && std::abs(y-themedY)<1);
    assert(px_document_relayout(doc,1000,576,32,css.c_str(),error,sizeof error));
    assert(px_document_lookup_position_bookmark(doc,b,&page,&y) && page>=0);
    const auto legacy=px_document_make_bookmark(doc,page);
    assert(px_document_lookup_position_bookmark(doc,legacy,&page,&y) && y==0);
    assert(!px_document_lookup_position_bookmark(doc,-1,&page,&y));
    assert(!px_document_lookup_position_bookmark(doc,b,nullptr,&y));
    assert(!px_document_lookup_position_bookmark(doc,INT64_C(0x6000000000001fff),&page,&y)); // Invalid residual for a text anchor.
    assert(px_document_make_position_bookmark(doc,-1,0)==-1);
    px_document_close(doc);
    doc=px_document_open_limited((fixtures/"illustrated.epub").string().c_str(),1200,0,26,generateEpubCss(ReaderPreferences{}).c_str(),error,sizeof error,64u*1024u*1024u);
    assert(doc); chapter=character=-1; offset=0;
    assert(px_document_anchor(doc,0,400.5f,&chapter,&character,&offset) && character==-1);
    const auto imageMark=px_document_make_position_bookmark(doc,0,400.5f);
    assert(px_document_lookup_position_bookmark(doc,imageMark,&page,&y) && page==0 && std::abs(y-400.5f)<1);
    px_document_close(doc);
    doc=px_document_open_limited((fixtures/"illustrated.epub").string().c_str(),1200,0,26,generateEpubCss(ReaderPreferences{}).c_str(),error,sizeof error,64u*1024u*1024u);
    assert(doc && px_document_lookup_position_bookmark(doc,imageMark,&page,&y) && std::abs(y-400.5f)<1);
    px_document_close(doc);
}
static void v03StressWorkflow(const fs::path &parent) {
    const fs::path fixtures="build/test-data/v03-stress";
    for(const auto *name:{"large.epub","illustrated.epub","academic-320.pdf","tiny-inline.epub","normal-inline.epub"})
        assert(fs::is_regular_file(fixtures/name) && "Run tests/generate_stress_fixtures.py first");
    v03PositionBookmarks(fixtures);
    const auto tiny=v03InkBounds(fixtures/"tiny-inline.epub",false);
    const auto normal=v03InkBounds(fixtures/"normal-inline.epub",false);
    std::cerr<<"BENCH inline_font tiny="<<tiny.first<<"x"<<tiny.second<<" normal="<<normal.first<<"x"<<normal.second<<"\n";
    assert(std::abs(tiny.first-normal.first)<=2 && std::abs(tiny.second-normal.second)<=2 &&
           "Reader-controlled typography must normalize inline publisher font-size:6px");
    const auto publisherTiny=v03InkBounds(fixtures/"tiny-inline.epub",true);
    assert(publisherTiny.second<normal.second && publisherTiny.first<normal.first);
    const auto root=parent/"v03-stress"; fs::create_directories(root/"books");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    for(const auto *name:{"large.epub","illustrated.epub","academic-320.pdf"})
        fs::copy_file(fixtures/name,root/"books"/name);
    double largeOpenMs=0,imageOpenMs=0,pdfOpenMs=0,cancelMs=0,pdfRenderMs=0;
    std::size_t peakTexture=0; std::size_t renderedFrames=0;
    auto elapsed=[](auto start){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
    {
        PaperNxApp app(root); assert(initReady(app));
        auto findBook=[&](const char *name) {
            auto book=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==name;});
            assert(book!=app.books_.end()); return *book;
        };
        Progress defaults; defaults.readingMode=1; defaults.fontSize=26;
        app.progressStore_.put("PAPERNX_DEFAULTS",defaults);
        auto start=std::chrono::steady_clock::now(); app.openBook(findBook("large.epub")); largeOpenMs=elapsed(start);
        assert(app.doc_ && app.continuous() && app.pageCount_==24);
        for(int chapter=0;chapter<app.pageCount_;++chapter) {
            app.currentPage_=chapter;
            const float height=app.segmentHeight(chapter);
            for(float y:{0.f,height*.25f,height*.5f,height*.75f}) {
                app.scrollY_=y; app.render(); ++renderedFrames;
                peakTexture=std::max(peakTexture,app.pageCache_.currentBytes());
                assert(app.pageCache_.currentBytes()<=64u*1024u*1024u);
            }
        }
        assert(peakTexture>32u*1024u*1024u && "Fixture should exercise actual cache eviction pressure");
        // Force a genuinely in-flight request obsolete before accepting its worker result.
        app.openSettings(); app.aaTab_=0;
        const auto accepted=app.completedLayouts_; start=std::chrono::steady_clock::now();
        v03Control(app,0); SDL_Delay(150); app.processLayout();
        assert(app.layoutWorker_.joinable());
        v03Control(app,0); v03Control(app,0); v03WaitLayout(app); cancelMs=elapsed(start);
        assert(app.completedLayouts_==accepted+1 && app.renderedStyle_.fontSize==32);
        v03CloseSettings(app); app.closeBook();
        // Eight separate 2048x1536 illustrations exercise decode/store eviction.
        start=std::chrono::steady_clock::now(); app.openBook(findBook("illustrated.epub")); imageOpenMs=elapsed(start);
        assert(app.doc_ && app.continuous() && app.pageCount_==8);
        for(int chapter=0;chapter<8;++chapter) {
            app.currentPage_=chapter; app.scrollY_=0; app.render(); ++renderedFrames;
            peakTexture=std::max(peakTexture,app.pageCache_.currentBytes());
            assert(app.pageCache_.currentBytes()<=64u*1024u*1024u);
        }
        app.closeBook();
        start=std::chrono::steady_clock::now(); app.openBook(findBook("academic-320.pdf")); pdfOpenMs=elapsed(start);
        assert(app.doc_ && !app.continuous() && app.pageCount_==320);
        app.setPdfView(1); assert(app.extraStyle_.pdfViewMode==1);
        app.zoomPdf(2); app.toolbarVisible_=false;
        finger(app,SDL_FINGERDOWN,.6f,.7f); finger(app,SDL_FINGERMOTION,.4f,.3f); finger(app,SDL_FINGERUP,.4f,.3f);
        assert(app.pdfPanY_>0 && app.currentPage_==0);
        start=std::chrono::steady_clock::now();
        for(int page=0;page<320;page+=7) {
            app.turnPage(page-app.currentPage_); app.render(); ++renderedFrames;
            peakTexture=std::max(peakTexture,app.pageCache_.currentBytes());
            assert(app.pageCache_.currentBytes()<=64u*1024u*1024u);
        }
        app.turnPage(319-app.currentPage_); app.render(); pdfRenderMs=elapsed(start);
        assert(app.currentPage_==319);
    }
    // EPUB themes preserve original image pixels instead of filtering the raster.
    for(int theme:{0,2,3}) {
        ReaderPreferences style; style.theme=theme;
        char error[256]{}; const auto css=generateEpubCss(style);
        auto *doc=px_document_open_limited((fixtures/"illustrated.epub").string().c_str(),1200,0,26,css.c_str(),error,sizeof error,64u*1024u*1024u);
        assert(doc); PxImage image{};
        assert(px_document_render_tile(doc,0,0,1200,1024,&image,error,sizeof error));
        std::size_t colored=0;
        for(int y=0;y<image.height;++y) for(int x=0;x<image.width;++x) {
            const auto *p=image.rgb+y*image.stride+x*3;
            if(std::abs(int(p[0])-24)<=1 && std::abs(int(p[1])-160)<=1 && std::abs(int(p[2])-80)<=1) ++colored;
        }
        assert(colored>10000 && "Original image pixels must survive all EPUB reader themes");
        px_image_free(&image); px_document_close(doc);
    }
    long processPeakKiB=0; std::ifstream status("/proc/self/status"); std::string line;
    while(std::getline(status,line)) if(line.rfind("VmHWM:",0)==0) { std::istringstream value(line.substr(6)); value>>processPeakKiB; }
    std::ofstream out("verification/v03-stress-benchmark.json");
    out<<"{\n  \"backend\": \"desktop SDL dummy; physical Switch UNVERIFIED\",\n"
       <<"  \"large_epub_chapters\": 24,\n  \"large_epub_open_ms\": "<<largeOpenMs
       <<",\n  \"illustrated_epub_chapters\": 8,\n  \"image_dimensions\": \"2048x1536\",\n  \"illustrated_epub_open_ms\": "<<imageOpenMs
       <<",\n  \"pdf_pages\": 320,\n  \"pdf_open_ms\": "<<pdfOpenMs<<",\n  \"pdf_sampled_render_total_ms\": "<<pdfRenderMs
       <<",\n  \"obsolete_then_last_layout_ms\": "<<cancelMs<<",\n  \"accepted_layouts_for_three_requests\": 1"
       <<",\n  \"peak_page_texture_bytes\": "<<peakTexture<<",\n  \"process_high_water_rss_kib\": "<<processPeakKiB
       <<",\n  \"sampled_render_frames\": "<<renderedFrames<<",\n  \"reader_controlled_inline_font_pixel_width\": "<<tiny.first
       <<",\n  \"publisher_inline_font_pixel_width\": "<<publisherTiny.first<<"\n}\n";
    std::cout<<"[PASS] Native 24-chapter EPUB, eight 2048x1536 images, 320-page PDF, in-flight layout cancellation, image colors and bounded textures\n";
}
