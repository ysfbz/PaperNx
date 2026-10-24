LibraryScanner libraryScanner_;
bool libraryScanAgain_{false};
std::unordered_set<std::string> scanKnownIds_;
std::unordered_map<std::string,std::size_t> libraryLookup_;
unsigned completedLibraryScans_{0};

void rebuildLibraryLookup() {
    libraryLookup_.clear();libraryLookup_.reserve(books_.size());
    for(std::size_t i=0;i<books_.size();++i)libraryLookup_[books_[i].id]=i;
}
void restoreLibraryLabels(bool discoverProgressFiles=true) {
    rebuildLibraryLookup();
    for(const auto &[id,p]:progressStore_.all()) {
        const auto found=libraryLookup_.find(id);
        if(found!=libraryLookup_.end()) {
            auto &book=books_[found->second];
            if(!p.bookTitle.empty())book.title=p.bookTitle;
            if(!p.bookAuthor.empty())book.author=p.bookAuthor;
        } else if(discoverProgressFiles && !p.bookPath.empty() && books_.size()<10000) {
            std::error_code ec;const auto path=fs::u8path(p.bookPath);const auto size=fs::file_size(path,ec);
            if(ec || detectFormat(path)==Format::Unsupported || documentId(p.bookPath,size)!=id)continue;
            Book book;book.id=id;book.path=p.bookPath;book.bytes=size;book.format=detectFormat(path);
            book.title=p.bookTitle.empty()?defaultTitle(path):p.bookTitle;book.author=p.bookAuthor;
            libraryLookup_[id]=books_.size();books_.push_back(std::move(book));
        }
    }
}
void startLibraryScan() {
    if(libraryScanner_.pending()){libraryScanAgain_=true;libraryScanner_.cancel();dirty_=true;return;}
    std::vector<fs::path> roots{root_/"books"};
    for(const auto &folder:appSettings_.get().libraryFolders)roots.push_back(fs::u8path(folder));
    scanKnownIds_.clear();for(const auto &book:books_)scanKnownIds_.insert(book.id);
    auto known=books_;
    for(const auto &[id,p]:progressStore_.all()) {
        if(known.size()>=10000)break;
        if(p.bookPath.empty() || id.size()!=16 || !std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))continue;
        const auto path=fs::u8path(p.bookPath);const auto format=detectFormat(path);
        if(format==Format::Unsupported)continue;
        Book book;book.id=id;book.path=p.bookPath;book.title=p.bookTitle;book.author=p.bookAuthor;book.format=format;
        known.push_back(std::move(book));
    }
    libraryScanner_.start(std::move(roots),std::move(known),root_/"data"/"library-index.txt");dirty_=true;
}
void processLibraryScan() {
    if(!activeFingers_.empty() || pointerActive_ || !libraryScanner_.ready())return;
    auto result=libraryScanner_.take();if(!result)return;
    if(result->cancelled) {
        dirty_=true;
        if(libraryScanAgain_){libraryScanAgain_=false;startLibraryScan();}
        return;
    }
    if(!result->complete) {
        if(!result->error.empty())currentErrorMessage_=ui::label(result->error);
        dirty_=true;
        if(libraryScanAgain_){libraryScanAgain_=false;startLibraryScan();}
        return;
    }
    std::string selected;
    if(selectedBookIdx_>=0 && selectedBookIdx_<int(visibleBooks_.size()))selected=visibleBooks_[selectedBookIdx_].id;
    std::unordered_map<std::string,const Book*> current;current.reserve(books_.size());
    for(const auto &book:books_)current.emplace(book.id,&book);
    std::unordered_set<std::string> seen;
    for(auto &book:result->books) {
        seen.insert(book.id);const auto found=current.find(book.id);
        if(found!=current.end()){book.title=found->second->title;book.author=found->second->author;}
    }
    // A file opened while the worker was running must survive its older batch.
    for(const auto &book:books_)if(!scanKnownIds_.count(book.id) && !seen.count(book.id) && result->books.size()<10000)result->books.push_back(book);
    books_=std::move(result->books);restoreLibraryLabels(false);
    if(!result->error.empty())currentErrorMessage_=ui::label(result->error);
    scheduleMetadataIndex();updateVisibleBooks();
    for(int i=0;i<int(visibleBooks_.size());++i)if(visibleBooks_[i].id==selected){selectedBookIdx_=i;break;}
    listScroll_=std::clamp(listScroll_,0.f,maxUiScroll());++completedLibraryScans_;dirty_=true;
    if(libraryScanAgain_){libraryScanAgain_=false;startLibraryScan();}
}
SDL_Rect browserFolderButton(int w) const {return {368,16,std::max(160,w-464),56};}
void addBrowserFolder() {
    std::error_code ec;auto folder=fs::weakly_canonical(browserPath_,ec);
    if(ec || !fs::is_directory(folder,ec))return;
    auto preferences=appSettings_.get();const auto value=folder.u8string();
    if(std::find(preferences.libraryFolders.begin(),preferences.libraryFolders.end(),value)==preferences.libraryFolders.end()) {
        if(preferences.libraryFolders.size()>=32){currentErrorMessage_=ui::label("Folder limit reached");dirty_=true;return;}
        preferences.libraryFolders.push_back(value);
    }
    preferences.defaultBrowserPath=value;appSettings_.put(std::move(preferences));persistAppSettings();
    screen_=ScreenState::Library;quickMenu_=false;catalogPanel_=CatalogPanel::None;listScroll_=0;startLibraryScan();dirty_=true;
}
