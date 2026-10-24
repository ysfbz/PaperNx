// UI state is owned by the SDL thread. The worker exposes value snapshots only.
bool updateDialog_{false},updateNotes_{false},updateManual_{false};
int updateFocus_{2},updatePercent_{-1};
float updateNotesScroll_{0},updateTouchY_{0};
UpdateState updateSeen_{UpdateState::Idle};
std::string updateNotesKey_;
std::vector<std::string> updateNoteLines_;
float updateAxes_[4]{};
Uint32 updateAxisTick_{0},updateAxisRepeat_{0};

void layoutUpdateNotes(const std::string &text,int width) {
    const auto key=std::to_string(width)+":"+text;if(key==updateNotesKey_)return;
    updateNotesKey_=key;updateNoteLines_.clear();
    std::istringstream paragraphs(text);std::string paragraph;
    auto measure=[&](const std::string &s){int result=0;typography_->measureText(typography_->smallFont(),s,&result,nullptr);return result;};
    while(std::getline(paragraphs,paragraph)) {
        std::istringstream words(paragraph);std::string word,line;
        auto append=[&](const std::string &s){updateNoteLines_.push_back(s);};
        while(words>>word) {
            const auto candidate=line.empty()?word:line+" "+word;
            if(!line.empty()&&measure(candidate)>width){append(line);line=word;}else line=candidate;
        }
        if(!line.empty()) append(line);
    }
}

void showUpdateDialog() {
    clearHeldControls();cancelPageTurn();cancelReaderScrub(true);
    scrollVelocity_=panVelocityX_=panVelocityY_=listVelocity_=0;
    activeFingers_.clear();gestureTracker_.reset();ui::pointer(0,0,false);
    std::fill(std::begin(updateAxes_),std::end(updateAxes_),0.f);updateAxisRepeat_=0;updateAxisTick_=SDL_GetTicks();
    quickMenu_=helpMenu_=false;updateDialog_=true;updateNotes_=false;updateFocus_=2;dirty_=true;
}
void checkForUpdate() {
    updateManual_=true;showUpdateDialog();updateSeen_=UpdateState::Idle;
    if(!startUpdateNetwork())updater_->reportError("Network services are unavailable");
    else if(!updater_->check())dirty_=true;
}
void pollUpdates() {
    const auto state=updater_->status();
    if(updateDialog_){
        const auto now=SDL_GetTicks();const float elapsed=std::min(50u,now-updateAxisTick_)/1000.f;updateAxisTick_=now;
        int w,h;getWindowSize(w,h);const DisplayTransform transform{w,h,appSettings_.get().displayOrientation};
        const auto axis=transform.directionToLogical(std::clamp(updateAxes_[0]+updateAxes_[2],-1.f,1.f),std::clamp(updateAxes_[1]+updateAxes_[3],-1.f,1.f));
        if(updateNotes_&&std::abs(axis.second)>.25f){updateNotesScroll_=std::max(0.f,updateNotesScroll_+axis.second*360*elapsed);dirty_=true;}
        else if(!updateNotes_&&!updateBusy(state)&&std::abs(axis.first)>.45f){
            if(!updateAxisRepeat_||now>=updateAxisRepeat_){updateFocus_=std::clamp(updateFocus_+(axis.first>0?1:-1),0,updateButtonCount(state)-1);updateAxisRepeat_=now+180;dirty_=true;}
        }else updateAxisRepeat_=0;
    }
    if(state.state==updateSeen_ && state.percent==updatePercent_)return;
    updateSeen_=state.state;updatePercent_=state.percent;dirty_=true;
    if(state.state==UpdateState::Ready)showUpdateDialog();
    else if(state.state==UpdateState::Available && (state.release.installable() || updateManual_))showUpdateDialog();
    // Offline/no-update startup checks stay quiet. Explicit checks report all outcomes.
    else if(updateManual_ && (state.state==UpdateState::Current || state.state==UpdateState::Error))showUpdateDialog();
}
SDL_Rect updatePanel(int w,int h) const {const int width=std::min(660,w-48),height=std::min(540,h-112);return {(w-width)/2,(h-height)/2,width,height};}
SDL_Rect updateButton(int index,int count,int w,int h) const {const auto r=updatePanel(w,h);const int step=(r.w-48)/count;return {r.x+24+index*step,r.y+r.h-80,step-8,56};}
int updateButtonCount(const UpdateStatus &s) const {return updateBusy(s)||s.state==UpdateState::Cancelled?1:!updateNotes_&&s.state==UpdateState::Available&&s.release.installable()?3:1;}
bool updateBusy(const UpdateStatus &s) const {return s.state==UpdateState::Checking||s.state==UpdateState::Downloading||s.state==UpdateState::Verifying;}
void activateUpdate(int index) {
    const auto s=updater_->status();if(updateBusy(s)){updater_->cancel();return;}
    if(updateNotes_){updateNotes_=false;updateFocus_=1;std::fill(std::begin(updateAxes_),std::end(updateAxes_),0.f);dirty_=true;return;}
    if(s.state==UpdateState::Available && s.release.installable()) {
        if(index==1){updateNotes_=true;updateNotesScroll_=0;updateFocus_=0;}
        else if(index==2){saveCurrentPosition();flushProgress();updater_->download();}
        else {updateDialog_=false;updateManual_=false;}
    } else if(s.state==UpdateState::Ready) {updateDialog_=false;updateManual_=false;}
    else {updateDialog_=false;updateManual_=false;}
    dirty_=true;
}
bool updateEvent(SDL_Event e) {
    if(!updateDialog_)return false;
    const auto s=updater_->status();int w,h;getWindowSize(w,h);
    if(e.type==SDL_FINGERDOWN){updateTouchY_=e.tfinger.y*h;return true;}
    if(e.type==SDL_FINGERMOTION){
        if(updateNotes_){const float y=e.tfinger.y*h;updateNotesScroll_=std::max(0.f,updateNotesScroll_+updateTouchY_-y);updateTouchY_=y;dirty_=true;}return true;
    }
    if(e.type==SDL_FINGERUP){
        const int count=updateButtonCount(s),x=int(e.tfinger.x*w),y=int(e.tfinger.y*h);
        for(int i=0;i<count;++i)if(ui::contains(updateButton(i,count,w,h),x,y)){activateUpdate(i);break;}
        ui::pointer(0,0,false);return true;
    }
    if(e.type==SDL_MOUSEWHEEL){if(updateNotes_)updateNotesScroll_=std::max(0.f,updateNotesScroll_-e.wheel.y*48);dirty_=true;return true;}
    if(e.type==SDL_CONTROLLERAXISMOTION){
        const int axis=e.caxis.axis;
        if(axis>=SDL_CONTROLLER_AXIS_LEFTX&&axis<=SDL_CONTROLLER_AXIS_RIGHTY){float value=e.caxis.value/32768.f;updateAxes_[axis]=std::abs(value)>.25f?value:0.f;pollUpdates();}
        return true;
    }
    if(e.type==SDL_KEYDOWN||e.type==SDL_CONTROLLERBUTTONDOWN){
        orientDirectionalEvent(e);const bool pad=e.type==SDL_CONTROLLERBUTTONDOWN;const int k=pad?e.cbutton.button:e.key.keysym.sym;
        auto is=[&](int key,int button){return k==(pad?button:key);};
        if(is(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B)){if(updateBusy(s))updater_->cancel();else if(updateNotes_){updateNotes_=false;updateFocus_=1;}else if(s.state!=UpdateState::Ready){updateDialog_=false;updateManual_=false;}else {updateDialog_=false;updateManual_=false;}}
        else if(is(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A))activateUpdate(std::clamp(updateFocus_,0,updateButtonCount(s)-1));
        else if(is(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT))updateFocus_=std::max(0,updateFocus_-1);
        else if(is(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)||(!pad&&k==SDLK_TAB))updateFocus_=(updateFocus_+1)%updateButtonCount(s);
        else if(updateNotes_){if(is(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP))updateNotesScroll_=std::max(0.f,updateNotesScroll_-60);if(is(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN))updateNotesScroll_+=60;}
        dirty_=true;return true;
    }
    if(e.type==SDL_TEXTINPUT||e.type==SDL_MULTIGESTURE||e.type==SDL_KEYUP||e.type==SDL_CONTROLLERBUTTONUP)return true;
    return false;
}
void renderUpdateDialog(int w,int h,const ThemePalette &pal) {
    if(!updateDialog_)return;
    const auto s=updater_->status();const auto panel=updatePanel(w,h);
    SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer_,0,0,0,160);SDL_Rect screen{0,0,w,h};SDL_RenderFillRect(renderer_,&screen);
    ui::elevated(renderer_,panel,pal);
    typography_->renderText(renderer_,typography_->sectionFont(),ui::label(updateNotes_?"What's new":"Software update"),panel.x+24,panel.y+22,pal.fg,panel.w-48);
    const SDL_Rect body{panel.x+24,panel.y+76,panel.w-48,panel.h-176};
    std::string text;
    if(updateNotes_) {
        text=s.release.title+"\n\n"+s.release.notes;if(s.release.notes.empty())text=ui::label("No release notes");
        layoutUpdateNotes(text,body.w);const int step=TTF_FontLineSkip(typography_->smallFont());
        updateNotesScroll_=std::clamp(updateNotesScroll_,0.f,float(std::max(0,int(updateNoteLines_.size())*step-body.h)));
        SDL_RenderSetClipRect(renderer_,&body);
        const int first=int(updateNotesScroll_)/step,last=std::min(int(updateNoteLines_.size()),first+body.h/step+2);
        for(int i=first;i<last;++i)typography_->renderText(renderer_,typography_->smallFont(),updateNoteLines_[i],body.x,body.y+i*step-int(updateNotesScroll_),pal.fg,body.w);
        SDL_RenderSetClipRect(renderer_,nullptr);
    } else {
        if(s.state==UpdateState::Checking)text=ui::label("Checking for updates...");
        else if(s.state==UpdateState::Downloading)text=ui::label("Downloading update...")+" "+std::to_string(s.percent)+"%";
        else if(s.state==UpdateState::Current)text=ui::label("You are on the latest version")+"\n\n"+APP_VERSION_FULL;
        else if(s.state==UpdateState::Error)text=ui::label("Could not update PaperNX")+"\n\n"+ui::label(s.error);
        else if(s.state==UpdateState::Verifying)text=ui::label("Verifying update...");
        else if(s.state==UpdateState::Ready) {
            // STAGED only: the running PaperNX.nro has not been replaced.
            const auto folder=updater_->executable().parent_path().string();
            text=ui::label("Downloaded and verified; NOT installed yet")+"\n\n"+ui::label("Version")+": "+s.release.version+"\n"+"SHA-256: "+s.release.sha256+"\n"+ui::label("Staged file")+": "+folder+"/PaperNX.nro.new\n\n"+
                ui::label("Manual installation required: close PaperNX, back up PaperNX.nro, then replace it with PaperNX.nro.new using a file manager. Do not delete other files.");
        }
        else if(s.state==UpdateState::Cancelled)text=ui::label("Update cancelled");
        else if(s.state==UpdateState::Available){
            text=ui::label("New version available")+"\n\n"+s.release.version+"\n"+s.release.title;
            if(s.release.bytes)text+="\n"+std::to_string((s.release.bytes+1048575)/1048576)+" MB";
            if(!s.release.installable())text+="\n\n"+ui::label("This release has no installable NRO and checksum");
        }
        SDL_RenderSetClipRect(renderer_,&body);typography_->renderWrapped(renderer_,typography_->uiFont(),text,body,pal.fg);SDL_RenderSetClipRect(renderer_,nullptr);
        if(s.state==UpdateState::Downloading)typography_->drawProgressBar(renderer_,body.x,panel.y+panel.h-118,body.w,8,s.percent/100.f,pal.cardBorder,pal.accent);
    }
    const int count=updateButtonCount(s);
    for(int i=0;i<count;++i){
        const char *label=count==3?(i==0?"Later":i==1?"What's new":"Update"):
            updateBusy(s)?s.state==UpdateState::Checking?"Cancel check":s.state==UpdateState::Verifying?"Cancel":"Cancel download":
            updateNotes_?"Back":"Close";
        auto r=updateButton(i,count,w,h);ui::button(renderer_,*typography_,r,ui::label(label),pal,i==std::clamp(updateFocus_,0,count-1));
    }
}
