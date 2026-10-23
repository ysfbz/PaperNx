// Sheet rendering is shared by resting pages and animation snapshots. Folios
// belong to the paper, never to a fixed overlay (except the open-menu summary).
void drawReaderLeaf(int page,SDL_Rect leaf) {
    if(page<0 || page>=pageCount_)return; // Blank facing leaf at the end.
    auto texture=comic()?nullptr:getOrRenderPageTexture(page);
    if(comic())drawComicLeaf(page,leaf);
    if(texture) {
        int tw=0,th=0;SDL_QueryTexture(texture,nullptr,nullptr,&tw,&th);
        const auto layout=readerLayout();
        const int inset=layout.contentInset();
        SDL_Rect content{leaf.x+inset,leaf.y+ReaderLayout::top,
            std::max(1,leaf.w-inset*2),layout.contentHeight()};
        SDL_Rect destination{content.x+(content.w-tw)/2,content.y+(content.h-th)/2,tw,th};
        SDL_Rect oldClip{};const bool clipped=SDL_RenderIsClipEnabled(renderer_);
        SDL_RenderGetClipRect(renderer_,&oldClip);
        SDL_RenderSetClipRect(renderer_,&content);
        SDL_RenderCopy(renderer_,texture,nullptr,&destination);
        SDL_RenderSetClipRect(renderer_,clipped?&oldClip:nullptr);
    }
    drawReaderCaption(std::to_string(page+1),
        {leaf.x+16,leaf.y+leaf.h-readerLayout().footerHeight(),std::max(1,leaf.w-32),readerLayout().footerHeight()},palette());
}

int readerSidePage(int base,int side) const {
    return base+(readingDirection()<0?1-side:side);
}
void drawReaderSheets(int page) {
    auto layout=readerLayout();
    if(comicSpreadActive())layout.spread=comicSpreadPages(page).second==2;
    if(!layout.spread){drawReaderLeaf(page,layout.leaf(0));return;}
    const int base=comicSpreadActive()?comicSpreadPages(page).first:page/2*2;
    for(int side=0;side<2;++side)drawReaderLeaf(readerSidePage(base,side),layout.leaf(side));
}

template<class Draw>
PageTurnTexture captureReaderSurface(int width,int height,Draw draw,bool transparent=false,bool paperBack=false) {
    PageTurnTexture result{nullptr,SDL_DestroyTexture};
    if(!SDL_RenderTargetSupported(renderer_))return result;
    result.reset(SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height));
    if(!result)return result;
    auto *oldTarget=SDL_GetRenderTarget(renderer_);
    SDL_Rect viewport{},clip{};SDL_RenderGetViewport(renderer_,&viewport);SDL_RenderGetClipRect(renderer_,&clip);
    const bool clipped=SDL_RenderIsClipEnabled(renderer_);
    float sx=1,sy=1;SDL_RenderGetScale(renderer_,&sx,&sy);
    Uint8 r,g,b,a;SDL_GetRenderDrawColor(renderer_,&r,&g,&b,&a);
    if(SDL_SetRenderTarget(renderer_,result.get())!=0){result.reset();return result;}
    SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetViewport(renderer_,nullptr);SDL_RenderSetClipRect(renderer_,nullptr);
    const auto paper=paperBack?comicPaperBackColor():palette().bg;SDL_SetRenderDrawColor(renderer_,paper.r,paper.g,paper.b,transparent?0:255);
    SDL_RenderClear(renderer_);draw();
    SDL_SetRenderTarget(renderer_,oldTarget);SDL_RenderSetScale(renderer_,sx,sy);
    SDL_RenderSetViewport(renderer_,&viewport);SDL_RenderSetClipRect(renderer_,clipped?&clip:nullptr);
    SDL_SetRenderDrawColor(renderer_,r,g,b,a);
    if(transparent)SDL_SetTextureBlendMode(result.get(),SDL_BLENDMODE_BLEND);
    return result;
}

PageTurnTexture captureComicGroup(int base,SDL_Rect bounds,bool head=false,bool paperBack=false) {
    // A naturally wide scan stays one whole sheet. Pair/single transitions bend
    // its paper rectangle, never a fullscreen image containing empty margins.
    const float scale=std::min({1.f,3072.f/std::max(1,bounds.w),3072.f/std::max(1,bounds.h)});
    bool copied=true;
    auto result=captureReaderSurface(std::max(1,int(bounds.w*scale)),std::max(1,int(bounds.h*scale)),[&]{
        const auto group=comicSpreadPages(base);
        const int count=comicSpreadActive()?group.second:1;
        for(int side=0;side<count;++side) {
            const int page=count==2?readerSidePage(group.first,side):group.first;
            auto texture=comicZoomTexture(page);if(!texture){copied=false;continue;}
            const auto r=comicArtworkRect(base,side,head);
            SDL_FRect dst{(r.x-bounds.x)*scale,(r.y-bounds.y)*scale,r.w*scale,r.h*scale};
            if(copyComicTexture(texture,dst,paperBack)!=0)copied=false;
        }
    },false,paperBack);
    if(!copied){comicTurnError_=catLabel("Sayfa dönüşü hazırlanamadı","Could not prepare comic turn");result.reset();}
    return result;
}
std::string comicTurnFolioCaption(int page) {
    const auto group=comicSpreadPages(page);
    const int first=group.first+1,last=std::min(pageCount_,group.first+group.second);
    const std::string range=last>first?std::to_string(first)+"–"+std::to_string(last):std::to_string(first);
    return range+" / "+std::to_string(pageCount_);
}
bool prepareComicTurn(int from,int target,int direction) {
    auto &turn=pageTurnAnimation_;const auto layout=readerLayout();
    const auto oldGroup=comicSpreadPages(from),newGroup=comicSpreadPages(target);
    turn.comicSheet=true;turn.spread=comicSpreadActive() && oldGroup.second==2 && newGroup.second==2;
    turn.comicFolioCaption=comicTurnFolioCaption(from);
    const int side=direction>0?1:0;
    if(turn.spread) {
        turn.viewport=visibleComicRect(comicArtworkRect(from,side));
        turn.oppositeViewport=visibleComicRect(comicArtworkRect(target,1-side,true));
        const int frontPage=readerSidePage(oldGroup.first,side),backPage=readerSidePage(newGroup.first,1-side);
        turn.previous=captureTurnPage(frontPage,false,turn.viewport);
        turn.back=captureTurnPage(backPage,false,turn.oppositeViewport);
        turn.paperBack=captureTurnPage(frontPage,true,turn.viewport);
        turn.under=captureReaderSurface(layout.width,layout.height,[&]{
            for(auto pair:{std::pair<int,int>{from,1-side},{target,side}}) {
                const auto group=comicSpreadPages(pair.first);const auto r=comicArtworkRect(pair.first,pair.second,pair.first==target);
                if(auto texture=comicZoomTexture(readerSidePage(group.first,pair.second)))SDL_RenderCopy(renderer_,texture,nullptr,&r);
            }
        });
        return turn.previous && turn.back && turn.paperBack && turn.under;
    }
    turn.viewport=comicTurnBounds(from);turn.nextViewport=comicTurnBounds(target,true);
    turn.previous=captureComicGroup(from,turn.viewport);
    turn.next=captureComicGroup(target,turn.nextViewport,true);
    turn.back=captureComicGroup(from,turn.viewport,false,true);
    return turn.previous && turn.next && turn.back;
}

bool prepareReaderTurn(int from,int target,int direction) {
    if(comic())return prepareComicTurn(from,target,direction);
    auto &turn=pageTurnAnimation_;
    const auto layout=readerLayout();
    turn.spread=layout.spread;
    if(layout.spread) {
        const int side=direction>0?1:0;
        const int oldBase=comicSpreadActive()?comicSpreadPages(from).first:from/2*2;
        const int newBase=comicSpreadActive()?comicSpreadPages(target).first:target/2*2;
        const auto leaf=layout.leaf(side),opposite=layout.leaf(1-side);
        turn.previous=captureReaderSurface(leaf.w,leaf.h,[&]{drawReaderLeaf(readerSidePage(oldBase,side),{0,0,leaf.w,leaf.h});});
        turn.back=captureReaderSurface(opposite.w,opposite.h,[&]{drawReaderLeaf(readerSidePage(newBase,1-side),{0,0,opposite.w,opposite.h});});
        turn.under=captureReaderSurface(layout.width,layout.height,[&]{
            drawReaderLeaf(readerSidePage(oldBase,1-side),opposite);
            drawReaderLeaf(readerSidePage(newBase,side),leaf);
        });
        turn.viewport=leaf;
        return turn.previous && turn.back && turn.under;
    }
    turn.viewport=turnCanvas(target);
    turn.comicSheet=comic() && std::abs(pdfZoom_-1.f)>.01f;
    turn.nextViewport=turn.comicSheet?comicPageRect(target,true):turn.viewport;
    turn.previous=captureTurnPage(from,false,turn.viewport);
    turn.back=turn.previous?captureTurnPage(from,true,turn.viewport):PageTurnTexture{nullptr,SDL_DestroyTexture};
    turn.next=turn.previous?captureTurnPage(target,false,turn.viewport):PageTurnTexture{nullptr,SDL_DestroyTexture};
    return turn.previous && turn.next;
}
