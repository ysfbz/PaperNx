// Included inside PaperNxApp. Integrate held controls once per frame; SDL does
// not generate repeat events for controller buttons or stationary stick axes.
int stickAxes_[SDL_CONTROLLER_AXIS_MAX]{};
ControllerNavigator rightControllerNav_;
bool controllerFocused_{true};
unsigned heldKeyboardPan_{0},heldPadPan_{0};
Uint32 controllerInputTick_{0};
float controllerRemainderX_{0},controllerRemainderY_{0};
std::pair<float,float> controlDirection(float x,float y) const {
    return DisplayTransform{0,0,appSettings_.get().displayOrientation}.directionToLogical(x,y);
}
void orientDirectionalEvent(SDL_Event &event) const {
    const bool key=event.type==SDL_KEYDOWN || event.type==SDL_KEYUP;
    const bool pad=event.type==SDL_CONTROLLERBUTTONDOWN || event.type==SDL_CONTROLLERBUTTONUP;
    if(!key && !pad)return;
    const int value=key?event.key.keysym.sym:event.cbutton.button;
    const int left=key?int(SDLK_LEFT):int(SDL_CONTROLLER_BUTTON_DPAD_LEFT),right=key?int(SDLK_RIGHT):int(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    const int up=key?int(SDLK_UP):int(SDL_CONTROLLER_BUTTON_DPAD_UP),down=key?int(SDLK_DOWN):int(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    const int dx=value==left?-1:value==right?1:0,dy=value==up?-1:value==down?1:0;
    if(!dx && !dy)return;
    const auto direction=controlDirection(float(dx),float(dy));
    const int mapped=direction.first<0?left:direction.first>0?right:direction.second<0?up:down;
    if(key)event.key.keysym.sym=SDL_Keycode(mapped);else event.cbutton.button=Uint8(mapped);
}
bool zoomPanContext() const {
    return doc_ && screen_==ScreenState::Reader && !quickMenu_ && !scrub_.active && !readerToolbarFocusActive_ &&
        catalogPanel_==CatalogPanel::None && choiceKind_<0 && colorEditing_<0 &&
        !continuous() && !px_document_is_epub(doc_) && pdfZoom_>1.001f;
}
static float stickAmount(int value) {
    constexpr int dead=8000;
    if(std::abs(value)<=dead) return 0;
    return (value<0?-1.f:1.f)*std::min(1.f,(std::abs(value)-dead)/float(32767-dead));
}
bool controllerMotionActive() const {
    if(heldKeyboardPan_ || heldPadPan_)return true;
    for(int axis:stickAxes_)if(std::abs(axis)>4000)return true;
    return false;
}
void clearHeldControls() {
    heldKeyboardPan_=heldPadPan_=0;
    for(auto &axis:stickAxes_)axis=0;
    controllerRemainderX_=controllerRemainderY_=0;controllerInputTick_=SDL_GetTicks();
    controllerNav_=ControllerNavigator{};rightControllerNav_=ControllerNavigator{};
    zoomFractionX_=zoomFractionY_=0;zoomLastPage_=-1;
}
void navigateStick(int axis,int value) {
    const bool right=axis>=SDL_CONTROLLER_AXIS_RIGHTX;
    auto &navigator=right?rightControllerNav_:controllerNav_;
    int dx=0,dy=0;
    if(!navigator.processAxis(right?axis-SDL_CONTROLLER_AXIS_RIGHTX:axis,value,dx,dy))return;
    if(choiceKind_>=0 || colorEditing_>=0 || catalogPanel_!=CatalogPanel::None) {
        // Send the same directional action to the topmost modal for either stick.
        SDL_Event event{};event.type=SDL_CONTROLLERBUTTONDOWN;
        event.cbutton.button=dx<0?SDL_CONTROLLER_BUTTON_DPAD_LEFT:dx>0?SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            dy<0?SDL_CONTROLLER_BUTTON_DPAD_UP:SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        bool running=true;handleEvent(event,running);
    } else {
        const auto direction=controlDirection(float(dx),float(dy));
        handleAnalog(int(direction.first),int(direction.second));
    }
}
void panReaderBy(float dx,float dy) {
    if(!doc_ || px_document_is_epub(doc_) || pageTurnAnimation_.active)return;
    panVelocityX_=panVelocityY_=0;
    controllerRemainderX_+=dx;controllerRemainderY_+=dy;
    const int x=int(controllerRemainderX_),y=int(controllerRemainderY_);
    controllerRemainderX_-=x;controllerRemainderY_-=y;
    const int oldX=pdfPanX_,oldY=pdfPanY_;
    pdfPanX_+=x;pdfPanY_+=y;clampPdfPan();
    if(pdfPanX_!=oldX+x)controllerRemainderX_=0;
    if(pdfPanY_!=oldY+y)controllerRemainderY_=0;
    if(pdfPanX_!=oldX || pdfPanY_!=oldY)dirty_=true;
}
bool trackHeldControl(const SDL_Event &event) {
    if(event.type==SDL_CONTROLLERAXISMOTION && event.caxis.axis<SDL_CONTROLLER_AXIS_MAX) {
        if(!controllerMotionActive())controllerInputTick_=SDL_GetTicks();
        stickAxes_[event.caxis.axis]=event.caxis.value;
        if(controllerFocused_ && !appBackground_ && !zoomPanContext() && event.caxis.axis<=SDL_CONTROLLER_AXIS_RIGHTY)
            navigateStick(event.caxis.axis,event.caxis.value);
        return true;
    }
    const bool key=event.type==SDL_KEYDOWN || event.type==SDL_KEYUP;
    const bool pad=event.type==SDL_CONTROLLERBUTTONDOWN || event.type==SDL_CONTROLLERBUTTONUP;
    if(!key && !pad)return false;
    const bool down=event.type==SDL_KEYDOWN || event.type==SDL_CONTROLLERBUTTONDOWN;
    const int value=key?event.key.keysym.sym:event.cbutton.button;
    const int left=key?int(SDLK_LEFT):int(SDL_CONTROLLER_BUTTON_DPAD_LEFT),right=key?int(SDLK_RIGHT):int(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
    const int up=key?int(SDLK_UP):int(SDL_CONTROLLER_BUTTON_DPAD_UP),bottom=key?int(SDLK_DOWN):int(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    unsigned bit=value==left?1u:value==right?2u:value==up?4u:value==bottom?8u:0u;
    if(!bit)return false;
    auto &held=key?heldKeyboardPan_:heldPadPan_;
    if(!down) {held&=~bit;return false;}
    if(!zoomPanContext() || !comic())return false;
    if(!heldKeyboardPan_ && !heldPadPan_)controllerInputTick_=SDL_GetTicks();
    held|=bit; // Integrate from elapsed time, including the very first frame.
    return true;
}
void tickControllerInput() {
    const auto now=SDL_GetTicks();
    const float dt=std::min(.05f,(now-controllerInputTick_)/1000.f);controllerInputTick_=now;
    if(appBackground_ || !controllerFocused_) {clearHeldControls();return;}
    if(controller_) for(int axis=0;axis<SDL_CONTROLLER_AXIS_MAX;++axis)
        stickAxes_[axis]=SDL_GameControllerGetAxis(controller_,static_cast<SDL_GameControllerAxis>(axis));
    const bool fixedReader=doc_ && screen_==ScreenState::Reader && !quickMenu_ && !scrub_.active && !continuous() &&
        !px_document_is_epub(doc_) && catalogPanel_==CatalogPanel::None && choiceKind_<0 && colorEditing_<0;
    if(fixedReader && !pageTurnAnimation_.active && activeFingers_.empty()) {
        auto trigger=[](int value){return std::clamp((value-4000)/28767.f,0.f,1.f);};
        const float zoom=trigger(stickAxes_[SDL_CONTROLLER_AXIS_TRIGGERRIGHT])-trigger(stickAxes_[SDL_CONTROLLER_AXIS_TRIGGERLEFT]);
        if(std::abs(zoom)>.001f)zoomPdf(pdfZoom_*std::exp(zoom*1.1f*dt),.5f,.5f,true);
    }
    const float rx=stickAmount(stickAxes_[2]),ry=stickAmount(stickAxes_[3]);
    if(zoomPanContext()) {
        if(pageTurnAnimation_.active || !activeFingers_.empty())return;
        const unsigned held=heldKeyboardPan_|heldPadPan_;
        float x=stickAmount(stickAxes_[0])+rx+float(bool(held&2))-float(bool(held&1));
        float y=stickAmount(stickAxes_[1])+ry+float(bool(held&8))-float(bool(held&4));
        const float length=std::max(1.f,std::hypot(x,y));
        // A centered stick must not cancel momentum from a touch gesture.
        const auto direction=controlDirection(x/length,y/length);
        if(std::abs(x)+std::abs(y)>.001f)panReaderBy(direction.first*850.f*dt,direction.second*850.f*dt);
        return;
    }
    heldKeyboardPan_=heldPadPan_=0;
    for(int axis=0;axis<=SDL_CONTROLLER_AXIS_RIGHTY;++axis)navigateStick(axis,stickAxes_[axis]);
}
