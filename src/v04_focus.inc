    // Shared geometric focus registry: single authority for active navigation focus and focus-ring rendering.
    FocusManager navigationFocus_;

    void rebuildFocus() {
        int w, h;
        getWindowSize(w, h);
        std::string context;
        if (choiceKind_ >= 0) context = "choice." + std::to_string(choiceKind_);
        else if (catalogPanel_ != CatalogPanel::None) context = "catalog." + std::to_string(int(catalogPanel_));
        else if (quickMenu_) context = helpMenu_ ? "quick.help" : "quick";
        else if (screen_ == ScreenState::AaModal) context = "aa." + std::to_string(aaTab_);
        else if (screen_ == ScreenState::TocModal) context = "toc." + std::to_string(tocTab_);
        else if (screen_ == ScreenState::Library) context = openSeriesKey_.empty() ? "library" : "library.series";
        else context = std::to_string(int(screen_));

        navigationFocus_.begin(context);
        auto add = [&](const std::string &id, SDL_Rect r, bool enabled = true, std::string alias = {}) {
            navigationFocus_.add(id, {r.x, r.y, r.w, r.h}, enabled, std::move(alias));
        };
        auto addClipped = [&](const std::string &id, SDL_Rect r, SDL_Rect clip, bool enabled = true, std::string alias = {}) {
            navigationFocus_.addClipped(id, {r.x, r.y, r.w, r.h}, {clip.x, clip.y, clip.w, clip.h}, enabled, std::move(alias));
        };

        if (choiceKind_ >= 0) {
            const auto cr = choiceContentRect(w, h);
            const auto options = choiceOptions();
            for (int i = 0; i < int(options.size()); ++i) {
                addClipped("choice." + std::to_string(i), {cr.x, cr.y + i * 56 - int(choiceScroll_), cr.w, 56}, cr);
            }
        } else if (catalogPanel_ != CatalogPanel::None) {
            auto r = catalogPanelRect(w, h);
            if (catalogPanel_ == CatalogPanel::Text) {
                auto keys = catalogKeyboard();
                int width = (r.w - 40) / 10;
                for (int i = 0; i < int(keys.size()); ++i) {
                    add("key." + std::to_string(i), {r.x + 20 + (i % 10) * width, r.y + 156 + (i / 10) * 48, width - 4, 44});
                }
                add("text.save", {r.x + 20, r.y + r.h - 76, r.w / 2 - 24, 56});
                add("text.erase", {r.x + r.w / 2 + 4, r.y + r.h - 76, r.w / 2 - 24, 56});
            } else {
                auto rows = catalogRows();
                int top = catalogContentTop(r);
                SDL_Rect clip{r.x + 12, top, r.w - 24, std::max(0, r.y + r.h - 16 - top)};
                for (int i = 0; i < int(rows.size()); ++i) {
                    bool manage = catalogPanel_ == CatalogPanel::Values && catalogGroup_ == CatalogGroup::Collections;
                    addClipped("catalog.row." + std::to_string(i),
                               {r.x + 20, top + i * 64 - int(catalogScroll_), r.w - 40 - (manage ? 132 : 0), 56}, clip);
                    if (manage) {
                        addClipped("catalog.manage." + std::to_string(i),
                                   {r.x + r.w - 144, top + i * 64 - int(catalogScroll_), 124, 56}, clip);
                    }
                }
            }
            add("catalog.close", {r.x + r.w - 124, r.y + 12, 108, 56});
        } else if (quickMenu_) {
            if (!helpMenu_) {
                auto r = quickSheet(w, h);
                auto vp = quickActionViewport(r);
                auto actions = quickActions();
                for (int i = 0; i < int(actions.size()); ++i) {
                    addClipped("quick." + std::to_string(i),
                               {r.x + 12, r.y + QUICK_HEADER_HEIGHT + i * 60 - int(quickScroll_), r.w - 24, 56}, vp);
                }
                add("quick.pages", quickPageShortcut(r));
                add("quick.orientation", quickOrientationShortcut(r));
            }
            add("quick.close", {w - 80, 8, 64, 64});
        } else if (screen_ == ScreenState::AaModal) {
            ui::SettingsLayout l(w, h);
            for (int i = 0; i < settingsRows(); ++i) {
                if (aaTab_ == 2 && i < 5) continue;
                if (aaTab_ == 4 && i == 4 && (!doc_ || px_document_is_epub(doc_))) continue;
                if (aaTab_ == 5 && i == 4 && browserPath_.empty()) continue;
                auto r = l.control(i);
                r.y -= int(settingsScroll_);
                addClipped("setting." + std::to_string(i), r, l.content);
            }
            if (aaTab_ == 2) {
                for (int i = 0; i < 10; ++i) {
                    auto r = l.control(i / 2);
                    r.y -= int(settingsScroll_);
                    int half = (r.w - 12) / 2;
                    r.x += (i % 2) * (half + 12);
                    r.w = half;
                    addClipped("theme." + std::to_string(i), r, l.content);
                }
            }
            for (int i = 0; i < 6; ++i) add("tab." + std::to_string(i), l.tab(i));
            if (doc_ && aaTab_ != 4 && aaTab_ != 5) add("scope", l.scope);
            add("close", l.close);
            auto footer = l.footer;
            footer.w = (footer.w - 16) / 2;
            add("save", footer);
            footer.x += footer.w + 16;
            add("reset", footer);
        } else if (screen_ == ScreenState::Library) {
            ui::LibraryLayout l(w);
            SDL_Rect libClip{0, l.top, w, std::max(0, h - l.top - 28)};
            for (int i = 0; i < int(visibleBooks_.size()); ++i) {
                SDL_Rect r = viewMode_ == ViewMode::Grid
                    ? SDL_Rect{l.left + (i % l.columns) * l.stepX, l.top + (i / l.columns) * l.stepY - int(listScroll_), l.cardWidth, l.cardHeight}
                    : SDL_Rect{32, l.top + i * 112 - int(listScroll_), w - 64, 104};
                addClipped("book:" + visibleBooks_[i].id, r, libClip, true, "book." + std::to_string(i));
            }
            for (int i = 0; i < 6; ++i) add("category." + std::to_string(i), categoryRect(i, w));
            if (openSeriesKey_.empty()) {
                if (w >= 900) add("library.browse", {208, 16, 180, 56});
            } else {
                add("series.back", {16, 16, 152, 56});
            }
            add("library.journal", {w - 440, 16, 200, 56});
            add("search", {w - 216, 16, 112, 56});
            add("plus", {w - 80, 8, 64, 64});
            if (books_.empty()) add("browse", {56, l.top + 16 + std::min(260, h - l.top - 48) - 72, 260, 56});
        } else if (screen_ == ScreenState::FileBrowser) {
            SDL_Rect browserClip{24, 112, std::max(0, w - 48), std::max(0, h - 128)};
            for (int i = 0; i < int(browserEntries_.size()); ++i) {
                addClipped("file." + std::to_string(i), {32, 112 + i * 64 - int(browserScroll_), w - 64, 56}, browserClip);
            }
            add("browser.folder", browserFolderButton(w));
            add("browser.back", {24, 16, 152, 56});
            if (browserPath_ != browserRoot_) add("browser.up", {192, 16, 168, 56});
            add("plus", {w - 80, 8, 64, 64});
        } else if (screen_ == ScreenState::TocModal) {
            const auto clip = tocContent(w, h);
            const int first = std::max(0, int(tocScroll_) / tocStep() - 1) * tocColumns(w);
            const int last = std::min(tocCount(), first + (clip.h / tocStep() + 4) * tocColumns(w));
            for (int i = first; i < last; ++i) {
                if (tocItem(i).page >= 0 && tocItem(i).page < pageCount_) {
                    addClipped("toc." + std::to_string(i), tocCell(i, w, h), clip);
                }
            }
            if (tocSelected_ >= 0 && tocSelected_ < tocCount() && (tocSelected_ < first || tocSelected_ >= last) &&
                tocItem(tocSelected_).page >= 0 && tocItem(tocSelected_).page < pageCount_) {
                addClipped("toc." + std::to_string(tocSelected_), tocCell(tocSelected_, w, h), clip);
            }
            for (int tab = 0; tab < 3; ++tab) add("contents.tab." + std::to_string(tab), tocTabRect(tab, w));
            add("close", {w - 184, 88, 112, 56});
        } else if (screen_ == ScreenState::BookmarksModal) {
            SDL_Rect bmClip{48, 152, std::max(0, w - 96), std::max(0, h - 280)};
            auto p = progressStore_.get(currentBookId_);
            for (int i = 0; i < int(p.bookmarks.size()); ++i) {
                addClipped("bookmark." + std::to_string(i), {56, 152 + i * 56 - int(bookmarkScroll_), w - 256, 56}, bmClip);
                addClipped("bookmark.delete." + std::to_string(i), {w - 184, 152 + i * 56 - int(bookmarkScroll_), 112, 56}, bmClip);
            }
            add("bookmark.add", {48, h - 112, w - 96, 56});
            add("close", {w - 184, 88, 112, 56});
        } else if (screen_ == ScreenState::Reader && (toolbarVisible_ || appSettings_.get().toolbar)) {
            add("reader.library", {16, 8, 152, 56});
            add("reader.toc", {w < 900 ? 184 : w - 520, 8, w < 900 ? 136 : 160, 56});
            add("reader.bookmarks", {w < 900 ? 336 : w - 344, 8, w < 900 ? 112 : 136, 56});
            add("reader.settings", {w < 900 ? 464 : w - 192, 8, w < 900 ? 80 : 96, 56});
            add("reader.previous", readerPrev(w, h));
            add("reader.seek", readerSeekHit(w, h));
            add("reader.next", readerNext(w, h));
            add("reader.pages", readerPages(w, h));
            if (showZoomReset()) add("reader.zoom-reset", zoomResetRect(w, h));
        }

        if (choiceKind_ >= 0) {
            navigationFocus_.select("choice." + std::to_string(std::max(0, choiceFocusedIndex_)));
        }
        navigationFocus_.finish();
    }

    void focusReveal() {
        const auto id = navigationFocus_.active();
        auto index = [&](const char *prefix) { return std::stoi(id.substr(std::char_traits<char>::length(prefix))); };
        int w, h;
        getWindowSize(w, h);
        auto reveal = [](float &scroll, float top, float height, float viewport) {
            if (top < scroll) scroll = top;
            if (top + height > scroll + viewport) scroll = std::max(0.f, top + height - viewport);
        };
        if (id.rfind("choice.", 0) == 0) {
            choiceFocusedIndex_ = index("choice.");
            ensureChoiceVisible(w, h);
        } else if (id.rfind("catalog.manage.", 0) == 0) {
            catalogFocus_ = index("catalog.manage.");
            auto r = catalogPanelRect(w, h);
            reveal(catalogScroll_, catalogFocus_ * 64.f, 56, float(r.y + r.h - 16 - catalogContentTop(r)));
            catalogScroll_ = std::clamp(catalogScroll_, 0.f, float(std::max(0, int(catalogRows().size()) * 64 - (r.y + r.h - 16 - catalogContentTop(r)))));
        } else if (id.rfind("catalog.row.", 0) == 0) {
            catalogFocus_ = index("catalog.row.");
            auto r = catalogPanelRect(w, h);
            reveal(catalogScroll_, catalogFocus_ * 64.f, 56, float(r.y + r.h - 16 - catalogContentTop(r)));
            catalogScroll_ = std::clamp(catalogScroll_, 0.f, float(std::max(0, int(catalogRows().size()) * 64 - (r.y + r.h - 16 - catalogContentTop(r)))));
        } else if (id == "catalog.close") {
            catalogFocus_ = -1;
        } else if (id.rfind("bookmark.delete.", 0) == 0) {
            bmSelected_ = index("bookmark.delete.");
            reveal(bookmarkScroll_, bmSelected_ * 56.f, 56, float(h - 280));
            bookmarkScroll_ = std::clamp(bookmarkScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("theme.", 0) == 0) {
            aaRow_ = index("theme.") / 2;
            ui::SettingsLayout l(w, h);
            reveal(settingsScroll_, aaRow_ * 64.f, 56, float(l.content.h));
            settingsScroll_ = std::clamp(settingsScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("setting.", 0) == 0) {
            aaRow_ = index("setting.");
            ui::SettingsLayout l(w, h);
            reveal(settingsScroll_, aaRow_ * 64.f, 56, float(l.content.h));
            settingsScroll_ = std::clamp(settingsScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("tab.", 0) == 0 || id == "scope" || id == "close" || id == "save" || id == "reset") {
            if (screen_ == ScreenState::AaModal) aaRow_ = -1;
        } else if (id.rfind("book:", 0) == 0) {
            const std::string bookId = id.substr(5);
            for (int i = 0; i < int(visibleBooks_.size()); ++i) {
                if (visibleBooks_[i].id == bookId) {
                    selectedBookIdx_ = i;
                    break;
                }
            }
            ui::LibraryLayout l(w);
            int columns = viewMode_ == ViewMode::Grid ? l.columns : 1;
            int step = viewMode_ == ViewMode::Grid ? l.stepY : 112;
            reveal(listScroll_, float(selectedBookIdx_ / columns * step), float(viewMode_ == ViewMode::Grid ? l.cardHeight : 104), float(h - l.top - 28));
            listScroll_ = std::clamp(listScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("file.", 0) == 0) {
            browserSelected_ = index("file.");
            reveal(browserScroll_, browserSelected_ * 64.f, 56, float(h - 128));
            browserScroll_ = std::clamp(browserScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("toc.", 0) == 0) {
            tocSelected_ = index("toc.");
            revealContentsItem();
        } else if (id.rfind("bookmark.", 0) == 0 && id.rfind("bookmark.delete.", 0) != 0 && id != "bookmark.add") {
            bmSelected_ = index("bookmark.");
            reveal(bookmarkScroll_, bmSelected_ * 56.f, 56, float(h - 280));
            bookmarkScroll_ = std::clamp(bookmarkScroll_, 0.f, maxUiScroll());
        } else if (id.rfind("quick.", 0) == 0 && id != "quick.close" && id != "quick.orientation" && id != "quick.pages") {
            quickFocus_ = index("quick.");
            auto r = quickSheet(w, h);
            auto vp = quickActionViewport(r);
            reveal(quickScroll_, quickFocus_ * 60.f, 56, float(vp.h));
            quickScroll_ = std::clamp(quickScroll_, 0.f, maxUiScroll());
        } else if (id == "quick.close" || id == "quick.orientation" || id == "quick.pages") {
            quickFocus_ = -1;
        }
        dirty_ = true;
    }

    bool moveFocus(int dx, int dy) {
        if (updateDialog_ || colorEditing_ >= 0) return false;
        if (screen_ == ScreenState::Reader && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0) {
            // D-pad Left/Right turns pages until the toolbar is deliberately focused.
            if (!(toolbarVisible_ || appSettings_.get().toolbar)) {
                readerToolbarFocusActive_ = false;
                return false;
            }
            if (!readerToolbarFocusActive_) {
                if (dy >= 0) return false;
                readerToolbarFocusActive_ = true;
            } else if (dy > 0) {
                int w, h;
                getWindowSize(w, h);
                for (const auto &item : navigationFocus_.items()) {
                    if (item.id == navigationFocus_.active() && item.rect.y >= h - 80) {
                        readerToolbarFocusActive_ = false;
                        dirty_ = true;
                        return true;
                    }
                }
            }
        }
        if (screen_ == ScreenState::AaModal && settingsEditing_ && catalogPanel_ == CatalogPanel::None && !quickMenu_ && choiceKind_ < 0) return false;
        rebuildFocus();
        navigationFocus_.move(dx, dy);
        focusReveal();
        return true;
    }

    bool activateFocus() {
        if (updateDialog_ || colorEditing_ >= 0) return false;
        if (screen_ == ScreenState::Reader && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0 && !readerToolbarFocusActive_) {
            return false;
        }
        if (screen_ == ScreenState::AaModal && settingsEditing_ && catalogPanel_ == CatalogPanel::None && !quickMenu_ && choiceKind_ < 0) {
            return false;
        }
        rebuildFocus();
        const auto id = navigationFocus_.active();
        if (id.empty()) return false;
        if (quickMenu_) {
            if (id == "quick.close") {
                quickMenu_ = false;
                helpMenu_ = false;
                dirty_ = true;
                return true;
            }
            if (id == "quick.pages") {
                quickMenu_ = false;
                helpMenu_ = false;
                toggleLandscapeSpreads();
                return true;
            }
            if (id == "quick.orientation") {
                quickMenu_ = false;
                helpMenu_ = false;
                openChoice(1, appSettings_.get().displayOrientation);
                return true;
            }
            if (id.rfind("quick.", 0) == 0) {
                const int idx = std::stoi(id.substr(6));
                activateQuickAction(idx);
                return true;
            }
            return false;
        }
        if (catalogPanel_ != CatalogPanel::None) {
            if (id == "catalog.close") {
                closeCatalogPanel();
                return true;
            }
            if (id.rfind("catalog.row.", 0) == 0) {
                const int idx = std::stoi(id.substr(12));
                catalogFocus_ = idx;
                focusReveal();
                activateCatalogRow(idx);
                return true;
            }
            if (id.rfind("catalog.manage.", 0) == 0) {
                const int idx = std::stoi(id.substr(15));
                catalogFocus_ = idx;
                auto g = catalog_.groups(books_, catalogGroup_);
                auto it = g.begin();
                std::advance(it, std::min(idx, int(g.size())));
                if (it != g.end()) {
                    selectedCollection_ = it->first;
                    setCatalogPanel(CatalogPanel::CollectionActions);
                }
                return true;
            }
        }
        if (id == "reader.seek") { startReaderScrub(true); return true; }
        if (catalogPanel_ == CatalogPanel::None && screen_ == ScreenState::AaModal && id.rfind("setting.", 0) == 0) {
            aaRow_ = std::stoi(id.substr(8));
            if (numericSetting()) {
                editOriginal_ = currentStyle();
                editOriginalZoom_ = pdfZoom_;
                settingsEditing_ = true;
                dirty_ = true;
                return true;
            }
            focusReveal();
        }
        if (id == "reader.library") { closeBook(); return true; }
        if (id == "reader.toc") { openContents(); return true; }
        if (id == "reader.bookmarks") { screen_ = ScreenState::BookmarksModal; dirty_ = true; return true; }
        if (id == "reader.settings") { openSettings(); return true; }
        if (id.rfind("book:", 0) == 0) {
            const std::string bookId = id.substr(5);
            for (int i = 0; i < int(visibleBooks_.size()); ++i) {
                if (visibleBooks_[i].id == bookId) {
                    selectedBookIdx_ = i;
                    openLibraryItem(i);
                    return true;
                }
            }
            return false;
        }
        focusReveal();
        rebuildFocus();
        if (const auto *item = navigationFocus_.activeItem()) {
            int w, h;
            getWindowSize(w, h);
            auto r = item->rect;
            handleTap(float(r.x + r.w / 2) / w, float(r.y + r.h / 2) / h);
            return true;
        }
        return false;
    }

    void paintFocus() {
        if (updateDialog_ || colorEditing_ >= 0) return;
        if (screen_ == ScreenState::Reader && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0 && !readerToolbarFocusActive_) {
            return;
        }
        if (screen_ == ScreenState::ReadingJournal && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0) {
            return;
        }
        if (screen_ == ScreenState::JournalPreview && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0) {
            return;
        }
        rebuildFocus();
        const auto pal = palette();
        const auto *item = navigationFocus_.activeItem();
        if (!item || !item->enabled) return;
        const auto r = item->rect;
        if (item->hasClip) {
            SDL_Rect clip{item->clip.x, item->clip.y, item->clip.w, item->clip.h};
            SDL_RenderSetClipRect(renderer_, &clip);
        }
        ui::focusRing(renderer_, {r.x, r.y, r.w, r.h}, pal,
                      screen_ == ScreenState::AaModal && !quickMenu_ && catalogPanel_ == CatalogPanel::None && choiceKind_ < 0 && settingsEditing_);
        if (item->hasClip) {
            SDL_RenderSetClipRect(renderer_, nullptr);
        }
    }
