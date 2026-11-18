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

    static void run(const fs::path &root) {
        fs::remove_all(root);
        fs::create_directories(root / "books");
        fs::create_directories(root / "data");
        fs::create_directory_symlink(fs::absolute("fonts"), root / "fonts");

        std::vector<Book> initial;
        for (int i = 1; i <= 25; ++i) {
            const fs::path dst = root / "books" / ("baseline_" + (i < 10 ? "0" + std::to_string(i) : std::to_string(i)) + ".epub");
            fs::copy_file("tests/fixtures/sample_book.epub", dst);
            Book b;
            b.path = dst.u8string();
            b.bytes = fs::file_size(dst);
            b.format = Format::Epub;
            b.id = documentId(b.path, b.bytes);
            b.title = "Baseline " + std::to_string(i);
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
