    int choiceKind_{-1},choiceIndex_{0},choiceFocusedIndex_{0};
    float choiceScroll_{0.f};
    bool choiceDragging_{false},choiceTouchActive_{false};
    float choiceDragStartY_{0.f},choiceDragStartScroll_{0.f};

    std::vector<std::string> choiceOptions() const {
        switch(choiceKind_) {
        case 0:return {"Serif","Sans",ui::language?"Publisher":"Yay\u0131nc\u0131"};
        case 1:return ui::language?std::vector<std::string>{"Landscape","Portrait clockwise","Portrait counterclockwise"}:std::vector<std::string>{"Yatay","Dikey: saat y\u00f6n\u00fc","Dikey: saat y\u00f6n\u00fcn\u00fcn tersi"};
        case 7:{std::vector<std::string> out;for(int i=0;i<int(sizeof(g_paperPresets)/sizeof(g_paperPresets[0]));++i)out.push_back(paperPresetLabel(i));return out;}
        case 6:return ui::language?std::vector<std::string>{"Off","Soft paper","Natural paper"}:std::vector<std::string>{"Kapal\u0131","Yumu\u015fak ka\u011f\u0131t","Do\u011fal ka\u011f\u0131t"};
        default:return {};
        }
    }

    SDL_Rect choiceSheet(int w,int h) const {
        int width=std::min(560,w-32);
        int maxHeight=std::max(180,h-64);
        int totalHeight=int(choiceOptions().size())*56;
        int height=std::min(maxHeight,80+totalHeight);
        return {(w-width)/2,(h-height)/2,width,height};
    }
    SDL_Rect choiceContentRect(int w,int h) const {
        auto box=choiceSheet(w,h);
        return {box.x+16,box.y+64,box.w-32,std::max(56,box.h-76)};
    }
    void ensureChoiceVisible(int w,int h) {
        auto cr=choiceContentRect(w,h);
        float itemTop=choiceFocusedIndex_*56.f;
        float itemBottom=itemTop+56.f;
        float maxScroll=std::max(0.f,float(choiceOptions().size()*56)-cr.h);
        if(itemTop<choiceScroll_)choiceScroll_=itemTop;
        else if(itemBottom>choiceScroll_+cr.h)choiceScroll_=itemBottom-cr.h;
        choiceScroll_=std::clamp(choiceScroll_,0.f,maxScroll);
    }
    void openChoice(int kind,int selected) {
        choiceKind_=kind;
        choiceIndex_=selected;
        // A custom palette has no selected built-in preset; focus starts at the first item.
        choiceFocusedIndex_=std::max(0,selected);
        choiceScroll_=0.f;
        choiceDragging_=false;
        choiceTouchActive_=false;
        int w,h;getWindowSize(w,h);
        ensureChoiceVisible(w,h);
        dirty_=true;
    }
    void commitChoice() {
        auto a=appSettings_.get();
        if(choiceKind_==0){currentFontFamily_=choiceIndex_;queueStyleChange();}
        if(choiceKind_==1){a.displayOrientation=choiceIndex_;appSettings_.put(a);pageCache_.clear();if(doc_ && px_document_is_epub(doc_)){++layoutGeneration_;layoutPending_=true;layoutDue_=SDL_GetTicks()+100;}else if(doc_)setPdfView(extraStyle_.pdfViewMode);}
        if(choiceKind_==7)applyGlobalPaperPreset(choiceIndex_);
        // Kind 5 (obsolete PDF-only filter) intentionally has no handler.
        if(choiceKind_==6){a.paperTexture=choiceIndex_;pageCache_.clear();}
        if(choiceKind_==1 || choiceKind_==6){appSettings_.put(a);persistAppSettings();}
        choiceKind_=-1;choiceDragging_=false;choiceTouchActive_=false;gestureTracker_.reset();activeFingers_.clear();dirty_=true;
    }
    bool choiceEvent(const SDL_Event &e) {
        if(choiceKind_<0)return false;
        int w,h;getWindowSize(w,h);
        auto box=choiceSheet(w,h);
        auto cr=choiceContentRect(w,h);
        auto options=choiceOptions();
        float maxScroll=std::max(0.f,float(options.size()*56)-cr.h);

        if(e.type==SDL_KEYDOWN || e.type==SDL_CONTROLLERBUTTONDOWN){
            bool pad=e.type==SDL_CONTROLLERBUTTONDOWN;int k=pad?e.cbutton.button:e.key.keysym.sym;
            auto m=[&](int key,int button){return k==(pad?button:key);};
            if(m(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B)){choiceKind_=-1;choiceDragging_=false;dirty_=true;}
            else if(m(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A)){
                choiceIndex_=choiceFocusedIndex_;
                commitChoice();
            } else {
                int d=(m(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN)||m(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))?1:
                      (m(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP)||m(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT))?-1:0;
                if(d!=0) {
                    choiceFocusedIndex_=std::clamp(choiceFocusedIndex_+d,0,int(options.size())-1);
                    choiceIndex_=choiceFocusedIndex_;
                    ensureChoiceVisible(w,h);
                    dirty_=true;
                }
            }
            return true;
        }

        if(e.type==SDL_FINGERDOWN){
            int x=int(e.tfinger.x*w),y=int(e.tfinger.y*h);
            if(ui::contains(cr,x,y)) {
                choiceDragging_=false;
                choiceTouchActive_=true;
                choiceDragStartY_=float(y);
                choiceDragStartScroll_=choiceScroll_;
            } else if(ui::contains(box,x,y)) {
                choiceDragging_=false;
                choiceTouchActive_=true;
            } else {
                choiceTouchActive_=false;
            }
            return true;
        }

        if(e.type==SDL_FINGERMOTION){
            if(choiceTouchActive_){
                float curY=e.tfinger.y*h;
                float dy=curY-choiceDragStartY_;
                if(!choiceDragging_ && std::abs(dy)>8.f) choiceDragging_=true;
                if(choiceDragging_) {
                    choiceScroll_=std::clamp(choiceDragStartScroll_-dy,0.f,maxScroll);
                    dirty_=true;
                }
            }
            return true;
        }

        if(e.type==SDL_FINGERUP){
            int x=int(e.tfinger.x*w),y=int(e.tfinger.y*h);
            choiceTouchActive_=false;
            if(choiceDragging_){
                choiceDragging_=false;
                dirty_=true;
                return true;
            }
            if(!ui::contains(box,x,y)){choiceKind_=-1;dirty_=true;return true;}
            if(ui::contains(cr,x,y)){
                int localY=y-cr.y+int(choiceScroll_);
                int index=localY/56;
                if(index>=0 && index<int(options.size()) && (localY%56)<52){
                    choiceIndex_=index;
                    choiceFocusedIndex_=index;
                    commitChoice();
                }
            }
            lastFingerTick_=SDL_GetTicks();sawFinger_=true;
            return true;
        }

        if(e.type==SDL_MOUSEBUTTONDOWN || e.type==SDL_MOUSEMOTION || e.type==SDL_MOUSEBUTTONUP) {
            if(!(sawFinger_ && SDL_GetTicks()-lastFingerTick_<750)) {
                int pw,ph;SDL_GetWindowSize(window_,&pw,&ph);
                DisplayTransform transform{pw,ph,appSettings_.get().displayOrientation};
                float mx=float(e.type==SDL_MOUSEMOTION?e.motion.x:e.button.x);
                float my=float(e.type==SDL_MOUSEMOTION?e.motion.y:e.button.y);
                auto point=transform.toLogical(mx,my);
                SDL_Event finger{};
                finger.type=(e.type==SDL_MOUSEMOTION)?SDL_FINGERMOTION:(e.type==SDL_MOUSEBUTTONDOWN?SDL_FINGERDOWN:SDL_FINGERUP);
                finger.tfinger.x=point.first/transform.logicalWidth();
                finger.tfinger.y=point.second/transform.logicalHeight();
                choiceEvent(finger);
            }
            return true;
        }
        return true;
    }
    void renderChoices(int w,int h,const ThemePalette &pal) {
        if(choiceKind_<0)return;
        SDL_SetRenderDrawBlendMode(renderer_,SDL_BLENDMODE_BLEND);SDL_SetRenderDrawColor(renderer_,0,0,0,150);SDL_Rect full{0,0,w,h};SDL_RenderFillRect(renderer_,&full);
        auto box=choiceSheet(w,h);ui::surface(renderer_,box,pal.card,18);
        typography_->renderText(renderer_,typography_->sectionFont(),ui::language?"Choose an option":"Bir se\u00e7enek se\u00e7in",box.x+24,box.y+16,pal.fg,box.w-48);
        auto cr=choiceContentRect(w,h);
        SDL_RenderSetClipRect(renderer_,&cr);
        auto options=choiceOptions();
        for(int i=0;i<int(options.size());++i){
            int itemY=cr.y+i*56-int(choiceScroll_);
            if(itemY+56>=cr.y && itemY<=cr.y+cr.h){
                bool isSelected=(i==choiceIndex_);
                bool isFocused=(i==choiceFocusedIndex_);
                ui::button(renderer_,*typography_,{cr.x,itemY,cr.w,52},options[i],pal,isSelected,false,false,isFocused);
            }
        }
        SDL_RenderSetClipRect(renderer_,nullptr);
        float maxScroll=std::max(0.f,float(options.size()*56)-cr.h);
        if(maxScroll>0.f){
            int barW=4;
            int barX=box.x+box.w-12;
            int barY=cr.y;
            int barH=cr.h;
            SDL_SetRenderDrawColor(renderer_,pal.muted.r,pal.muted.g,pal.muted.b,80);
            SDL_Rect track{barX,barY,barW,barH};
            SDL_RenderFillRect(renderer_,&track);
            int thumbH=std::max(20,int(float(cr.h*cr.h)/(options.size()*56)));
            int thumbY=barY+int((choiceScroll_/maxScroll)*(barH-thumbH));
            SDL_SetRenderDrawColor(renderer_,pal.accent.r,pal.accent.g,pal.accent.b,200);
            SDL_Rect thumb{barX,thumbY,barW,thumbH};
            SDL_RenderFillRect(renderer_,&thumb);
        }
    }
