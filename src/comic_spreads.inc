// Only neighbouring headers are inspected. A wide scan already contains two
// printed pages and stays whole, rather than being paired with another scan.
bool comicSpreadActive() {
    int w,h;getWindowSize(w,h);
    return comic() && !continuous() && appSettings_.get().landscapeSpreads && w>h;
}
bool comicWidePage(int page) {
    if(px_document_page_is_spread(doc_,page))return true;
    float w=0,h=0;
    return page>=0 && page<pageCount_ && px_document_page_size(doc_,page,&w,&h) && h>0 && w/h>=1.2f;
}
std::string comicGroupsBook_;
std::map<int,int> comicGroups_;
std::pair<int,int> comicSpreadPages(int page) {
    if(!comicSpreadActive())return {page,1};
    if(page<0 || page>=pageCount_)return {page,0};
    if(comicGroupsBook_!=currentBookId_){comicGroupsBook_=currentBookId_;comicGroups_.clear();}
    auto next=comicGroups_.upper_bound(page);
    if(next!=comicGroups_.begin()) {
        const auto previous=std::prev(next);
        if(page<previous->first+previous->second)return *previous;
    }
    // Cache visited pair boundaries. Forward reading resumes pairing directly
    // after a wide sheet. A distant bookmark needs only its neighbours, never
    // decompression of all preceding entries in a solid RAR archive.
    const int count=page+1>=pageCount_ || (next!=comicGroups_.end() && next->first==page+1) ||
        comicWidePage(page) || comicWidePage(page+1)?1:2;
    comicGroups_[page]=count;return {page,count};
}
int comicTurnTarget(int page,int steps) {
    auto group=comicSpreadPages(page);int target=group.first;
    const int direction=steps>0?1:-1;
    for(int i=0;i<std::abs(steps);++i) {
        int candidate=direction>0?group.first+group.second:group.first-1;
        if(direction<0) {
            auto previous=comicGroups_.lower_bound(group.first);
            if(previous!=comicGroups_.begin() && std::prev(previous)->first+std::prev(previous)->second==group.first)
                candidate=std::prev(previous)->first;
            else if(group.first>=2 && !comicWidePage(group.first-1) && !comicWidePage(group.first-2))candidate=group.first-2;
        }
        if(candidate<0 || candidate>=pageCount_)break;
        group=comicSpreadPages(candidate);target=group.first;
    }
    return target;
}
int comicViewportHeight(int h) const {return std::max(1,h-72-(appSettings_.get().expandedViewport?44:72));}
SDL_Rect visibleComicRect(SDL_Rect rect) {
    int w,h;getWindowSize(w,h);const SDL_Rect viewport{currentMargin_,72,fixedViewportWidth(w),comicViewportHeight(h)};
    SDL_Rect visible{};return SDL_IntersectRect(&rect,&viewport,&visible)?visible:SDL_Rect{rect.x,rect.y,0,0};
}
SDL_FRect comicLeafArtwork(int page,SDL_Rect leaf) {
    float pw=0,ph=0;if(!px_document_page_size(doc_,page,&pw,&ph) || pw<=0 || ph<=0)return {};
    const auto group=comicSpreadPages(page);
    const bool pair=comicSpreadActive() && group.second==2;
    const bool left=pair && page==readerSidePage(group.first,0);
    const int pad=std::min(currentMargin_,std::max(0,leaf.w-2));
    const float areaW=float(std::max(1,leaf.w-(pair?pad+1:pad*2)));
    const float areaH=float(comicViewportHeight(leaf.h));
    const float fit=std::min(areaW/pw,areaH/ph),width=pw*fit,height=ph*fit;
    const float x=pair?(left?leaf.x+leaf.w-1-width:leaf.x+1):leaf.x+(leaf.w-width)/2;
    return {x,leaf.y+72+(areaH-height)/2,width,height};
}
void drawComicLeaf(int page,SDL_Rect leaf) {
    auto *texture=comicZoomTexture(page);if(!texture)return;
    const auto rectangle=comicLeafArtwork(page,leaf);
    SDL_RenderCopyF(renderer_,texture,nullptr,&rectangle);
    const auto group=comicSpreadPages(page);
    if(comicSpreadActive() && group.second==2) {
        const bool left=page==readerSidePage(group.first,0);
        const int seam=left?leaf.x+leaf.w-1:leaf.x;
        const auto pal=palette();SDL_SetRenderDrawColor(renderer_,pal.cardBorder.r,pal.cardBorder.g,pal.cardBorder.b,255);
        SDL_RenderDrawLine(renderer_,seam,int(rectangle.y),seam,int(rectangle.y+rectangle.h)-1);
    }
}
void renderComicSpreadZoom(int w,int h) {
    const auto layout=readerLayout();const auto group=comicSpreadPages(currentPage_);
    SDL_Rect clip{currentMargin_,72,fixedViewportWidth(w),comicViewportHeight(h)};
    SDL_RenderSetClipRect(renderer_,&clip);
    const float originX=std::max(0.f,clip.w*(1-pdfZoom_)/2),originY=std::max(0.f,clip.h*(1-pdfZoom_)/2);
    for(int side=0;side<group.second;++side) {
        const int page=group.second==2?readerSidePage(group.first,side):group.first;
        auto texture=comicZoomTexture(page);if(!texture)continue;
        auto rect=comicLeafArtwork(page,layout.leaf(side));
        rect={clip.x+originX+(rect.x-clip.x)*pdfZoom_-pdfPanX_,clip.y+originY+(rect.y-clip.y)*pdfZoom_-pdfPanY_,rect.w*pdfZoom_,rect.h*pdfZoom_};
        SDL_RenderCopyF(renderer_,texture,nullptr,&rect);
    }
    if(group.second==2) {
        const float seam=clip.x+originX+(w/2.f-clip.x)*pdfZoom_-pdfPanX_;
        const auto pal=palette();SDL_SetRenderDrawColor(renderer_,pal.cardBorder.r,pal.cardBorder.g,pal.cardBorder.b,255);
        for(int side=0;side<2;++side) {
            const auto r=comicArtworkRect(currentPage_,side);
            SDL_RenderDrawLineF(renderer_,seam+(side?0:-1),float(r.y),seam+(side?0:-1),float(r.y+r.h-1));
        }
    }
    SDL_RenderSetClipRect(renderer_,nullptr);
}
SDL_Rect zoomTopRect(int w,int h) const {return {w-80,h-136,56,56};}
void returnComicToTop() {
    cancelPageTurn();pdfPanY_=0;panVelocityX_=panVelocityY_=0;panInertiaY_=0;
    zoomFractionY_=0;zoomLastPage_=-1;
    gestureTracker_.reset();dirty_=true;
}
void drawZoomTop(int w,int h,const ThemePalette &pal) {
    const auto r=zoomTopRect(w,h);ui::button(renderer_,*typography_,r,"",pal);
    SDL_SetRenderDrawColor(renderer_,pal.fg.r,pal.fg.g,pal.fg.b,255);
    const int x=r.x+r.w/2,y=r.y+19;
    SDL_RenderDrawLine(renderer_,x-9,y+8,x,y);SDL_RenderDrawLine(renderer_,x,y,x+9,y+8);
    SDL_RenderDrawLine(renderer_,x,y,x,y+22);
}

// Artwork bounds are independent of the fullscreen interaction area. The same
// geometry drives resting pages, touch contact and the animated physical leaf.
SDL_Rect comicArtworkRect(int base,int side,bool head=false) {
    auto layout=readerLayout();const auto group=comicSpreadPages(base);
    layout.spread=comicSpreadActive() && group.second==2;
    const int page=layout.spread?readerSidePage(group.first,side):group.first;
    if(!layout.spread && std::abs(pdfZoom_-1.f)>.0001f && !comicSpreadActive())return comicPageRect(page,head);
    auto r=comicLeafArtwork(page,layout.leaf(side));
    if(std::abs(pdfZoom_-1.f)>.0001f) {
        const float vw=fixedViewportWidth(layout.width),vh=comicViewportHeight(layout.height);
        const float panX=head?(readingDirection()<0?std::max(0.f,vw*(pdfZoom_-1)):0):float(pdfPanX_);
        r={currentMargin_+std::max(0.f,vw*(1-pdfZoom_)/2)+(r.x-currentMargin_)*pdfZoom_-panX,
           72+std::max(0.f,vh*(1-pdfZoom_)/2)+(r.y-72)*pdfZoom_-(head?0:pdfPanY_),r.w*pdfZoom_,r.h*pdfZoom_};
    }
    return {int(std::lround(r.x)),int(std::lround(r.y)),std::max(1,int(std::lround(r.w))),std::max(1,int(std::lround(r.h)))};
}
SDL_Rect comicTurnBounds(int base,bool head=false) {
    auto r=comicArtworkRect(base,0,head);
    if(comicSpreadActive() && comicSpreadPages(base).second==2) {
        auto other=comicArtworkRect(base,1,head);SDL_Rect joined{};SDL_UnionRect(&r,&other,&joined);r=joined;
    }
    return visibleComicRect(r);
}
bool comicCanGrip(float x,float y) {
    int w,h;getWindowSize(w,h);const auto r=comicTurnBounds(currentPage_);
    // Outside the paper is always a paging zone. Within an enlarged sheet,
    // preserve panning; only its outer grip or an exhausted horizontal pan turns.
    return x*w<r.x || x*w>=r.x+r.w || y*h<r.y || y*h>=r.y+r.h ||
        x<.12f || x>.88f || (pdfPanX_==0 && x>.5f) ||
        (pdfPanX_>=std::max(0,int(fixedViewportWidth(w)*(pdfZoom_-1))) && x<.5f);
}
