LibraryScanner libraryScanner_;
bool libraryScanAgain_{false};
std::unordered_set<std::string> scanKnownIds_;
std::unordered_map<std::string,std::size_t> libraryLookup_;
unsigned completedLibraryScans_{0};
std::uint64_t libraryScanGeneration_{0}; // only the result of this request may replace books_
std::optional<LibraryScanResult> lastScanResult_;
std::string lastScanSummary_;
bool currentErrorIsFromScan_{false};

std::string formatScanSummary(const LibraryScanResult &res) const {
    const bool en = (ui::language == 1);
    std::string msg = std::to_string(res.books.size()) + (en ? " books found" : " kitap bulundu");
    if (res.addedCount > 0) {
        msg += " · " + std::to_string(res.addedCount) + (en ? " new added" : " yeni kitap eklendi");
    }
    if (res.updatedCount > 0) {
        msg += " · " + std::to_string(res.updatedCount) + (en ? " updated" : " güncellendi");
    }
    std::size_t missingRoots = 0;
    for (const auto &rs : res.rootStatuses) {
        if (!rs.available) ++missingRoots;
    }
    if (missingRoots > 0) {
        msg += " · " + std::to_string(missingRoots) + (en ? " folder(s) unavailable" : " klasöre erişilemedi");
    }
    const std::size_t dirErrors = res.inaccessibleDirs.size() > missingRoots
        ? (res.inaccessibleDirs.size() - missingRoots) : 0;
    if (dirErrors > 0) {
        msg += " · " + std::to_string(dirErrors) + (en ? " folder error(s)" : " klasör hatası");
    }
    if (!res.unreadableFiles.empty()) {
        msg += " · " + std::to_string(res.unreadableFiles.size()) +
            (en ? (res.unreadableFiles.size() == 1 ? " file unreadable" : " files unreadable")
                : " dosya okunamadı");
    }
    if (res.status == ScanStatus::InventoryWriteFailed) {
        msg += en ? " · Cache save failed" : " · Önbellek kaydedilemedi";
    } else if (res.truncatedByLimit) {
        msg += en ? " · Scan limit reached" : " · Tarama sınırına ulaşıldı";
    }
    if (res.status == ScanStatus::Failed && !res.error.empty()) {
        msg = ui::label(res.error);
    }
    return msg;
}

void rebuildLibraryLookup() {
    libraryLookup_.clear();
    libraryLookup_.reserve(books_.size());
    for (std::size_t i = 0; i < books_.size(); ++i) libraryLookup_[books_[i].id] = i;
}

void restoreLibraryLabels(bool discoverProgressFiles = true) {
    rebuildLibraryLookup();
    for (const auto &[id, p] : progressStore_.all()) {
        const auto found = libraryLookup_.find(id);
        if (found != libraryLookup_.end()) {
            auto &book = books_[found->second];
            if (!p.bookTitle.empty()) book.title = p.bookTitle;
            if (!p.bookAuthor.empty()) book.author = p.bookAuthor;
        } else if (discoverProgressFiles && !p.bookPath.empty() && books_.size() < 10000) {
            const auto path = fs::u8path(p.bookPath);
            const auto size = regularBookFileSize(path);
            const auto format = detectFormat(path);
            if (!size || format == Format::Unsupported || documentId(p.bookPath, *size) != id) continue;
            Book book;
            book.id = id;
            book.path = p.bookPath;
            book.bytes = *size;
            book.format = format;
            book.title = p.bookTitle.empty() ? defaultTitle(path) : p.bookTitle;
            book.author = p.bookAuthor;
            libraryLookup_[id] = books_.size();
            books_.push_back(std::move(book));
        }
    }
}

void startLibraryScan() {
    if (libraryScanner_.pending()) {
        libraryScanAgain_ = true;
        libraryScanner_.cancel();
        dirty_ = true;
        return;
    }
    std::vector<fs::path> roots{root_ / "books"};
    for (const auto &folder : appSettings_.get().libraryFolders) roots.push_back(fs::u8path(folder));
    scanKnownIds_.clear();
    for (const auto &book : books_) scanKnownIds_.insert(book.id);
    auto known = books_;
    for (const auto &[id, p] : progressStore_.all()) {
        if (known.size() >= 10000) break;
        if (p.bookPath.empty() || id.size() != 16 ||
            !std::all_of(id.begin(), id.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); })) {
            continue;
        }
        const auto path = fs::u8path(p.bookPath);
        const auto format = detectFormat(path);
        if (format == Format::Unsupported) continue;
        Book book;
        book.id = id;
        book.path = p.bookPath;
        book.title = p.bookTitle;
        book.author = p.bookAuthor;
        book.format = format;
        known.push_back(std::move(book));
    }
    libraryScanGeneration_ = libraryScanner_.begin(ScanRequest{
        std::move(roots), std::move(known), root_ / "data" / "library-index.txt", nullptr});
    dirty_ = true;
}

void processLibraryScan() {
    if (!activeFingers_.empty() || pointerActive_ || !libraryScanner_.ready()) return;
    auto result = libraryScanner_.take();
    if (!result) return;
    const bool stale = result->generation != libraryScanGeneration_;
    if (result->cancelled || stale) {
        dirty_ = true;
        if (libraryScanAgain_) {
            libraryScanAgain_ = false;
            startLibraryScan();
        }
        return;
    }

    lastScanResult_ = *result;
    lastScanSummary_ = formatScanSummary(*result);
    appendLibraryScanLog(root_ / "data" / "library-scan.log", *result, APP_VERSION_FULL, PAPERNX_SOURCE_COMMIT);

    if (!result->hasUsableBooks()) {
        if (!result->error.empty() && (currentErrorMessage_.empty() || currentErrorIsFromScan_)) {
            currentErrorMessage_ = ui::label(result->error);
            currentErrorIsFromScan_ = true;
        }
        dirty_ = true;
        if (libraryScanAgain_) {
            libraryScanAgain_ = false;
            startLibraryScan();
        }
        return;
    }

    std::string selected;
    if (navigationFocus_.active().rfind("book:", 0) == 0) {
        selected = navigationFocus_.active().substr(5);
    } else if (selectedBookIdx_ >= 0 && selectedBookIdx_ < int(visibleBooks_.size())) {
        selected = visibleBooks_[selectedBookIdx_].id;
    }

    std::unordered_map<std::string, const Book *> current;
    current.reserve(books_.size());
    for (const auto &book : books_) current.emplace(book.id, &book);
    std::unordered_set<std::string> seen;
    for (auto &book : result->books) {
        seen.insert(book.id);
        const auto found = current.find(book.id);
        if (found != current.end()) {
            if (!found->second->title.empty()) book.title = found->second->title;
            if (!found->second->author.empty()) book.author = found->second->author;
        }
    }
    // A file opened while the worker was running must survive its older batch.
    for (const auto &book : books_) {
        if (!scanKnownIds_.count(book.id) && !seen.count(book.id) && result->books.size() < 10000) {
            result->books.push_back(book);
        }
    }

    books_ = std::move(result->books);
    restoreLibraryLabels(false);

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        for (const auto &book : books_) {
            failedThumbIds_.erase(book.id);
            auto stIt = coverStates_.find(book.id);
            if (stIt != coverStates_.end() && stIt->second == CoverState::Failed) {
                stIt->second = CoverState::NotRequested;
            }
        }
        for (auto it = activeCoverRequests_.begin(); it != activeCoverRequests_.end();) {
            if (it->second.libraryGeneration != libraryScanGeneration_) {
                coverTelemetry_.discarded(it->first, it->second.token, false);
                queuedThumbIds_.erase(it->first);
                if (coverStates_[it->first] == CoverState::Loading) {
                    coverStates_[it->first] = CoverState::NotRequested;
                }
                it = activeCoverRequests_.erase(it);
            } else {
                ++it;
            }
        }
    }

    const bool hasScanProblems = !result->complete || !result->error.empty() ||
        !result->unreadableFiles.empty() || !result->inaccessibleDirs.empty();
    if (!hasScanProblems) {
        if (currentErrorIsFromScan_) {
            currentErrorMessage_.clear();
            currentErrorIsFromScan_ = false;
        }
    } else if (currentErrorMessage_.empty() || currentErrorIsFromScan_) {
        currentErrorMessage_ = lastScanSummary_;
        currentErrorIsFromScan_ = true;
    }

    scheduleMetadataIndex();
    updateVisibleBooks();
    if (!selected.empty()) {
        for (int i = 0; i < int(visibleBooks_.size()); ++i) {
            if (visibleBooks_[i].id == selected) {
                selectedBookIdx_ = i;
                break;
            }
        }
        if (screen_ == ScreenState::Library && !quickMenu_ && catalogPanel_ == CatalogPanel::None) {
            rebuildFocus();
            navigationFocus_.select("book:" + selected);
            focusReveal();
        }
    }
    listScroll_ = std::clamp(listScroll_, 0.f, maxUiScroll());
    ++completedLibraryScans_;
    dirty_ = true;
    if (libraryScanAgain_) {
        libraryScanAgain_ = false;
        startLibraryScan();
    }
}

SDL_Rect browserFolderButton(int w) const { return {368, 16, std::max(160, w - 464), 56}; }

void addBrowserFolder() {
    std::error_code ec;
    const bool deviceMode =
#ifdef __SWITCH__
        true;
#else
        isDevicePath(browserPath_);
#endif
    auto folder = normalizeScanRoot(browserPath_, ec, deviceMode);
    if (ec) return;
    const auto info = defaultFsOps().statPath(folder, true);
    if (!info.exists || !info.isDirectory) return;
    auto preferences = appSettings_.get();
    const auto value = folder.u8string();
    const bool alreadyPresent = std::any_of(preferences.libraryFolders.begin(), preferences.libraryFolders.end(),
        [&](const std::string &existing) {
            return pathsEquivalentSafe(fs::u8path(existing), folder, defaultFsOps());
        });
    if (!alreadyPresent) {
        if (preferences.libraryFolders.size() >= 32) {
            currentErrorMessage_ = ui::label("Folder limit reached");
            currentErrorIsFromScan_ = false;
            dirty_ = true;
            return;
        }
        preferences.libraryFolders.push_back(value);
    }
    preferences.defaultBrowserPath = value;
    appSettings_.put(std::move(preferences));
    persistAppSettings();
    screen_ = ScreenState::Library;
    quickMenu_ = false;
    catalogPanel_ = CatalogPanel::None;
    listScroll_ = 0;
    startLibraryScan();
    dirty_ = true;
}
