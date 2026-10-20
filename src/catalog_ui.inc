// Included inside PaperNxApp: catalog overlays use the shared native UI components.
static std::string catLabel(const std::string &tr,const std::string &en){return ui::language==1?en:tr;}
enum class CatalogPanel { None, Actions, Details, Metadata, Collections, CollectionActions,
    Browse, Values, Hidden, Statistics, DeleteConfirm, Text };
CatalogStore catalog_{root_/"data"/"catalog.txt"};
bool catalogSavePending_{false};
Uint32 catalogSaveRetryAt_{0};
ReadingStatsStore readingStats_{root_/"data"/"reading-stats.txt"};
CatalogPanel catalogPanel_{CatalogPanel::None}, textReturn_{CatalogPanel::None};
Book actionBook_;
std::set<SDL_FingerID> catalogFingers_;
bool catalogTouchOwned_{false},catalogMultiTouch_{false};
SDL_FingerID catalogTouchOwner_{0};
float catalogTouchStartX_{0},catalogTouchStartY_{0};
int catalogFocus_{0}, textTarget_{0};
float catalogScroll_{0};
float catalogTouchY_{0};bool catalogTouchMoved_{false};
CatalogGroup catalogGroup_{CatalogGroup::Authors};
std::optional<CatalogGroup> catalogFilter_;
std::string catalogFilterValue_, selectedCollection_, inputText_;
bool libraryHold_{false}, libraryHoldConsumed_{false}, appBackground_{false};
Uint32 libraryHoldStart_{0}, statsSaveAt_{0};
float libraryHoldX_{0},libraryHoldY_{0};
int libraryHoldIndex_{-1};
std::optional<HistoryPosition> pageFlipPosition_;
float pageFlipZoom_{1};int pageFlipPanX_{0},pageFlipPanY_{0};
void pinPageFlip(){
    if(!doc_)return;
    HistoryPosition position{currentPage_,scrollY_,-1,-1,0};
    px_document_anchor(doc_,currentPage_,scrollY_,&position.chapter,&position.character,&position.offset);
    pageFlipPosition_=position;pageFlipZoom_=pdfZoom_;pageFlipPanX_=pdfPanX_;pageFlipPanY_=pdfPanY_;
    quickMenu_=false;dirty_=true;
}
void returnToPageFlip(){
    if(!pageFlipPosition_||!doc_)return;
    const auto p=*pageFlipPosition_;readingStats_.reanchor();
    currentPage_=std::clamp(p.page,0,std::max(0,pageCount_-1));scrollY_=p.y;
    if(px_document_is_epub(doc_))px_document_restore_anchor(doc_,p.chapter,p.character,p.offset,&currentPage_,&scrollY_);
    else {pdfZoom_=pageFlipZoom_;pdfPanX_=pageFlipPanX_;pdfPanY_=pageFlipPanY_;clampPdfPan();}
    pageFlipPosition_.reset();quickMenu_=false;scrollVelocity_=0;saveCurrentPosition();dirty_=true;
}

std::string localReadingDay() const {
    const auto now=std::time(nullptr);char day[16]{};const auto *date=std::localtime(&now);
    if(date)std::strftime(day,sizeof day,"%Y-%m-%d",date);
    return day;
}
void tickReadingStats() {
    const auto now=SDL_GetTicks();
    double fraction=pageCount_>0?(currentPage_+(continuous()?std::clamp(double(scrollY_)/std::max(1.f,segmentHeight(currentPage_)),0.0,1.0):0.0))/pageCount_:0;
    const bool visible=doc_&&screen_==ScreenState::Reader&&!quickMenu_&&
        catalogPanel_==CatalogPanel::None&&!appBackground_&&!layoutPending_;
    readingStats_.tick(now,currentBookId_,fraction,visible,localReadingDay());
    journal_.tick(now,std::time(nullptr),localReadingDay(),currentBookId_,journalTitle_,journalAuthor_,fraction,visible);
    if(now-statsSaveAt_>=60000){
        readingStats_.save();
        if(journal_.hasUnsavedChanges()&&!journal_.save())
            currentErrorMessage_="Reading journal not saved: "+journal_.diagnostic();
        statsSaveAt_=now;
    }
}
int libraryBookAt(float x,float y) {
    int w,h;getWindowSize(w,h);ui::LibraryLayout l(w);const int px=int(x*w),py=int(y*h);
    if(py<l.top||py>=h-28)return -1;
    const int pos=py-l.top+int(listScroll_);int index=-1;
    if(viewMode_==ViewMode::Grid){const int col=(px-l.left)/l.stepX;
        if(px<l.left||col>=l.columns||(px-l.left)%l.stepX>=l.cardWidth||pos%l.stepY>=l.cardHeight)return -1;
        index=(pos/l.stepY)*l.columns+col;
    }else index=pos/112;
    return index>=0&&index<int(visibleBooks_.size())?index:-1;
}
void setCatalogPanel(CatalogPanel p){catalogPanel_=p;catalogFocus_=0;catalogScroll_=0;listVelocity_=0;dirty_=true;if(p==CatalogPanel::DeleteConfirm){rebuildFocus();navigationFocus_.select("catalog.row.0");}}
void openBookActions(const Book &book){actionBook_=book;quickMenu_=false;setCatalogPanel(CatalogPanel::Actions);}
void tickLibraryHold(){
    if(libraryHold_&&SDL_GetTicks()-libraryHoldStart_>=550){libraryHold_=false;libraryHoldConsumed_=true;
        listTracking_=false;listVelocity_=0;gestureTracker_.reset();
        if(libraryHoldIndex_>=0&&libraryHoldIndex_<int(visibleBooks_.size())){selectedBookIdx_=libraryHoldIndex_;openBookActions(visibleBooks_[libraryHoldIndex_]);}}
}
bool saveCatalogChange(const std::function<void()> &change){
    auto previous=catalog_;change();if(!catalog_.save()){currentErrorMessage_="Catalog could not be saved: "+catalog_.diagnostic();catalog_=std::move(previous);dirty_=true;return false;}
    catalogSavePending_=false;
    updateVisibleBooks();dirty_=true;return true;
}
std::vector<std::pair<int,std::string>> catalogRows() const {
    std::vector<std::pair<int,std::string>> rows;
    const auto e=catalog_.get(actionBook_.id);
    if(catalogPanel_==CatalogPanel::Actions){
        const auto p=progressStore_.get(actionBook_.id);
        rows={{0,p.lastRead?catLabel("Okumaya devam","Continue Reading"):catLabel("Kitabı aç","Open Book")},
            {1,p.favorite?catLabel("Favoriden çıkar","Remove Favorite"):catLabel("Favori","Favorite")},
            {2,catLabel("Okunuyor","Currently Reading")},
            {3,catLabel("Okunmadı","Unread")},
            {4,catLabel("Okundu / Tamamlandı","Read / Finished")},
            {5,catLabel("Koleksiyonlar","Collections")},
            {6,catLabel("Bilgileri düzenle","Edit Metadata")},{7,catLabel("Kitap bilgisi","Book Details")},{10,catLabel("Okuma geçmişi","Reading history")},
            {8,catLabel("Kitaplıktan gizle","Remove from Library")},{9,catLabel("Dosyayı sil","Delete File Permanently")}};
    }else if(catalogPanel_==CatalogPanel::Metadata){
        rows={{0,catLabel("Başlık: ","Title: ")+(e.title.empty()?actionBook_.title:e.title)},
            {1,catLabel("Yazar: ","Author: ")+(e.author.empty()?actionBook_.author:e.author)},
            {2,catLabel("Tür: ","Genre: ")+e.genre},{3,catLabel("Seri: ","Series: ")+(e.series.empty()?e.sourceSeries:e.series)},{4,catLabel("Etiketler: ","Tags: ")+e.tags}};
    }else if(catalogPanel_==CatalogPanel::Collections){
        rows.push_back({-1,catLabel("Koleksiyon oluştur","Create Collection")});
        for(const auto &c:catalog_.collections()){const bool member=std::find(e.collections.begin(),e.collections.end(),c.first)!=e.collections.end();
            rows.push_back({int(rows.size()),std::string(member?"[x] ":"[ ] ")+c.second});}
        rows.push_back({-2,catLabel("Koleksiyonları yönet","Manage Collections")});
    }else if(catalogPanel_==CatalogPanel::CollectionActions){rows={{0,catLabel("Yeniden adlandır","Rename Collection")},{1,catLabel("Koleksiyonu kaldır","Delete Collection")}};
    }else if(catalogPanel_==CatalogPanel::Browse){rows={{0,catLabel("Tüm kitaplar","All Books")},{1,catLabel("Yazarlar","Authors")},{2,catLabel("Türler","Genres")},{3,catLabel("Koleksiyonlar","Collections")},{4,catLabel("Seriler","Series")},{5,catLabel("Gizlenen kitaplar","Hidden Books")},{6,catLabel("Okuma istatistikleri","Reading Statistics")},{7,catLabel("Koleksiyon oluştur","Create Collection")},{8,catLabel("Dosya t\u00fcr\u00fc","File Format")}};
    }else if(catalogPanel_==CatalogPanel::Values){
        for(const auto &g:catalog_.groups(books_,catalogGroup_)){auto name=g.first;
            if(catalogGroup_==CatalogGroup::Collections){auto found=catalog_.collections().find(g.first);if(found!=catalog_.collections().end())name=found->second;}
            rows.push_back({int(rows.size()),name+"  ·  "+std::to_string(g.second)});}
    }else if(catalogPanel_==CatalogPanel::Hidden){for(const auto &b:books_)if(catalog_.get(b.id).hidden)rows.push_back({int(rows.size()),catLabel("Geri getir: ","Restore: ")+catalog_.apply(b).title});
    }else if(catalogPanel_==CatalogPanel::Statistics){rows={{0,catLabel("Günlük hedef: ","Daily Goal: ")+std::to_string(readingStats_.dailyGoal())+" min"},{1,catLabel("Hedefi azalt -5","Decrease Goal")},{2,catLabel("Hedefi artır +5","Increase Goal")},{3,catLabel("Hedefi kapat","Disable Goal")}};
    }else if(catalogPanel_==CatalogPanel::DeleteConfirm){rows={{0,catLabel("Vazgeç","Cancel")},{1,catLabel("Kalıcı olarak sil","Delete this file permanently")}};
    }else if(catalogPanel_==CatalogPanel::Details){rows={{0,catLabel("Bilgileri düzenle","Edit Metadata")},{1,catLabel("Koleksiyonlar","Collections")},{2,catLabel("Okuma geçmişi","Reading history")}};}
    return rows;
}
std::string catalogHeading() const {
    switch(catalogPanel_){case CatalogPanel::Actions:return catLabel("Kitap işlemleri","Book Actions");
    case CatalogPanel::Details:return catLabel("Kitap bilgisi","Book Details");case CatalogPanel::Metadata:return catLabel("Kitap bilgileri","Metadata");
    case CatalogPanel::Collections:return catLabel("Koleksiyonlar","Collections");case CatalogPanel::CollectionActions:return catLabel("Koleksiyon yönet","Manage Collection");
    case CatalogPanel::Browse:return catLabel("Kitaplık gezgini","Browse Library");case CatalogPanel::Values:return catLabel("Grup seçin","Select a group");
    case CatalogPanel::Hidden:return catLabel("Gizlenen kitaplar","Restore Hidden Books");case CatalogPanel::Statistics:return catLabel("Okuma istatistikleri","Reading Statistics");
    case CatalogPanel::DeleteConfirm:return catLabel("Kalıcı silinsin mi?","Delete Permanently?");case CatalogPanel::Text:return catLabel("Düzenle","Edit");default:return {};}
}
SDL_Rect catalogPanelRect(int w,int h)const{return {std::max(16,(w-900)/2),32,std::min(900,w-32),h-64};}
int catalogContentTop(const SDL_Rect &r)const{
    if(catalogPanel_==CatalogPanel::Details)return r.y+std::min(420,r.h-160);
    if(catalogPanel_==CatalogPanel::DeleteConfirm)return r.y+220;
    if(catalogPanel_==CatalogPanel::Statistics)return r.y+180;
    return r.y+88;
}
void closeCatalogPanel(){
    if(catalogPanel_==CatalogPanel::Text){SDL_StopTextInput();setCatalogPanel(textReturn_);return;}
    if(catalogPanel_==CatalogPanel::Metadata||catalogPanel_==CatalogPanel::Details||catalogPanel_==CatalogPanel::Collections||catalogPanel_==CatalogPanel::DeleteConfirm)setCatalogPanel(CatalogPanel::Actions);
    else if(catalogPanel_==CatalogPanel::Values||catalogPanel_==CatalogPanel::Hidden||catalogPanel_==CatalogPanel::Statistics||catalogPanel_==CatalogPanel::CollectionActions)setCatalogPanel(CatalogPanel::Browse);
    else setCatalogPanel(CatalogPanel::None);
}
void commitCatalogText(){
    const auto value=utf8SafeTruncate(inputText_,textTarget_>=5?128:1024);
    if(textTarget_<5)saveCatalogChange([&]{auto e=catalog_.get(actionBook_.id);if(textTarget_==0)e.title=value;if(textTarget_==1)e.author=value;if(textTarget_==2)e.genre=value;if(textTarget_==3)e.series=value;if(textTarget_==4)e.tags=value;catalog_.put(actionBook_.id,e);});
    else if(textTarget_==5){if(!value.empty())saveCatalogChange([&]{catalog_.createCollection(value);});}
    else if(textTarget_==6){if(!value.empty())saveCatalogChange([&]{catalog_.renameCollection(selectedCollection_,value);});}
    SDL_StopTextInput();setCatalogPanel(textReturn_);
}
void requestCatalogText(int target,const std::string &initial){
    textTarget_=target;inputText_=initial;textReturn_=catalogPanel_;
#ifdef __SWITCH__
    SwkbdConfig config;
    if(R_SUCCEEDED(swkbdCreate(&config,0))){swkbdConfigMakePresetDefault(&config);swkbdConfigSetGuideText(&config,catLabel("Kitap bilgileri / koleksiyon","Book metadata / collection").c_str());swkbdConfigSetInitialText(&config,initial.c_str());swkbdConfigSetStringLenMax(&config,target>=5?128:512);
        char text[2048]{};const Result result=swkbdShow(&config,text,sizeof text);swkbdClose(&config);if(R_SUCCEEDED(result)){inputText_=text;commitCatalogText();}}
#else
    SDL_StartTextInput();setCatalogPanel(CatalogPanel::Text);
#endif
}
void activateCatalogRow(int index){
    const auto rows=catalogRows();if(index<0||index>=int(rows.size()))return;
    const int action=rows[index].first;
    if(catalogPanel_==CatalogPanel::Actions){
        if(action==0){setCatalogPanel(CatalogPanel::None);openBook(actionBook_);return;}
        if(action==1){auto p=progressStore_.get(actionBook_.id);p.favorite=!p.favorite;progressStore_.put(actionBook_.id,p);progressDirty_=true;updateVisibleBooks();}
        if(action>=2&&action<=4){
            const auto newStatus=action==2?ReadingStatus::Reading:action==3?ReadingStatus::NotStarted:ReadingStatus::Completed;
            // Status and progress are one user intent. Catalog label alone is not
            // sufficient: unread=0%, completed=100%, reading=real position.
            // Do not erase bookmarks, per-book appearance, or journal sessions.
            const auto oldProgress=progressStore_.get(actionBook_.id);
            auto updated=oldProgress;
            applyManualReadingStatus(updated,newStatus,std::time(nullptr));
            // Save position FIRST, then catalog label. On either failure, restore
            // the previous in-memory state and attempt durable compensation.
            // A torn cross-file transaction is detected by regression/fault tests.
            progressStore_.put(actionBook_.id,updated);
            progressDirty_=true;
            if(!progressStore_.save()) {
                progressStore_.put(actionBook_.id,oldProgress);
                currentErrorMessage_="Reading status unchanged: progress write failed.";
            } else if(!saveCatalogChange([&]{
                    auto e=catalog_.get(actionBook_.id);
                    e.status=newStatus;catalog_.put(actionBook_.id,e);
               })) {
                progressStore_.put(actionBook_.id,oldProgress);
                progressDirty_=!progressStore_.save();
                if(progressDirty_)
                    currentErrorMessage_="Status rollback failed. Check SD card before changing books.";
            } else {
                progressDirty_=false;
                catalogFocus_=action;
                updateVisibleBooks();
            }
        }
        if(action==5)setCatalogPanel(CatalogPanel::Collections);
        if(action==6)setCatalogPanel(CatalogPanel::Metadata);
        if(action==7)setCatalogPanel(CatalogPanel::Details);
        if(action==10){setCatalogPanel(CatalogPanel::None);enterJournal(false,actionBook_.id);}
        if(action==8){saveCatalogChange([&]{auto e=catalog_.get(actionBook_.id);e.hidden=true;catalog_.put(actionBook_.id,e);});setCatalogPanel(CatalogPanel::None);}
        if(action==9)setCatalogPanel(CatalogPanel::DeleteConfirm);
    }else if(catalogPanel_==CatalogPanel::Metadata){auto e=catalog_.get(actionBook_.id);const std::string values[]={e.title.empty()?actionBook_.title:e.title,e.author.empty()?actionBook_.author:e.author,e.genre.empty()?e.sourceGenre:e.genre,e.series.empty()?e.sourceSeries:e.series,e.tags};requestCatalogText(action,values[action]);
    }else if(catalogPanel_==CatalogPanel::Details){
        if(action==2){setCatalogPanel(CatalogPanel::None);enterJournal(false,actionBook_.id);}
        else setCatalogPanel(action==0?CatalogPanel::Metadata:CatalogPanel::Collections);
    }else if(catalogPanel_==CatalogPanel::Collections){
        if(action==-1)requestCatalogText(5,"");else if(action==-2){catalogGroup_=CatalogGroup::Collections;setCatalogPanel(CatalogPanel::Values);}
        else {auto it=catalog_.collections().begin();std::advance(it,index-1);if(it!=catalog_.collections().end()){auto id=it->first;saveCatalogChange([&]{auto e=catalog_.get(actionBook_.id);if(std::find(e.collections.begin(),e.collections.end(),id)==e.collections.end())catalog_.addToCollection(actionBook_.id,id);else catalog_.removeFromCollection(actionBook_.id,id);});}}
    }else if(catalogPanel_==CatalogPanel::Browse){
        if(action==0){catalogFilter_.reset();updateVisibleBooks();setCatalogPanel(CatalogPanel::None);}
        if(action>=1&&action<=4){catalogGroup_=action==1?CatalogGroup::Authors:action==2?CatalogGroup::Genres:action==3?CatalogGroup::Collections:CatalogGroup::Series;setCatalogPanel(CatalogPanel::Values);}
        if(action==5)setCatalogPanel(CatalogPanel::Hidden);
        if(action==6)setCatalogPanel(CatalogPanel::Statistics);
        if(action==7)requestCatalogText(5,"");
        if(action==8){catalogGroup_=CatalogGroup::Formats;setCatalogPanel(CatalogPanel::Values);}
    }else if(catalogPanel_==CatalogPanel::Values){auto groups=catalog_.groups(books_,catalogGroup_);auto it=groups.begin();std::advance(it,index);if(it!=groups.end()){catalogFilter_=catalogGroup_;catalogFilterValue_=it->first;selectedCollection_=it->first;category_=Category::All;updateVisibleBooks();setCatalogPanel(CatalogPanel::None);}
    }else if(catalogPanel_==CatalogPanel::Hidden){std::vector<Book> hidden;for(const auto &b:books_)if(catalog_.get(b.id).hidden)hidden.push_back(b);if(index<int(hidden.size()))saveCatalogChange([&]{auto e=catalog_.get(hidden[index].id);e.hidden=false;catalog_.put(hidden[index].id,e);});
    }else if(catalogPanel_==CatalogPanel::CollectionActions){if(action==0){auto it=catalog_.collections().find(selectedCollection_);if(it!=catalog_.collections().end())requestCatalogText(6,it->second);}else {saveCatalogChange([&]{catalog_.deleteCollection(selectedCollection_);});catalogFilter_.reset();setCatalogPanel(CatalogPanel::Browse);}
    }else if(catalogPanel_==CatalogPanel::Statistics){int goal=readingStats_.dailyGoal();if(action==0||action==2)goal+=5;if(action==1)goal-=5;if(action==3)goal=0;readingStats_.setDailyGoal(std::clamp(goal,0,720));if(!readingStats_.save())currentErrorMessage_="Reading goal could not be saved";
    }else if(catalogPanel_==CatalogPanel::DeleteConfirm){if(action==0){setCatalogPanel(CatalogPanel::Actions);return;}
        std::string error;
        fs::path allowed=root_/"books";
        if(!catalog_.validateDeletion(actionBook_,allowed,error)&&!browserRoot_.empty())allowed=browserRoot_;
        if(catalog_.deleteBookFile(actionBook_,allowed,true,error)){books_.erase(std::remove_if(books_.begin(),books_.end(),[&](const Book &b){return b.id==actionBook_.id;}),books_.end());updateVisibleBooks();setCatalogPanel(CatalogPanel::None);}
        else currentErrorMessage_=error;
    }
    dirty_=true;
}
void moveCatalogFocus(int delta){
    const auto rows=catalogRows();catalogFocus_=std::clamp(catalogFocus_+delta,0,std::max(0,int(rows.size())-1));
    int w,h;getWindowSize(w,h);const auto r=catalogPanelRect(w,h);const int top=catalogContentTop(r);const int height=r.y+r.h-24-top;
    const float a=catalogFocus_*64.f,b=a+56;if(a<catalogScroll_)catalogScroll_=a;if(b>catalogScroll_+height)catalogScroll_=std::max(0.f,b-height);dirty_=true;
}
void handleCatalogTap(int px,int py,int w,int h){
    rebuildFocus();navigationFocus_.touch(px,py);
    const auto r=catalogPanelRect(w,h);
    if(ui::contains({r.x+r.w-124,r.y+12,108,56},px,py)){closeCatalogPanel();return;}
    if(!ui::contains(r,px,py)){if(catalogPanel_!=CatalogPanel::DeleteConfirm)closeCatalogPanel();return;}
    if(catalogPanel_==CatalogPanel::Text){
        if(ui::contains({r.x+20,r.y+r.h-76,r.w/2-24,56},px,py)){commitCatalogText();return;}
        if(ui::contains({r.x+r.w/2+4,r.y+r.h-76,r.w/2-24,56},px,py)){if(!inputText_.empty()){auto n=inputText_.size()-1;while(n>0&&(static_cast<unsigned char>(inputText_[n])&0xc0)==0x80)--n;inputText_.resize(n);}dirty_=true;return;}
        const auto keys=catalogKeyboard();const int width=(r.w-40)/10;const int keyboardY=r.y+156;
        const int col=(px-r.x-20)/width,row=(py-keyboardY)/48,index=row*10+col;
        if(px>=r.x+20&&py>=keyboardY&&col>=0&&col<10&&row>=0&&index<int(keys.size())&&inputText_.size()+keys[index].size()<=1024){inputText_+=keys[index];dirty_=true;}return;
    }
    const int top=catalogContentTop(r);if(py<top||py>=r.y+r.h-16)return;
    const int index=int(py-top+catalogScroll_)/64;catalogFocus_=index;
    if(catalogPanel_==CatalogPanel::Values&&catalogGroup_==CatalogGroup::Collections&&px>=r.x+r.w-148){auto g=catalog_.groups(books_,catalogGroup_);auto it=g.begin();std::advance(it,std::min(index,int(g.size())));if(it!=g.end()){selectedCollection_=it->first;setCatalogPanel(CatalogPanel::CollectionActions);}return;}
    activateCatalogRow(index);
}
std::vector<std::string> catalogKeyboard()const{return {"a","b","c","ç","d","e","f","g","ğ","h","ı","i","j","k","l","m","n","o","ö","p","r","s","ş","t","u","ü","v","y","z"," ","0","1","2","3","4","5","6","7","8","9","A","B","C","D","E","F","G","I","İ","."};}
bool handleCatalogEvent(const SDL_Event &e){
    if(catalogPanel_==CatalogPanel::None)return false;
    if(e.type==SDL_TEXTINPUT&&catalogPanel_==CatalogPanel::Text){if(inputText_.size()+std::strlen(e.text.text)<=1024)inputText_+=e.text.text;dirty_=true;return true;}
    if(e.type==SDL_KEYDOWN||e.type==SDL_CONTROLLERBUTTONDOWN){const bool pad=e.type==SDL_CONTROLLERBUTTONDOWN;const int key=pad?e.cbutton.button:e.key.keysym.sym;
        auto m=[&](int k,int b){return pad?key==b:key==k;};
        if(m(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B)){closeCatalogPanel();return true;}
        if(m(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP)||m(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN)||m(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT)||m(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)){moveFocus(m(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT)?-1:m(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)?1:0,m(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP)?-1:m(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN)?1:0);return true;}
        if(m(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A)){activateFocus();return true;}
        if(catalogPanel_==CatalogPanel::Text){if(m(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A))commitCatalogText();else if(!pad&&key==SDLK_BACKSPACE&&!inputText_.empty()){auto pos=inputText_.size()-1;while(pos>0&&(static_cast<unsigned char>(inputText_[pos])&0xc0)==0x80)--pos;inputText_.resize(pos);}dirty_=true;return true;}
        if(m(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP))moveCatalogFocus(-1);else if(m(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN))moveCatalogFocus(1);
        else if(m(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A))activateCatalogRow(catalogFocus_);
        else if(m(SDLK_x,SDL_CONTROLLER_BUTTON_X)&&catalogPanel_==CatalogPanel::Values&&catalogGroup_==CatalogGroup::Collections){auto g=catalog_.groups(books_,catalogGroup_);auto it=g.begin();std::advance(it,std::min(catalogFocus_,int(g.size())));if(it!=g.end()){selectedCollection_=it->first;setCatalogPanel(CatalogPanel::CollectionActions);}}
        return true;
    }
    if(e.type==SDL_CONTROLLERAXISMOTION||e.type==SDL_MULTIGESTURE)return true;
    if(e.type==SDL_FINGERDOWN){int w,h;getWindowSize(w,h);
        catalogFingers_.insert(e.tfinger.fingerId);
        if(catalogFingers_.size()>1){catalogMultiTouch_=true;catalogTouchOwned_=false;catalogTouchMoved_=true;ui::pointer(0,0,false);dirty_=true;return true;}
        catalogTouchOwned_=true;catalogTouchOwner_=e.tfinger.fingerId;catalogTouchStartX_=e.tfinger.x*w;catalogTouchStartY_=e.tfinger.y*h;
        catalogTouchY_=catalogTouchStartY_;catalogTouchMoved_=false;ui::pointer(int(catalogTouchStartX_),int(catalogTouchStartY_),true);dirty_=true;return true;}
    if(e.type==SDL_FINGERUP){int w,h;getWindowSize(w,h);ui::pointer(0,0,false);
        const bool owns=catalogTouchOwned_ && e.tfinger.fingerId==catalogTouchOwner_;catalogFingers_.erase(e.tfinger.fingerId);
        if(libraryHoldConsumed_){libraryHoldConsumed_=false;activeFingers_.clear();catalogTouchOwned_=false;return true;}
        const bool activate=owns && !catalogMultiTouch_ && !catalogTouchMoved_;
        if(owns)catalogTouchOwned_=false;
        if(catalogFingers_.empty())catalogMultiTouch_=false;
        if(activate){const auto panel=catalogPanelRect(w,h);const int top=catalogContentTop(panel);int px=int(e.tfinger.x*w),py=int(e.tfinger.y*h);
            const int downRow=int(catalogTouchStartY_-top+catalogScroll_)/64,upRow=int(py-top+catalogScroll_)/64;
            if((catalogTouchStartY_<top && py<top) || downRow==upRow || catalogPanel_==CatalogPanel::Text)handleCatalogTap(px,py,w,h);
        }dirty_=true;return true;}
    if(e.type==SDL_FINGERMOTION){if(!catalogTouchOwned_ || catalogMultiTouch_ || e.tfinger.fingerId!=catalogTouchOwner_)return true;
        int w,h;getWindowSize(w,h);const float current=e.tfinger.y*h,delta=catalogTouchY_-current;
        if(std::hypot(e.tfinger.x*w-catalogTouchStartX_,e.tfinger.y*h-catalogTouchStartY_)>8 || catalogTouchMoved_){catalogTouchMoved_=true;catalogTouchY_=current;const auto r=catalogPanelRect(w,h);const int available=r.y+r.h-16-catalogContentTop(r);catalogScroll_=std::clamp(catalogScroll_+delta,0.f,float(std::max(0,int(catalogRows().size())*64-available)));dirty_=true;}return true;}
    if(e.type==SDL_MOUSEWHEEL){catalogScroll_=std::max(0.f,catalogScroll_-e.wheel.y*48.f);dirty_=true;return true;}
    return false; // Mouse fallback is translated by the shared event path.
}
void renderCatalogPanel(int w,int h,const ThemePalette &pal){
    if(catalogPanel_==CatalogPanel::None)return;
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer_,0,0,0,150);SDL_Rect full{0,0,w,h};SDL_RenderFillRect(renderer_,&full);
    const auto r=catalogPanelRect(w,h);ui::surface(renderer_,r,pal.card,16);
    typography_->renderText(renderer_,typography_->titleFont(),catalogHeading(),r.x+20,r.y+18,pal.fg,r.w-160);
    ui::button(renderer_,*typography_,{r.x+r.w-124,r.y+12,108,56},catLabel("Geri","Back"),pal,false,false,true,false);
    // Persisted catalog errors must remain visible above the modal, not hidden under it.
    if(!currentErrorMessage_.empty())
        typography_->renderText(renderer_,typography_->smallFont(),ui::label(currentErrorMessage_),r.x+20,r.y+60,
                                ui::destructiveColor(pal),r.w-44);
    if(catalogPanel_==CatalogPanel::Text){
        typography_->renderWrapped(renderer_,typography_->uiFont(),inputText_+"|",{r.x+20,r.y+80,r.w-40,64},pal.fg);
        const auto keys=catalogKeyboard();const int width=(r.w-40)/10;
        for(int i=0;i<int(keys.size());++i)ui::button(renderer_,*typography_,{r.x+20+(i%10)*width,r.y+156+(i/10)*48,width-4,44},keys[i]==" "?"_":keys[i],pal);
        ui::button(renderer_,*typography_,{r.x+20,r.y+r.h-76,r.w/2-24,56},catLabel("Kaydet","Save"),pal,true);
        ui::button(renderer_,*typography_,{r.x+r.w/2+4,r.y+r.h-76,r.w/2-24,56},catLabel("Geri sil","Backspace"),pal);return;
    }
    if(catalogPanel_==CatalogPanel::Details){auto e=catalog_.get(actionBook_.id);const auto p=progressStore_.get(actionBook_.id);const auto display=catalog_.apply(actionBook_);
        std::string last=catLabel("Henüz okunmadı","Not read yet");if(p.lastRead>0){const std::time_t time=p.lastRead;const auto *date=std::localtime(&time);char formatted[32]{};if(date){std::strftime(formatted,sizeof formatted,"%Y-%m-%d %H:%M",date);last=formatted;}}
        std::string memberships;for(const auto &id:e.collections){auto it=catalog_.collections().find(id);if(it!=catalog_.collections().end()){if(!memberships.empty())memberships+=", ";memberships+=it->second;}}
        const auto state=e.status==ReadingStatus::Completed?catLabel("Tamamlandı","Completed"):e.status==ReadingStatus::NotStarted?catLabel("Başlanmadı","Not started"):e.status==ReadingStatus::Reading?catLabel("Okunuyor","Reading"):catLabel("Otomatik","Automatic");
        const std::string genre=e.genre.empty()?e.sourceGenre:e.genre,series=(e.series.empty()?e.sourceSeries:e.series)+(e.sourceSeriesIndex.empty()?std::string{}:" #"+e.sourceSeriesIndex);
        const int coverWidth=std::min(140,r.w/4),contentHeight=catalogContentTop(r)-r.y-90;
        drawCover(display,{r.x+r.w-coverWidth-20,r.y+82,coverWidth,std::min(contentHeight,coverWidth*14/10)},pal);
        const std::string formatName = actionBook_.format == Format::Pdf ? "PDF" : actionBook_.format == Format::Cbz ? "CBZ" : "EPUB";
        const std::string details=display.title+"\n"+display.author+"\n"+formatName+" · "+std::to_string(actionBook_.bytes/1024)+" KiB · "+state+"\n"+catLabel("Tür: ","Genre: ")+genre+"\n"+catLabel("Seri: ","Series: ")+series+"\n"+catLabel("Koleksiyonlar: ","Collections: ")+memberships+"\n"+catLabel("İlerleme: ","Progress: ")+std::to_string(int(p.fraction*100))+"%\n"+catLabel("Son okuma: ","Last read: ")+last+"\n"+actionBook_.path;
        typography_->renderWrapped(renderer_,typography_->smallFont(),details,{r.x+20,r.y+82,r.w-coverWidth-56,contentHeight},pal.fg);
    }
    if(catalogPanel_==CatalogPanel::DeleteConfirm){typography_->renderWrapped(renderer_,typography_->uiFont(),catLabel("Bu dosya SD karttan kalıcı olarak silinecek. Bu işlem geri alınamaz.\n","This file will be permanently deleted from SD storage. This cannot be undone.\n")+fs::path(actionBook_.path).filename().string()+"\n"+actionBook_.path,{r.x+20,r.y+80,r.w-40,132},pal.fg);}
    if(catalogPanel_==CatalogPanel::Statistics){const auto today=readingStats_.dayMilliseconds(localReadingDay())/60000;const auto total=readingStats_.totalMilliseconds()/60000;
        typography_->renderWrapped(renderer_,typography_->uiFont(),catLabel("Bugün: ","Today: ")+std::to_string(today)+catLabel(" dk\nToplam: "," min\nTotal: ")+std::to_string(total)+catLabel(" dk · Hedef: "," min · Goal: ")+std::to_string(readingStats_.dailyGoal())+catLabel(" dk"," min"),{r.x+20,r.y+80,r.w-40,96},pal.fg);}
    const auto rows=catalogRows();const int top=catalogContentTop(r);SDL_Rect clip{r.x+12,top,r.w-24,std::max(0,r.y+r.h-16-top)};SDL_RenderSetClipRect(renderer_,&clip);
    for(int i=0;i<int(rows.size());++i){SDL_Rect row{r.x+20,top+i*64-int(catalogScroll_),r.w-40,56};if(row.y+row.h<clip.y||row.y>clip.y+clip.h)continue;
        const bool danger=(catalogPanel_==CatalogPanel::Actions&&rows[i].first==9)||(catalogPanel_==CatalogPanel::DeleteConfirm&&rows[i].first==1);
        const bool manage=catalogPanel_==CatalogPanel::Values&&catalogGroup_==CatalogGroup::Collections;
        if(manage)row.w-=132;
        const auto status=catalog_.get(actionBook_.id).status;
        const bool statusSelected=catalogPanel_==CatalogPanel::Actions && ((rows[i].first==2&&status==ReadingStatus::Reading)||(rows[i].first==3&&status==ReadingStatus::NotStarted)||(rows[i].first==4&&status==ReadingStatus::Completed));
        ui::menuRow(renderer_,*typography_,row,rows[i].second,pal,statusSelected,i==catalogFocus_ && catalogPanel_==CatalogPanel::Actions,danger);
        if(manage)ui::button(renderer_,*typography_,{row.x+row.w+8,row.y,124,56},catLabel("Düzenle","Manage"),pal);}
    if(rows.empty())typography_->renderText(renderer_,typography_->uiFont(),catLabel("Henüz öğe yok","No items"),r.x+24,top+24,pal.muted,r.w-48);
    SDL_RenderSetClipRect(renderer_,nullptr);
    if(int(rows.size())*64>clip.h){const int bar=std::max(24,clip.h*clip.h/(int(rows.size())*64));SDL_Rect indicator{r.x+r.w-10,top+int(catalogScroll_*(clip.h-bar)/std::max(1,int(rows.size())*64-clip.h)),4,bar};ui::surface(renderer_,indicator,pal.accent,2);}
}
