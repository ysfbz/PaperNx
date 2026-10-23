// Open-menu summaries occupy the chrome strips. Hidden-menu folios are drawn
// on each physical leaf by reader_sheets.inc.
std::string bookPageKey_;
std::vector<int> bookPagePrefix_;

SDL_Rect readerHeaderButton(int index,int w) const {
    const int x[]={16,80,w-216,w-152};return {x[index],8,56,56};
}
void drawBookHeaderButton(int index,int w,const ThemePalette &pal) {
    const ui::Icon glyphs[]={ui::Icon::Back,ui::Icon::Books,ui::Icon::Bookmark,ui::Icon::Type};
    const auto r=readerHeaderButton(index,w);ui::iconButton(renderer_,*typography_,r,glyphs[index],pal);
}

void cacheBookPageCounts(int w,int h,int through) {
    // A continuous EPUB uses one MuPDF page per spine item. Count screen-sized
    // reading pages instead of incorrectly displaying the chapter number.
    const auto key=currentBookId_+":"+std::to_string(w)+":"+std::to_string(h)+":"+
        std::to_string(layoutGeneration_)+":"+buildEpubCss();
    if(key!=bookPageKey_){bookPageKey_=key;bookPagePrefix_={0};}
    const int height=std::max(1,h-144);
    through=std::clamp(through,0,pageCount_);
    while(int(bookPagePrefix_.size())<=through) {
        const int page=int(bookPagePrefix_.size())-1;
        const int count=std::max(1,int(std::ceil(segmentHeight(page)/height)));
        bookPagePrefix_.push_back(bookPagePrefix_.back()+count);
    }
}

int readerPageNumber(int w,int h) {
    if(!continuous() || !px_document_is_epub(doc_))return currentPage_+1;
    cacheBookPageCounts(w,h,currentPage_+1);
    const int height=std::max(1,h-144);
    const int lastWithin=bookPagePrefix_[currentPage_+1]-bookPagePrefix_[currentPage_]-1;
    // The last partial screen cannot scroll to a full viewport boundary.
    const bool atEnd=currentPage_+1==pageCount_ && scrollY_>=std::max(0.f,segmentHeight(currentPage_)-height)-.5f;
    const int within=atEnd?lastWithin:std::clamp(int(std::max(0.f,scrollY_)/height),0,lastWithin);
    return bookPagePrefix_[currentPage_]+within+1;
}

int readerPageCount(int w,int h) {
    if(!continuous() || !px_document_is_epub(doc_))return std::max(1,pageCount_);
    cacheBookPageCounts(w,h,pageCount_);
    return std::max(1,bookPagePrefix_.back());
}

std::string readerFooterCaption(int w,int h) {
    const int page=readerPageNumber(w,h);
    if(!toolbarVisible_ && !appSettings_.get().toolbar)return std::to_string(page);
    const int total=readerPageCount(w,h);
    const bool spread=readerLayout().spread;
    const int first=spread?(comicSpreadActive()?comicSpreadPages(currentPage_).first+1:currentPage_/2*2+1):page;
    const int last=spread?std::min(total,first+1):page;
    const int percent=std::clamp(int(std::lround(100.0*last/total)),0,100);
    const auto pages=std::to_string(first)+(last>first?"–"+std::to_string(last):"");
    return pages+ui::label(" of ")+std::to_string(total)+" • "+std::to_string(percent)+"%";
}

int bookChapterPagesLeft(int h) {
    if(!doc_ || comic())return -1;
    const int current=currentContentsChapter();
    int end=pageCount_;float endY=0;
    bool found=false;
    if(current>=0) {
        const auto &chapter=tocEntries_[current];
        for(const auto &next:tocEntries_) {
            if(next.page<0 || next.level>chapter.level)continue;
            const bool after=next.page>chapter.page || (continuous() && next.page==chapter.page && next.y>chapter.y+1);
            if(!after)continue;
            if(!found || next.page<end || (next.page==end && next.y<endY)) {end=next.page;endY=next.y;found=true;}
        }
    } else if(px_document_is_epub(doc_)) {
        int chapter=0,page=0;
        if(!px_document_get_location(doc_,currentPage_,&chapter,&page))return -1;
        end=currentPage_+px_document_chapter_page_count(doc_,chapter)-page;
        // In continuous EPUB a spine item is an unpaginated chapter strip.
        if(continuous())end=currentPage_+1;
    } else return -1; // A PDF without an outline has no known chapter boundary.
    if(!continuous())return std::max(0,end-currentPage_-1);
    float remaining=-scrollY_;
    for(int page=currentPage_;page<std::min(end,pageCount_);++page)remaining+=segmentHeight(page);
    if(end<pageCount_)remaining+=screenScrollY(end,endY);
    return std::max(0,int(std::ceil(remaining/std::max(1,h-144)))-1);
}

void drawReaderCaption(const std::string &text,SDL_Rect area,const ThemePalette &pal) {
    int width=0,height=0;
    typography_->measureText(typography_->smallFont(),text,&width,&height,area.w);
    SDL_RenderSetClipRect(renderer_,&area);
    typography_->renderText(renderer_,typography_->smallFont(),text,
        area.x+(area.w-width)/2,area.y+(area.h-height)/2,pal.muted,area.w);
    SDL_RenderSetClipRect(renderer_,nullptr);
}

std::string bookChapterCaption(int h) {
    const int left=bookChapterPagesLeft(h);
    if(left<0)return {};
    if(left==0)return ui::label("Last page in chapter");
    return std::to_string(left)+ui::label(left==1?" page left in chapter":" pages left in chapter");
}
