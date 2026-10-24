#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>

namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    static void rotatedControls(PaperNxApp &app) {
        const auto original=app.appSettings_.get();const int oldX=app.pdfPanX_,oldY=app.pdfPanY_;
        const int keys[]{SDLK_RIGHT,SDLK_DOWN,SDLK_LEFT,SDLK_UP};
        const int buttons[]{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_DOWN,SDL_CONTROLLER_BUTTON_DPAD_LEFT,SDL_CONTROLLER_BUTTON_DPAD_UP};
        const int dx[]{1,0,-1,0},dy[]{0,1,0,-1};
        bool running=true;
        for(int orientation=0;orientation<3;++orientation) {
            auto settings=original;settings.displayOrientation=orientation;app.appSettings_.put(settings);
            const DisplayTransform transform{1280,720,orientation};
            for(int control=0;control<4;++control)for(int direction=0;direction<4;++direction) {
                app.clearHeldControls();app.pdfPanX_=app.pdfPanY_=500;app.clampPdfPan();
                const auto before=transform.toPhysical(-float(app.pdfPanX_),-float(app.pdfPanY_));
                SDL_Event event{};
                if(control==0){event.type=SDL_KEYDOWN;event.key.keysym.sym=SDL_Keycode(keys[direction]);}
                else if(control==1){event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=Uint8(buttons[direction]);}
                else {event.type=SDL_CONTROLLERAXISMOTION;event.caxis.axis=Uint8((control==2?SDL_CONTROLLER_AXIS_LEFTX:SDL_CONTROLLER_AXIS_RIGHTX)+(dy[direction]?1:0));event.caxis.value=Sint16((dx[direction]+dy[direction])*32767);}
                app.handleEvent(event,running);
                for(int frame=0;frame<3;++frame){app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();}
                const auto after=transform.toPhysical(-float(app.pdfPanX_),-float(app.pdfPanY_));
                const float movementX=after.first-before.first,movementY=after.second-before.second;
                // Artwork moves opposite the requested physical scroll direction,
                // regardless of how the logical reader was rotated for rendering.
                assert(-(movementX*dx[direction]+movementY*dy[direction])>=39);
                assert(std::abs(movementX*dy[direction]-movementY*dx[direction])<1);
                if(control==0)event.type=SDL_KEYUP;else if(control==1)event.type=SDL_CONTROLLERBUTTONUP;else event.caxis.value=0;
                app.handleEvent(event,running);const int stoppedX=app.pdfPanX_,stoppedY=app.pdfPanY_;
                app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanX_==stoppedX && app.pdfPanY_==stoppedY);
            }
        }
        // Keep a physical direction held while rotating: no new press is needed.
        app.clearHeldControls();app.pdfPanX_=app.pdfPanY_=500;
        SDL_Event held{};held.type=SDL_CONTROLLERBUTTONDOWN;held.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_RIGHT;app.handleEvent(held,running);
        auto settings=original;settings.displayOrientation=1;app.appSettings_.put(settings);
        app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanX_==500 && app.pdfPanY_<500);
        settings.displayOrientation=2;app.appSettings_.put(settings);const int y=app.pdfPanY_;
        app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanX_==500 && app.pdfPanY_>y);
        app.clearHeldControls();app.appSettings_.put(original);app.pdfPanX_=oldX;app.pdfPanY_=oldY;app.clampPdfPan();
        std::cout<<"PASS: keyboard/D-pad/LS/RS scroll in all four physical directions at all three rotations, release and live held rotation\n";
    }
    static void finish(PaperNxApp &app) {
        assert(app.pageTurnAnimation_.active);
        app.pageTurnAnimation_.started=SDL_GetTicks()-1000;app.tickPageTurn();
    }
    static void waitComicTurn(PaperNxApp &app) {
        const auto deadline=SDL_GetTicks()+20000;
        while(app.comicPendingTurn_.active && SDL_GetTicks()<deadline){app.processComicPages();SDL_Delay(2);}
        app.processComicPages();assert(!app.comicPendingTurn_.active);
    }
    static void waitComicPages(PaperNxApp &app) {
        const auto deadline=SDL_GetTicks()+20000;
        while(app.comicPages_.pending() && SDL_GetTicks()<deadline){app.processComicPages();SDL_Delay(2);}
        app.processComicPages();assert(!app.comicPages_.pending());
    }
    static void waitTouchTurn(PaperNxApp &app) {
        const auto deadline=SDL_GetTicks()+20000;
        while(app.pageTurnTouch_.waitingForTexture && SDL_GetTicks()<deadline) {
            app.processComicPages();app.tickPageTurn();SDL_Delay(2);
        }
        assert(!app.pageTurnTouch_.waitingForTexture);
    }
    static void turnComicPage(PaperNxApp &app,int delta) {app.turnPage(delta);waitComicTurn(app);}
    static void touch(PaperNxApp &app,Uint32 type,float x,float y) {
        SDL_Event e{};e.type=type;e.tfinger.fingerId=8;e.tfinger.x=x;e.tfinger.y=y;
        bool running=true;app.handleEvent(e,running);assert(running);
    }
    static void snapshot(PaperNxApp &app,const std::string &name) {
        app.render();int w,h;SDL_GetRendererOutputSize(app.renderer_,&w,&h);
        auto surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,surface->format->format,surface->pixels,surface->pitch)==0);
        fs::create_directories("verification/comics");
        assert(SDL_SaveBMP(surface,("verification/comics/"+name+".bmp").c_str())==0);SDL_FreeSurface(surface);
    }
    static void run(const fs::path &root) {
        PaperNxApp app(root);assert(initReady(app));assert(app.typography_->uiFont());ui::language=1;
        auto preferences=app.appSettings_.get();preferences.bookLandscapeSpreads=false;preferences.comicLandscapeSpreads=false;app.appSettings_.put(preferences);
        SDL_RendererInfo info{};SDL_GetRendererInfo(app.renderer_,&info);std::cout<<"Comic renderer: "<<info.name<<"\n";
        assert(app.books_.size()==6);
        auto book=[&](const char *filename) {return *std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &b){return fs::path(b.path).filename()==filename;});};
        const Book cbz=book("sample.cbz"),rar=book("sample.cbr"),manga=book("manga.cbz"),broken=book("broken.cbz");
        assert(cbz.format==Format::Cbz && rar.format==Format::Cbr);
        app.getOrRenderThumbnail(cbz);const Uint32 coverDeadline=SDL_GetTicks()+3000;
        while(!app.thumbCache_.get("thumb_"+cbz.id) && SDL_GetTicks()<coverDeadline) {SDL_Delay(10);app.processCompletedThumbnails();}
        assert(app.thumbCache_.get("thumb_"+cbz.id));
        const auto cachedCover=app.coverRepository_.read(cbz);assert(cachedCover);
        const auto *pixel=cachedCover->rgb.data()+(cachedCover->height/4)*cachedCover->stride+(cachedCover->width/4)*3;
        assert(std::abs(int(pixel[1])-185)<8); // ComicInfo FrontCover, not the first page.
        // A real, compressed RAR5 scan exceeds the old 16 MiB thumbnail budget.
        const Book large=book("large.cbr");app.getOrRenderThumbnail(large);
        const Uint32 largeDeadline=SDL_GetTicks()+5000;
        while(!app.thumbCache_.get("thumb_"+large.id) && SDL_GetTicks()<largeDeadline){SDL_Delay(10);app.processCompletedThumbnails();}
        assert(app.thumbCache_.get("thumb_"+large.id));
        app.openBook(large);app.zoomPdf(5);const unsigned beforeColdRender=app.comicTextureBuilds_;app.render();
        assert(app.comicTextureBuilds_==beforeColdRender && app.comicPages_.pending());
        waitComicPages(app);app.render();
        const unsigned builds=app.comicTextureBuilds_;assert(builds==beforeColdRender && app.pageCache_.get(app.comicTextureKey(app.currentPage_)));
        app.setPdfView(0);app.zoomPdf(3);app.pdfPanY_=400;
        auto imagePoint=[&]{int w,h;app.getWindowSize(w,h);float pw=0,ph=0;assert(px_document_page_size(app.doc_,app.currentPage_,&pw,&ph));
            const float vw=app.fixedViewportWidth(w),vh=app.comicViewportHeight(h),scale=std::min(vw/pw,vh/ph)*app.pdfZoom_;
            return std::pair<float,float>{(.65f*w-app.currentMargin_-std::max(0.f,(vw-pw*scale)/2)+app.pdfPanX_)/scale,
                (.45f*h-72-std::max(0.f,(vh-ph*scale)/2)+app.pdfPanY_)/scale};};
        const auto focal=imagePoint();app.zoomPdf(4,.65f,.45f);const auto after=imagePoint();
        if(std::abs(focal.first-after.first)>=4 || std::abs(focal.second-after.second)>=4)
            std::cerr<<"Zoom focal drift: "<<focal.first<<","<<focal.second<<" -> "<<after.first<<","<<after.second<<"\n";
        assert(std::abs(focal.first-after.first)<4 && std::abs(focal.second-after.second)<4);
        app.zoomPdf(5);app.pdfPanX_=200;app.pdfPanY_=600;app.toolbarVisible_=false;
        app.toolbarVisible_=true;
        touch(app,SDL_FINGERDOWN,.65f,.4f);touch(app,SDL_FINGERMOTION,.64f,.4f);touch(app,SDL_FINGERUP,.64f,.4f);
        assert(app.pdfPanX_>200 && app.toolbarVisible_ && app.currentPage_==0);
        app.pdfPanX_=0;
        touch(app,SDL_FINGERDOWN,.5f,.4f);touch(app,SDL_FINGERMOTION,.6f,.4f);assert(app.pdfPanX_==0);
        touch(app,SDL_FINGERMOTION,.59f,.4f);assert(app.pdfPanX_>0); // Immediate reversal at an edge.
        touch(app,SDL_FINGERUP,.59f,.4f);
        app.toolbarVisible_=false;app.pdfPanX_=200;
        const auto begin=std::chrono::steady_clock::now();
        touch(app,SDL_FINGERDOWN,.65f,.5f);
        for(int frame=1;frame<=60;++frame){touch(app,SDL_FINGERMOTION,.65f-frame*(.2f/1280),.5f);app.render();}
        assert(app.pdfPanX_==212); // Fractional motion accumulates instead of rounding away every event.
        touch(app,SDL_FINGERUP,.65f-60*(.2f/1280),.5f);
        app.zoomPdf(4);app.render();app.zoomPdf(5);app.render();
        assert(app.comicTextureBuilds_==builds); // Neither panning nor zooming decodes/uploads another page.
        std::cout<<"60 high-resolution comic pan frames: "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()<<" ms; UI rasterizations: "<<app.comicTextureBuilds_-builds<<"\n";
        app.pdfPanX_=200;app.pdfPanY_=600;
        touch(app,SDL_FINGERDOWN,.6f,.5f);app.pdfLastMotionTick_=SDL_GetTicks()-16;
        touch(app,SDL_FINGERMOTION,.5f,.5f);touch(app,SDL_FINGERUP,.5f,.5f);
        assert(app.currentPage_==0 && app.panVelocityX_>0);
        app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.panVelocityX_>0);
        const int releasedX=app.pdfPanX_;app.panPhysicsTick_=SDL_GetTicks()-16;app.tickComicPan();assert(app.pdfPanX_>releasedX);
        rotatedControls(app);
        auto portrait=app.appSettings_.get();portrait.displayOrientation=1;app.appSettings_.put(portrait);
        app.setPdfView(0);app.zoomPdf(5);app.panVelocityX_=app.panVelocityY_=0;app.render();
        app.idlePrefetch();app.comicDetailDue_=SDL_GetTicks()-1;app.refineComicZoom();
        const Uint32 detailDeadline=SDL_GetTicks()+5000;
        while(app.comicPages_.pending() && SDL_GetTicks()<detailDeadline){app.processComicPages();SDL_Delay(2);}
        app.processComicPages();
        const auto detailGroup=app.comicSpreadPages(app.currentPage_);
        SDL_RendererInfo rendererInfo{};SDL_GetRendererInfo(app.renderer_,&rendererInfo);
        const int textureLimit=std::min(rendererInfo.max_texture_width>0?rendererInfo.max_texture_width:3072,
                                        rendererInfo.max_texture_height>0?rendererInfo.max_texture_height:3072);
        std::size_t expectedDetails=0,createdDetails=0;
        for(int side=0;side<detailGroup.second;++side) {
            const int page=detailGroup.first+side;float width=0,height=0;
            assert(px_document_page_size(app.doc_,page,&width,&height));
            if(ComicResolution::size(width,height,false,textureLimit)==ComicResolution::size(width,height,true,textureLimit)) {
                assert(!app.comicDetails_.count(page));continue;
            }
            ++expectedDetails;
            auto base=app.pageCache_.get(app.comicTextureKey(page));assert(base);
            if(auto detail=app.comicDetails_.find(page);detail!=app.comicDetails_.end()) {
                ++createdDetails;int baseWidth=0,baseHeight=0,detailWidth=0,detailHeight=0;
                SDL_QueryTexture(base,nullptr,nullptr,&baseWidth,&baseHeight);
                SDL_QueryTexture(detail->second.texture.get(),nullptr,nullptr,&detailWidth,&detailHeight);
                assert(detailWidth>baseWidth && detailHeight>baseHeight);
            } else assert(app.comicDetailRetry_.count(page)); // Keep the base texture when MuPDF's bounded worker cannot allocate detail.
        }
        assert(app.comicDetails_.size()==createdDetails && createdDetails<=expectedDetails);
        portrait.displayOrientation=0;app.appSettings_.put(portrait);app.setPdfView(0);app.zoomPdf(5);
        snapshot(app,"zoom-reset");
        int width,height;app.getWindowSize(width,height);const auto reset=app.zoomResetRect(width,height);
        touch(app,SDL_FINGERDOWN,float(reset.x+reset.w/2)/width,float(reset.y+reset.h/2)/height);
        touch(app,SDL_FINGERUP,float(reset.x+reset.w/2)/width,float(reset.y+reset.h/2)/height);
        assert(app.pdfZoom_==1 && app.pdfPanX_==0 && app.pdfPanY_==0 && app.canCurlPage() && !app.showZoomReset());
        app.closeBook();
        app.openBook(cbz);assert(app.doc_ && app.comic() && app.pageCount_==3 && !app.comicReadingColors());
        assert(app.catalog_.apply(cbz).title=="The Signal — Issue 7");
        assert(app.catalog_.get(cbz.id).sourceSeries=="PaperNX Test Comics");
        assert(app.tocEntries_.size()==2 && app.tocEntries_[0].page==1);
        {
            const auto before=app.appSettings_.get();auto spread=before;
            spread.displayOrientation=0;spread.comicLandscapeSpreads=true;app.appSettings_.put(spread);
            app.currentPage_=0;assert(app.comicSpreadPages(0).second==2);
            app.configureComicPages();
            app.comicPages_.setSuspended(true);app.failComicPage(0,"synthetic spread failure");app.render();
            assert(!app.pageRenderError_.empty()); // A healthy facing page must not clear its failed partner's message.
            app.comicPages_.clear();app.comicPages_.setSuspended(false);app.closeBook();app.appSettings_.put(before);
            app.openBook(cbz);assert(app.comic());
        }
        // Reader themes surround the artwork without changing its colors.
        app.currentTheme_=5;app.extraStyle_.backgroundColor=0;app.pageCache_.clear();
        assert(app.getOrRenderPageTexture(0));snapshot(app,"original-night");
        // Regression Video 1 & 3: outer margin / viewport surround matches theme, NEVER pure white
        {
            app.currentTheme_=2; // Vintage Sepia
            app.extraStyle_.backgroundColor=0;
            app.toolbarVisible_=false;
            app.render();
            int sw, sh; SDL_GetRendererOutputSize(app.renderer_, &sw, &sh);
            auto surf = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_ARGB8888);
            assert(surf && SDL_RenderReadPixels(app.renderer_, nullptr, surf->format->format, surf->pixels, surf->pitch) == 0);
            Uint8 r, g, b;
            auto *p = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(surf->pixels) + 10 * surf->pitch);
            SDL_GetRGB(p[10], surf->format, &r, &g, &b);
            const auto palBg = app.palette().bg;
            assert(std::abs(int(r) - palBg.r) < 5 && std::abs(int(g) - palBg.g) < 5 && std::abs(int(b) - palBg.b) < 5);
            assert(!(r > 240 && g > 240 && b > 240)); // Absolutely never bright white
            SDL_FreeSurface(surf);
        }
        turnComicPage(app,1);assert(app.pageTurnAnimation_.active);finish(app);assert(app.currentPage_==1);
        turnComicPage(app,-1);finish(app);
        touch(app,SDL_FINGERDOWN,.92f,.2f);touch(app,SDL_FINGERMOTION,.55f,.3f);
        if(!app.pageTurnAnimation_.dragging || app.pageTurnAnimation_.anchorY>=.2f || app.currentPage_!=0)
            std::cerr<<"Comic curl state: active="<<app.pageTurnAnimation_.active<<" dragging="<<app.pageTurnAnimation_.dragging
                <<" anchor="<<app.pageTurnAnimation_.anchorY<<" page="<<app.currentPage_<<" spread="<<app.pageTurnAnimation_.spread
                <<" layoutPending="<<app.layoutPending_<<"\n";
        assert(app.pageTurnAnimation_.dragging && app.pageTurnAnimation_.anchorY<.22f && app.currentPage_==0);
        snapshot(app,"touch-curl");touch(app,SDL_FINGERUP,.4f,.3f);finish(app);assert(app.currentPage_==1);
        app.zoomPdf(2);assert(app.canCurlPage());app.handleAnalog(1,1);assert(app.pdfPanX_>=0);
        app.pdfPanY_=150;app.clampPdfPan();
        const auto sheet=app.comicPageRect(app.currentPage_);
        const auto nextSheet=app.comicPageRect(app.currentPage_+1,true);
        turnComicPage(app,1);assert(app.pageTurnAnimation_.active && app.pdfZoom_==2 && app.pdfPanY_==0);
        const auto &turn=app.pageTurnAnimation_;
        const auto visibleSheet=app.visibleComicRect(sheet),visibleNext=app.visibleComicRect(nextSheet);
        assert(turn.comicSheet && turn.viewport.x==visibleSheet.x && turn.viewport.y==visibleSheet.y && turn.viewport.w==visibleSheet.w && turn.viewport.h==visibleSheet.h);
        assert(turn.viewport.h>height-144 && turn.nextViewport.y==visibleNext.y && turn.nextViewport.h==visibleNext.h);
        app.pageTurnAnimation_.started=SDL_GetTicks()-210;
        snapshot(app,"zoomed-curl");finish(app);assert(app.currentPage_==2 && app.pdfZoom_==2 && app.pdfPanY_==0);
        turnComicPage(app,-1);finish(app);turnComicPage(app,-1);finish(app);
        touch(app,SDL_FINGERDOWN,.94f,.4f);touch(app,SDL_FINGERMOTION,.5f,.42f);
        assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.dragging);
        touch(app,SDL_FINGERUP,.5f,.42f);finish(app);assert(app.currentPage_==1 && app.pdfPanY_==0 && app.pdfZoom_==2);
        // Held arrows and both sticks pan continuously; release/deadzone stop.
        app.zoomPdf(5);app.pdfPanX_=0;app.pdfPanY_=0;app.clearHeldControls();
        SDL_Event down{};down.type=SDL_CONTROLLERBUTTONDOWN;down.cbutton.button=SDL_CONTROLLER_BUTTON_DPAD_DOWN;
        bool keepRunning=true;app.handleEvent(down,keepRunning);assert(keepRunning && app.pdfPanY_==0);
        app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanY_>=12 && app.pdfPanY_<=15);
        for(int i=0;i<4;++i) {app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();}
        assert(app.pdfPanY_>=66 && app.pdfPanY_<=70 && app.currentPage_==1);
        down.type=SDL_CONTROLLERBUTTONUP;app.handleEvent(down,keepRunning);
        int stopped=app.pdfPanY_;app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanY_==stopped);
        SDL_Event axis{};axis.type=SDL_CONTROLLERAXISMOTION;axis.caxis.axis=SDL_CONTROLLER_AXIS_LEFTY;axis.caxis.value=32767;
        app.handleEvent(axis,keepRunning);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanY_>stopped);
        axis.caxis.value=0;app.handleEvent(axis,keepRunning);
        axis.caxis.axis=SDL_CONTROLLER_AXIS_RIGHTY;axis.caxis.value=-32767;app.handleEvent(axis,keepRunning);
        stopped=app.pdfPanY_;app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfPanY_<stopped);
        axis.caxis.value=0;app.handleEvent(axis,keepRunning);app.clearHeldControls();app.zoomPdf(2);
        // Stationary triggers keep zooming without more SDL events, at equal speed
        // for equal elapsed time. Neutral, opposing triggers and menus stop zoom.
        const auto zoomBuilds=app.comicTextureBuilds_;app.render();
        auto trigger=[&](int which,int value){axis.caxis.axis=Uint8(which);axis.caxis.value=Sint16(value);app.handleEvent(axis,keepRunning);};
        trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);assert(app.pdfZoom_==2);
        float lastZoom=app.pdfZoom_;
        for(int frame=0;frame<30;++frame){app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfZoom_>lastZoom && app.pdfZoom_-lastZoom<.07f);lastZoom=app.pdfZoom_;app.render();}
        const float halfSecond=app.pdfZoom_;assert(halfSecond>3.3f && halfSecond<3.6f);
        assert(app.comicTextureBuilds_==zoomBuilds);
        trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfZoom_==halfSecond);
        trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,0);
        for(int frame=0;frame<30;++frame){app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();}
        assert(std::abs(app.pdfZoom_-2)<.015f);
        trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT,0);lastZoom=app.pdfZoom_;
        app.controllerInputTick_=SDL_GetTicks()-50;app.tickControllerInput();assert(app.pdfZoom_==lastZoom);
        app.quickMenu_=true;trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfZoom_==lastZoom);
        app.quickMenu_=false;app.clearHeldControls();
        // Tiny zoom steps retain the focal point instead of accumulating pan rounding.
        app.zoomPdf(4);app.pdfPanX_=100;app.pdfPanY_=400;app.clampPdfPan();const auto stable=imagePoint();
        for(int i=0;i<100;++i)app.zoomPdf(app.pdfZoom_*1.001f,.65f,.45f,true);
        const auto stableAfter=imagePoint();assert(std::abs(stable.first-stableAfter.first)<4 && std::abs(stable.second-stableAfter.second)<4);
        trigger(SDL_CONTROLLER_AXIS_TRIGGERRIGHT,32767);app.zoomPdf(5);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfZoom_==5);
        SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;app.handleEvent(lost,keepRunning);
        for(int value:app.stickAxes_)assert(value==0);
        lastZoom=app.pdfZoom_;trigger(SDL_CONTROLLER_AXIS_TRIGGERLEFT,32767);app.controllerInputTick_=SDL_GetTicks()-16;app.tickControllerInput();assert(app.pdfZoom_==lastZoom);
        lost.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;app.handleEvent(lost,keepRunning);assert(app.controllerFocused_);
        app.zoomPdf(2);
        app.extraStyle_.pdfColorMode^=COMIC_READING_COLORS;app.queueStyleChange();assert(app.pdfZoom_==2);
        app.extraStyle_.pdfColorMode^=COMIC_READING_COLORS;app.queueStyleChange();assert(app.pdfZoom_==2);
        app.setPdfView(1);assert(app.pdfZoom_>=1);app.setPdfView(0);assert(app.canCurlPage());
        app.zoomPdf(3);app.render();
        int resetW,resetH;app.getWindowSize(resetW,resetH);const auto button=app.zoomResetRect(resetW,resetH);
        app.handleTap(float(button.x+button.w/2)/resetW,float(button.y+button.h/2)/resetH);
        assert(app.pdfZoom_==1 && app.extraStyle_.pdfViewMode==0 && app.canCurlPage());
        turnComicPage(app,1);assert(app.pageTurnAnimation_.active);finish(app);
        app.openSettings(false,0);assert(app.settingsRows()==1);snapshot(app,"artwork-settings");app.dismissSettings();
        app.extraStyle_.pdfColorMode|=COMIC_READING_COLORS;app.queueStyleChange();
        assert(app.comicReadingColors());snapshot(app,"reader-palette");
        app.extraStyle_.pdfColorMode&=~COMIC_READING_COLORS;app.queueStyleChange();
        app.extraStyle_.readingMode=1;app.extraStyle_.modeOverride=1;app.queueStyleChange();app.dismissSettings();
        assert(app.continuous() && !app.canCurlPage());app.currentPage_=0;app.scrollY_=0;
        const float firstHeight=app.segmentHeight(0);
        app.scrollBy(firstHeight+110);assert(app.currentPage_==1 && std::abs(app.scrollY_-110)<1);
        app.toggleBookmark();const auto bm=app.progressStore_.get(cbz.id).bookmarks.back();
        app.rememberPosition();app.scrollBy(180);app.jumpBack();assert(std::abs(app.scrollY_-110)<1);
        app.scrollBy(180);app.openBookmark(bm);assert(app.currentPage_==1 && std::abs(app.scrollY_-110)<1);
        app.pinPageFlip();app.scrollBy(170);app.returnToPageFlip();assert(std::abs(app.scrollY_-110)<1);
        // The same panel position survives border and orientation changes.
        const double fraction=app.scrollY_/app.segmentHeight(app.currentPage_);
        app.currentMargin_+=10;app.queueStyleChange();assert(std::abs(app.scrollY_/app.segmentHeight(app.currentPage_)-fraction)<.0001);
        auto settings=app.appSettings_.get();settings.displayOrientation=1;app.appSettings_.put(settings);app.setPdfView(0);
        assert(std::abs(app.scrollY_/app.segmentHeight(app.currentPage_)-fraction)<.0001);
        settings.displayOrientation=0;app.appSettings_.put(settings);app.setPdfView(0);
        app.currentMargin_-=10;app.queueStyleChange();assert(std::abs(app.scrollY_-110)<1);
        snapshot(app,"continuous");app.saveCurrentPosition();app.closeBook();app.openBook(cbz);
        assert(app.continuous() && app.currentPage_==1 && std::abs(app.scrollY_-110)<1);
        app.closeBook();app.openBook(rar);assert(app.comic() && !app.continuous() && !app.comicReadingColors());
        app.currentPage_=0;turnComicPage(app,1);finish(app);app.toggleBookmark();app.closeBook();app.openBook(rar);
        assert(app.currentPage_==1 && app.progressStore_.get(rar.id).bookmarks.size()==1);app.closeBook();
        auto favorite=app.progressStore_.get(manga.id);favorite.favorite=true;app.progressStore_.put(manga.id,favorite);
        app.openBook(manga);assert(app.readingDirection()==-1);app.currentPage_=0;
        touch(app,SDL_FINGERDOWN,.1f,.8f);touch(app,SDL_FINGERMOTION,.7f,.65f);
        waitTouchTurn(app);
        assert(app.pageTurnAnimation_.dragging && app.pageTurnAnimation_.direction==-1 && app.pageTurnAnimation_.targetPage==1);
        snapshot(app,"manga-curl");touch(app,SDL_FINGERUP,.7f,.65f);finish(app);assert(app.currentPage_==1);
        turnComicPage(app,1);assert(app.pageTurnAnimation_.direction==-1);finish(app);app.closeBook();
        app.openBook(broken);app.currentPage_=1;assert(!app.getOrRenderPageTexture(1));assert(!app.pageRenderError_.empty());
        turnComicPage(app,1);assert(app.currentPage_==1 && !app.pageTurnAnimation_.active && !app.comicTurnError_.empty());
        app.currentPage_=2;assert(app.getOrRenderPageTexture(2) && app.pageRenderError_.empty());app.closeBook();
        app.browserPath_=root/"external";app.scanBrowserDirectory();assert(app.browserEntries_.size()==1);
        app.handleBrowserSelect();assert(app.comic() && app.books_.size()==7);app.closeBook();
        app.flushProgress();
    }
};
}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS)==0);
    assert(TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-comic-app";fs::remove_all(root);
    fs::create_directories(root/"books");fs::create_directories(root/"external");
    for(const auto &pair:std::vector<std::pair<std::string,std::string>>{{"sample.cbz","sample.cbz"},{"sample-rar4-solid.cbr","sample.cbr"},{"manga.cbz","manga.cbz"},{"broken-page.cbz","broken.cbz"}})
        fs::copy_file("tests/fixtures/comics/"+pair.first,root/"books"/pair.second);
    fs::copy_file("tests/fixtures/comics/large-cover.cbr",root/"books"/"large.cbr");
    fs::copy_file("tests/fixtures/sample_book.epub",root/"books"/"sample.epub");
    fs::copy_file("tests/fixtures/comics/sample-rar5.cbr",root/"external"/"outside.CBR");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();
    std::cout<<"[PASS] Comics library/browser, metadata, curl/manga touch, zoom, colors, continuous anchors, history, bookmarks, persistence and damaged-page recovery\n";
}
