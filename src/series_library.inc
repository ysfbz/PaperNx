// Series shelves contain real Book records. Covers, saved positions and actions
// continue to use each issue's existing identity; no synthetic books are stored.
struct SeriesShelf {std::string key,title;std::vector<Book> issues;};
std::map<std::string,SeriesShelf> seriesShelves_;
std::string openSeriesKey_,openSeriesTitle_,seriesSeedId_;
float seriesHomeScroll_{0};int seriesHomeSelection_{0};
void arrangeSeries() {
    seriesShelves_.clear();
    if(!openSeriesKey_.empty()) {
        std::sort(visibleBooks_.begin(),visibleBooks_.end(),[&](const Book &a,const Book &b){return seriesIssueLess(a,b,catalog_);});
        return;
    }
    // A search, Continue Reading or Favorites opens its matching issue directly.
    if(catalogFilter_ && *catalogFilter_==CatalogGroup::Series) {
        std::sort(visibleBooks_.begin(),visibleBooks_.end(),[&](const Book &a,const Book &b){return seriesIssueLess(a,b,catalog_);});return;
    }
    if(category_!=Category::All || !searchQuery_.empty() || catalogFilter_)return;
    std::map<std::string,SeriesShelf> groups;
    std::vector<Book> singles;
    for(const auto &book:visibleBooks_) {
        auto identity=identifySeries(book,catalog_.get(book.id));
        if(identity.key.empty()) {singles.push_back(book);continue;}
        auto &shelf=groups[identity.key];shelf.key=identity.key;shelf.title=identity.title;shelf.issues.push_back(book);
    }
    for(auto &entry:groups) {
        auto &shelf=entry.second;
        std::sort(shelf.issues.begin(),shelf.issues.end(),[&](const Book &a,const Book &b){return seriesIssueLess(a,b,catalog_);});
        const Book first=shelf.issues.front();singles.push_back(first);
        if(shelf.issues.size()>1)seriesShelves_.emplace(first.id,std::move(shelf));
    }
    visibleBooks_=searchEx(singles,progressStore_,"",sort_,Category::All);
    if(sort_==Sort::Title)std::sort(visibleBooks_.begin(),visibleBooks_.end(),[&](const Book &a,const Book &b){
        int n=naturalCompare(libraryItemTitle(a),libraryItemTitle(b));return n?n<0:a.id<b.id;});
    if(sort_==Sort::LastRead || sort_==Sort::Modified) {
        auto newest=[&](const Book &book) {
            auto value=sort_==Sort::LastRead?progressStore_.get(book.id).lastRead:book.modified;
            auto shelf=seriesShelves_.find(book.id);
            if(shelf!=seriesShelves_.end())for(const auto &issue:shelf->second.issues)
                value=std::max(value,sort_==Sort::LastRead?progressStore_.get(issue.id).lastRead:issue.modified);
            return value;
        };
        std::sort(visibleBooks_.begin(),visibleBooks_.end(),[&](const Book &a,const Book &b){auto x=newest(a),y=newest(b);return x==y?a.id<b.id:x>y;});
    }
}
std::string libraryItemTitle(const Book &book) const {
    auto shelf=seriesShelves_.find(book.id);return shelf==seriesShelves_.end()?book.title:shelf->second.title;
}
std::string libraryItemSubtitle(const Book &book) const {
    auto shelf=seriesShelves_.find(book.id);
    if(shelf==seriesShelves_.end())return book.author;
    return std::to_string(shelf->second.issues.size())+catLabel(" sayı · Seri"," issues · Series");
}
float libraryItemFraction(const Book &book) const {
    auto shelf=seriesShelves_.find(book.id);
    if(shelf==seriesShelves_.end())return float(progressStore_.get(book.id).fraction);
    double total=0;for(const auto &issue:shelf->second.issues)total+=progressStore_.get(issue.id).fraction;
    return float(total/shelf->second.issues.size());
}
bool openSeriesShelf(const Book &book) {
    auto found=seriesShelves_.find(book.id);if(found==seriesShelves_.end())return false;
    seriesHomeScroll_=listScroll_;seriesHomeSelection_=selectedBookIdx_;
    openSeriesKey_=found->second.key;openSeriesTitle_=found->second.title;seriesSeedId_=found->second.issues.front().id;
    selectedBookIdx_=0;updateVisibleBooks();rebuildFocus();navigationFocus_.select("book.0");return true;
}
void openLibraryItem(int index) {
    if(index<0 || index>=int(visibleBooks_.size()))return;
    Book book=visibleBooks_[index];
    if(!openSeriesShelf(book))openBook(book);
}
bool closeSeriesShelf() {
    if(openSeriesKey_.empty())return false;
    openSeriesKey_.clear();openSeriesTitle_.clear();seriesSeedId_.clear();
    updateVisibleBooks();selectedBookIdx_=std::clamp(seriesHomeSelection_,0,std::max(0,int(visibleBooks_.size())-1));
    listScroll_=std::clamp(seriesHomeScroll_,0.f,maxUiScroll());rebuildFocus();navigationFocus_.select("book."+std::to_string(selectedBookIdx_));return true;
}
