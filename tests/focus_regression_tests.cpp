#define main papernx_application_entry
#include "../src/main.cpp"
#include "test_evidence.hpp"
#undef main

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <vector>

#ifdef NDEBUG
#error Tests require enabled assertions
#endif

namespace {

struct AppTest {
#include "library_fixture_helpers.inc"

    static void dispatchPad(PaperNxApp &app, SDL_GameControllerButton button) {
        bool running = true;
        SDL_Event down{};
        down.type = SDL_CONTROLLERBUTTONDOWN;
        down.cbutton.button = button;
        app.handleEvent(down, running);
        SDL_Event up{};
        up.type = SDL_CONTROLLERBUTTONUP;
        up.cbutton.button = button;
        app.handleEvent(up, running);
    }

    static void dispatchAxis(PaperNxApp &app, SDL_GameControllerAxis axis, Sint16 value) {
        bool running = true;
        SDL_Event ev{};
        ev.type = SDL_CONTROLLERAXISMOTION;
        ev.caxis.axis = axis;
        ev.caxis.value = value;
        app.handleEvent(ev, running);
    }

    static void dispatchTouch(PaperNxApp &app, float xNorm, float yNorm) {
        bool running = true;
        SDL_Event down{};
        down.type = SDL_FINGERDOWN;
        down.tfinger.fingerId = 42;
        down.tfinger.x = xNorm;
        down.tfinger.y = yNorm;
        app.handleEvent(down, running);
        SDL_Event up{};
        up.type = SDL_FINGERUP;
        up.tfinger.fingerId = 42;
        up.tfinger.x = xNorm;
        up.tfinger.y = yNorm;
        app.handleEvent(up, running);
    }

    static void settleScanner(PaperNxApp &app) {
        const auto deadline = SDL_GetTicks() + 15000;
        while (app.libraryScanner_.pending() && SDL_GetTicks() < deadline) {
            app.processLibraryScan();
            SDL_Delay(2);
        }
        assert(!app.libraryScanner_.pending());
        app.processLibraryScan();
    }

    static void renderAndAssertSingleFocus(PaperNxApp &app, int expectedRings = 1) {
        app.render();
        assert(int(ui::focusRingDrawCount) == expectedRings);
    }

    static void saveShot(PaperNxApp &app, const std::string &name) {
        app.render();
        int w = 0, h = 0;
        app.getWindowSize(w, h);
        auto *surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
        assert(surface);
        assert(SDL_RenderReadPixels(app.renderer_, nullptr, surface->format->format, surface->pixels, surface->pitch) == 0);
        const auto folder = test_evidence::path("v094-focus");
        fs::create_directories(folder);
        assert(SDL_SaveBMP(surface, (folder / (name + ".bmp")).string().c_str()) == 0);
        SDL_FreeSurface(surface);
    }

    static std::vector<Uint32> readPixels(PaperNxApp &app, int &w, int &h) {
        app.getWindowSize(w, h);
        std::vector<Uint32> px(std::size_t(w) * std::size_t(h), 0);
        assert(SDL_RenderReadPixels(app.renderer_, nullptr, SDL_PIXELFORMAT_ARGB8888, px.data(), w * 4) == 0);
        return px;
    }

    static void assertFocusInsideClip(PaperNxApp &app) {
        const auto *item = app.navigationFocus_.activeItem();
        assert(item != nullptr);
        if (!item->hasClip) return;
        int w = 0, h = 0;
        app.getWindowSize(w, h);
        assert(item->clip.w >= 0 && item->clip.h >= 0);
        assert(item->clip.x >= 0 && item->clip.y >= 0 && item->clip.x + item->clip.w <= w && item->clip.y + item->clip.h <= h);
    }

    static void testQuickMenuUnifiedFocusAndClipping(PaperNxApp &app) {
        for (const auto &res : {std::pair<int, int>{1280, 720}, std::pair<int, int>{1920, 1080}, std::pair<int, int>{720, 1280}}) {
            SDL_SetWindowSize(app.window_, res.first, res.second);
            int w = 0, h = 0;
            app.getWindowSize(w, h);

            app.toggleQuickMenu();
            assert(app.quickMenu_);
            assert(!app.helpMenu_);
            renderAndAssertSingleFocus(app, 1);

            const auto sheet = app.quickSheet(w, h);
            const auto vp = app.quickActionViewport(sheet);
            assert(sheet.y >= 0 && sheet.y + sheet.h <= h - 16);
            assert(vp.y >= sheet.y && vp.y + vp.h <= sheet.y + sheet.h);

            const int actionCount = int(app.quickActions().size());
            for (int step = 0; step < actionCount + 4; ++step) {
                renderAndAssertSingleFocus(app, 1);
                const auto id = app.navigationFocus_.active();
                if (id.rfind("quick.", 0) == 0 && id != "quick.pages" && id != "quick.orientation" && id != "quick.close") {
                    const int idx = std::stoi(id.substr(6));
                    assert(app.quickFocus_ == idx);
                    assertFocusInsideClip(app);
                } else {
                    assert(app.quickFocus_ == -1);
                }
                dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            }

            assert(app.navigationFocus_.select("quick.pages"));
            app.focusReveal();
            assert(app.quickFocus_ == -1);
            renderAndAssertSingleFocus(app, 1);

            assert(app.navigationFocus_.select("quick.orientation"));
            app.focusReveal();
            assert(app.quickFocus_ == -1);
            renderAndAssertSingleFocus(app, 1);

            assert(app.navigationFocus_.select("quick.close"));
            app.focusReveal();
            assert(app.quickFocus_ == -1);
            renderAndAssertSingleFocus(app, 1);

            if (res.first == 1280) saveShot(app, "quick-menu-1280x720");
            if (res.first == 720) saveShot(app, "quick-menu-portrait");

            dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
            assert(!app.quickMenu_);
            app.toggleQuickMenu();
            assert(app.quickMenu_ && app.quickFocus_ == 0);
            assert(app.navigationFocus_.focused("quick.0"));
            renderAndAssertSingleFocus(app, 1);

            app.helpMenu_ = true;
            renderAndAssertSingleFocus(app, 1);
            assert(app.navigationFocus_.focused("quick.close"));
            dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
            assert(!app.quickMenu_ && !app.helpMenu_);
        }
        SDL_SetWindowSize(app.window_, 1280, 720);
    }

    static void testQuickMenuOverflowPixelClip(PaperNxApp &app) {
        auto it = std::find_if(app.books_.begin(), app.books_.end(), [](const Book &b) { return b.format == Format::Epub; });
        assert(it != app.books_.end());
        app.openBook(*it);
        SDL_SetWindowSize(app.window_, 960, 460);
        app.toggleQuickMenu();
        assert(app.quickMenu_);

        int w = 0, h = 0;
        app.getWindowSize(w, h);
        const auto sheet = app.quickSheet(w, h);
        const auto vp = app.quickActionViewport(sheet);
        const int n = int(app.quickActions().size());
        assert(n * 60 > vp.h);

        for (int i = 0; i < n - 1; ++i) {
            dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        }
        renderAndAssertSingleFocus(app, 1);
        const auto pal = app.palette();
        const Uint32 fgPixel = (Uint32(pal.fg.r) << 16) | (Uint32(pal.fg.g) << 8) | Uint32(pal.fg.b);
        const auto px = readPixels(app, w, h);
        for (int y = sheet.y + sheet.h + 1; y < std::min(h, sheet.y + sheet.h + 10); ++y) {
            for (int x = sheet.x; x < sheet.x + sheet.w; ++x) {
                assert((px[std::size_t(y) * std::size_t(w) + std::size_t(x)] & 0xffffffu) != fgPixel);
            }
        }

        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        app.closeBook();
        SDL_SetWindowSize(app.window_, 1280, 720);
    }

    static void testAaModalCatalogAndAlternatingInputs(PaperNxApp &app) {
        app.openSettings(true);
        for (int tab = 0; tab < 6; ++tab) {
            app.aaTab_ = tab;
            renderAndAssertSingleFocus(app, 1);
            dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            renderAndAssertSingleFocus(app, 1);
            dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            renderAndAssertSingleFocus(app, 1);
        }
        app.aaTab_ = 0;
        app.rebuildFocus();
        assert(app.navigationFocus_.select("setting.0"));
        app.focusReveal();
        renderAndAssertSingleFocus(app, 1);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_A);
        assert(app.settingsEditing_);
        renderAndAssertSingleFocus(app, 1);
        saveShot(app, "aa-modal-focus");
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(!app.settingsEditing_);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.screen_ == ScreenState::Library);

        app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);
        assert(app.catalogPanel_ == PaperNxApp::CatalogPanel::Browse);
        renderAndAssertSingleFocus(app, 1);
        for (int step = 0; step < 30; ++step) {
            const auto btn = (step % 3 == 0) ? SDL_CONTROLLER_BUTTON_DPAD_DOWN :
                             (step % 3 == 1) ? SDL_CONTROLLER_BUTTON_DPAD_UP :
                                               SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
            dispatchPad(app, btn);
            renderAndAssertSingleFocus(app, 1);
        }
        const int reportRowIdx = int(app.catalogRows().size()) - 1;
        assert(app.navigationFocus_.select("catalog.row." + std::to_string(reportRowIdx)));
        app.focusReveal();
        dispatchPad(app, SDL_CONTROLLER_BUTTON_A);
        assert(app.catalogPanel_ == PaperNxApp::CatalogPanel::ScanReport);
        renderAndAssertSingleFocus(app, 1);
        saveShot(app, "catalog-scan-report");
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.catalogPanel_ == PaperNxApp::CatalogPanel::Browse);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.catalogPanel_ == PaperNxApp::CatalogPanel::None);

        assert(app.visibleBooks_.size() >= 4);
        for (int step = 0; step < 100; ++step) {
            if (step % 4 == 0) {
                dispatchAxis(app, SDL_CONTROLLER_AXIS_LEFTX, (step % 8 == 0) ? 30000 : -30000);
                dispatchAxis(app, SDL_CONTROLLER_AXIS_LEFTX, 0);
            } else if (step % 4 == 1) {
                dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            } else if (step % 4 == 2) {
                dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_UP);
            } else {
                const auto cat = app.categoryRect(0, 1280);
                dispatchTouch(app, float(cat.x + cat.w / 2) / 1280.f, float(cat.y + cat.h / 2) / 720.f);
            }
            renderAndAssertSingleFocus(app, 1);
        }

        app.returnToLibraryBooks();
        const std::string targetId = app.visibleBooks_[2].id;
        app.rebuildFocus();
        assert(app.navigationFocus_.select("book:" + targetId));
        app.focusReveal();
        app.viewMode_ = ViewMode::List;
        app.updateVisibleBooks();
        app.rebuildFocus();
        assert(app.navigationFocus_.focused("book:" + targetId));
        assert(app.visibleBooks_[app.selectedBookIdx_].id == targetId);
        renderAndAssertSingleFocus(app, 1);

        app.viewMode_ = ViewMode::Grid;
        app.updateVisibleBooks();
        app.rebuildFocus();
        assert(app.navigationFocus_.focused("book:" + targetId));
        assert(app.navigationFocus_.select("book.1"));
        assert(app.navigationFocus_.focused("book:" + app.visibleBooks_[1].id));

        app.openBrowser();
        assert(app.screen_ == ScreenState::FileBrowser);
        renderAndAssertSingleFocus(app, 1);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        renderAndAssertSingleFocus(app, 1);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.screen_ == ScreenState::Library);
    }

    static void testLibraryUxZonesExitScrollAndCovers(PaperNxApp &app) {
        app.screen_ = ScreenState::Library;
        app.quickMenu_ = false;
        app.catalogPanel_ = PaperNxApp::CatalogPanel::None;
        app.exitConfirmOpen_ = false;
        app.exitRequested_ = false;
        app.category_ = Category::All;
        app.searchQuery_.clear();
        app.catalogFilter_.reset();
        app.openSeriesKey_.clear();
        app.viewMode_ = ViewMode::Grid;
        app.updateVisibleBooks();
        app.listScroll_ = 0;
        app.returnToLibraryBooks();
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);
        assert(app.visibleBooks_.size() >= 28);

        // 1. Grid navigation: single step moves exact neighbor & missing last row picks nearest valid book
        app.selectedBookIdx_ = 0;
        app.rebuildFocus();
        assert(app.navigationFocus_.select("book:" + app.visibleBooks_[0].id));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        assert(app.selectedBookIdx_ == 1);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);
        renderAndAssertSingleFocus(app, 1);
        saveShot(app, "modern-covers-grid");

        const int cols = ui::LibraryLayout(1280, app.libraryScrollbar(1280, 720).visible).columns;
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.selectedBookIdx_ == 1 + cols);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);

        // Short Up from row > 0 navigates within BOOKS zone (never jumps to HEADER)
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_UP);
        assert(app.selectedBookIdx_ == 1);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);

        // Instant HEADER transition when on first row (row 0) + short Up
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_UP);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Header);
        renderAndAssertSingleFocus(app, 1);

        // Return to BOOKS from HEADER via Down; restores book focus & scroll
        assert(app.navigationFocus_.select("category.0"));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);
        assert(app.selectedBookIdx_ == 1);

        // 2. Incomplete last row clamping
        const int lastIdx = int(app.visibleBooks_.size()) - 1;
        const int rowAboveLast = std::max(0, lastIdx - cols);
        app.selectedBookIdx_ = rowAboveLast;
        app.lastLibraryBookId_ = app.visibleBooks_[rowAboveLast].id;
        app.rebuildFocus();
        assert(app.navigationFocus_.select("book:" + app.lastLibraryBookId_));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.selectedBookIdx_ >= rowAboveLast + 1 && app.selectedBookIdx_ <= lastIdx);

        // 3. Long-press Up (600 ms) from lower row jumps directly to HEADER preserving scroll, no repeat while held
        app.selectedBookIdx_ = std::min(lastIdx, cols * 3);
        app.lastLibraryBookId_ = app.visibleBooks_[app.selectedBookIdx_].id;
        app.focusReveal();
        {
            bool running = true;
            SDL_Event down{};
            down.type = SDL_CONTROLLERBUTTONDOWN;
            down.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
            app.handleEvent(down, running);
            assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);
            const float scrollBeforeHold = app.listScroll_;
            app.libraryUpHoldStart_ = SDL_GetTicks() - 620;
            app.tickLibraryUpHold(SDL_GetTicks());
            assert(app.libraryZone_ == PaperNxApp::LibraryZone::Header);
            assert(std::abs(app.listScroll_ - scrollBeforeHold) < 1.f);
            // Still held: must not retrigger
            app.tickLibraryUpHold(SDL_GetTicks());
            assert(app.libraryZone_ == PaperNxApp::LibraryZone::Header);
            SDL_Event up{};
            up.type = SDL_CONTROLLERBUTTONUP;
            up.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP;
            app.handleEvent(up, running);
        }

        // Return to BOOKS keeps scroll & restores book
        assert(app.navigationFocus_.select("category.0"));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);

        // 4. List view single-step + instant header on index 0
        app.viewMode_ = ViewMode::List;
        app.listScroll_ = 0;
        app.selectedBookIdx_ = 0;
        app.lastLibraryBookId_ = app.visibleBooks_[0].id;
        app.rebuildFocus();
        assert(app.navigationFocus_.select("book:" + app.lastLibraryBookId_));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.selectedBookIdx_ == 1);
        renderAndAssertSingleFocus(app, 1);
        saveShot(app, "library-scrollbar-list");
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_UP);
        assert(app.selectedBookIdx_ == 0);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_UP);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Header);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);

        // 5. Right stick precision nonlinear scrolling & drift immunity
        app.listScroll_ = 0;
        app.clearHeldControls();
        // Inside deadzone (~23%): zero scroll movement
        dispatchAxis(app, SDL_CONTROLLER_AXIS_RIGHTY, 5000);
        app.controllerInputTick_ = SDL_GetTicks() - 40;
        app.tickControllerInput();
        assert(app.listScroll_ == 0.f);
        // Gentle push -> slow scroll; full push -> fast scroll
        dispatchAxis(app, SDL_CONTROLLER_AXIS_RIGHTY, 12000);
        app.controllerInputTick_ = SDL_GetTicks() - 40;
        app.tickControllerInput();
        const float gentleDelta = app.listScroll_;
        assert(gentleDelta > 0.f && gentleDelta < 20.f);

        app.listScroll_ = 0;
        dispatchAxis(app, SDL_CONTROLLER_AXIS_RIGHTY, 32767);
        app.controllerInputTick_ = SDL_GetTicks() - 40;
        app.tickControllerInput();
        const float fullDelta = app.listScroll_;
        assert(fullDelta > gentleDelta * 3.f);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books); // never jumps to header
        // Release stick -> stops immediately
        dispatchAxis(app, SDL_CONTROLLER_AXIS_RIGHTY, 0);
        const float stoppedAt = app.listScroll_;
        app.controllerInputTick_ = SDL_GetTicks() - 40;
        app.tickControllerInput();
        assert(app.listScroll_ == stoppedAt);

        // 6. Scrollbar drag & track tap + no accidental book open
        const auto sb = app.libraryScrollbar(1280, 720);
        assert(sb.visible);
        const float trackX = float(sb.track.x + sb.track.w / 2) / 1280.f;
        const float trackBottomY = float(sb.track.y + sb.track.h - 6) / 720.f;
        bool running = true;
        {
            SDL_Event down{}; down.type = SDL_FINGERDOWN; down.tfinger.fingerId = 77;
            down.tfinger.x = trackX; down.tfinger.y = float(sb.thumb.y + sb.thumb.h / 2) / 720.f;
            app.handleEvent(down, running);
            assert(app.scrollbarDragging_);
            SDL_Event mot{}; mot.type = SDL_FINGERMOTION; mot.tfinger.fingerId = 77;
            mot.tfinger.x = trackX; mot.tfinger.y = trackBottomY;
            app.handleEvent(mot, running);
            assert(app.listScroll_ > stoppedAt);
            assert(!app.doc_); // never opens a book while dragging scrollbar
            SDL_Event up{}; up.type = SDL_FINGERUP; up.tfinger.fingerId = 77;
            up.tfinger.x = trackX; up.tfinger.y = trackBottomY;
            app.handleEvent(up, running);
            assert(!app.scrollbarDragging_ && !app.doc_);
        }
        renderAndAssertSingleFocus(app, 1);

        // 7. B button -> Header -> Exit confirmation modal (default No, B cancels, Yes sets exitRequested_)
        app.viewMode_ = ViewMode::Grid;
        app.listScroll_ = 0;
        app.returnToLibraryBooks();
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Books);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.libraryZone_ == PaperNxApp::LibraryZone::Header && !app.exitConfirmOpen_);
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(app.exitConfirmOpen_);
        assert(app.navigationFocus_.focused("exit.no"));
        renderAndAssertSingleFocus(app, 1);
        saveShot(app, "exit-confirm-modal");
        dispatchPad(app, SDL_CONTROLLER_BUTTON_B);
        assert(!app.exitConfirmOpen_ && !app.exitRequested_);

        // Quick Menu exit action also opens confirmation instead of exiting immediately
        app.toggleQuickMenu();
        assert(app.quickMenu_);
        app.activateQuickAction(8); // "PaperNX'ten çık"
        assert(!app.quickMenu_ && app.exitConfirmOpen_);
        assert(app.navigationFocus_.focused("exit.no"));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        assert(app.navigationFocus_.focused("exit.yes"));
        dispatchPad(app, SDL_CONTROLLER_BUTTON_A);
        assert(!app.exitConfirmOpen_ && app.exitRequested_);
        app.exitRequested_ = false;
    }

    static void run(const fs::path &root) {
        fs::remove_all(root);
        fs::create_directories(root / "books");
        fs::create_directories(root / "data");
        fs::create_directory_symlink(fs::absolute("fonts"), root / "fonts");

        std::vector<Book> initial;
        for (int i = 1; i <= 25; ++i) {
            const std::string tag = std::string(1, char('a' + (i - 1))) + "_doc";
            const fs::path dst = root / "books" / ("item_" + tag + ".pdf");
            fs::copy_file("tests/fixtures/sample_doc.pdf", dst);
            Book b;
            b.path = dst.u8string();
            b.bytes = fs::file_size(dst);
            b.format = Format::Pdf;
            b.id = documentId(b.path, b.bytes);
            b.title = "Item " + tag;
            initial.push_back(b);
        }
        assert(InventoryRepository(root / "data" / "library-index.txt").save(initial));
        ProgressStore ps(root / "data" / "progress.txt");
        Progress savedProg;
        savedProg.page = 5;
        savedProg.bookPath = initial[1].path;
        savedProg.bookTitle = "User Custom Progress";
        ps.put(initial[1].id, savedProg);
        assert(ps.save());

        const fs::path newEpub = root / "books" / "Türkçe Örnek NoCover.epub";
        const fs::path newPdf = root / "books" / "yeni_belge.pdf";
        const fs::path newCbz = root / "books" / "yeni_cizgi.cbz";
        const fs::path newCbr = root / "books" / "yeni_rar5.cbr";
        fs::copy_file("tests/fixtures/metadata_missing.epub", newEpub);
        fs::copy_file("tests/fixtures/sample_doc.pdf", newPdf);
        fs::copy_file("tests/fixtures/comics/sample.cbz", newCbz);
        fs::copy_file("tests/fixtures/comics/sample-rar5.cbr", newCbr);

        Book cbrMeta;
        cbrMeta.format = Format::Cbr;
        cbrMeta.title = "Test CBR";
        assert(std::string(ui::coverBadgeLabel(cbrMeta.format)) == "CBR");
        assert(std::string(ui::coverEditionLabel(cbrMeta.format)) == "CBR COMIC");

        {
            PaperNxApp app(root);
            assert(initReady(app));
            auto prefs = app.appSettings_.get();
            prefs.libraryFolders.push_back((root / "missing-extra-root").u8string());
            app.appSettings_.put(prefs);
            app.startLibraryScan();
            settleScanner(app);

            assert(app.books_.size() == 29);
            assert(app.lastScanResult_.has_value());
            assert(app.lastScanResult_->partial || !app.lastScanResult_->complete);
            assert(!app.currentErrorMessage_.empty());
            assert(app.currentErrorIsFromScan_);
            assert(fs::exists(root / "data" / "library-scan.log"));
            saveShot(app, "library-partial-29books");

            app.openBrowser();
            app.browserPath_ = root / "books";
            app.scanBrowserDirectory();
            int browserBooks = 0;
            for (const auto &entry : app.browserEntries_) {
                if (!entry.isDirectory) ++browserBooks;
            }
            assert(browserBooks == 29);
            app.screen_ = ScreenState::Library;

            app.currentErrorMessage_ = "Ayarlar kaydedilemedi";
            app.currentErrorIsFromScan_ = false;
            prefs = app.appSettings_.get();
            prefs.libraryFolders.clear();
            app.appSettings_.put(prefs);
            app.startLibraryScan();
            settleScanner(app);
            assert(app.currentErrorMessage_ == "Ayarlar kaydedilemedi");
            assert(!app.currentErrorIsFromScan_);

            app.currentErrorMessage_ = "Old scan warning";
            app.currentErrorIsFromScan_ = true;
            app.startLibraryScan();
            settleScanner(app);
            assert(app.currentErrorMessage_.empty());

            // Scroll to newPdf so prioritizeCovers keeps it visible during background cover upload
            std::optional<Book> pdfBook;
            for (int i = 0; i < int(app.visibleBooks_.size()); ++i) {
                if (app.visibleBooks_[i].path == newPdf.u8string()) {
                    pdfBook = app.visibleBooks_[i];
                    ui::LibraryLayout l(1280);
                    app.listScroll_ = std::clamp(float((i / l.columns) * l.stepY), 0.f, app.maxUiScroll());
                    break;
                }
            }
            assert(pdfBook.has_value());
            app.getOrRenderThumbnail(*pdfBook);
            const auto coverDeadline = SDL_GetTicks() + 10000;
            bool pdfUploaded = false;
            while (SDL_GetTicks() < coverDeadline) {
                app.processCompletedThumbnails();
                if (app.thumbCache_.get("thumb_" + pdfBook->id)) {
                    pdfUploaded = true;
                    break;
                }
                SDL_Delay(5);
            }
            assert(pdfUploaded);
            app.listScroll_ = 0;

            testQuickMenuUnifiedFocusAndClipping(app);
            testQuickMenuOverflowPixelClip(app);
            testAaModalCatalogAndAlternatingInputs(app);
            testLibraryUxZonesExitScrollAndCovers(app);
        }

        {
            PaperNxApp appRestart(root);
            assert(appRestart.init());
            assert(appRestart.books_.size() == 29);
            assert(appRestart.progressStore_.get(initial[1].id).page == 5);
            settleScanner(appRestart);
            assert(appRestart.books_.size() == 29);
        }

        fs::remove_all(root);
    }
};

} // namespace

int main(int, char **) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    const fs::path root = fs::temp_directory_path() / "papernx-focus-regression-tests";
    AppTest::run(root);
    SDL_Quit();
    std::cout << "PASS: unified focus authority, single focus ring, clipping, partial scan UI, covers, and restart persistence\n";
    return 0;
}
