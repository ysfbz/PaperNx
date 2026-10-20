// Reader navigation: previews never mutate the reading position. A release or
// explicit confirmation commits one history entry; cancellation commits none.
PagePreviews pagePreviews_;
PageCache pagePreviewCache_{16u*1024u*1024u};
std::string previewIdentity_;
std::set<std::string> failedPreviews_;
unsigned previewEpoch_{0};
int tocTab_{0}; // chapters, all pages, bookmarked positions
float tocTabScroll_[3]{};
std::vector<Bookmark> tocBookmarks_;
std::vector<std::pair<int,float>> tocBookmarkPositions_;
std::set<int> tocBookmarkPages_;
bool tocReturnToolbar_{false};
int previewGridRow_{-1};
struct ScrubState {
    bool active{false},controller{false};float fraction{0},y{0},within{0};int page{0},orientation{0},width{0},height{0};
    unsigned epoch{0};
} scrub_;
bool scrubTouchOwned_{false};SDL_FingerID scrubFinger_{0};

SDL_Rect readerTrack(int w,int h) const {return {96,h-28,std::max(1,w-280),8};}
SDL_Rect readerSeekHit(int w,int h) const {auto r=readerTrack(w,h);return {r.x-8,h-52,r.w+16,44};}
SDL_Rect readerPrev(int,int h) const {return {24,h-64,56,56};}
SDL_Rect readerNext(int w,int h) const {return {w-152,h-64,56,56};}
SDL_Rect readerPages(int w,int h) const {return {w-88,h-64,64,56};}
SDL_Rect tocContent(int w,int h) const {return {48,216,std::max(1,w-96),std::max(1,h-272)};}
SDL_Rect tocTabRect(int tab,int w) const {const int step=(w-96)/3;return {48+tab*step,152,step-8,48};}
int tocColumns(int w) const {return tocTab_==0?1:std::clamp((w-96)/260,2,4);}
int tocStep() const {return tocTab_==0?120:224;}
int tocCount() const {return tocTab_==0?int(tocEntries_.size()):tocTab_==1?pageCount_:int(tocBookmarks_.size());}
int currentContentsChapter() const {
    int found=-1,page=-1;float y=-1;
    for(int i=0;i<int(tocEntries_.size());++i) {
        const auto &entry=tocEntries_[i];const float offset=continuous()?entry.y:0;
        if(entry.page<0 || entry.page>currentPage_ || (entry.page==currentPage_ && offset>scrollY_+1))continue;
        if(entry.page>page || (entry.page==page && offset>=y)){found=i;page=entry.page;y=offset;}
    }
    return found;
}
SDL_Rect tocCell(int index,int w,int h) {
    const auto content=tocContent(w,h);const int columns=tocColumns(w),step=content.w/columns;
    int column=index%columns;if(readingDirection()<0 && columns>1)column=columns-1-column;
    return {content.x+column*step,content.y+(index/columns)*tocStep()-int(tocScroll_),step-12,tocStep()-12};
}
float tocMaxScroll(int w,int h) const {return float(std::max(0,((tocCount()+tocColumns(w)-1)/tocColumns(w))*tocStep()-tocContent(w,h).h));}
struct NavigationItem {int page{-1},level{0};float y{0};std::string title;};
NavigationItem tocItem(int index) {
    if(index<0 || index>=tocCount())return {};
    if(tocTab_==0){const auto &e=tocEntries_[index];return {e.page,e.level,continuous()?e.y:0,e.title};}
    if(tocTab_==1)return {index,0,0,{}};
    const auto position=tocBookmarkPositions_[index];
    return {position.first,0,position.second,tocBookmarks_[index].title};
}
std::string navigationPageLabel(int page) const {
    return std::string(continuous() && !comic()?(ui::language?"Chapter ":"Bölüm "):(ui::language?"Page ":"Sayfa "))+std::to_string(page+1)+" / "+std::to_string(std::max(1,pageCount_));
}
void invalidatePagePreviews() {
    ++previewEpoch_;pagePreviews_.clear();pagePreviewCache_.clear();previewIdentity_.clear();failedPreviews_.clear();
}
void preparePagePreviews() {
    if(!doc_)return;
    const auto book=std::find_if(books_.begin(),books_.end(),[&](const Book &b){return b.id==currentBookId_;});
    if(book==books_.end())return;
    int w,h;getWindowSize(w,h);const auto style=presentedStyle();const auto color=palette();
    PagePreviews::Spec spec;spec.path=book->path;spec.viewportHeight=std::max(1,h-144);
    spec.width=px_document_is_epub(doc_)?std::max(1,layoutWidth_-style.margin*2):fixedViewportWidth(w);
    spec.height=px_document_is_epub(doc_) && continuous()?0:std::max(1,layoutHeight_-144);
    spec.em=float(style.fontSize);spec.css=px_document_is_epub(doc_)?generateEpubCss(style):"";
    spec.identity=currentBookId_+":"+std::to_string(previewEpoch_)+":"+std::to_string(spec.width)+":"+std::to_string(spec.height)+":"+std::to_string(spec.viewportHeight)+":"+std::to_string(spec.em)+":"+spec.css+":"+
        std::to_string(comicReadingColors())+":"+std::to_string((color.bg.r<<16)|(color.bg.g<<8)|color.bg.b)+":"+std::to_string((color.fg.r<<16)|(color.fg.g<<8)|color.fg.b);
    if(spec.identity!=previewIdentity_) {pagePreviewCache_.clear();failedPreviews_.clear();previewIdentity_=spec.identity;}
    pagePreviews_.configure(std::move(spec));
}
int previewOffset(int page,float y) {return continuous() && !comic()?std::max(0,int(documentScrollY(page,y))/32*32):0;}
std::string previewKey(int page,int y) const {return previewIdentity_+":"+std::to_string(page)+":"+std::to_string(y);}
void processPagePreviews() {
    for(auto &result:pagePreviews_.take()) {
        if(!doc_ || result.identity!=previewIdentity_)continue;
        const auto key=previewKey(result.page,result.y);
        if(result.rgb.empty()){failedPreviews_.insert(key);dirty_=true;continue;}
        if(!px_document_is_epub(doc_) && (!comic() || comicReadingColors())) {
            PxImage image{result.width,result.height,result.stride,result.rgb.data()};const auto color=palette();
            px_image_apply_reading_colors(&image,(color.bg.r<<16)|(color.bg.g<<8)|color.bg.b,(color.fg.r<<16)|(color.fg.g<<8)|color.fg.b);
        }
        auto texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC,result.width,result.height);
        if(texture && SDL_UpdateTexture(texture,nullptr,result.rgb.data(),result.stride)!=0){SDL_DestroyTexture(texture);texture=nullptr;}
        if(texture) {
#if SDL_VERSION_ATLEAST(2,0,12)
            SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
#endif
            pagePreviewCache_.put(key,texture,result.width,result.height);
        } else failedPreviews_.insert(key);
        dirty_=true;
    }
}
void drawPagePreview(int page,float y,SDL_Rect area,const ThemePalette &pal,bool priority=false) {
    const int offset=previewOffset(page,y);const auto key=previewKey(page,offset);
    int width=0,height=0;auto texture=pagePreviewCache_.get(key,&width,&height);
    if(texture && width>0 && height>0) {
        const float scale=std::min(float(area.w)/width,float(area.h)/height);
        SDL_Rect destination{area.x+(area.w-int(width*scale))/2,area.y+(area.h-int(height*scale))/2,std::max(1,int(width*scale)),std::max(1,int(height*scale))};
        SDL_RenderCopy(renderer_,texture,nullptr,&destination);
    } else {
        ui::surface(renderer_,area,pal.header,8);
        if(page>=0 && !failedPreviews_.count(key))pagePreviews_.request(page,offset,priority);
        const auto text=failedPreviews_.count(key)?catLabel("Önizleme yok","Preview unavailable"):catLabel("Yükleniyor…","Loading…");
        if(area.w>=110)typography_->renderText(renderer_,typography_->smallFont(),text,area.x+8,area.y+area.h/2-10,pal.muted,area.w-16);
    }
}
void cancelReaderScrub(bool clearTouch=false) {
    if(scrub_.active)pagePreviews_.clear();
    scrub_.active=false;scrub_.controller=false;
    if(clearTouch){scrubTouchOwned_=false;activeFingers_.clear();ui::pointer(0,0,false);}
    gestureTracker_.reset();dirty_=true;
}
void navigationJump(int page,float y=0) {
    if(!doc_ || page<0 || page>=pageCount_)return;
    rememberPosition();cancelPageTurn();clearHeldControls();pagePreviews_.clear();
    currentPage_=page;scrollY_=continuous()?std::max(0.f,y):0;scrollVelocity_=listVelocity_=panVelocityX_=panVelocityY_=0;
    if(comic() && !continuous())resetComicPagePan();else clampPdfPan();
    screen_=ScreenState::Reader;toolbarVisible_=false;readerToolbarFocusActive_=false;
    saveCurrentPosition();dirty_=true;
}
void setScrubFraction(float fraction) {
    scrub_.fraction=std::clamp(fraction,0.f,1.f);const int previous=scrub_.page;const int previousOffset=previewOffset(scrub_.page,scrub_.y);
    if(continuous()) {
        const float logical=scrub_.fraction*pageCount_;scrub_.page=std::clamp(int(logical),0,std::max(0,pageCount_-1));
        scrub_.within=std::clamp(logical-scrub_.page,0.f,1.f);
        // Unvisited comic dimensions may require decompressing a solid archive.
        // Keep that work off the drag path; resolve the intra-page offset once.
        if(comic())scrub_.y=0;
        else {int w,h;getWindowSize(w,h);const float extent=std::max(0.f,segmentHeight(scrub_.page)-(scrub_.page+1==pageCount_?h-144:0));scrub_.y=extent*scrub_.within;}
    } else {scrub_.page=std::clamp(int(std::lround(scrub_.fraction*std::max(0,pageCount_-1))),0,std::max(0,pageCount_-1));scrub_.y=0;}
    const int offset=previewOffset(scrub_.page,scrub_.y);
    if(previous!=scrub_.page || previousOffset!=offset) {
        const auto key=previewKey(scrub_.page,offset);
        if(!pagePreviewCache_.get(key) && !failedPreviews_.count(key))pagePreviews_.request(scrub_.page,offset,true);
    }
    dirty_=true;
}
void startReaderScrub(bool controller) {
    if(!doc_ || pageCount_<=0)return;
    cancelPageTurn();scrollVelocity_=panVelocityX_=panVelocityY_=0;gestureTracker_.reset();pdfDragging_=false;
    preparePagePreviews();scrub_={};scrub_.active=true;scrub_.controller=controller;
    scrub_.orientation=appSettings_.get().displayOrientation;scrub_.epoch=previewEpoch_;getWindowSize(scrub_.width,scrub_.height);
    setScrubFraction(float(readingFraction()));toolbarVisible_=true;
}
void moveReaderScrub(float x) {
    int w,h;getWindowSize(w,h);const auto track=readerTrack(w,h);
    float fraction=std::clamp((x*w-track.x)/std::max(1.f,float(track.w)),0.f,1.f);
    setScrubFraction(readingDirection()<0?1.f-fraction:fraction);
}
void finishReaderScrub() {
    const int page=scrub_.page;float y=scrub_.y;
    if(continuous() && comic()){int w,h;getWindowSize(w,h);y=std::max(0.f,segmentHeight(page)-(page+1==pageCount_?h-144:0))*scrub_.within;}
    cancelReaderScrub();navigationJump(page,y);
}
void seekDirection(int dx,int dy) {
    const int delta=dx?dx*readingDirection():dy*10;
    if(continuous())setScrubFraction(scrub_.fraction+delta/float(std::max(1,pageCount_)));
    else setScrubFraction(float(std::clamp(scrub_.page+delta,0,std::max(0,pageCount_-1)))/std::max(1,pageCount_-1));
}
void tickPageNavigation() {
    if(!scrub_.active)return;
    int w,h;getWindowSize(w,h);
    if(!doc_ || screen_!=ScreenState::Reader || quickMenu_ || appBackground_ || layoutPending_ || scrub_.epoch!=previewEpoch_ ||
       scrub_.orientation!=appSettings_.get().displayOrientation || w!=scrub_.width || h!=scrub_.height)cancelReaderScrub(true);
}
bool pageNavigationEvent(const SDL_Event &event) {
    if(event.type==SDL_APP_WILLENTERBACKGROUND || (event.type==SDL_WINDOWEVENT && (event.window.event==SDL_WINDOWEVENT_FOCUS_LOST || event.window.event==SDL_WINDOWEVENT_SIZE_CHANGED))) {
        cancelReaderScrub(true);return false;
    }
    if(scrubTouchOwned_) {
        if(event.type==SDL_FINGERDOWN){activeFingers_.insert(event.tfinger.fingerId);cancelReaderScrub();return true;}
        if(event.type==SDL_FINGERMOTION){if(scrub_.active && event.tfinger.fingerId==scrubFinger_)moveReaderScrub(event.tfinger.x);return true;}
        if(event.type==SDL_FINGERUP) {
            activeFingers_.erase(event.tfinger.fingerId);
            if(event.tfinger.fingerId==scrubFinger_ && scrub_.active && activeFingers_.empty()) {moveReaderScrub(event.tfinger.x);finishReaderScrub();}
            if(activeFingers_.empty()){scrubTouchOwned_=false;cancelReaderScrub();ui::pointer(0,0,false);}return true;
        }
    }
    if(event.type==SDL_KEYDOWN || event.type==SDL_CONTROLLERBUTTONDOWN) {
        const bool pad=event.type==SDL_CONTROLLERBUTTONDOWN;const int k=pad?event.cbutton.button:event.key.keysym.sym;
        auto match=[&](int key,int button){return k==(pad?button:key);};
        if(scrub_.active) {
            if(match(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B)){cancelReaderScrub();return true;}
            if(scrub_.controller) {
                if(match(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A)){finishReaderScrub();return true;}
                int dx=match(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT)?-1:match(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)?1:0;
                int dy=match(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP)?-1:match(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN)?1:0;
                if(dx || dy)seekDirection(dx,dy);
            }
            return true;
        }
        if(screen_==ScreenState::Reader && !quickMenu_ && match(SDLK_g,SDL_CONTROLLER_BUTTON_LEFTSTICK)){startReaderScrub(true);return true;}
        if(screen_==ScreenState::TocModal && !quickMenu_) {
            if(match(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B)){closeContents();return true;}
            if(match(SDLK_PAGEUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER)){changeContentsTab((tocTab_+2)%3);return true;}
            if(match(SDLK_PAGEDOWN,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)){changeContentsTab((tocTab_+1)%3);return true;}
        }
    }
    if(event.type==SDL_FINGERDOWN && doc_ && screen_==ScreenState::Reader && !quickMenu_ && (toolbarVisible_ || appSettings_.get().toolbar)) {
        int w,h;getWindowSize(w,h);
        if(ui::contains(readerSeekHit(w,h),int(event.tfinger.x*w),int(event.tfinger.y*h))) {
            startReaderScrub(false);scrubTouchOwned_=true;scrubFinger_=event.tfinger.fingerId;activeFingers_.insert(scrubFinger_);
            ui::pointer(int(event.tfinger.x*w),int(event.tfinger.y*h),true);moveReaderScrub(event.tfinger.x);return true;
        }
    }
    return false;
}
void revealContentsItem() {
    int w,h;getWindowSize(w,h);const auto content=tocContent(w,h);const int top=tocSelected_/tocColumns(w)*tocStep();
    if(top<tocScroll_)tocScroll_=float(top);
    if(top+tocStep()>tocScroll_+content.h)tocScroll_=float(top+tocStep()-content.h);
    tocScroll_=std::clamp(tocScroll_,0.f,tocMaxScroll(w,h));
}
void changeContentsTab(int tab) {
    pagePreviews_.discardQueue();previewGridRow_=-1;
    tocTabScroll_[tocTab_]=tocScroll_;tocTab_=std::clamp(tab,0,2);tocScroll_=tocTabScroll_[tocTab_];tocBookmarks_=progressStore_.get(currentBookId_).bookmarks;
    tocBookmarkPositions_.clear();tocBookmarkPages_.clear();
    for(const auto &bookmark:tocBookmarks_) {
        int page=resolveBookmark(bookmark);float y=0;px_document_lookup_position_bookmark(doc_,bookmark.muBookmark,&page,&y);
        page=std::clamp(page,0,std::max(0,pageCount_-1));tocBookmarkPositions_.push_back({page,screenScrollY(page,y)});tocBookmarkPages_.insert(page);
    }
    tocSelected_=tocTab_==1?currentPage_:0;
    if(tocTab_==0)tocSelected_=std::max(0,currentContentsChapter());
    if(tocTab_==2)for(int i=0;i<int(tocBookmarkPositions_.size());++i)if(tocBookmarkPositions_[i].first==currentPage_ && (!continuous() || std::abs(tocBookmarkPositions_[i].second-scrollY_)<32))tocSelected_=i;
    revealContentsItem();listTracking_=false;listVelocity_=0;gestureTracker_.reset();preparePagePreviews();
    rebuildFocus();navigationFocus_.select("toc."+std::to_string(tocSelected_));dirty_=true;
}
void openContents(int tab=-1) {
    if(!doc_)return;
    cancelReaderScrub(true);cancelPageTurn();tocReturnToolbar_=toolbarVisible_;screen_=ScreenState::TocModal;
    scrollVelocity_=panVelocityX_=panVelocityY_=0;changeContentsTab(tab<0?(comic()?1:0):tab);
}
void closeContents() {pagePreviews_.clear();screen_=ScreenState::Reader;toolbarVisible_=tocReturnToolbar_;readerToolbarFocusActive_=false;listTracking_=false;listVelocity_=0;gestureTracker_.reset();dirty_=true;}
void activateContentsItem(int index) {
    auto item=tocItem(index);if(item.page<0 || item.page>=pageCount_)return;
    tocSelected_=index;
    if(tocTab_==2){pagePreviews_.clear();openBookmark(tocBookmarks_[index]);}else navigationJump(item.page,item.y);
    listTracking_=false;listVelocity_=0;
}
void contentsTap(int x,int y,int w,int h) {
    if(!ui::contains(navigationPanel(w,h),x,y) || ui::contains({w-184,88,112,56},x,y)){closeContents();return;}
    for(int tab=0;tab<3;++tab)if(ui::contains(tocTabRect(tab,w),x,y)){changeContentsTab(tab);return;}
    if(!ui::contains(tocContent(w,h),x,y))return;
    const int first=std::max(0,int(tocScroll_)/tocStep()*tocColumns(w));
    const int last=std::min(tocCount(),first+(tocContent(w,h).h/tocStep()+2)*tocColumns(w));
    for(int i=first;i<last;++i)if(ui::contains(tocCell(i,w,h),x,y)){activateContentsItem(i);return;}
}
void renderContents(int w,int h,const ThemePalette &pal) {
    preparePagePreviews();ui::surface(renderer_,navigationPanel(w,h),pal.card,18);
    typography_->renderText(renderer_,typography_->titleFont(),ui::label("İçindekiler"),56,96,pal.fg,w-280);
    ui::button(renderer_,*typography_,{w-184,88,112,56},"Kapat",pal);
    const std::string tabs[]{catLabel("Bölümler","Chapters"),catLabel("Tüm sayfalar","All pages"),ui::label("Yer imleri")};
    for(int tab=0;tab<3;++tab)ui::button(renderer_,*typography_,tocTabRect(tab,w),tabs[tab],pal,tocTab_==tab,false,false);
    const auto content=tocContent(w,h);SDL_RenderSetClipRect(renderer_,&content);
    if(!tocCount())typography_->renderWrapped(renderer_,typography_->uiFont(),tocTab_==2?catLabel("Henüz yer imi yok.","No bookmarks yet."):catLabel("Bu kitapta bölüm listesi yok. Tüm sayfalar sekmesini kullanın.","No chapter list in this book. Use All pages to browse."),{content.x+12,content.y+16,content.w-24,100},pal.muted);
    const int first=std::max(0,int(tocScroll_)/tocStep()*tocColumns(w));
    const int last=std::min(tocCount(),first+(content.h/tocStep()+2)*tocColumns(w));
    if(first!=previewGridRow_){pagePreviews_.discardQueue();previewGridRow_=first;}
    for(int i=first;i<last;++i) {
        const auto item=tocItem(i);auto cell=tocCell(i,w,h);
        if(cell.y>=content.y+content.h || cell.y+cell.h<=content.y)continue;
        const bool valid=item.page>=0 && item.page<pageCount_;
        const bool selected=tocTab_==0?i==currentContentsChapter():item.page==currentPage_ && (tocTab_!=2 || !continuous() || std::abs(item.y-scrollY_)<32);
        ui::surface(renderer_,cell,pal.header,10);
        if(tocTab_==0) {
            const int indent=std::clamp(item.level,0,3)*12;
            SDL_Rect image{cell.x+8,cell.y+6,68,cell.h-12};
            if(valid)drawPagePreview(item.page,item.y,image,pal);
            SDL_Rect text{cell.x+88+indent,cell.y+8,cell.w-100-indent,64};
            typography_->renderWrapped(renderer_,typography_->uiFont(),item.title,text,valid?pal.fg:pal.muted);
            if(valid)typography_->renderText(renderer_,typography_->smallFont(),navigationPageLabel(item.page),text.x,cell.y+cell.h-26,pal.muted,text.w);
        } else {
            drawPagePreview(item.page,item.y,{cell.x+10,cell.y+8,cell.w-20,cell.h-44},pal);
            const auto label=tocTab_==2 && !item.title.empty()?item.title:std::to_string(item.page+1);int tw=0;typography_->measureText(typography_->smallFont(),label,&tw,nullptr,cell.w-20);
            typography_->renderText(renderer_,typography_->smallFont(),label,tocTab_==2?cell.x+10:cell.x+(cell.w-tw)/2,cell.y+cell.h-30,selected?pal.accent:pal.fg,tocTab_==2?cell.w-20:0);
            if(tocBookmarkPages_.count(item.page))ui::surface(renderer_,{cell.x+cell.w-24,cell.y+12,10,10},pal.accent,5);
        }
        if(selected)ui::focusRing(renderer_,cell,pal);
    }
    SDL_RenderSetClipRect(renderer_,nullptr);
}
void renderReaderNavigation(int w,int h,const ThemePalette &pal) {
    ui::surface(renderer_,{0,h-72,w,72},pal.header,0);
    ui::button(renderer_,*typography_,readerPrev(w,h),"<",pal);
    ui::button(renderer_,*typography_,readerNext(w,h),">",pal);
    const auto pages=readerPages(w,h);ui::button(renderer_,*typography_,pages,"",pal);
    SDL_SetRenderDrawColor(renderer_,pal.fg.r,pal.fg.g,pal.fg.b,255);
    for(int row=0;row<2;++row)for(int col=0;col<2;++col){SDL_Rect box{pages.x+19+col*15,pages.y+15+row*15,10,10};SDL_RenderDrawRect(renderer_,&box);}
    const auto track=readerTrack(w,h);const float fraction=scrub_.active?scrub_.fraction:float(readingFraction());
    typography_->renderText(renderer_,typography_->smallFont(),navigationPageLabel(scrub_.active?scrub_.page:currentPage_),track.x,h-65,pal.fg,std::max(1,track.w-72));
    const auto percent=std::to_string(int(fraction*100))+"%";
    typography_->renderText(renderer_,typography_->smallFont(),percent,track.x+track.w-64,h-65,pal.muted,64);
    ui::surface(renderer_,track,pal.cardBorder,4);const int fill=int(track.w*std::clamp(fraction,0.f,1.f));
    SDL_Rect completed{readingDirection()<0?track.x+track.w-fill:track.x,track.y,fill,track.h};ui::surface(renderer_,completed,pal.accent,4);
    const float x=track.x+track.w*(readingDirection()<0?1.f-fraction:fraction);
    ui::surface(renderer_,{int(x)-11,track.y-7,22,22},scrub_.active?pal.fg:pal.accent,11);
    if(scrub_.active) {
        preparePagePreviews();const int width=184,height=std::min(264,h-164);
        SDL_Rect bubble{std::clamp(int(x)-width/2,16,std::max(16,w-width-16)),h-84-height,width,height};
        ui::surface(renderer_,{bubble.x+3,bubble.y+5,bubble.w,bubble.h},{0,0,0,100},14);ui::surface(renderer_,bubble,pal.card,14);
        drawPagePreview(scrub_.page,scrub_.y,{bubble.x+12,bubble.y+12,bubble.w-24,bubble.h-56},pal);
        const auto label=std::to_string(scrub_.page+1)+" / "+std::to_string(pageCount_);int tw=0;typography_->measureText(typography_->smallFont(),label,&tw,nullptr);
        typography_->renderText(renderer_,typography_->smallFont(),label,bubble.x+(bubble.w-tw)/2,bubble.y+bubble.h-33,pal.fg);
    }
}
