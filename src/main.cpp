#include "papernx/series.hpp"
#include "papernx/core.hpp"
#include "papernx/mupdf_bridge.h"
#include "papernx/theme.hpp"
#include "papernx/typography.hpp"
#include "papernx/gestures.hpp"
#include "papernx/page_cache.hpp"
#include "papernx/page_previews.hpp"
#include "papernx/comic_pages.hpp"
#include "papernx/page_curl.hpp"
#include "papernx/reader_layout.hpp"
#include "papernx/ui.hpp"
#include "papernx/app_visuals.hpp"
#include "papernx/app_settings.hpp"
#include "papernx/cover_repository.hpp"
#include "papernx/orientation.hpp"
#include "papernx/pdf_gestures.hpp"
#include "papernx/focus.hpp"
#include "papernx/paper_texture.hpp"
#include "papernx/catalog.hpp"
#include "papernx/reading_stats.hpp"
#include "papernx/reading_journal.hpp"
#include "papernx/reading_journal_ui.hpp"
#include "papernx/cover_design.hpp"
#include "papernx/self_update.hpp"
#include "papernx/library_index.hpp"
#include <cstring>

#ifdef __SWITCH__
#include <switch.h>
#endif


#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cmath>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <vector>

using namespace papernx;

namespace {

constexpr unsigned thumbnailBurstBeforeMetadata = 3;
bool shouldProcessCoverMetadata(bool hasThumbnails, bool hasMetadata, unsigned thumbnailBurst) {
    return hasMetadata && (!hasThumbnails || thumbnailBurst >= thumbnailBurstBeforeMetadata);
}

// devkitPro SDL2's built-in Switch mapping swaps A/B and X/Y. Keep labels
// consistent with the native pad_mapping_default[] button indices.
[[maybe_unused]] std::string switchControllerMapping(const char *guid) {
    return std::string(guid) + ",PaperNX Switch,a:b0,b:b1,x:b2,y:b3,"
        "leftstick:b4,rightstick:b5,leftshoulder:b6,rightshoulder:b7,"
        "lefttrigger:b8,righttrigger:b9,start:b10,back:b11,"
        "dpleft:b12,dpup:b13,dpright:b14,dpdown:b15,"
        "leftx:a0,lefty:a1,rightx:a2,righty:a3,";
}

void prepareControllerMapping(int index) {
#ifdef __SWITCH__
    const char *name = SDL_JoystickNameForIndex(index);
    if (name && std::string(name) == "Switch Controller") {
        char guid[33] = {};
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index), guid, sizeof guid);
        SDL_GameControllerAddMapping(switchControllerMapping(guid).c_str());
    }
#else
    (void)index;
#endif
}

enum class ScreenState {
    Library,
    Reader,
    FileBrowser,
    AaModal,
    TocModal,
    BookmarksModal,
    ReadingJournal,
    JournalPreview
};

enum class ViewMode {
    Grid,
    List
};

struct BrowserEntry {
    fs::path path;
    bool isDirectory{false};
    std::uintmax_t size{0};
};

struct LoadedThumb {
    std::string id;
    int w{0};
    int h{0};
    int stride{0};
    std::vector<unsigned char> rgb;
    CoverState state{CoverState::Loaded};
};
struct LoadedMetadata {
    std::string id,title,author,series,seriesIndex;
    std::string fingerprint;
};

class PaperNxApp {
public:
    static constexpr int QUICK_HEADER_HEIGHT=112;
    explicit PaperNxApp(fs::path rootPath,std::shared_ptr<SelfUpdater> updater=std::make_shared<SelfUpdater>())
        : root_(std::move(rootPath)),
          updater_(std::move(updater)),
          progressStore_(root_ / "data" / "progress.txt"),
          appSettings_(root_ / "data" / "settings.txt"),
          coverRepository_(root_ / "data" / "covers"),
          pageCache_(64 * 1024 * 1024),
          thumbCache_(32 * 1024 * 1024) {}

    ~PaperNxApp() {
        libraryScanner_.stop();
        updater_->stop();
        restoreSystemBrightness();
        stopUpdateNetwork();
        pagePreviews_.stop();
        comicPages_.stop();
        exportTimings();
        if(brightnessDragRow_>=0)persistAppSettings();
        if(appSavePending_)appSettings_.save();
        if(catalogSavePending_)catalog_.save();
        if(layoutWorker_.joinable()) layoutWorker_.join();
        if(preparedDoc_) { px_document_close(preparedDoc_); preparedDoc_=nullptr; }
        // 1. Stop background thumbnail worker thread safely
        workerRunning_ = false;
        queueCv_.notify_all();
        if (thumbWorker_.joinable()) {
            thumbWorker_.join();
        }

        // 2. Close active document
        closeBook();

        flushProgress();
        readingStats_.save();
        journal_.save();

        // 3. Crucial: Release all textures and fonts BEFORE destroying renderer
        pageCache_.clear();
        pagePreviewCache_.clear();
        thumbCache_.clear();
        interfaceBackground_.reset();
        typography_.reset();

        if (controller_) {
            SDL_GameControllerClose(controller_);
            controller_ = nullptr;
        }
        if (orientationTarget_) SDL_DestroyTexture(orientationTarget_);
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
#ifdef __SWITCH__
        if (romfsReady_) romfsExit();
#endif
    }

    void logMessage(const std::string &msg) {
        std::error_code ec;
        fs::path logDir = root_ / "logs";
        fs::create_directories(logDir, ec);
        fs::path logFile = logDir / "papernx.log";
        auto sz = fs::file_size(logFile, ec);
        std::ofstream out(logFile, (sz > 256 * 1024) ? (std::ios::trunc | std::ios::out) : (std::ios::app | std::ios::out));
        if (out) {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timeBuf[32];
            std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            out << "[" << timeBuf << "] " << msg << "\n";
        }
    }

    bool init() {
#ifdef __SWITCH__
        Result rc = romfsInit();
        romfsReady_ = R_SUCCEEDED(rc);
        if (R_FAILED(rc)) {
            std::fprintf(stderr, "Warning: romfsInit failed with rc=0x%x\n", rc);
        }
#endif
        std::error_code ec;
        fs::create_directories(root_ / "books", ec);
        fs::create_directories(root_ / "data", ec);
        fs::create_directories(root_ / "fonts", ec);
        fs::create_directories(root_ / "logs", ec);

        logMessage("Starting PaperNX application...");
        debugTouch_ = fs::exists(root_ / "data" / "touch-debug.flag");
        progressStore_.load();
        catalog_.load();
        readingStats_.load();
        if(!journal_.load()) logMessage("Reading journal recovery: "+journal_.diagnostic());
        appSettings_.load();
        ui::language=appSettings_.get().language;
        viewMode_=appSettings_.get().libraryLayout ? ViewMode::List : ViewMode::Grid;
        sort_=static_cast<Sort>(appSettings_.get().sort);
        books_ = loadLibraryInventory(root_/"data"/"library-index.txt");
        restoreLibraryLabels(!appSettings_.get().scanOnLaunch);

        SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
        window_ = SDL_CreateWindow("PaperNX", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
        if (!window_) {
            logMessage("Failed to create SDL window");
            return false;
        }

        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer_) {
            logMessage("Failed to create SDL renderer");
            return false;
        }

        typography_ = std::make_unique<Typography>(root_);
        if (!typography_->init()) {
            logMessage("Warning: Default typography missing, fallback active");
        }

        // Open first available game controller if connected
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            prepareControllerMapping(i);
            if (SDL_IsGameController(i)) {
                controller_ = SDL_GameControllerOpen(i);
                if (controller_) {
                    logMessage("Primary controller attached");
                    break;
                }
            }
        }

        // Start background thumbnail worker thread
        scheduleMetadataIndex();
        workerRunning_ = true;
        thumbWorker_ = std::thread([this]() {
            thumbWorkerLoop();
        });

        updateVisibleBooks();
        if(appSettings_.get().scanOnLaunch)startLibraryScan();
        dirty_ = true;
        return true;
    }

    void startUpdates(int argc,char **argv) {
        std::string path=argc>0&&argv&&argv[0]?argv[0]:"";
#ifdef __SWITCH__
        if(!path.empty()&&path[0]=='/')path="sdmc:"+path;
        updater_->configure(path,APP_VERSION,"romfs:/cacert.pem");
#else
        updater_->configure(path,APP_VERSION);
#endif
        autoUpdateCheckPending_=appSettings_.get().automaticUpdateCheck;
    }
    void showStartupMessage(std::string message) {currentErrorMessage_=std::move(message);dirty_=true;}
    void confirmBootAfterFirstRender() {
        if(bootFrameConfirmed_)return;
        bootFrameConfirmed_=true;std::string error;
        if(!updater_->confirmBoot(error)){logMessage("Update startup confirmation: "+error);currentErrorMessage_=error;dirty_=true;}
    }

    bool startUpdateNetwork() {
#ifdef __SWITCH__
        if(networkReady_)return true;
        if(!socketsReady_) {
            if(R_FAILED(socketInitializeDefault()))return false;
            socketsReady_=true;
        }
        if(R_FAILED(nifmInitialize(NifmServiceType_User)))return false;
        networkReady_=true;
#endif
        return true;
    }
    void stopUpdateNetwork() {
#ifdef __SWITCH__
        if(networkReady_){nifmExit();networkReady_=false;}
        if(socketsReady_){socketExit();socketsReady_=false;}
#endif
    }
    bool setSystemBrightness(int percent) {
#ifdef __SWITCH__
        if(backlightUnavailable_)return false;
        if(!backlightReady_) {
            if(appletGetOperationMode()!=AppletOperationMode_Handheld || R_FAILED(lblInitialize())) {
                backlightUnavailable_=true;return false;
            }
            backlightReady_=true;
            bool automatic=false;
            if(R_FAILED(lblIsAutoBrightnessControlEnabled(&automatic)) || automatic ||
               R_FAILED(lblGetCurrentBrightnessSetting(&backlightOriginal_)) ||
               !std::isfinite(backlightOriginal_) || backlightOriginal_<0.f || backlightOriginal_>1.f) {
                lblExit();backlightReady_=false;backlightUnavailable_=true;return false;
            }
            backlightOriginalReady_=true;
        }
        if(R_FAILED(lblSetCurrentBrightnessSetting(std::clamp(percent,10,100)/100.f))) {
            restoreSystemBrightness();backlightUnavailable_=true;return false;
        }
        backlightAvailable_=true;return true;
#else
        (void)percent;return false;
#endif
    }
    void restoreSystemBrightness() {
#ifdef __SWITCH__
        if(backlightReady_) {
            if(backlightOriginalReady_)lblSetCurrentBrightnessSetting(backlightOriginal_);
            lblExit();backlightReady_=false;backlightOriginalReady_=false;
        }
#endif
        backlightAvailable_=false;
    }

    void run() {
        bool running = true;
        while (running) {
#ifdef __SWITCH__
            if (!appletMainLoop()) break;
#endif
            SDL_Event e;
            auto dispatch=[&](const SDL_Event &event) {
                const bool wasTurning=pageTurnAnimation_.active;handleEvent(event,running);
                if(dirty_ && (event.type==SDL_FINGERDOWN || event.type==SDL_MOUSEBUTTONDOWN ||
                   (!wasTurning && pageTurnAnimation_.active && !pageTurnAnimation_.dragging))) {render();dirty_=false;confirmBootAfterFirstRender();}
            };
            // Event-driven loop: wait up to 100ms for events, then flush and draw if dirty
            bool receivedEvent=SDL_WaitEventTimeout(&e, (continuous() && std::abs(scrollVelocity_) > 1) || pageTurnAnimation_.active || std::abs(panVelocityX_)+std::abs(panVelocityY_)>2 || std::abs(listVelocity_)>1 || layoutPending_ || layoutWorker_.joinable() ? 8 : controller_ || controllerMotionActive() || pagePreviews_.pending() || comicPages_.pending() ? 16 : 100);
            if (receivedEvent) {
                dispatch(e);
                int events=0;
                while (events++<48 && SDL_PollEvent(&e)) {
                    dispatch(e);
                }
            }

            if (!pageTurnAnimation_.active && progressDirty_ && std::chrono::steady_clock::now() - lastProgressSave_ >
                    std::chrono::seconds(saveRetrySeconds_)) flushProgress();

            pollUpdates();
            tickControllerInput();
            tickPageNavigation();
            tickScroll();
            tickComicPan();
            tickListScroll();
            tickPageTurn();
            tickLibraryHold();
            tickReadingStats();
            processLayout();
            processLibraryScan();
            if(exitRequested_) running=false;
            // Process any newly rendered background thumbnails
            processCompletedThumbnails();
            processPagePreviews();
            processComicPages();
            // Queue work even while the user pans or a turn animates. This
            // scheduling performs no archive reads and no image rasterization.
            if(comic() && screen_==ScreenState::Reader && !continuous() && !quickMenu_ && !appBackground_)prefetchComicPages();

            if (hintUntil_ && SDL_GetTicks() > hintUntil_) { hintUntil_ = 0; dirty_ = true; }
            if (dirty_ && running) {
                render();
                dirty_ = false;
                confirmBootAfterFirstRender();
            } else if(!receivedEvent && running) idlePrefetch();
            if(backlightApplyPending_ && running && !appBackground_) {
                backlightApplyPending_=false;
                backlightAvailable_=setSystemBrightness(appSettings_.get().systemBrightness);
            }
            if(autoUpdateCheckPending_ && running && !appBackground_) {
                autoUpdateCheckPending_=false;
                if(startUpdateNetwork())updater_->check();
                else updater_->reportError("Network services are unavailable");
            }
            if(appSavePending_ && !dirty_ && SDL_GetTicks()>=appSaveRetryAt_) {appSavePending_=!appSettings_.save();if(appSavePending_){appSaveRetryAt_=SDL_GetTicks()+3000;currentErrorMessage_="Settings could not be saved";dirty_=true;}}
            // Persist newly extracted metadata only after the first responsive reader frame.
            if(catalogSavePending_ && !dirty_ && !pageTurnAnimation_.active && SDL_GetTicks()>=catalogSaveRetryAt_) {
                catalogSavePending_=!catalog_.save();
                if(catalogSavePending_) {catalogSaveRetryAt_=SDL_GetTicks()+3000;currentErrorMessage_="Book metadata could not be saved";dirty_=true;}
            }

        }
        closeBook();
        flushProgress();
        readingStats_.save();
        journal_.save();
    }

private:
    friend struct AppTest;
    fs::path root_;
    std::shared_ptr<SelfUpdater> updater_;
    ProgressStore progressStore_;
    AppSettingsStore appSettings_;
    ReadingJournal journal_{root_/"data"/"reading_journal"};
    ReadingJournalUI journalUI_;
    ScreenState journalReturn_{ScreenState::Library};
    std::string journalTitle_,journalAuthor_;
    CoverRepository coverRepository_;
    PageCache pageCache_;
    PageCache thumbCache_;
    std::unique_ptr<Typography> typography_;

    bool romfsReady_{false};
#ifdef __SWITCH__
    bool socketsReady_{false},networkReady_{false};
    bool backlightReady_{false},backlightOriginalReady_{false},backlightUnavailable_{false};
    float backlightOriginal_{1.f};
#endif
    bool backlightApplyPending_{true},backlightAvailable_{false},autoUpdateCheckPending_{false};
    bool bootFrameConfirmed_{false};
    SDL_FingerID brightnessFinger_{0};
    int brightnessDragRow_{-1};
    SDL_Window *window_{nullptr};
    SDL_Renderer *renderer_{nullptr};
    SDL_Texture *orientationTarget_{nullptr};
    int orientationWidth_{0}, orientationHeight_{0};
    bool settingsEditing_{false};
    ReaderPreferences editOriginal_;
    float editOriginalZoom_{1};
    int settingsColumn_{0}, quickFocus_{0}, swatchFocus_{0};
    Uint32 pdfGestureStart_{0};
    int pdfStartPanX_{0},pdfStartPanY_{0};
    SDL_GameController *controller_{nullptr};

    ScreenState screen_{ScreenState::Library};
    ViewMode viewMode_{ViewMode::Grid};
    Category category_{Category::All};
    Sort sort_{Sort::Title};

    std::vector<Book> books_;
    std::vector<Book> visibleBooks_;
    int selectedBookIdx_{0};
    int scrollOffset_{0};
    std::string searchQuery_;
    bool searchActive_{false};

    // Reader state
    PxDocument *doc_{nullptr};
    int epubReadingDirection_{1};
    std::string currentBookId_;
    int currentPage_{0};
    int pageCount_{0};
    using PageTurnTexture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    struct PageTurnAnimation {
        PageTurnTexture previous{nullptr, SDL_DestroyTexture};
        PageTurnTexture next{nullptr, SDL_DestroyTexture};
        PageTurnTexture back{nullptr, SDL_DestroyTexture};
        PageTurnTexture under{nullptr, SDL_DestroyTexture};
        bool spread{false};
        bool active{false};
        int page{-1}, targetPage{-1};
        bool interactive{false}, dragging{false}, complete{true};
        float progress{0}, from{0}, to{1}, anchorY{.78f}, dragY{0};
        Uint32 duration{460};
        int direction{1};
        int queued{0};
        int width{0}, height{0};
        Uint32 started{0};
        SDL_Rect viewport{0,0,0,0};
        SDL_Rect nextViewport{0,0,0,0};
        bool comicSheet{false};
        PageTurnTexture paperBack{nullptr,SDL_DestroyTexture};
        std::string comicFolioCaption;
        SDL_Rect oppositeViewport{};
    } pageTurnAnimation_;
    struct PageTurnTouch {
        bool tracking{false};
        SDL_FingerID finger{0};
        float startX{0}, startY{0}, lastX{0}, velocity{0};
        Uint32 lastTick{0};
        bool paperContact{false};
        bool waitingForTexture{false};
        int pendingTarget{-1}, pendingDirection{1};
        float currentX{0}, currentY{0};
    } pageTurnTouch_;
    struct PendingComicTurn {
        bool active{false};
        std::string identity;
        int anchorPage{0}, steps{0}, targetPage{-1};
    } comicPendingTurn_;
    std::string comicTurnError_;
    static constexpr Uint32 PAGE_TURN_MS = 460;
    float scrollY_{0}, scrollVelocity_{0};
    float scrollLastX_{0}, scrollLastY_{0};
    bool scrollTracking_{false}, scrollMoved_{false};
    Uint32 scrollTick_{0}, physicsTick_{0};
    bool globalSettings_{false};
    bool quickMenu_{false}, helpMenu_{false}, exitRequested_{false};
    ScreenState settingsReturn_{ScreenState::Library};
    int documentMode_{0};
    float listScroll_{0}, listVelocity_{0}, listLastY_{0};
    float settingsScroll_{0}, quickScroll_{0}, browserScroll_{0}, tocScroll_{0}, bookmarkScroll_{0}, journalScroll_{0};
    bool listTracking_{false}, listMoved_{false};
    Uint32 listTick_{0}, listPhysicsTick_{0};
    struct HistoryPosition { int page; float y; int chapter,character; double offset; };
    std::deque<HistoryPosition> navigationHistory_;
    std::thread layoutWorker_;
    std::atomic<bool> layoutReady_{false};
    PxDocument *preparedDoc_{nullptr};
    ReaderPreferences preparedStyle_;
    ReaderPreferences renderedStyle_;
    std::string preparedError_;
    unsigned layoutGeneration_{0}, preparedGeneration_{0};
    bool layoutPending_{false};
    Uint32 layoutDue_{0};
    int preparedWidth_{0},preparedHeight_{0};
    int layoutWidth_{0},layoutHeight_{0};
    std::uint64_t completedLayouts_{0};
    Uint32 prefetchDue_{0};
    int prefetchPage_{-1}, prefetchStep_{0};

    ReaderPreferences extraStyle_;
    int currentTheme_{0};
    int currentFontSize_{22};
    int currentLineHeight_{140};
    int currentMargin_{40};
    int currentTextAlign_{0};
    int currentFontFamily_{0}; // 0: Serif, 1: Sans
    float pdfZoom_{1.0f};
    int pdfPanX_{0};
    int pdfPanY_{0};
    bool toolbarVisible_{false};
    bool pdfDragging_{false};
    bool pdfMoved_{false};
    SDL_FingerID pdfFinger_{0};
    float pdfTouchX_{0}, pdfTouchY_{0};
    float pdfLastMotionX_{0.f};
    Uint32 pdfLastMotionTick_{0};
    float pdfReleaseVelocityX_{0.f};
    std::string currentErrorMessage_;
    std::string pageRenderError_;
    int comicScrollWidth_{0};
    unsigned comicTextureBuilds_{0};
    bool zoomResetTracking_{false};
    bool zoomReturnTop_{false};
    SDL_FingerID zoomResetFinger_{0};
    float panVelocityX_{0},panVelocityY_{0},panInertiaX_{0},panInertiaY_{0};
    float panLastY_{0};
    Uint32 panPhysicsTick_{0};
    ComicPages comicPages_;
    struct ComicDetail {std::string identity;PageTurnTexture texture{nullptr,SDL_DestroyTexture};};
    std::map<int,ComicDetail> comicDetails_;
    std::string comicPagesIdentity_;
    int comicPrefetchDirection_{1};
    std::map<int,Uint32> comicDetailRetry_,comicBaseRetry_;
    Uint32 comicDetailDue_{0};

    // Table of contents and bookmarks
    std::vector<PxTocEntry> tocEntries_;
    int tocSelected_{0};
    int tocOffset_{0};
    int bmSelected_{0};
    int bmOffset_{0};

    // Modal navigation
    int aaTab_{0};
    int aaRow_{0}; // 0: Tipografi, 1: Düzen, 2: Temalar, 3: Kontroller

    // File browser
    fs::path browserPath_;
    fs::path browserRoot_;
    std::vector<BrowserEntry> browserEntries_;
    int browserSelected_{0};
    int browserOffset_{0};

    // Gestures and input
    TouchGestureTracker gestureTracker_;
    ControllerNavigator controllerNav_;
    bool readerToolbarFocusActive_{false};
    bool dirty_{true};
    bool progressDirty_{false};
    bool appSavePending_{false};
    Uint32 appSaveRetryAt_{0};
    int saveRetrySeconds_{3};
    std::string lastSaveFailure_;
    std::set<SDL_FingerID> activeFingers_;
    bool multiTouch_{false};
    std::set<SDL_FingerID> modalFingers_;
    SDL_FingerID modalOwner_{0};
    bool modalOwned_{false},modalCancelled_{false};
    float modalStartX_{0},modalStartY_{0};
    std::string modalTouchContext_;
    bool modalTouchGuard(const SDL_Event &e) {
        const bool active=colorEditing_>=0 || choiceKind_>=0 || catalogPanel_!=CatalogPanel::None;
        const std::string context=std::to_string(colorEditing_)+":"+std::to_string(choiceKind_)+":"+std::to_string(int(catalogPanel_));
        if(context!=modalTouchContext_){catalogFingers_.clear();catalogTouchOwned_=false;modalFingers_.clear();modalOwned_=false;modalCancelled_=false;modalTouchContext_=context;}
        if(!active)return false;
        if(e.type==SDL_FINGERDOWN){if(modalFingers_.empty()){modalOwner_=e.tfinger.fingerId;modalOwned_=true;modalCancelled_=false;modalStartX_=e.tfinger.x;modalStartY_=e.tfinger.y;}
            modalFingers_.insert(e.tfinger.fingerId);if(modalFingers_.size()>1){modalCancelled_=true;catalogTouchOwned_=false;catalogFingers_.clear();ui::pointer(0,0,false);dirty_=true;return true;}return false;}
        if(e.type==SDL_FINGERMOTION){if(!modalOwned_ || modalCancelled_ || e.tfinger.fingerId!=modalOwner_)return true;
            int w,h;getWindowSize(w,h);if(catalogPanel_==CatalogPanel::None && choiceKind_<0 && std::hypot((e.tfinger.x-modalStartX_)*w,(e.tfinger.y-modalStartY_)*h)>12)modalCancelled_=true;return modalCancelled_;}
        if(e.type==SDL_FINGERUP){if(libraryHoldConsumed_){libraryHoldConsumed_=false;activeFingers_.clear();modalOwned_=false;modalFingers_.erase(e.tfinger.fingerId);return true;}
            const bool valid=modalOwned_ && !modalCancelled_ && e.tfinger.fingerId==modalOwner_;modalFingers_.erase(e.tfinger.fingerId);
            if(e.tfinger.fingerId==modalOwner_)modalOwned_=false;
            if(modalFingers_.empty())modalCancelled_=false;
            ui::pointer(0,0,false);dirty_=true;return !valid;}
        return false;
    }
    bool pointerActive_{false};
    Uint32 lastFingerTick_{0};
    bool sawFinger_{false};
    float pressX_{0}, pressY_{0};
    struct TimingEvent { Uint64 counter; const char *stage; };
    std::deque<TimingEvent> timingRing_;
    void trace(const char *stage) {if(timingRing_.size()==512)timingRing_.pop_front();timingRing_.push_back({SDL_GetPerformanceCounter(),stage});}
    void exportTimings() {
        if(!fs::exists(root_/"data"/"timing-debug.flag") || timingRing_.empty())return;
        std::ofstream out(root_/"logs"/"interaction-timing.csv");out<<"elapsed_ms,stage\n";
        for(const auto &event:timingRing_)out<<double(event.counter-timingRing_.front().counter)*1000/SDL_GetPerformanceFrequency()<<","<<event.stage<<"\n";
    }
    bool debugTouch_{false};
    Uint32 hintUntil_{0};
    std::string lastTouchDebug_;
    int colorEditing_{-1};
    std::string colorInput_;
    std::chrono::steady_clock::time_point lastProgressSave_{std::chrono::steady_clock::now()};

    // Background thumbnail worker
    std::thread thumbWorker_;
    std::atomic<bool> workerRunning_{false};
    std::atomic<bool> coversPaused_{false};
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    std::atomic<std::size_t> coverDiskEntries_{0};
    std::atomic<std::uintmax_t> coverDiskBytes_{0};
    std::atomic<std::uint64_t> coverDiskHits_{0}, coverDiskMisses_{0};
    std::atomic<int> coverClearStatus_{0}; // 0 idle, 1 pending, 2 success, 3 failed
    bool coverClearRequested_{false}; // guarded by queueMutex_
    bool coverClearConfirm_{false};
    std::deque<Book> thumbQueue_;
    std::deque<Book> metadataQueue_;
    unsigned thumbnailJobsSinceMetadata_{};
    std::set<std::string> metadataIndexedIds_;
    std::set<std::string> queuedThumbIds_;
    std::map<std::string, std::chrono::steady_clock::time_point> failedThumbIds_;
    std::map<std::string,CoverState> coverStates_;

    std::mutex completedMutex_;
    std::vector<LoadedThumb> completedThumbs_;
    std::vector<LoadedMetadata> completedMetadata_;

    // Caller holds queueMutex_. Pending IDs include queued, processing and upload-ready work.
    void failThumbnail(const std::string &id) {
        queuedThumbIds_.erase(id);
        if (failedThumbIds_.size() >= 512) failedThumbIds_.erase(failedThumbIds_.begin());
        failedThumbIds_[id] = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    }

    static std::string bookMetadataFingerprint(const Book &book) {
        return std::to_string(book.bytes)+":"+std::to_string(book.modified)+":series-v1";
    }
    void scheduleMetadataIndex() {
        std::lock_guard<std::mutex> lock(queueMutex_);
        for(const auto &book:books_) {
            const auto fingerprint=bookMetadataFingerprint(book);
            if(catalog_.get(book.id).metadataFingerprint!=fingerprint && metadataIndexedIds_.insert(book.id+":"+fingerprint).second)
                metadataQueue_.push_back(book);
        }
        queueCv_.notify_one();
    }
    bool collectBookMetadata(const Book &book,PxDocument *document=nullptr) {
        bool owned=document==nullptr;char error[256]{};
        if(owned)document=px_document_open_metadata(book.path.c_str(),error,sizeof error,isComic(book.format)?96u*1024u*1024u:16u*1024u*1024u);
        if(!document)return false;
        char title[512]{},author[512]{},series[512]{},index[64]{};
        px_document_metadata(document,"info:Title",title,sizeof title);
        px_document_metadata(document,"info:Author",author,sizeof author);
        px_document_metadata(document,"info:Series",series,sizeof series);
        px_document_metadata(document,"info:SeriesIndex",index,sizeof index);
        {std::lock_guard<std::mutex> lock(completedMutex_);completedMetadata_.push_back({book.id,title,author,series,index,bookMetadataFingerprint(book)});}
        if(owned)px_document_close(document);
        return true;
    }
    static std::string formatBytes(std::uintmax_t bytes) {
        if (bytes >= 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
        if (bytes >= 1024) return std::to_string(bytes / 1024) + " KB";
        return std::to_string(bytes) + " B";
    }
    void updateCoverCounters() {
        const auto values = coverRepository_.stats();
        coverDiskEntries_ = values.entries;
        coverDiskBytes_ = values.bytes;
        coverDiskHits_ = values.hits;
        coverDiskMisses_ = values.misses;
    }
    void requestCoverCacheClear() {
        if (coverClearStatus_.load() == 1) return;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            thumbQueue_.clear();
            queuedThumbIds_.clear();
            failedThumbIds_.clear();
            coverClearRequested_ = true;
            coverClearStatus_ = 1;
        }
        queueCv_.notify_one();
        dirty_ = true;
    }
    void thumbWorkerLoop() {
        updateCoverCounters();
        while (workerRunning_) {
            Book b;bool metadataOnly=false, clearNow=false;
            {
                std::unique_lock<std::mutex> lock(queueMutex_);
                queueCv_.wait(lock, [this]() {
                    return !workerRunning_ || coverClearRequested_ ||
                        (coverClearStatus_.load()!=1 && !coversPaused_ &&
                         (!thumbQueue_.empty() || !metadataQueue_.empty()));
                });
                if (!workerRunning_) break;
                if (coverClearRequested_) {
                    coverClearRequested_ = false;
                    clearNow = true;
                } else {
                    metadataOnly=shouldProcessCoverMetadata(!thumbQueue_.empty(),!metadataQueue_.empty(),thumbnailJobsSinceMetadata_);
                    auto &queue=metadataOnly?metadataQueue_:thumbQueue_;b=queue.front();queue.pop_front();
                    if(metadataOnly)thumbnailJobsSinceMetadata_=0;
                    else thumbnailJobsSinceMetadata_=std::min(thumbnailJobsSinceMetadata_+1,thumbnailBurstBeforeMetadata);
                }
            }
            if (clearNow) {
                const bool ok = coverRepository_.clear();
                updateCoverCounters();
                coverClearStatus_ = ok ? 2 : 3;
                continue;
            }
            if(metadataOnly) {
                if(!collectBookMetadata(b)) {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    metadataIndexedIds_.erase(b.id+":"+bookMetadataFingerprint(b));
                }
                continue;
            }

            auto cached = coverRepository_.read(b);
            updateCoverCounters();
            if(cached) {
                LoadedThumb lt; lt.id=b.id; lt.w=cached->width; lt.h=cached->height;
                lt.stride=cached->stride; lt.rgb=std::move(cached->rgb);
                lt.state=CoverState::Cached;
                {std::lock_guard<std::mutex> lock(completedMutex_);completedThumbs_.push_back(std::move(lt));}
                continue;
            }
            char err[256] = {};
            PxDocument *thumbDoc = px_document_open_limited(b.path.c_str(),160,220,16.0f,"",err,sizeof err,isComic(b.format)?96u*1024u*1024u:16u*1024u*1024u);
            if (!thumbDoc) {
                std::lock_guard<std::mutex> lock(queueMutex_);
                coverStates_[b.id]=CoverState::Failed;
                failThumbnail(b.id);
                continue;
            }
            // Metadata extraction happens in the existing bounded cover worker,
            // not on the UI thread. A book without an illustration can still
            // have a real title and author in OPF/PDF metadata.
            collectBookMetadata(b,thumbDoc);
            if(b.format==Format::Epub && !px_document_has_embedded_cover(thumbDoc)) {
                px_document_close(thumbDoc);
                std::lock_guard<std::mutex> lock(queueMutex_);
                coverStates_[b.id]=CoverState::NoEmbeddedCover; queuedThumbIds_.erase(b.id);
                continue;
            }

            PxImage img{};
            if (px_document_render_thumbnail(thumbDoc, 150, 210, &img, err, sizeof(err)) && img.rgb && img.width > 0 && img.height > 0) {
                LoadedThumb lt;
                lt.id = b.id;
                lt.w = img.width;
                lt.h = img.height;
                lt.stride = img.stride;
                lt.rgb.assign(img.rgb, img.rgb + (size_t)img.height * img.stride);
                px_image_free(&img);
                coverRepository_.write(b, CoverImage{lt.w,lt.h,lt.stride,lt.rgb});
                updateCoverCounters();

                {
                    std::lock_guard<std::mutex> lock(completedMutex_);
                    completedThumbs_.push_back(std::move(lt));
                }
            } else {
                std::lock_guard<std::mutex> lock(queueMutex_);
                coverStates_[b.id]=CoverState::Failed;
                failThumbnail(b.id);
                logMessage(std::string("Cover decode failed: ")+err);
            }
            px_image_free(&img);
            px_document_close(thumbDoc);
        }
    }

    void processCompletedThumbnails() {
        // A reader opening has priority over background library cover uploads.
        // Never regroup the card under a finger between press and release.
        if(coversPaused_ || !activeFingers_.empty() || pointerActive_)return;
        std::vector<LoadedMetadata> metadata;
        {
            std::lock_guard<std::mutex> lock(completedMutex_);
            const auto count=std::min<std::size_t>(32,completedMetadata_.size());
            metadata.insert(metadata.end(),std::make_move_iterator(completedMetadata_.begin()),
                            std::make_move_iterator(completedMetadata_.begin()+count));
            completedMetadata_.erase(completedMetadata_.begin(),completedMetadata_.begin()+count);
        }
        bool metadataChanged=false;
        if(libraryLookup_.size()!=books_.size())rebuildLibraryLookup();
        for(const auto &md:metadata) {
            const auto found=libraryLookup_.find(md.id);if(found==libraryLookup_.end())continue;
            const auto *book=&books_[found->second];
            const auto fingerprint=bookMetadataFingerprint(*book);
            if(md.fingerprint!=fingerprint)continue;
            auto entry=catalog_.get(md.id);
            if(entry.sourceTitle==md.title && entry.sourceAuthor==md.author &&
               entry.sourceSeries==md.series && entry.sourceSeriesIndex==md.seriesIndex &&
               entry.metadataFingerprint==fingerprint)continue;
            entry.sourceTitle=md.title;entry.sourceAuthor=md.author;entry.metadataFingerprint=fingerprint;
            entry.sourceSeries=md.series;entry.sourceSeriesIndex=md.seriesIndex;
            catalog_.put(md.id,entry);catalogSavePending_=true;metadataChanged=true;
        }
        if(metadataChanged){
            float previous=listScroll_;std::string selected;
            const bool focusedBook=navigationFocus_.active().rfind("book.",0)==0;
            if(selectedBookIdx_>=0 && selectedBookIdx_<int(visibleBooks_.size()))selected=visibleBooks_[selectedBookIdx_].id;
            updateVisibleBooks();
            for(int i=0;i<int(visibleBooks_.size());++i) {
                bool match=visibleBooks_[i].id==selected;
                auto shelf=seriesShelves_.find(visibleBooks_[i].id);
                if(shelf!=seriesShelves_.end())for(const auto &issue:shelf->second.issues)match=match || issue.id==selected;
                if(match){selectedBookIdx_=i;break;}
            }
            listScroll_=std::clamp(previous,0.f,maxUiScroll());dirty_=true;
            if(focusedBook && screen_==ScreenState::Library && !quickMenu_ && catalogPanel_==CatalogPanel::None) {
                rebuildFocus();navigationFocus_.select("book."+std::to_string(selectedBookIdx_));
            }
        }
        std::set<std::string> visibleCovers;
        bool filterVisibleCovers=false;
        if(screen_==ScreenState::Library) {
            bool haveCompleted=false;
            {std::lock_guard<std::mutex> lock(completedMutex_);haveCompleted=!completedThumbs_.empty();}
            if(haveCompleted){int w,h;getWindowSize(w,h);visibleCovers=prioritizeCovers(w,h);filterVisibleCovers=true;}
        }
        std::vector<LoadedThumb> toUpload;
        {
            std::lock_guard<std::mutex> lock(completedMutex_);
            for(auto it=completedThumbs_.begin();it!=completedThumbs_.end();) {
                if(filterVisibleCovers && !visibleCovers.count(it->id)) {
                    std::lock_guard<std::mutex> queueLock(queueMutex_);
                    queuedThumbIds_.erase(it->id);
                    if(coverStates_[it->id]==CoverState::Loading)coverStates_[it->id]=CoverState::NotRequested;
                    it=completedThumbs_.erase(it);continue;
                }
                // Cap renderer-thread texture uploads to two covers per frame.
                if(toUpload.size()<2) {
                    toUpload.push_back(std::move(*it));it=completedThumbs_.erase(it);continue;
                }
                ++it;
            }
        }
        if(toUpload.empty())return;
        for (const auto &lt : toUpload) {
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                queuedThumbIds_.erase(lt.id);
                coverStates_[lt.id]=lt.state;
            }
            std::string key = "thumb_" + lt.id;
            SDL_Texture *tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, lt.w, lt.h);
            if (tex) {
                if (SDL_UpdateTexture(tex, nullptr, lt.rgb.data(), lt.stride) == 0)
                    thumbCache_.put(key, tex, lt.w, lt.h);
                else SDL_DestroyTexture(tex);
                dirty_ = true;
            }
        }
    }

    void getWindowSize(int &w, int &h) {
        SDL_GetWindowSize(window_, &w, &h);
        if(appSettings_.get().displayOrientation) std::swap(w,h);
    }

    ReaderLayout epubLayout(int w,int h,const ReaderPreferences &style) const {
        const auto &prefs=appSettings_.get();
        return {w,h,std::clamp(style.margin,0,120),prefs.bookLandscapeSpreads && w>h && !style.readingMode,prefs.expandedViewport};
    }
    ReaderLayout readerLayout() {
        int w,h;getWindowSize(w,h);
        const auto &prefs=appSettings_.get();
        const bool spread=comic()?comicSpreadPages(currentPage_).second==2:
            (px_document_is_epub(doc_) || std::abs(pdfZoom_-1.f)<.01f);
        return {w,h,std::clamp(presentedStyle().margin,0,120),
            (comic()?prefs.comicLandscapeSpreads:prefs.bookLandscapeSpreads) && w>h && !continuous() && spread,prefs.expandedViewport};
    }
    int readerTurnTarget(int page,int steps) {
        if(comicSpreadActive())return comicTurnTarget(page,steps);
        if(readerLayout().spread) {
            const int base=page/2*2;
            return std::clamp(base+steps*2,0,(std::max(1,pageCount_)-1)/2*2);
        }
        return std::clamp(page+steps,0,std::max(0,pageCount_-1));
    }
    bool sameReaderSpread(int a,int b) {
        if(comicSpreadActive())return comicSpreadPages(a).first==comicSpreadPages(b).first;
        return readerLayout().spread && a/2==b/2;
    }
    int shoulderPageDirection(bool right) {
        int w,h;getWindowSize(w,h);
        return (right?1:-1)*(w>h?readingDirection():1);
    }
    void applyReaderLayoutOptions() {
        cancelPageTurn();pageCache_.clear();invalidatePagePreviews();
        comicDetails_.clear();
        if(doc_ && px_document_is_epub(doc_)) {
            layoutWidth_=0;++layoutGeneration_;layoutPending_=true;layoutDue_=SDL_GetTicks()+35;
        } else if(doc_)clampPdfPan();
        dirty_=true;
    }

    ReaderPreferences currentStyle() const {
        auto p = extraStyle_;
        p.theme = currentTheme_; p.fontSize = currentFontSize_; p.fontFamily = currentFontFamily_;
        p.lineHeight = currentLineHeight_; p.margin = currentMargin_; p.textAlign = currentTextAlign_;
        return p;
    }
    void applyStyle(const ReaderPreferences &p) {
        extraStyle_ = p;
        currentTheme_ = p.theme; currentFontSize_ = p.fontSize; currentFontFamily_ = p.fontFamily;
        currentLineHeight_ = p.lineHeight; currentMargin_ = p.margin; currentTextAlign_ = p.textAlign;
    }
    ReaderPreferences presentedStyle() const {
        // Until the reflowed EPUB document is published, the old page is still
        // on screen. Keep its canvas/chrome exactly in sync with that old page.
        if(doc_ && px_document_is_epub(doc_) && (layoutPending_ || layoutWorker_.joinable()))
            return renderedStyle_;
        return currentStyle();
    }
    ThemePalette palette() const {
        if(screen_==ScreenState::ReadingJournal)
            return readerChromePalette(progressStore_.get("PAPERNX_DEFAULTS"),false,0,0);
        if(doc_ && !globalSettings_)
            return readerChromePalette(presentedStyle(),false,0,0);
        return readerChromePalette(progressStore_.get("PAPERNX_DEFAULTS"),false,0,0);
    }
    ThemePalette interfacePalette() const {
        const auto &a=appSettings_.get();return resolveInterfacePalette(palette(),a.interfaceStyle,a.interfaceAccent);
    }
    PageTurnTexture interfaceBackground_{nullptr,SDL_DestroyTexture};
    int interfaceBackgroundHeight_{0},interfaceBackgroundStyle_{-1},interfaceBackgroundAccent_{-1};
    Uint32 interfaceBackgroundColor_{0};
    void interfaceBackground(int w,int h,const ThemePalette &pal) {
        const auto &a=appSettings_.get();
        const Uint32 color=(Uint32(pal.bg.r)<<16)|(Uint32(pal.bg.g)<<8)|pal.bg.b;
        if(!interfaceBackground_ || interfaceBackgroundHeight_!=h || interfaceBackgroundStyle_!=a.interfaceStyle || interfaceBackgroundAccent_!=a.interfaceAccent || interfaceBackgroundColor_!=color) {
            std::vector<Uint32> pixels(h);const auto top=blendColor(pal.bg,pal.accent,9);
            for(int y=0;y<h;++y){auto c=blendColor(top,pal.bg,std::min(100,y*100/360));pixels[y]=0xff000000u|(Uint32(c.r)<<16)|(Uint32(c.g)<<8)|c.b;}
            interfaceBackground_.reset(SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STATIC,1,h));
            if(interfaceBackground_)SDL_UpdateTexture(interfaceBackground_.get(),nullptr,pixels.data(),4);
            interfaceBackgroundColor_=color;interfaceBackgroundHeight_=h;interfaceBackgroundStyle_=a.interfaceStyle;interfaceBackgroundAccent_=a.interfaceAccent;
        }
        if(interfaceBackground_){SDL_Rect dst{0,0,w,h};SDL_RenderCopy(renderer_,interfaceBackground_.get(),nullptr,&dst);}
    }
    int globalPaperPresetIndex() const {
        const auto defaults=progressStore_.get("PAPERNX_DEFAULTS");
        for(int i=0;i<int(sizeof(g_paperPresets)/sizeof(g_paperPresets[0]));++i) {
            const auto &p=g_paperPresets[i];
            if(defaults.theme==p.theme && defaults.backgroundColor==p.background &&
               defaults.textColor==p.text)return i;
        }
        // Older installs only stored one of the original six named palettes.
        if(defaults.backgroundColor<0 && defaults.textColor<0) {
            const int legacy[]={0,1,4,6,7,8};
            return legacy[std::clamp(defaults.theme,0,5)];
        }
        return -1; // User-defined colors chosen through the appearance palette.
    }
    void applyGlobalPaperPreset(int index) {
        if(index<0 || index>=int(sizeof(g_paperPresets)/sizeof(g_paperPresets[0]))) return;
        const auto preset=g_paperPresets[index];
        auto defaults=progressStore_.get("PAPERNX_DEFAULTS");
        defaults.theme=preset.theme;
        defaults.backgroundColor=preset.background;
        defaults.textColor=preset.text;
        defaults.styleOverride=0;
        defaults.modeOverride=0;
        progressStore_.put("PAPERNX_DEFAULTS",defaults);
        progressDirty_=true;
        auto prefs=appSettings_.get();
        prefs.uiTheme=preset.theme; // Backward-compatible serialized value only.
        prefs.chromePolicy=0;
        appSettings_.put(prefs);
        persistAppSettings();
        if(!doc_) {
            applyStyle(defaults);
        } else if(!extraStyle_.styleOverride) {
            // Existing books inherit global changes without losing their anchors.
            auto effective=resolveReadingPreferences(defaults,currentStyle());
            applyStyle(effective);
            queueStyleChange(true); // Still inherits the new global theme.
        }
        pageCache_.clear();
        dirty_=true;
    }
    std::string buildEpubCss() const { return generateEpubCss(currentStyle(),appSettings_.get().expandedViewport); }
    void storeDefaults() {
        Progress p;
        static_cast<ReaderPreferences &>(p) = currentStyle();
        if(doc_ && !globalSettings_) {
            auto defaults=progressStore_.get("PAPERNX_DEFAULTS");
            if(extraStyle_.modeOverride) {
                p.readingMode=defaults.readingMode; p.scrollSensitivity=defaults.scrollSensitivity;
                p.scrollInertia=defaults.scrollInertia;
            }
            p.pdfViewMode=defaults.pdfViewMode; p.pdfColorMode=defaults.pdfColorMode;
        }
        p.modeOverride=0; p.styleOverride=0;
        progressStore_.put("PAPERNX_DEFAULTS", p);
        progressDirty_ = true;
        dirty_ = true;
    }
    void resetBookStyle() {
        applyStyle(progressStore_.get("PAPERNX_DEFAULTS"));
        reflowEpub(currentFontSize_);
    }
    void persistAppSettings() {
        auto p=appSettings_.get(); p.libraryLayout=viewMode_==ViewMode::List; p.sort=static_cast<int>(sort_);
        appSettings_.put(p);
        appSavePending_=true;
        dirty_=true;
    }
    void openSettings(bool global=false,int tab=0) {
        cancelPageTurn();
        quickMenu_=false; helpMenu_=false;
        if(screen_!=ScreenState::AaModal) settingsReturn_=screen_;
        globalSettings_=global;
        if(global && !doc_) applyStyle(progressStore_.get("PAPERNX_DEFAULTS"));
        // PDF opens the SAME appearance tab that EPUB uses; PDF-only tab
        // contains layout and zoom controls, never a second color source.
        aaTab_=doc_ && !px_document_is_epub(doc_) && !comic() && tab==0?2:tab; aaRow_=0;settingsEditing_=false;settingsScroll_=0; screen_=ScreenState::AaModal; dirty_=true;
    }
    void dismissSettings() {
        coverClearConfirm_ = false;
        screen_=globalSettings_ ? settingsReturn_ : ScreenState::Reader;
        globalSettings_=false; gestureTracker_.reset(); dirty_=true; flushProgress();
    }
    void queueStyleChange(bool preserveScope=false) {
        cancelPageTurn();
        if(doc_ && !globalSettings_) {
            // Reader appearance changes belong to THIS book; never silently
            // rewrite the global theme. Scope-toggle reset is the exception.
            if(!preserveScope)extraStyle_.styleOverride=1;
            auto progress=progressStore_.get(currentBookId_);
            static_cast<ReaderPreferences&>(progress)=currentStyle();
            progressStore_.put(currentBookId_,progress);progressDirty_=true;
        } else storeDefaults();
        // PDFs use the same appearance settings, without EPUB re-layout.
        // Invalidate old PDF textures immediately; render recolored paper
        // and chrome together from the resolved current ReaderPreferences.
        if(doc_ && !px_document_is_epub(doc_)) pageCache_.clear();
        if(comic()) {
            const bool resized=syncComicScrollWidth();
            scrollY_=std::clamp(scrollY_,0.f,std::max(0.f,segmentHeight(currentPage_)-1));
            if(resized)setPdfView(extraStyle_.pdfViewMode);else clampPdfPan();
        }
        // Debounced preparation is off-thread, but the old EPUB page and its
        // chrome remain on the same palette until the new layout is published.
        if(doc_ && px_document_is_epub(doc_)) {
            ++layoutGeneration_;layoutPending_=true;layoutDue_=SDL_GetTicks()+35;
        }
        dirty_=true;
    }
    void cancelLayout() {
        ++layoutGeneration_; layoutPending_=false;
        // Worker owns an independent context. Closing a book only invalidates its result;
        // the event thread never waits for expensive pagination here.
    }
    void processLayout() {
        if(layoutReady_.load()) {
            layoutWorker_.join(); layoutReady_=false;
            if(preparedGeneration_==layoutGeneration_ && doc_) {
                if(preparedDoc_) {
                    int ch=-1,character=-1; double offset=0;
                    px_document_anchor(doc_,currentPage_,scrollY_,&ch,&character,&offset);
                    int fallback=currentPage_; float newY=0;
                    pageCache_.clear(); px_document_close(doc_); doc_=preparedDoc_; preparedDoc_=nullptr;
                    pageCount_=px_document_pages(doc_); currentPage_=std::clamp(fallback,0,std::max(0,pageCount_-1));
                    if(!px_document_restore_anchor(doc_,ch,character,offset,&currentPage_,&newY)) newY=0;
                    scrollY_=newY; scrollVelocity_=0; documentMode_=preparedStyle_.readingMode;
                    renderedStyle_=preparedStyle_; layoutWidth_=preparedWidth_; layoutHeight_=preparedHeight_;
                    loadToc(); saveCurrentPosition(); ++completedLayouts_;
                } else {
                    applyStyle(renderedStyle_); currentErrorMessage_="Düzen hazırlanamadı: "+preparedError_;
                }
                layoutPending_=false; dirty_=true;
            }
            if(preparedDoc_) { px_document_close(preparedDoc_); preparedDoc_=nullptr; }
        }
        if(!layoutPending_ || layoutWorker_.joinable() || !doc_ || SDL_GetTicks()<layoutDue_) return;
        int w,h; getWindowSize(w,h); auto style=currentStyle();
        if(generateEpubCss(style)==generateEpubCss(renderedStyle_) && style.margin==renderedStyle_.margin &&
            style.readingMode==renderedStyle_.readingMode && w==layoutWidth_ && h==layoutHeight_) {
            layoutPending_=false; return;
        }
        auto book=std::find_if(books_.begin(),books_.end(),[&](const Book &b){return b.id==currentBookId_;});
        if(book==books_.end()) { layoutPending_=false; return; }
        std::string path=book->path, css=generateEpubCss(style,appSettings_.get().expandedViewport); unsigned generation=layoutGeneration_;
        const auto layout=epubLayout(w,h,style);
        // Context and document exclusively owned by this worker until atomic publication.
        layoutWorker_=std::thread([this,path,css,style,w,h,generation,layout]() {
            char err[256]{};
            preparedDoc_=px_document_open_limited(path.c_str(),layout.contentWidth(),style.readingMode?0:layout.contentHeight(),
                float(style.fontSize),css.c_str(),err,sizeof err,64u*1024u*1024u);
            preparedStyle_=style; preparedError_=err; preparedGeneration_=generation;
            preparedWidth_=w; preparedHeight_=h; layoutReady_.store(true);
        });
    }
    void rememberPosition() {
        if(!doc_) return;
        readingStats_.reanchor();
        HistoryPosition p{currentPage_,scrollY_,-1,-1,0};
        px_document_anchor(doc_,currentPage_,documentScrollY(currentPage_,scrollY_),&p.chapter,&p.character,&p.offset);
        if(!navigationHistory_.empty() && navigationHistory_.back().page==p.page && std::abs(navigationHistory_.back().y-p.y)<1) return;
        if(navigationHistory_.size()>=24) navigationHistory_.pop_front();
        navigationHistory_.push_back(p);
    }
    void jumpBack() {
        if(navigationHistory_.empty() || !doc_) return;
        readingStats_.reanchor();
        auto p=navigationHistory_.back(); navigationHistory_.pop_back();
        currentPage_=std::clamp(p.page,0,std::max(0,pageCount_-1)); scrollY_=p.y;
        float anchorY=0;
        if(px_document_restore_anchor(doc_,p.chapter,p.character,p.offset,&currentPage_,&anchorY))scrollY_=screenScrollY(currentPage_,anchorY);
        scrollVelocity_=0; saveCurrentPosition(); dirty_=true;
    }
    void chapterJump(int direction) {
        if(continuous() && !comic() && !tocEntries_.empty()) {
            int chosen=-1;
            for(int i=0;i<int(tocEntries_.size());++i) {
                const auto &entry=tocEntries_[i];if(entry.page<0)continue;
                const bool ahead=entry.page>currentPage_ || (entry.page==currentPage_ && entry.y>scrollY_+1);
                const bool behind=entry.page<currentPage_ || (entry.page==currentPage_ && entry.y<scrollY_-1);
                if((direction>0 && !ahead) || (direction<0 && !behind))continue;
                if(chosen<0 || (direction>0?(entry.page<tocEntries_[chosen].page || (entry.page==tocEntries_[chosen].page && entry.y<tocEntries_[chosen].y)):
                    (entry.page>tocEntries_[chosen].page || (entry.page==tocEntries_[chosen].page && entry.y>tocEntries_[chosen].y))))chosen=i;
            }
            if(chosen>=0)navigationJump(tocEntries_[chosen].page,tocEntries_[chosen].y);
            return;
        }
        if(comic()) {
            currentPage_=std::clamp(currentPage_+direction,0,std::max(0,pageCount_-1));
            scrollY_=scrollVelocity_=0;saveCurrentPosition();dirty_=true;return;
        }
        if(tocEntries_.empty()) return;
        int target=direction>0?pageCount_-1:0;
        for(const auto &entry:tocEntries_) {
            if(direction>0 && entry.page>currentPage_) { target=entry.page; break; }
            if(direction<0 && entry.page<currentPage_) target=entry.page;
        }
        rememberPosition(); currentPage_=std::clamp(target,0,std::max(0,pageCount_-1));
        scrollY_=scrollVelocity_=0; saveCurrentPosition(); dirty_=true;
    }
    std::vector<std::string> quickActions() const {
        auto actions=baseQuickActions();
        if(!helpMenu_ && screen_==ScreenState::FileBrowser)actions.push_back(ui::label("Add this folder"));
        if(!helpMenu_)actions.push_back(ui::label("Check for update"));
        return actions;
    }
    std::vector<std::string> baseQuickActions() const {
        if(helpMenu_) return {};
        if(screen_==ScreenState::ReadingJournal)return {catLabel("Geri dön","Go back"),catLabel("Hedeflerim","My goals")};
        if(doc_) {
            std::vector<std::string> result={"Kitaplığa dön","Okuma ayarları · Aa","İçindekiler","Yer imleri","İlerleme / sayfaya git"};
            if(!px_document_is_epub(doc_) && !continuous())result.push_back(catLabel("Yakınlaştırmayı sıfırla · Sayfaya sığdır","Reset zoom · Fit page"));
            if(!navigationHistory_.empty()) result.push_back("Önceki konuma dön");
            if(!tocEntries_.empty()) { result.push_back("Önceki bölüm"); result.push_back("Sonraki bölüm"); }
            result.push_back(pageFlipPosition_?catLabel("Saklanan konuma dön","Return to saved place"):catLabel("Bu konumu sakla · Sayfa gezintisi","Keep this place · Page Flip"));
            result.push_back(catLabel("Bu kitabın istatistikleri","This book's statistics"));
            result.push_back("Kitap bilgisi / yardım");
            result.push_back(catLabel("Ekran yönü · Yatay / Dikey","Display orientation · Landscape / Portrait"));
            result.push_back(catLabel("Sayfa küçük resimleri","Page thumbnails"));
            return result;
        }
        if(screen_==ScreenState::FileBrowser) return {"Kitaplığa dön","Üst klasör","Okuma ayarları","Görünümü yenile","Yardım","PaperNX’ten çık",catLabel("Ekran yönü · Yatay / Dikey","Display orientation · Landscape / Portrait"),catLabel("Günlüğüm","My journal")};
        const char *sorts[]={"Başlık","Yazar","Dosya adı","Değiştirilme","Son okunan"};
        return {"Okuma ayarları","Uygulama görünümü","SD’ye gözat / kitap aç","Kitaplıkta ara",
            std::string("Sırala: ")+sorts[static_cast<int>(sort_)],viewMode_==ViewMode::Grid?"Liste görünümüne geç":"Kapak ızgarasına geç",
            "Kitaplığı yenile","Yardım / dokunma rehberi","PaperNX’ten çık",catLabel("Yazarlar, türler ve koleksiyonlar","Authors, genres and collections"),catLabel("Ekran yönü · Yatay / Dikey","Display orientation · Landscape / Portrait"),catLabel("Günlüğüm","My journal")};
    }
    std::string quickSpreadLabel() const {
        const auto &prefs=appSettings_.get();
        const bool enabled=doc_&&comic()?prefs.comicLandscapeSpreads:prefs.bookLandscapeSpreads;
        if(!doc_)return catLabel(enabled?"Kitap · Çift sayfa: Açık":"Kitap · Çift sayfa: Kapalı",
                                 enabled?"Books · Double page: On":"Books · Double page: Off");
        return catLabel(enabled?"Çift sayfa: Açık":"Çift sayfa: Kapalı",
                        enabled?"Double page: On":"Double page: Off");
    }
    SDL_Rect quickPageShortcut(SDL_Rect r) const {
        const int width=(r.w-36)/2;
        return {r.x+12,r.y+48,width,52};
    }
    SDL_Rect quickOrientationShortcut(SDL_Rect r) const {
        const auto pages=quickPageShortcut(r);
        return {pages.x+pages.w+12,pages.y,pages.w,pages.h};
    }
    bool toggleQuickMenu() {
        if(quickMenu_) {quickMenu_=helpMenu_=false;dirty_=true;return true;}
        if(updateDialog_ || choiceKind_>=0 || colorEditing_>=0 || settingsEditing_ || screen_==ScreenState::AaModal || catalogPanel_!=CatalogPanel::None || searchActive_ ||
           (screen_==ScreenState::ReadingJournal && journalUI_.hasSubpage()) ||
           coverClearConfirm_ || screen_==ScreenState::TocModal || screen_==ScreenState::BookmarksModal || screen_==ScreenState::JournalPreview)
            return false;
        quickMenu_=true;helpMenu_=false;quickScroll_=0;quickFocus_=0;listVelocity_=scrollVelocity_=0;dirty_=true;
        return true;
    }
    void toggleLandscapeSpreads() {
        if(doc_)saveCurrentPosition();
        auto prefs=appSettings_.get();
        if(doc_&&comic())prefs.comicLandscapeSpreads=!prefs.comicLandscapeSpreads;
        else prefs.bookLandscapeSpreads=!prefs.bookLandscapeSpreads;
        appSettings_.put(prefs);persistAppSettings();applyReaderLayoutOptions();
    }
    SDL_Rect quickSheet(int w,int h) const {
        int width=std::min(460,w-32),height=std::min(h-96,QUICK_HEADER_HEIGHT+int(quickActions().size())*60);
        if(helpMenu_) height=std::min(560,h-96);
        return {w-width-16,80,width,height};
    }
    void handleQuickMenuTap(int px,int py,int w,int h) {
        auto r=quickSheet(w,h);
        if(helpMenu_ || !ui::contains(r,px,py)) { quickMenu_=helpMenu_=false; dirty_=true; return; }
        // Fixed header shortcuts remain visible while the long action list scrolls.
        if(ui::contains(quickPageShortcut(r),px,py)) {quickMenu_=false;toggleLandscapeSpreads();return;}
        if(ui::contains(quickOrientationShortcut(r),px,py)){
            quickMenu_=false;openChoice(1,appSettings_.get().displayOrientation);return;
        }
        int index=int(py-r.y-QUICK_HEADER_HEIGHT+quickScroll_)/60;
        auto actions=quickActions();
        if(py<r.y+QUICK_HEADER_HEIGHT || index<0 || index>=int(actions.size())) return;
        std::string action=actions[index]; quickMenu_=false;
        if(action==ui::label("Add this folder")){addBrowserFolder();return;}
        if(action==ui::label("Check for update")){checkForUpdate();return;}
        if(screen_==ScreenState::ReadingJournal){
            if(index==0){screen_=journalReturn_;dirty_=true;}
            else journalUI_.tap(w-200,40,w,h,journal_);
            return;
        }
        if(action==catLabel("Günlüğüm","My journal")){enterJournal(false);return;}
        if(action==catLabel("Ekran yönü · Yatay / Dikey","Display orientation · Landscape / Portrait")) {
            openChoice(1,appSettings_.get().displayOrientation);
            return;
        }
        if(doc_) {
            if(action==catLabel("Sayfa küçük resimleri","Page thumbnails")){openContents(1);return;}
            if(action==catLabel("Yakınlaştırmayı sıfırla · Sayfaya sığdır","Reset zoom · Fit page")){resetReaderZoom();return;}
            if(index==0) closeBook();
            else if(index==1) openSettings();
            else if(index==2) openContents();
            else if(index==3) screen_=ScreenState::BookmarksModal;
            else if(index==4) { screen_=ScreenState::Reader; toolbarVisible_=true; }
            else if(action=="Önceki konuma dön" || action=="Return to previous location") { jumpBack(); screen_=ScreenState::Reader; }
            else if(action=="Önceki bölüm" || action=="Previous chapter") { chapterJump(-1); screen_=ScreenState::Reader; }
            else if(action=="Sonraki bölüm" || action=="Next chapter") { chapterJump(1); screen_=ScreenState::Reader; }
            else if(action==catLabel("Bu kitabın istatistikleri","This book's statistics") || action=="Bu kitabın istatistikleri" || action=="This book's statistics"){
                journalUI_.showPreview();screen_=ScreenState::JournalPreview;
            }
            else if(action==catLabel("Saklanan konuma dön","Return to saved place") || action=="Saklanan konuma dön" || action=="Return to saved place")returnToPageFlip();
            else if(action==catLabel("Bu konumu sakla · Sayfa gezintisi","Keep this place · Page Flip") || action=="Bu konumu sakla · Sayfa gezintisi" || action=="Keep this place · Page Flip")pinPageFlip();
            else quickMenu_=helpMenu_=true;
        } else if(screen_==ScreenState::FileBrowser) {
            if(index==0) screen_=ScreenState::Library;
            if(index==1 && browserPath_!=browserRoot_) { browserPath_=browserPath_.parent_path(); scanBrowserDirectory(); }
            if(index==2) openSettings(true);
            if(index==3) scanBrowserDirectory();
            if(index==4) quickMenu_=helpMenu_=true;
            if(index==5) exitRequested_=true;
        } else {
            if(index==0) openSettings(true);
            if(index==1) openSettings(true,5);
            if(index==2) openBrowser();
            if(index==3) toggleSearch();
            if(index==4) { sort_=static_cast<Sort>((static_cast<int>(sort_)+1)%5); updateVisibleBooks(); persistAppSettings(); }
            if(index==5) { viewMode_=viewMode_==ViewMode::Grid?ViewMode::List:ViewMode::Grid; listScroll_=0; persistAppSettings(); }
            if(index==6)startLibraryScan();
            if(index==7) quickMenu_=helpMenu_=true;
            if(index==8) exitRequested_=true;
            if(index==9)setCatalogPanel(CatalogPanel::Browse);
        }
        dirty_=true;
    }
    void renderQuickMenu(int w,int h,const ThemePalette &pal) {
        auto r=quickSheet(w,h); ui::elevated(renderer_,r,pal);
        typography_->renderText(renderer_,typography_->smallFont(),ui::label(helpMenu_?"DOKUNMA REHBERİ · Kapatmak için dokun":"HIZLI İŞLEMLER"),r.x+20,r.y+12,pal.muted,r.w-40);
        if(!helpMenu_) {
            ui::button(renderer_,*typography_,quickPageShortcut(r),quickSpreadLabel(),pal);
            ui::button(renderer_,*typography_,quickOrientationShortcut(r),catLabel("Yatay / Dikey","Portrait / landscape"),pal);
        }
        if(helpMenu_) {
            int y=r.y+58; int metadataLines=0;
            std::vector<std::string> lines={"+ tüm işlemleri açar.","Kitaplarınızı dokunarak açın.","Listeleri parmağınızla sürükleyin.","Sayfalı: Kenarlara dokun / yatay kaydır.","Sürekli: Yukarı / aşağı sürükle.","PDF: İki parmakla yakınlaştır, sürükle.","Okuma ayarları tüm kitaplarda ortaktır.","Bu kitap seçeneği özel ayar oluşturur.","Kitaplarınız ve kayıtlarınız cihazda kalır."};
            lines.insert(lines.begin(),std::string("PaperNX ")+APP_VERSION);
            if(doc_) {
                lines.insert(lines.begin()+1,catLabel("İlerleme çubuğu: Sürükle, önizle, bırak.","Progress bar: drag, preview, release."));
                lines.insert(lines.begin()+2,catLabel("LS tıkla / G: Sayfa seç; A onayla, B vazgeç.","LS click / G: preview; A confirms, B cancels."));
                auto b=std::find_if(books_.begin(),books_.end(),[&](const Book &book){return book.id==currentBookId_;});
                if(b!=books_.end()) { lines.insert(lines.begin(),b->title); ++metadataLines; if(!b->author.empty()) { lines.insert(lines.begin()+1,b->author); ++metadataLines; } }
            }
            SDL_Rect clip{r.x+12,r.y+64,r.w-24,r.h-76}; SDL_RenderSetClipRect(renderer_,&clip);
            int lineIndex=0;
            for(const auto &line:lines) { typography_->renderText(renderer_,typography_->smallFont(),lineIndex++<metadataLines?line:ui::label(line),r.x+20,y,pal.fg,r.w-40); y+=38; }
            SDL_RenderSetClipRect(renderer_,nullptr);
        } else {
            SDL_Rect clip{r.x,r.y+QUICK_HEADER_HEIGHT,r.w,r.h-QUICK_HEADER_HEIGHT}; SDL_RenderSetClipRect(renderer_,&clip);
            auto actions=quickActions(); for(int i=0;i<int(actions.size());++i)
                ui::menuRow(renderer_,*typography_,{r.x+12,r.y+QUICK_HEADER_HEIGHT+i*60-int(quickScroll_),r.w-24,56},actions[i],pal,false,i==quickFocus_);
            SDL_RenderSetClipRect(renderer_,nullptr);
        }
    }
    float &uiScroll() {
        if(quickMenu_) return quickScroll_;
        if(screen_==ScreenState::ReadingJournal) return journalScroll_;
        if(screen_==ScreenState::AaModal) return settingsScroll_;
        if(screen_==ScreenState::FileBrowser) return browserScroll_;
        if(screen_==ScreenState::TocModal) return tocScroll_;
        if(screen_==ScreenState::BookmarksModal) return bookmarkScroll_;
        return listScroll_;
    }
    float maxUiScroll() {
        int w,h; getWindowSize(w,h);
        if(quickMenu_) return std::max(0,int(quickActions().size())*60-(quickSheet(w,h).h-QUICK_HEADER_HEIGHT));
        if(screen_==ScreenState::ReadingJournal) return float(journalUI_.maxScrollY());
        if(screen_==ScreenState::AaModal) return std::max(0,settingsRows()*64-ui::SettingsLayout(w,h).content.h);
        if(screen_==ScreenState::FileBrowser) return std::max(0,int(browserEntries_.size())*64-(h-128));
        if(screen_==ScreenState::TocModal) return tocMaxScroll(w,h);
        if(screen_==ScreenState::BookmarksModal) return std::max(0,int(progressStore_.get(currentBookId_).bookmarks.size())*56-(h-280));
        ui::LibraryLayout l(w); int rows=(int(visibleBooks_.size())+l.columns-1)/l.columns;
        return std::max(0,(viewMode_==ViewMode::Grid?rows*l.stepY:int(visibleBooks_.size())*112)-(h-l.top-28));
    }
    bool beginListTouch(float x,float y) {
        int w,h; getWindowSize(w,h); int px=int(x*w),py=int(y*h); bool capture=false;
        if(quickMenu_) capture=ui::contains(quickSheet(w,h),px,py) && !helpMenu_;
        else if(screen_==ScreenState::ReadingJournal) capture=!journalUI_.hasSubpage() && py>=76;
        else if(screen_==ScreenState::Library) capture=py>=ui::LibraryLayout(w).top;
        else if(screen_==ScreenState::FileBrowser) capture=py>=112;
        else if(screen_==ScreenState::AaModal) capture=ui::contains(ui::SettingsLayout(w,h).content,px,py);
        else if(screen_==ScreenState::TocModal) capture=ui::contains(tocContent(w,h),px,py);
        else if(screen_==ScreenState::BookmarksModal) capture=py>=136 && py<h-136;
        if(capture) { listTracking_=true; listMoved_=false; listLastY_=y; listTick_=SDL_GetTicks(); listVelocity_=0; }
        return capture;
    }
    void moveListTouch(float y) {
        int w,h; getWindowSize(w,h); auto now=SDL_GetTicks(); float dy=(listLastY_-y)*h;
        if(std::abs(y-pressY_)>.008f) listMoved_=true;
        if(listMoved_) { uiScroll()=std::clamp(uiScroll()+dy,0.f,maxUiScroll()); listVelocity_=std::clamp(dy*1000/std::max(8u,now-listTick_),-2200.f,2200.f); }
        listTick_=now; listLastY_=y; dirty_=true;
    }
    void tickListScroll() {
        auto now=SDL_GetTicks(); float dt=std::min(.032f,(now-listPhysicsTick_)/1000.f); listPhysicsTick_=now;
        if(listTracking_ || std::abs(listVelocity_)<1) return;
        if(screen_==ScreenState::Reader && !quickMenu_) { listVelocity_=0; return; }
        float old=uiScroll(), next=std::clamp(old+listVelocity_*dt,0.f,maxUiScroll());
        uiScroll()=next; listVelocity_*=std::exp(-7.f*dt);
        if(next==old) listVelocity_=0;
        dirty_=true;
    }
    void toggleBookmark() {
        if(!doc_) return;
        auto p = progressStore_.get(currentBookId_);
        auto mark=px_document_make_position_bookmark(doc_,currentPage_,continuous()?documentScrollY(currentPage_,scrollY_):0);
        auto it = std::find_if(p.bookmarks.begin(),p.bookmarks.end(),[&](const Bookmark &b) {
            if(mark>=0 && b.muBookmark==mark) return true;
            int page=resolveBookmark(b); float y=0;
            px_document_lookup_position_bookmark(doc_,b.muBookmark,&page,&y);
            return page==currentPage_ && (!continuous() || std::abs(screenScrollY(page,y)-scrollY_)<12);
        });
        if (it != p.bookmarks.end()) p.bookmarks.erase(it);
        else {
            std::string label=continuous() && !comic()?ui::label("Bölüm ")+std::to_string(currentPage_+1)+" · %"+std::to_string(int(readingFraction()*100)):
                ui::label("Sayfa ")+std::to_string(currentPage_+1);
            p.bookmarks.push_back({currentPage_,label,std::time(nullptr),mark});
        }
        progressStore_.put(currentBookId_, p); progressDirty_ = true; dirty_ = true;
    }
    void openBookmark(const Bookmark &bm) {
        rememberPosition(); int page=resolveBookmark(bm); float y=0;
        px_document_lookup_position_bookmark(doc_,bm.muBookmark,&page,&y);
        currentPage_=std::clamp(page,0,std::max(0,pageCount_-1)); scrollY_=std::max(0.f,screenScrollY(currentPage_,y)); scrollVelocity_=0;
        if(comic() && !continuous())resetComicPagePan();
        screen_=ScreenState::Reader; toolbarVisible_=false; saveCurrentPosition(); dirty_=true;
    }

    bool commitColor(std::string value, int target) {
        if (!value.empty() && value[0] == '#') value.erase(0, 1);
        if (value.size() != 6 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return false;
        const int rgb = int(std::strtol(value.c_str(), nullptr, 16));
        (target ? extraStyle_.backgroundColor : extraStyle_.textColor) = rgb;
        queueStyleChange(); return true;
    }
    static constexpr int swatchColors_[16]={0xffffff,0xf7f3e8,0xf4e8d0,0xeee4cf,0xe8d6b3,0xe5e1d6,0xe9ebe8,0x252522,0x000000,0x242424,0x35312b,0x41362a,0x383229,0x4b3828,0xe8e3d9,0xeae7df};
    SDL_Rect colorPanel(int w,int h) const {return {std::max(8,w/2-280),std::max(24,h/2-210),std::min(560,w-16),420};}
    void editColor(int target) {
        editOriginal_=currentStyle();colorEditing_=target;swatchFocus_=target?1:9;dirty_=true;
    }
    void previewSwatch() {
        (colorEditing_?extraStyle_.backgroundColor:extraStyle_.textColor)=swatchColors_[swatchFocus_];dirty_=true;
    }
    void closeColor(bool save) {
        if(!save) applyStyle(editOriginal_); else queueStyleChange();
        colorEditing_=-1;gestureTracker_.reset();activeFingers_.clear();ui::pointer(0,0,false);dirty_=true;
    }

    void toggleSearch() {
#ifdef __SWITCH__
        SwkbdConfig config;
        if (R_SUCCEEDED(swkbdCreate(&config, 0))) {
            swkbdConfigMakePresetDefault(&config);
            swkbdConfigSetGuideText(&config,ui::label("Kitap, yazar veya dosya adı ara").c_str());
            swkbdConfigSetInitialText(&config, searchQuery_.c_str());
            swkbdConfigSetStringLenMax(&config, 256);
            char text[1024] = {};
            Result result = swkbdShow(&config, text, sizeof text);
            swkbdClose(&config);
            if (R_SUCCEEDED(result)) searchQuery_ = utf8SafeTruncate(text, 256);
            updateVisibleBooks();
        }
#else
        searchActive_ = !searchActive_;
        if (searchActive_) SDL_StartTextInput();
        else { SDL_StopTextInput(); searchQuery_.clear(); updateVisibleBooks(); }
#endif
        dirty_ = true;
    }

    void updateVisibleBooks() {
        auto candidates=catalog_.visible(books_);
        if(catalogFilter_)candidates=catalog_.filter(candidates,*catalogFilter_,catalogFilterValue_);
        if(!openSeriesKey_.empty()) {
            auto seed=std::find_if(candidates.begin(),candidates.end(),[&](const Book &book){return book.id==seriesSeedId_;});
            if(seed!=candidates.end()) {auto identity=identifySeries(*seed,catalog_.get(seed->id));if(!identity.key.empty()){openSeriesKey_=identity.key;openSeriesTitle_=identity.title;}}
            candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const Book &book){return identifySeries(book,catalog_.get(book.id)).key!=openSeriesKey_;}),candidates.end());
        }
        Category category=category_;
        if(category==Category::Unread||category==Category::Completed||category==Category::ContinueReading){
            candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[&](const Book &b){
                const auto status=catalog_.get(b.id).status;
                if(status==ReadingStatus::Automatic)return filterByCategory({b},progressStore_,category).empty();
                return category==Category::Unread?status!=ReadingStatus::NotStarted:category==Category::Completed?status!=ReadingStatus::Completed:status!=ReadingStatus::Reading;
            }),candidates.end());category=Category::All;
        }
        visibleBooks_ = searchEx(candidates, progressStore_, searchQuery_, sort_, category);
        arrangeSeries();
        if (selectedBookIdx_ >= static_cast<int>(visibleBooks_.size())) {
            selectedBookIdx_ = std::max(0, static_cast<int>(visibleBooks_.size()) - 1);
        }
        scrollOffset_ = 0; listScroll_=0; listVelocity_=0;
        dirty_ = true;
    }

    void openBook(Book b) {
        std::error_code statError;const auto bytes=fs::file_size(fs::u8path(b.path),statError);
        if(!statError && bytes!=b.bytes) {
            b.bytes=bytes;b.id=documentId(b.path,bytes);b.title=defaultTitle(fs::u8path(b.path));b.author.clear();
            for(auto &entry:books_)if(entry.path==b.path){entry=b;break;}
            rebuildLibraryLookup();updateVisibleBooks();
        }
        clearHeldControls();
        closeBook();
        coversPaused_=true;
        { std::lock_guard<std::mutex> lock(queueMutex_);
          for(const auto &item:thumbQueue_) queuedThumbIds_.erase(item.id);
          thumbQueue_.clear(); }
        int w = 1280, h = 720;
        getWindowSize(w, h);

        auto p = progressStore_.get(b.id);
        auto defaults=progressStore_.get("PAPERNX_DEFAULTS");
        const bool newBook=!progressStore_.all().count(b.id) || p.pageCount<=0;
        auto resolved=resolveReadingPreferences(defaults,p);
        if(newBook) { resolved.pdfViewMode=defaults.pdfViewMode; resolved.pdfColorMode=isComic(b.format)?0:defaults.pdfColorMode; }
        static_cast<ReaderPreferences&>(p)=resolved;
        scrollY_ = scrollVelocity_ = 0;
        extraStyle_ = p;
        currentBookId_ = b.id;
        journalTitle_ = b.title;journalAuthor_ = b.author;
        auto fp = computeBookFingerprint(b.path, b.bytes);
        journal_.registerBookAlias(b.id, fp, b.title, b.author);
        journal_.interaction(SDL_GetTicks());
        currentTheme_ = p.theme;
        currentFontSize_ = p.fontSize > 0 ? p.fontSize : 22;
        currentLineHeight_ = p.lineHeight > 0 ? p.lineHeight : 140;
        currentMargin_ = p.margin > 0 ? p.margin : 40;
        currentTextAlign_ = p.textAlign;
        currentFontFamily_ = p.fontFamily;
        pdfZoom_ = 1.0f;
        pdfPanX_ = 0;
        pdfPanY_ = 0;

        char err[256] = {};
        const auto layout=epubLayout(w,h,currentStyle());
        doc_ = px_document_open(b.path.c_str(), layout.contentWidth(), b.format == Format::Epub && extraStyle_.readingMode ? 0 : layout.contentHeight(),
                                static_cast<float>(currentFontSize_),
                                buildEpubCss().c_str(), err, sizeof(err));

        if (!doc_) {
            coversPaused_=false; queueCv_.notify_one();
            currentBookId_.clear();
            currentErrorMessage_ = std::string("Kitap açılamadı: ") + err;
            logMessage(currentErrorMessage_);
            screen_ = ScreenState::Library;
            dirty_ = true;
            return;
        }

        epubReadingDirection_=1;
        if(px_document_is_epub(doc_)) {
            char direction[16]{};
            if(px_document_metadata(doc_,"info:ReadingDirection",direction,sizeof direction)>0 && std::string(direction)=="rtl")
                epubReadingDirection_=-1;
        }
        if(comic() && newBook) {
            char direction[16]{};
            if(px_document_metadata(doc_,"info:ReadingDirection",direction,sizeof direction)>0 && std::string(direction)=="rtl")
                extraStyle_.pdfColorMode|=COMIC_RIGHT_TO_LEFT;
        }
        hintUntil_ = p.lastRead == 0 ? SDL_GetTicks() + 8000 : 0;
        // Extract metadata if available
        char title[512] = {}, author[512] = {};
        if (px_document_metadata(doc_, "info:Title", title, sizeof(title)) > 0 && title[0]) {
            for (auto &bk : books_) {
                if (bk.id == b.id) { bk.title = title; break; }
            }
        }
        if (px_document_metadata(doc_, "info:Author", author, sizeof(author)) > 0 && author[0]) {
            for (auto &bk : books_) {
                if (bk.id == b.id) { bk.author = author; break; }
            }
        }
        {
            auto entry=catalog_.get(b.id);char subject[512]{},series[512]{},seriesIndex[64]{};
            px_document_metadata(doc_,"info:Subject",subject,sizeof subject);
            px_document_metadata(doc_,"info:Series",series,sizeof series);
            px_document_metadata(doc_,"info:SeriesIndex",seriesIndex,sizeof seriesIndex);
            const std::string fingerprint=std::to_string(b.bytes)+":"+std::to_string(b.modified)+":series-v1";
            if(entry.sourceTitle!=title || entry.sourceAuthor!=author || entry.sourceGenre!=subject ||
               entry.sourceSeries!=series || entry.sourceSeriesIndex!=seriesIndex || entry.metadataFingerprint!=fingerprint) {
                entry.sourceTitle=title;entry.sourceAuthor=author;entry.sourceGenre=subject;
                entry.sourceSeries=series;entry.sourceSeriesIndex=seriesIndex;entry.metadataFingerprint=fingerprint;
                catalog_.put(b.id,entry);catalogSavePending_=true;
            }
        }

        documentMode_=extraStyle_.readingMode; renderedStyle_=currentStyle();
        layoutWidth_=w; layoutHeight_=h;
        pageCount_ = px_document_pages(doc_);

        // Restore position: prioritize MuPDF bookmark if valid, fallback to restorePage
        int restored = -1;
        // Only bridge-versioned chapter/fraction marks survive document reopen.
        if (p.muBookmark >= INT64_C(0x4000000000000000))
            restored = px_document_lookup_bookmark(doc_, p.muBookmark);
        if (restored < 0 || restored >= pageCount_) {
            restored = restorePage(p, pageCount_);
        }
        currentPage_ = std::clamp(restored, 0, std::max(0, pageCount_ - 1));

        float anchorY = 0;
        if (px_document_is_epub(doc_) && px_document_restore_anchor(doc_, p.anchorChapter, p.anchorCharacter, p.anchorOffset, &currentPage_, &anchorY))
            scrollY_ = anchorY;
        if(comic() && continuous() && p.anchorChapter==currentPage_)
            scrollY_=float(std::clamp(p.anchorOffset,0.,1.))*segmentHeight(currentPage_);
        if (!px_document_is_epub(doc_)) setPdfView(extraStyle_.pdfViewMode);

        // Load TOC. Skip the additional expensive bookmark/anchor traversal
        // before the first reader frame; regular page changes still persist anchors.
        loadToc();
        auto opened=progressStore_.get(b.id);
        opened.bookPath=b.path;opened.bookTitle=b.title;opened.bookAuthor=b.author;
        opened.lastRead=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        progressStore_.put(b.id,opened);progressDirty_=true;

        screen_ = ScreenState::Reader;
        toolbarVisible_ = false;
        readerToolbarFocusActive_=false;
        dirty_ = true;
    }

    void closeBook() {
        comicPages_.clear();comicPagesIdentity_.clear();
        cancelReaderScrub(true);invalidatePagePreviews();
        journal_.pause(SDL_GetTicks(),std::time(nullptr));journal_.save();
        pageFlipPosition_.reset();readingStats_.reanchor();pageRenderError_.clear();comicTurnError_.clear();comicDetails_.clear();comicScrollWidth_=0;panVelocityX_=panVelocityY_=0;zoomResetTracking_=false;
        cancelPageTurn();
        cancelLayout(); navigationHistory_.clear(); quickMenu_=false;
        globalSettings_=false;
        coversPaused_=false; queueCv_.notify_one();
        scrollTracking_=false; scrollVelocity_=0;
        pageCache_.clear();
        if (doc_) {
            saveCurrentPosition();
            flushProgress();
            px_document_close(doc_);
            doc_ = nullptr;
        }
        tocEntries_.clear();
        updateVisibleBooks();
        screen_ = ScreenState::Library;
        toolbarVisible_ = false;
        readerToolbarFocusActive_=false;
        dirty_ = true;
    }

    void loadToc() {
        cancelReaderScrub(true);invalidatePagePreviews();tocTab_=0;tocScroll_=0;tocTabScroll_[0]=tocTabScroll_[1]=tocTabScroll_[2]=0;tocBookmarks_.clear();
        tocEntries_.clear();
        if (!doc_) return;
        PxTocEntry buffer[256];
        int count = px_document_toc(doc_, buffer, 256);
        for (int i = 0; i < count; ++i) {
            tocEntries_.push_back(buffer[i]);
        }
        tocSelected_ = 0;
        tocOffset_ = 0;
    }

    void saveCurrentPosition() {
        if (!doc_ || currentBookId_.empty()) return;
        auto p = progressStore_.get(currentBookId_);
        p.page = currentPage_;
        p.pageCount = pageCount_;
        p.fraction = readingFraction();
        const auto manualStatus=catalog_.get(currentBookId_).status;
        if(manualStatus==ReadingStatus::Completed) {
            // Explicitly finished remains 100% in every stored progress view;
            // browsing an already-finished book does not silently undo its status.
            p.fraction=1.0;
        } else if(manualStatus==ReadingStatus::NotStarted && p.fraction>0.0001) {
            // Actual forward reading is the first evidence that "unread" has
            // become "reading". Never mark a PDF finished just for visiting the end.
            auto state=catalog_.get(currentBookId_);
            state.status=ReadingStatus::Reading;
            if(catalog_.put(currentBookId_,state))catalogSavePending_=true;
        }
        auto book = std::find_if(books_.begin(), books_.end(), [&](const Book &b) { return b.id == currentBookId_; });
        if (book != books_.end()) {
            p.bookPath = book->path; p.bookTitle = book->title; p.bookAuthor = book->author;
        }
        static_cast<ReaderPreferences &>(p) = currentStyle();
        p.fontSize = currentFontSize_;
        p.theme = currentTheme_;
        p.lineHeight = currentLineHeight_;
        p.margin = currentMargin_;
        p.textAlign = currentTextAlign_;
        p.fontFamily = currentFontFamily_;
        p.muBookmark = px_document_make_bookmark(doc_, currentPage_);
        if(comic()) {
            p.anchorChapter=currentPage_;p.anchorCharacter=-1;
            p.anchorOffset=continuous()?std::clamp(double(scrollY_)/segmentHeight(currentPage_),0.,1.):0;
        }
        if (px_document_is_epub(doc_)) {
            int ch=-1, character=-1; double offset=0;
            if (px_document_anchor(doc_, currentPage_, scrollY_, &ch, &character, &offset)) {
                p.anchorChapter=ch; p.anchorCharacter=character; p.anchorOffset=offset;
            }
        }
        p.lastRead = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        int ch = -1, subP = -1;
        if (px_document_get_location(doc_, currentPage_, &ch, &subP)) {
            p.chapter = ch;
        }

        progressStore_.put(currentBookId_, p);
        progressDirty_ = true;
    }

    bool flushProgress() {
        if (!progressDirty_) return true;
        lastProgressSave_ = std::chrono::steady_clock::now();
        if (progressStore_.save()) {
            progressDirty_ = false;
            saveRetrySeconds_ = 3;
            if (!lastSaveFailure_.empty()) { currentErrorMessage_.clear(); dirty_ = true; }
            lastSaveFailure_.clear();
            return true;
        }
        saveRetrySeconds_ = std::min(60, saveRetrySeconds_ * 2);
        const auto &detail = progressStore_.saveDiagnostic();
        if (detail != lastSaveFailure_) {
            logMessage(detail);
            lastSaveFailure_ = detail;
            currentErrorMessage_ = "Kaydedilemedi. SD kartı kontrol edin; kayıt yeniden denenecek.";
            dirty_ = true;
        }
        return false;
    }

    bool comic() const {return px_document_is_comic(doc_)!=0;}
    bool comicReadingColors() const {return comic() && (extraStyle_.pdfColorMode&COMIC_READING_COLORS);}
    int readingDirection() const {
        if(doc_ && px_document_is_epub(doc_)) return epubReadingDirection_;
        return comic() && (extraStyle_.pdfColorMode&COMIC_RIGHT_TO_LEFT)?-1:1;
    }
    int fixedViewportWidth(int width) const {return std::max(1,width-(comic()?currentMargin_*2:80));}
    float documentScrollY(int page,float y) {
        if(!comic())return y;
        float w=0,h=0;if(!px_document_page_size(doc_,page,&w,&h))return 0;
        return y*h/segmentHeight(page);
    }
    float screenScrollY(int page,float y) {
        if(!comic())return y;
        float w=0,h=0;if(!px_document_page_size(doc_,page,&w,&h) || h<=0)return 0;
        return y*segmentHeight(page)/h;
    }
    bool syncComicScrollWidth() {
        if(!comic()) {comicScrollWidth_=0;return false;}
        int w,h;getWindowSize(w,h);const int width=fixedViewportWidth(w);
        const bool resized=comicScrollWidth_>0 && comicScrollWidth_!=width;
        if(continuous() && comicScrollWidth_>0)scrollY_*=float(width)/comicScrollWidth_;
        comicScrollWidth_=width;return resized;
    }
    void clampPdfPan() {
        if (!doc_ || px_document_is_epub(doc_)) return;
        if(comicSpreadActive()) {
            int w,h;getWindowSize(w,h);
            pdfPanX_=std::clamp(pdfPanX_,0,std::max(0,int(std::ceil(fixedViewportWidth(w)*(pdfZoom_-1)))));
            pdfPanY_=std::clamp(pdfPanY_,0,std::max(0,int(std::ceil(comicViewportHeight(h)*(pdfZoom_-1)))));
            return;
        }
        float pw = 0, ph = 0;
        if (!px_document_page_size(doc_, currentPage_, &pw, &ph)) return;
        int w, h; getWindowSize(w, h);
        const int vw = fixedViewportWidth(w), vh = comic()?comicViewportHeight(h):std::max(1,h-144);
        const float scale = std::min(vw / pw, vh / ph) * pdfZoom_;
        pdfPanX_ = std::clamp(pdfPanX_, 0, std::max(0, static_cast<int>(std::ceil(pw * scale)) - vw));
        pdfPanY_ = std::clamp(pdfPanY_, 0, std::max(0, static_cast<int>(std::ceil(ph * scale)) - vh));
    }
    void resetComicPagePan() {
        pdfPanX_=readingDirection()<0?1000000:0;pdfPanY_=0;clampPdfPan();
        panVelocityX_=panVelocityY_=0;panInertiaX_=float(pdfPanX_);panInertiaY_=0;
        comicDetails_.clear();
    }

    float zoomFractionX_{0},zoomFractionY_{0};
    int zoomLastX_{0},zoomLastY_{0},zoomLastPage_{-1};
    void zoomPdf(float zoom, float focalX = .5f, float focalY = .5f, bool smooth=false) {
        if (!doc_ || px_document_is_epub(doc_)) return;
        zoom=std::clamp(zoom,.1f,5.f);
        if(std::abs(zoom-pdfZoom_)<.000001f) {if(!smooth)cancelPageTurn();return;}
        cancelPageTurn();
        panVelocityX_=panVelocityY_=0;
        const float previous = pdfZoom_;
        pdfZoom_ = zoom;
        const bool retain=smooth && zoomLastPage_==currentPage_ && zoomLastX_==pdfPanX_ && zoomLastY_==pdfPanY_;
        float panX=pdfPanX_+(retain?zoomFractionX_:0),panY=pdfPanY_+(retain?zoomFractionY_:0);
        int w, h; getWindowSize(w, h);
        float pw=0,ph=0;
        if(comicSpreadActive()) {
            const float vw=float(fixedViewportWidth(w)),vh=float(comicViewportHeight(h)),ratio=pdfZoom_/previous;
            const float fx=std::clamp(focalX*w-currentMargin_,0.f,vw),fy=std::clamp(focalY*h-72,0.f,vh);
            panX=(panX+fx-std::max(0.f,vw*(1-previous)/2))*ratio-fx+std::max(0.f,vw*(1-pdfZoom_)/2);
            panY=(panY+fy-std::max(0.f,vh*(1-previous)/2))*ratio-fy+std::max(0.f,vh*(1-pdfZoom_)/2);
        } else if(comic() && px_document_page_size(doc_,currentPage_,&pw,&ph)) {
            const float vw=float(fixedViewportWidth(w)),vh=float(comicViewportHeight(h));
            const float fit=std::min(vw/pw,vh/ph),ratio=pdfZoom_/previous;
            const float fx=std::clamp(focalX*w-currentMargin_,0.f,vw),fy=std::clamp(focalY*h-72,0.f,vh);
            panX=(panX+fx-std::max(0.f,(vw-pw*fit*previous)/2))*ratio-fx+std::max(0.f,(vw-pw*fit*pdfZoom_)/2);
            panY=(panY+fy-std::max(0.f,(vh-ph*fit*previous)/2))*ratio-fy+std::max(0.f,(vh-ph*fit*pdfZoom_)/2);
        } else {
            panX=(panX+fixedViewportWidth(w)*focalX)*pdfZoom_/previous-fixedViewportWidth(w)*focalX;
            panY=(panY+(h-144)*focalY)*pdfZoom_/previous-(h-144)*focalY;
        }
        pdfPanX_=int(std::lround(panX));pdfPanY_=int(std::lround(panY));clampPdfPan();
        zoomFractionX_=pdfPanX_==int(std::lround(panX))?panX-pdfPanX_:0;
        zoomFractionY_=pdfPanY_==int(std::lround(panY))?panY-pdfPanY_:0;
        zoomLastX_=pdfPanX_;zoomLastY_=pdfPanY_;zoomLastPage_=currentPage_;
        if(!comic())pageCache_.clear();
        else if(pdfZoom_<=1.01f)comicDetails_.clear();
        dirty_ = true;
    }
    void setPdfView(int mode) {
        syncComicScrollWidth();
        if (!doc_ || px_document_is_epub(doc_)) return;
        float pw, ph; if (!px_document_page_size(doc_, currentPage_, &pw, &ph)) return;
        int w, h; getWindowSize(w, h);
        const float fit = std::min(fixedViewportWidth(w) / pw, float(comic()?comicViewportHeight(h):h-144) / ph);
        zoomPdf(mode == 1 ? (fixedViewportWidth(w) / pw) / fit : mode == 2 ? 1.0f / fit : 1.0f);
        pdfPanX_ = pdfPanY_ = 0;
        extraStyle_.pdfViewMode = mode;
        saveCurrentPosition(); dirty_ = true;
    }

    bool showZoomReset() const {
        return doc_ && !px_document_is_epub(doc_) && !continuous() &&
            (std::abs(pdfZoom_-1.f)>.01f || pdfPanX_ || pdfPanY_);
    }
    SDL_Rect zoomResetRect(int w,int h) const {return {w-(comic()?280:216),h-136,184,56};}
    void resetReaderZoom() {
        zoomResetTracking_=false;pdfDragging_=false;pdfMoved_=false;
        panVelocityX_=panVelocityY_=0;gestureTracker_.reset();
        setPdfView(0);toolbarVisible_=false;readerToolbarFocusActive_=false;
    }
    void tickComicPan() {
        const Uint32 now=SDL_GetTicks();
        const float dt=std::min(.032f,(now-panPhysicsTick_)/1000.f);panPhysicsTick_=now;
        if(!comic() || screen_!=ScreenState::Reader || quickMenu_ || appBackground_ ||
           continuous() || !activeFingers_.empty() || pdfZoom_<=1 || !extraStyle_.scrollInertia) {
            panVelocityX_=panVelocityY_=0;return;
        }
        if(std::abs(panVelocityX_)+std::abs(panVelocityY_)<=2)return;
        panInertiaX_+=panVelocityX_*dt;panInertiaY_+=panVelocityY_*dt;
        pdfPanX_=int(std::lround(panInertiaX_));pdfPanY_=int(std::lround(panInertiaY_));
        const int requestedX=pdfPanX_,requestedY=pdfPanY_;clampPdfPan();
        if(pdfPanX_!=requestedX){panInertiaX_=float(pdfPanX_);panVelocityX_=0;}
        if(pdfPanY_!=requestedY){panInertiaY_=float(pdfPanY_);panVelocityY_=0;}
        const float decay=std::exp(-6.f*dt);panVelocityX_*=decay;panVelocityY_*=decay;
        dirty_=true;
    }

    int resolveBookmark(const Bookmark &bm) {
        int resolved = px_document_lookup_bookmark(doc_, bm.muBookmark);
        return std::clamp(resolved >= 0 ? resolved : bm.page, 0, std::max(0, pageCount_ - 1));
    }

    void cancelPageTurn() {
        pageTurnAnimation_ = PageTurnAnimation{};
        pageTurnTouch_ = PageTurnTouch{};
        comicPendingTurn_ = PendingComicTurn{};
    }

    SDL_Rect comicPageRect(int page,bool head=false) {
        int w,h;getWindowSize(w,h);float pw=0,ph=0;
        if(!px_document_page_size(doc_,page,&pw,&ph) || pw<=0 || ph<=0)return {};
        const int vw=fixedViewportWidth(w),vh=comicViewportHeight(h);
        const float scale=std::min(vw/pw,vh/ph)*pdfZoom_;
        const int width=std::max(1,int(std::ceil(pw*scale))),height=std::max(1,int(std::ceil(ph*scale)));
        const int panX=head?(readingDirection()<0?std::max(0,width-vw):0):pdfPanX_;
        const int panY=head?0:pdfPanY_;
        return {currentMargin_+std::max(0,(vw-width)/2)-panX,72+std::max(0,(vh-height)/2)-panY,width,height};
    }

    // Own page snapshots independently of the LRU cache. Mixed PDF page sizes
    // share a canvas, and rendering/prefetch cannot invalidate animation textures.
    #include "library_scan.inc"
    #include "comic_spreads.inc"
    #include "reader_sheets.inc"

    SDL_Rect turnCanvas(int target) {
        (void)target;
        int w,h;getWindowSize(w,h);
        return comic() && std::abs(pdfZoom_-1.f)>.01f?comicPageRect(currentPage_):SDL_Rect{0,0,w,h};
    }
    SDL_Color comicPaperBackColor() const {
        const auto bg=palette().bg;
        const auto paper=[](Uint8 c,Uint8 tint){return Uint8(int(c)+(int(tint)-c)/3);};
        return {paper(bg.r,250),paper(bg.g,246),paper(bg.b,230),255};
    }
    int copyComicTexture(SDL_Texture *texture,const SDL_FRect &destination,bool paperBack=false) {
        if(!paperBack)return SDL_RenderCopyF(renderer_,texture,nullptr,&destination);
        Uint8 r,g,b,a;SDL_BlendMode blend{};
        if(SDL_GetTextureColorMod(texture,&r,&g,&b)!=0 || SDL_GetTextureAlphaMod(texture,&a)!=0 ||
           SDL_GetTextureBlendMode(texture,&blend)!=0)return -1;
        auto restore=[&]{SDL_SetTextureColorMod(texture,r,g,b);SDL_SetTextureAlphaMod(texture,a);SDL_SetTextureBlendMode(texture,blend);};
        if(SDL_SetTextureColorMod(texture,156,150,138)!=0 || SDL_SetTextureAlphaMod(texture,22)!=0 ||
           SDL_SetTextureBlendMode(texture,SDL_BLENDMODE_BLEND)!=0) {restore();return -1;}
        const int result=SDL_RenderCopyF(renderer_,texture,nullptr,&destination);
        restore();return result;
    }
    PageTurnTexture captureTurnPage(int page, bool underside=false,SDL_Rect canvas={0,0,0,0}) {
        PageTurnTexture snapshot{nullptr, SDL_DestroyTexture};
        int w, h; getWindowSize(w, h);
        if(!comic()) {
            return captureReaderSurface(w,h,[&]{drawReaderLeaf(page,{0,0,w,h});});
        }
        if(canvas.w<=0 || canvas.h<=0)canvas={0,72,w,std::max(1,h-144)};
        const bool zoomedComic=comic();
        SDL_Texture *source = zoomedComic?comicZoomTexture(page):getOrRenderPageTexture(page);
        if (!source || !SDL_RenderTargetSupported(renderer_)) return snapshot;
        int tw = 0, th = 0;
        if (SDL_QueryTexture(source, nullptr, nullptr, &tw, &th) != 0) return snapshot;
        // Enlarged comics deform the complete sheet, including offscreen edges.
        // Bound snapshot memory by the source resolution, never by 5x display size.
        const float resolution=zoomedComic?std::min({1.f,float(canvas.w)/tw,float(canvas.h)/th}):1.f;
        const int snapshotW=zoomedComic?std::max(1,int(tw*resolution)):canvas.w;
        const int snapshotH=zoomedComic?std::max(1,int(th*resolution)):canvas.h;
        snapshot.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET, snapshotW, snapshotH));
        if (!snapshot) return snapshot;
#if SDL_VERSION_ATLEAST(2,0,12)
        SDL_SetTextureScaleMode(snapshot.get(),SDL_ScaleModeLinear);
#endif
        SDL_Texture *target = SDL_GetRenderTarget(renderer_);
        SDL_Rect viewport{}, clip{};
        SDL_RenderGetViewport(renderer_, &viewport);
        const bool clipped = SDL_RenderIsClipEnabled(renderer_);
        SDL_RenderGetClipRect(renderer_, &clip);
        float sx = 1, sy = 1; SDL_RenderGetScale(renderer_, &sx, &sy);
        Uint8 r, g, b, a; SDL_GetRenderDrawColor(renderer_, &r, &g, &b, &a);
        if (SDL_SetRenderTarget(renderer_, snapshot.get()) != 0) {
            snapshot.reset(); return snapshot;
        }
        SDL_RenderSetScale(renderer_, 1, 1);
        SDL_RenderSetViewport(renderer_, nullptr);
        SDL_RenderSetClipRect(renderer_, nullptr);
        const auto paper=underside?comicPaperBackColor():palette().bg;
        SDL_SetRenderDrawColor(renderer_, paper.r, paper.g, paper.b, 255);
        SDL_Rect dst{(w-tw)/2-canvas.x,(h-th)/2-canvas.y,tw,th};
        if(zoomedComic)dst={0,0,snapshotW,snapshotH};
        const SDL_FRect copy{float(dst.x),float(dst.y),float(dst.w),float(dst.h)};
        const bool ok = SDL_RenderClear(renderer_) == 0 &&
                        copyComicTexture(source,copy,underside) == 0;
        SDL_SetRenderTarget(renderer_, target);
        SDL_RenderSetScale(renderer_, sx, sy);
        SDL_RenderSetViewport(renderer_, &viewport);
        SDL_RenderSetClipRect(renderer_, clipped ? &clip : nullptr);
        SDL_SetRenderDrawColor(renderer_, r, g, b, a);
        if (!ok) snapshot.reset();
        return snapshot;
    }

    bool canCurlPage() {
        return appSettings_.get().pageCurlAnimation && doc_ && screen_==ScreenState::Reader && !quickMenu_ && !appBackground_ &&
               !continuous() && !layoutPending_ && !layoutWorker_.joinable() &&
               (px_document_is_epub(doc_) || comic() || std::abs(pdfZoom_-1.f)<.01f);
    }

    void turnPage(int delta) {
        if (!doc_ || pageCount_<=0 || !delta) return;
        comicTurnError_.clear();
        const int stepLimit=std::min(pageCount_,10);
        const int bounded=std::clamp(delta,-stepLimit,stepLimit);
        if(comic() && pageTurnTouch_.waitingForTexture)pageTurnTouch_=PageTurnTouch{};
        if(comicPendingTurn_.active) {
            if(!comic() || comicPendingTurn_.identity!=comicTextureIdentity() || currentPage_!=comicPendingTurn_.anchorPage)
                comicPendingTurn_=PendingComicTurn{};
            else {
                const int requested=std::clamp(comicPendingTurn_.steps+bounded,-stepLimit,stepLimit);
                if(!requested || readerTurnTarget(comicPendingTurn_.anchorPage,requested)==comicPendingTurn_.anchorPage) {
                    comicPendingTurn_=PendingComicTurn{};dirty_=true;return;
                }
                comicPendingTurn_.steps=requested;
                comicPendingTurn_.targetPage=readerTurnTarget(comicPendingTurn_.anchorPage,requested>0?1:-1);
                ensureComicGroup(comicPendingTurn_.targetPage);
                dirty_=true;return;
            }
        }
        if (pageTurnAnimation_.active) {
            if (pageTurnAnimation_.dragging) return;
            auto &turn=pageTurnAnimation_;
            const int base=turn.interactive && turn.complete?turn.targetPage:currentPage_;
            const int requested=std::clamp(turn.queued+bounded,-stepLimit,stepLimit);
            int position=base;turn.queued=0;
            for(int i=0;i<std::min(pageCount_,std::abs(requested));++i) {
                const int next=readerTurnTarget(position,requested>0?1:-1);if(next==position)break;
                position=next;turn.queued+=requested>0?1:-1;
            }
            return;
        }
        const int finalTarget=readerTurnTarget(currentPage_,comic()?bounded:delta);
        if (finalTarget==currentPage_ || sameReaderSpread(finalTarget,currentPage_)) return;
        const int turnDelta=comic() && std::abs(bounded)>1?(bounded>0?1:-1):(comic()?bounded:delta);
        const int target=readerTurnTarget(currentPage_,turnDelta);
        const int queued=comic()?bounded-turnDelta:0;
        if(comic() && !ensureComicTurn(currentPage_,target)) {
            comicPendingTurn_={true,comicTextureIdentity(),currentPage_,bounded,target};
            dirty_=true;return;
        }
        const int direction=(target>currentPage_?1:-1)*readingDirection();
        if(!comic() && !px_document_is_epub(doc_) && !appSettings_.get().preservePdfZoom)
            setPdfView(extraStyle_.pdfViewMode);
        const bool animate=canCurlPage() && std::abs(turnDelta)==1 && prepareReaderTurn(currentPage_,target,direction);
        if(!animate)cancelPageTurn();
        scrollY_=scrollVelocity_=0;
        currentPage_=target;
        if(comic()) resetComicPagePan();
        clampPdfPan(); saveCurrentPosition();
        if(animate) {
            auto &turn=pageTurnAnimation_;
            getWindowSize(turn.width,turn.height);
            turn.page=turn.targetPage=currentPage_;turn.direction=direction;
            turn.queued=queued;turn.started=SDL_GetTicks();turn.active=true;
        }
        dirty_=true;
        if(queued && !animate)turnPage(queued);
    }

    void beginPageTurnTouch(float x,float y,SDL_FingerID finger) {
        if(!canCurlPage() || pageTurnAnimation_.active) return;
        int w,h;getWindowSize(w,h);
        // Reserve visible controls, not entire invisible bands. Hidden tools
        // must not turn a top/bottom drag into a release-only navigation action.
        if(ui::contains({w-80,8,64,64},int(x*w),int(y*h)))return;
        if(toolbarVisible_ || appSettings_.get().toolbar) {
            for(int i=0;i<4;++i)if(ui::contains(readerHeaderButton(i,w),int(x*w),int(y*h)))return;
            if(ui::contains(readerSeekHit(w,h),int(x*w),int(y*h)) ||
               ui::contains(readerPrev(w,h),int(x*w),int(y*h)) ||
               ui::contains(readerNext(w,h),int(x*w),int(y*h)) ||
               ui::contains(readerPages(w,h),int(x*w),int(y*h)))return;
        }
        pageTurnTouch_={true,finger,x,y,x,0,SDL_GetTicks()};
        pageTurnTouch_.currentX=x;pageTurnTouch_.currentY=y;
    }

    bool movePageTurnTouch(float x,float y,SDL_FingerID finger,bool allowBegin=true) {
        if(!pageTurnTouch_.tracking || finger!=pageTurnTouch_.finger) return false;
        const bool wasWaiting=pageTurnTouch_.waitingForTexture;
        pageTurnTouch_.currentX=x;pageTurnTouch_.currentY=y;
        int w,h;getWindowSize(w,h);
        const float dx=pageTurnTouch_.startX-x, dy=y-pageTurnTouch_.startY;
        if(wasWaiting) {
            if(std::abs(dx)*w<10 || std::abs(dx)*w<std::abs(dy)*h*.65f) {
                pageTurnTouch_.waitingForTexture=false;return true;
            }
            const int direction=dx>0?1:-1,target=readerTurnTarget(currentPage_,direction*readingDirection());
            if(target==currentPage_ || sameReaderSpread(target,currentPage_)) {
                pageTurnTouch_.waitingForTexture=false;return true;
            }
            pageTurnTouch_.pendingTarget=target;pageTurnTouch_.pendingDirection=direction;
            if(!ensureComicTurn(currentPage_,target)) {
                const auto now=SDL_GetTicks(),dt=now-pageTurnTouch_.lastTick;
                if(dt>=4) {pageTurnTouch_.velocity=direction*(pageTurnTouch_.lastX-x)*1000.f/float(dt);pageTurnTouch_.lastX=x;pageTurnTouch_.lastTick=now;}
                dirty_=true;return true;
            }
            pageTurnTouch_.waitingForTexture=false;
        }
        if(!pageTurnAnimation_.active) {
            if((!allowBegin && !wasWaiting) || !canCurlPage() || std::abs(dx)*w<10 || std::abs(dx)*w<std::abs(dy)*h*.65f) return false;
            const int direction=dx>0?1:-1, target=readerTurnTarget(currentPage_,direction*readingDirection());
            if(target==currentPage_ || sameReaderSpread(target,currentPage_)) return false;
            if(comic()) {
                const auto paper=comicTurnBounds(currentPage_);
                if(!pageTurnTouch_.paperContact) {
                    int x1=int(pageTurnTouch_.startX*w),y1=int(pageTurnTouch_.startY*h),x2=int(x*w),y2=int(y*h);
                    // The intersection also handles a fast event crossing the
                    // complete page, and never grabs from the wrong outer edge.
                    if(!SDL_IntersectRectAndLine(&paper,&x1,&y1,&x2,&y2))return true;
                    pageTurnTouch_.startX=float(x1)/w;pageTurnTouch_.startY=float(y1)/h;
                    pageTurnTouch_.paperContact=true;
                }
            }
            if(comic() && !ensureComicTurn(currentPage_,target)) {
                pageTurnTouch_.waitingForTexture=true;pageTurnTouch_.pendingTarget=target;pageTurnTouch_.pendingDirection=direction;
                dirty_=true;return true;
            }
            if(!prepareReaderTurn(currentPage_,target,direction)){cancelPageTurn();return false;}
            auto &turn=pageTurnAnimation_;
            const auto canvas=turn.viewport;
            turn.active=turn.interactive=turn.dragging=true;
            turn.page=currentPage_;turn.targetPage=target;turn.direction=direction;
            turn.width=w;turn.height=h;
            turn.anchorY=std::clamp((pageTurnTouch_.startY*h-canvas.y)/std::max(1.f,float(canvas.h)),0.f,1.f);
            toolbarVisible_=false;
            gestureTracker_.reset();
        }
        auto &turn=pageTurnAnimation_;
        if(!turn.dragging) return false;
        const float dragWidth=float(turn.comicSheet?(turn.spread?comicTurnBounds(currentPage_).w:std::min(turn.viewport.w,fixedViewportWidth(w))):turn.spread?w:turn.viewport.w);
        const float travel=(pageTurnTouch_.startX-x)*w;
        turn.progress=std::clamp(turn.direction*travel/std::max(1.f,dragWidth)*(turn.spread?1.f:.5f),0.f,.999f);
        turn.dragY=std::clamp(dy*h/std::max(1.f,float(turn.viewport.h)),-.5f,.5f);
        const auto now=SDL_GetTicks(), dt=now-pageTurnTouch_.lastTick;
        if(dt>=4 && std::abs(pageTurnTouch_.lastX-x)>.0001f) {
            pageTurnTouch_.velocity=turn.direction*(pageTurnTouch_.lastX-x)*1000.f/float(dt);
            pageTurnTouch_.lastX=x;pageTurnTouch_.lastTick=now;
        }
        dirty_=true;
        return true;
    }

    bool releasePageTurnTouch(float x,float y,SDL_FingerID finger) {
        if(!pageTurnTouch_.tracking || finger!=pageTurnTouch_.finger) return false;
        // A release-only short drift is still a tap; real motion updates are live.
        movePageTurnTouch(x,y,finger,std::abs(x-pageTurnTouch_.startX)>.06f);
        if(pageTurnTouch_.waitingForTexture) {
            const bool turn=pageTurnTouch_.paperContact && std::abs(x-pageTurnTouch_.startX)>.06f;
            const int direction=pageTurnTouch_.pendingDirection*readingDirection();
            pageTurnTouch_=PageTurnTouch{};
            if(turn)turnPage(direction);
            gestureTracker_.reset();dirty_=true;return true;
        }
        pageTurnTouch_.tracking=false;
        auto &turn=pageTurnAnimation_;
        if(!turn.active || !turn.dragging) {
            if(comic() && std::abs(x-pageTurnTouch_.startX)>.02f){gestureTracker_.reset();return true;}
            return false;
        }
        float velocity=SDL_GetTicks()-pageTurnTouch_.lastTick>100?0.f:pageTurnTouch_.velocity;
        turn.complete=turn.progress>=.14f || (velocity>.75f && turn.progress>.025f);
        if(velocity<-.65f && turn.progress<.30f) turn.complete=false;
        turn.dragging=false;turn.from=turn.progress;turn.to=turn.complete?1.f:0.f;
        turn.duration=Uint32(std::clamp(140.f+std::abs(turn.to-turn.from)*300.f,180.f,420.f));
        turn.started=SDL_GetTicks();
        gestureTracker_.reset();dirty_=true;
        return true;
    }

    void tickPageTurn() {
        if(pageTurnTouch_.waitingForTexture) {
            if(!pageTurnTouch_.tracking || !doc_ || screen_!=ScreenState::Reader || quickMenu_ || appBackground_ || continuous())
                pageTurnTouch_=PageTurnTouch{};
            else if(ensureComicTurn(currentPage_,pageTurnTouch_.pendingTarget))
                movePageTurnTouch(pageTurnTouch_.currentX,pageTurnTouch_.currentY,pageTurnTouch_.finger,true);
        }
        auto &turn=pageTurnAnimation_;
        if(!turn.active) return;
        int w,h;getWindowSize(w,h);
        if(!doc_ || screen_!=ScreenState::Reader || quickMenu_ || appBackground_ || continuous() ||
           layoutPending_ || layoutWorker_.joinable() || turn.page!=currentPage_ || w!=turn.width || h!=turn.height) {
            cancelPageTurn();dirty_=true;return;
        }
        if(turn.dragging) {dirty_=true;return;}
        const float time=std::clamp(float(SDL_GetTicks()-turn.started)/turn.duration,0.f,1.f);
        const float ease=turn.interactive?1.f-std::pow(1.f-time,3.f):time*time*(3.f-2.f*time);
        turn.progress=turn.from+(turn.to-turn.from)*ease;
        if(time>=1.f) {
            const int queued=turn.queued;
            const int target=turn.targetPage;
            const bool commit=turn.interactive && turn.complete;
            cancelPageTurn();
            if(commit) {currentPage_=target;scrollY_=scrollVelocity_=0;if(comic())resetComicPagePan();clampPdfPan();saveCurrentPosition();}
            if(queued) {
                turnPage(queued>0?1:-1);
                if(pageTurnAnimation_.active) pageTurnAnimation_.queued=queued-(queued>0?1:-1);
                else if(std::abs(queued)>1) turnPage(queued-(queued>0?1:-1));
            }
        }
        dirty_=true;
    }

    void reflowEpub(int newFontSize, int newTheme = -1) {
        currentFontSize_=std::clamp(newFontSize,14,44);
        if(newTheme>=0) currentTheme_=newTheme;
        if (!doc_) { storeDefaults(); return; }
        if (!px_document_is_epub(doc_)) {
            if (newTheme >= 0) { currentTheme_ = newTheme; saveCurrentPosition(); dirty_ = true; }
            return;
        }

        const auto previous = progressStore_.get(currentBookId_);
        int anchorChapter=-1, anchorCharacter=-1; double anchorOffset=0;
        px_document_anchor(doc_,currentPage_,scrollY_,&anchorChapter,&anchorCharacter,&anchorOffset);
        int64_t bookmark = px_document_make_bookmark(doc_, currentPage_);

        int w = 1280, h = 720;
        getWindowSize(w, h);
        currentFontSize_ = std::clamp(newFontSize, 14, 44);
        if (newTheme >= 0) currentTheme_ = newTheme;

        char err[256] = {};
        const auto layout=epubLayout(w,h,currentStyle());
        if (px_document_relayout(doc_, layout.contentWidth(), extraStyle_.readingMode ? 0 : layout.contentHeight(),
                                 static_cast<float>(currentFontSize_),
                                 buildEpubCss().c_str(), err, sizeof(err))) {
            documentMode_=extraStyle_.readingMode; renderedStyle_=currentStyle();
            layoutWidth_=w; layoutHeight_=h;
            pageCache_.clear();
            loadToc();
            pageCount_ = px_document_pages(doc_);
            int restored = px_document_lookup_bookmark(doc_, bookmark);
            if (restored < 0 || restored >= pageCount_) {
                auto p = progressStore_.get(currentBookId_);
                restored = restorePage(p, pageCount_);
            }
            currentPage_ = std::clamp(restored, 0, std::max(0, pageCount_ - 1));
            float anchorY=0;
            if(px_document_restore_anchor(doc_,anchorChapter,anchorCharacter,anchorOffset,&currentPage_,&anchorY)) scrollY_=anchorY;
            else scrollY_=0;
            scrollVelocity_=0;
            if(!extraStyle_.styleOverride) storeDefaults();
            saveCurrentPosition();
        } else {
            applyStyle(previous);
            currentErrorMessage_ = std::string("Yeniden boyutlandırma hatası: ") + err;
            logMessage(currentErrorMessage_);
        }
        dirty_ = true;
    }

    bool continuous() const { return doc_ && (px_document_is_epub(doc_)?documentMode_==1:comic() && extraStyle_.readingMode==1); }
    double readingFraction() {
        if(!continuous()) return pageCount_>1?double(currentPage_)/(pageCount_-1):0;
        int w,h; getWindowSize(w,h);
        bool last=currentPage_+1==pageCount_;
        double extent=segmentHeight(currentPage_)-(last?std::max(1,h-144):0);
        double chapterFraction=extent>0?std::clamp(double(scrollY_)/extent,0.,1.):(last?1.:0.);
        return std::clamp((currentPage_+chapterFraction)/std::max(1,pageCount_),0.,1.);
    }
    float segmentHeight(int page) {
        float w=0,h=0;
        if(!px_document_page_size(doc_,page,&w,&h))return 1.f;
        if(comic()) {int width,height;getWindowSize(width,height);h*=std::max(1,width-currentMargin_*2)/std::max(1.f,w);}
        return std::max(1.f,h);
    }
    void scrollBy(float delta) {
        if (!continuous()) return;
        scrollY_ += delta;
        while(scrollY_ >= segmentHeight(currentPage_) && currentPage_+1 < pageCount_) {
            scrollY_ -= segmentHeight(currentPage_); ++currentPage_;
        }
        while(scrollY_<0 && currentPage_>0) { --currentPage_; scrollY_+=segmentHeight(currentPage_); }
        int w,h; getWindowSize(w,h);
        float limit=currentPage_+1 == pageCount_ ? std::max(0.f,segmentHeight(currentPage_)-(h-144)) : segmentHeight(currentPage_);
        float clamped=std::clamp(scrollY_,0.f,limit);
        if(clamped!=scrollY_) scrollVelocity_=0;
        scrollY_=clamped; dirty_=true;
    }
    void tickScroll() {
        Uint32 now=SDL_GetTicks(); float dt=std::min(.032f,(now-physicsTick_)/1000.f); physicsTick_=now;
        if (!continuous() || screen_!=ScreenState::Reader || scrollTracking_) { scrollVelocity_=0; return; }
        if(std::abs(scrollVelocity_)>1) {
            scrollBy(scrollVelocity_*dt); scrollVelocity_*=std::exp(-5.f*dt);
            if(std::abs(scrollVelocity_)<=1) { scrollVelocity_=0; saveCurrentPosition(); }
        }
    }
    void idlePrefetch() {
        if(scrub_.active || screen_==ScreenState::TocModal)return;
        if(!doc_ || pageTurnAnimation_.active || continuous() || screen_!=ScreenState::Reader || quickMenu_ || layoutPending_ || layoutWorker_.joinable() || !activeFingers_.empty()) return;
        if(comic()){prefetchComicPages();return;}
        auto now=SDL_GetTicks();
        if(prefetchPage_!=currentPage_) { prefetchPage_=currentPage_; prefetchStep_=0; prefetchDue_=now+60; return; }
        if(prefetchStep_>=2 || now<prefetchDue_)return;
        const int target=currentPage_+(prefetchStep_==0?1:-1);++prefetchStep_;prefetchDue_=now+60;
        if(target>=0 && target<pageCount_)getOrRenderPageTexture(target);
    }

    SDL_Texture *scrollTile(int page,int tile,int width,int *height) {
        int y=tile*512; *height=std::min(512,std::max(1,int(std::ceil(segmentHeight(page)))-y));
        auto key="scroll:"+currentBookId_+":"+std::to_string(page)+":"+std::to_string(tile)+":"+std::to_string(width)+":paper:"+std::to_string(appSettings_.get().paperTexture)+":comic:"+std::to_string(extraStyle_.pdfColorMode)+":"+std::to_string(currentTheme_)+":"+std::to_string(extraStyle_.backgroundColor)+":"+std::to_string(extraStyle_.textColor);
        if(auto tex=pageCache_.get(key)) return tex;
        PxImage image{}; char error[256]{};
        if(comic())++comicTextureBuilds_;
        if(!px_document_render_tile(doc_,page,y,width,*height,&image,error,sizeof error)) {
            if(page==currentPage_)pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(page+1)+" · "+error;
            return nullptr;
        }
        if(page==currentPage_)pageRenderError_.clear();
        const auto style=presentedStyle();
        auto bg=readingColor(style.backgroundColor,g_palettes[style.theme].bg);
        if(!comic()) applyPaperTexture(image.rgb,image.width,image.height,image.stride,(bg.r<<16)|(bg.g<<8)|bg.b,appSettings_.get().paperTexture,y);
        else if(comicReadingColors()) {auto ink=palette().fg;px_image_apply_reading_colors(&image,(bg.r<<16)|(bg.g<<8)|bg.b,(ink.r<<16)|(ink.g<<8)|ink.b);}
        auto tex=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC,image.width,image.height);
        if(tex && SDL_UpdateTexture(tex,nullptr,image.rgb,image.stride)!=0) { SDL_DestroyTexture(tex); tex=nullptr; }
        px_image_free(&image);
        if(tex && !pageCache_.put(key,tex,width,*height)) return nullptr;
        return tex;
    }
    bool renderScroll(int w,int h) {
        pageRenderError_.clear(); // A previous failed page must not label a valid cached strip.
        SDL_Rect viewport{currentMargin_,72,w-currentMargin_*2,std::max(1,h-144)};
        SDL_RenderSetClipRect(renderer_,&viewport);
        if(comic()) {
            bool pending=false;
            int failedPage=-1;
            int page=currentPage_;float y=72-scrollY_;
            while(page<pageCount_ && y<h-72+512) {
                const float height=segmentHeight(page);
                if(auto texture=comicZoomTexture(page)) {
                    SDL_Rect destination{viewport.x,int(std::round(y)),viewport.w,std::max(1,int(std::ceil(height)))};
                    SDL_RenderCopy(renderer_,texture,nullptr,&destination);
                } else if(comicBaseRetry_.count(page))failedPage=page;
                else pending=true;
                y+=height;++page;
            }
            if(failedPage>=0)pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(failedPage+1);
            SDL_RenderSetClipRect(renderer_,nullptr);
            return pending;
        }
        if(currentPage_>0 && scrollY_<512) {
            int height; scrollTile(currentPage_-1,std::max(0,int(std::ceil(segmentHeight(currentPage_-1)/512))-1),viewport.w,&height);
        }
        int page=currentPage_; float y=72-scrollY_;
        // Visible strips plus one nearby strip on each side; LRU has a fixed byte ceiling.
        while(page<pageCount_ && y<h-72+512) {
            float height=segmentHeight(page);
            int first=std::max(0,int((72-y)/512)-1);
            int last=std::min(int(std::ceil(height/512)),int((h-72+512-y)/512)+1);
            for(int tile=first;tile<last;++tile) {
                int th; auto tex=scrollTile(page,tile,viewport.w,&th);
                SDL_Rect dst{viewport.x,int(std::round(y))+tile*512,viewport.w,th};
                if(tex) SDL_RenderCopy(renderer_,tex,nullptr,&dst);
            }
            y+=height; ++page;
        }
        SDL_RenderSetClipRect(renderer_,nullptr);
        return false;
    }

    std::string comicTextureIdentity() const {
        return currentBookId_+":"+std::to_string(comicReadingColors())+":"+std::to_string(currentTheme_)+":"+
            std::to_string(extraStyle_.backgroundColor)+":"+std::to_string(extraStyle_.textColor);
    }
    std::string comicTextureKey(int page) const {return "comic-base:"+comicTextureIdentity()+":"+std::to_string(page);}
    SDL_Texture *comicZoomTexture(int page=-1) {
        if(page<0)page=currentPage_;
        const auto identity=comicTextureIdentity();
        if(pdfZoom_>1.01f) {
            auto detail=comicDetails_.find(page);
            if(detail!=comicDetails_.end() && detail->second.identity==identity)return detail->second.texture.get();
        }
        const auto key=comicTextureKey(page);
        if(auto texture=pageCache_.get(key))return texture;
        configureComicPages();
        auto retry=comicBaseRetry_.find(page);
        if(retry!=comicBaseRetry_.end()) {
            if(Sint32(SDL_GetTicks()-retry->second)<0) {
                const auto group=comicSpreadPages(currentPage_);
                if(page>=group.first && page<group.first+group.second)
                    pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(page+1);
                return nullptr;
            }
            comicBaseRetry_.erase(retry);
        }
        comicPages_.request(page,ComicPages::Priority::Visible);
        return nullptr;
    }
    void renderComicZoom(int w,int h) {
        auto texture=comicZoomTexture();if(!texture)return;
        float pw=0,ph=0;if(!px_document_page_size(doc_,currentPage_,&pw,&ph))return;
        const SDL_Rect viewport{currentMargin_,72,fixedViewportWidth(w),comicViewportHeight(h)};
        const float scale=std::min(viewport.w/pw,viewport.h/ph)*pdfZoom_;
        const int width=std::max(1,int(std::ceil(pw*scale))),height=std::max(1,int(std::ceil(ph*scale)));
        const SDL_Rect dest{viewport.x+std::max(0,(viewport.w-width)/2)-pdfPanX_,
            viewport.y+std::max(0,(viewport.h-height)/2)-pdfPanY_,width,height};
        SDL_RenderSetClipRect(renderer_,&viewport);SDL_RenderCopy(renderer_,texture,nullptr,&dest);
        SDL_RenderSetClipRect(renderer_,nullptr);
    }
    void configureComicPages() {
        const auto identity=comicTextureIdentity();if(identity==comicPagesIdentity_)return;
        comicPagesIdentity_=identity;comicDetails_.clear();comicDetailRetry_.clear();comicBaseRetry_.clear();prefetchPage_=-1;comicPrefetchDirection_=1;
        auto book=std::find_if(books_.begin(),books_.end(),[&](const Book &b){return b.id==currentBookId_;});
        if(book==books_.end())return;
        SDL_RendererInfo info{};SDL_GetRendererInfo(renderer_,&info);const auto color=palette();
        const int limit=std::min(info.max_texture_width>0?info.max_texture_width:3072,info.max_texture_height>0?info.max_texture_height:3072);
        comicPages_.configure({identity,book->path,comicReadingColors(),
            (unsigned(color.bg.r)<<16)|(unsigned(color.bg.g)<<8)|color.bg.b,
            (unsigned(color.fg.r)<<16)|(unsigned(color.fg.g)<<8)|color.fg.b,limit});
    }
    bool ensureComicGroup(int page) {
        if(!comic() || page<0 || page>=pageCount_)return false;
        configureComicPages();const auto group=comicSpreadPages(page);bool ready=true;
        for(int side=0;side<group.second;++side) {
            const int member=group.second==2?readerSidePage(group.first,side):group.first;
            if(pageCache_.get(comicTextureKey(member)))continue;
            ready=false;
            comicPages_.request(member,ComicPages::Priority::TurnTarget);
        }
        return ready;
    }
    bool ensureComicTurn(int from,int target) {
        configureComicPages();
        const auto current=comicSpreadPages(from);
        bool currentReady=true;
        for(int side=0;side<current.second;++side) {
            const int page=current.second==2?readerSidePage(current.first,side):current.first;
            if(!pageCache_.get(comicTextureKey(page))) {
                currentReady=false;
                if(page==currentPage_ && comicZoomTexture(page))currentReady=true;
                else
                    comicPages_.request(page,ComicPages::Priority::CurrentSpreadPartner);
            }
        }
        const bool targetReady=ensureComicGroup(target);
        return currentReady && targetReady;
    }
    void failPendingComicPage(int page) {
        const auto contains=[&](int base) {
            const auto group=comicSpreadPages(base);
            return page>=group.first && page<group.first+group.second;
        };
        const bool pending=comicPendingTurn_.active &&
            (contains(comicPendingTurn_.anchorPage) || contains(comicPendingTurn_.targetPage));
        const bool touch=pageTurnTouch_.waitingForTexture &&
            (contains(currentPage_) || contains(pageTurnTouch_.pendingTarget));
        if(!pending && !touch)return;
        comicPendingTurn_=PendingComicTurn{};
        if(touch){pageTurnTouch_=PageTurnTouch{};gestureTracker_.reset();}
        comicTurnError_=catLabel("Sayfa yüklenemedi; mevcut sayfada kalındı.",
                                 "Page could not be loaded; stayed on the current page.");
        dirty_=true;
    }
    void failComicPage(int page,const std::string &error={}) {
        comicBaseRetry_[page]=SDL_GetTicks()+3000;
        const auto group=comicSpreadPages(currentPage_);
        if(page>=group.first && page<group.first+group.second)
            pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(page+1)+
                (error.empty()?std::string{}:" · "+error);
        failPendingComicPage(page);
        dirty_=true;
    }
    void prefetchComicPages() {
        configureComicPages();const auto now=SDL_GetTicks();
        if(prefetchPage_!=currentPage_) {
            if(prefetchPage_>=0)comicPrefetchDirection_=currentPage_>prefetchPage_?1:-1;
            prefetchPage_=currentPage_;prefetchStep_=0;prefetchDue_=now+60;comicDetailDue_=now+180;return;
        }
        if(now<prefetchDue_)return;
        if(prefetchStep_<2) {
            const auto group=comicSpreadPages(currentPage_);
            const int step=prefetchStep_++;
            // Read in the navigation direction. Speculative backward reads
            // would repeatedly restart decompression of a solid RAR archive.
            const int target=comicPrefetchDirection_>0?group.first+group.second+step:group.first-1-step;
            if(target>=0 && target<pageCount_ && !pageCache_.get(comicTextureKey(target)))comicPages_.request(target);
            prefetchDue_=now+30;return;
        }
        refineComicZoom();
    }
    void refineComicZoom() {
        if(!comic() || pdfZoom_<=1.01f || screen_!=ScreenState::Reader || quickMenu_ || continuous() ||
           appBackground_ || !activeFingers_.empty() || pageTurnAnimation_.active ||
           std::abs(panVelocityX_)+std::abs(panVelocityY_)>2 || Sint32(SDL_GetTicks()-comicDetailDue_)<0)return;
        configureComicPages();const auto group=comicSpreadPages(currentPage_);
        SDL_RendererInfo info{};SDL_GetRendererInfo(renderer_,&info);
        const int limit=std::min(info.max_texture_width>0?info.max_texture_width:3072,
                                 info.max_texture_height>0?info.max_texture_height:3072);
        for(int i=0;i<group.second;++i) {
            const int page=group.first+i;
            if(comicDetails_.count(page) ||
               (comicDetailRetry_.count(page) && Sint32(SDL_GetTicks()-comicDetailRetry_[page])<0))continue;
            float width=0,height=0;
            if(!px_document_page_size(doc_,page,&width,&height) ||
               ComicResolution::size(width,height,false,limit)==ComicResolution::size(width,height,true,limit))continue;
            comicPages_.request(page,true);
        }
    }
    void processComicPages() {
        const bool detailAllowed=activeFingers_.empty() && !pageTurnAnimation_.active && std::abs(panVelocityX_)+std::abs(panVelocityY_)<=2;
        for(auto &result:comicPages_.take()) {
            if(!comic() || result.identity!=comicTextureIdentity())continue;
            px_document_remember_comic_size(doc_,result.page,result.nativeWidth,result.nativeHeight);
            if(result.detail && !detailAllowed){comicDetailRetry_[result.page]=SDL_GetTicks()+180;continue;}
            if(result.rgb.empty()){
                if(result.detail)comicDetailRetry_[result.page]=SDL_GetTicks()+3000;
                else failComicPage(result.page,result.error);
                continue;
            }
            const auto group=comicSpreadPages(currentPage_);const bool visible=result.page>=group.first && result.page<group.first+group.second;
            if(result.detail && (!visible || pdfZoom_<=1.01f))continue;
            const auto key=comicTextureKey(result.page);
            if(!result.detail && pageCache_.get(key))continue;
            auto *texture=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STATIC,result.width,result.height);
            if(!texture){if(!result.detail)failComicPage(result.page);continue;}
            if(SDL_UpdateTexture(texture,nullptr,result.rgb.data(),result.stride)!=0){SDL_DestroyTexture(texture);if(!result.detail)failComicPage(result.page);continue;}
#if SDL_VERSION_ATLEAST(2,0,12)
            SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
#endif
            if(result.detail) {
                for(auto it=comicDetails_.begin();it!=comicDetails_.end();) {
                    if(it->first<group.first || it->first>=group.first+group.second)it=comicDetails_.erase(it);else ++it;
                }
                ComicDetail detail;detail.identity=result.identity;detail.texture.reset(texture);comicDetails_[result.page]=std::move(detail);
            } else {
                std::vector<std::string> protectedKeys;
                const auto protect=[&](int base) {
                    const auto visible=comicSpreadPages(base);
                    for(int i=0;i<visible.second;++i) {
                        const int page=visible.first+i;const auto protectedKey=comicTextureKey(page);
                        if(std::find(protectedKeys.begin(),protectedKeys.end(),protectedKey)==protectedKeys.end())protectedKeys.push_back(protectedKey);
                    }
                };
                protect(currentPage_);if(comicPendingTurn_.active)protect(comicPendingTurn_.targetPage);
                if(!pageCache_.put(key,texture,result.width,result.height,protectedKeys))failComicPage(result.page);
                else comicBaseRetry_.erase(result.page);
            }
            if(visible || (comicPendingTurn_.active && result.page>=comicSpreadPages(comicPendingTurn_.targetPage).first &&
                           result.page<comicSpreadPages(comicPendingTurn_.targetPage).first+comicSpreadPages(comicPendingTurn_.targetPage).second))dirty_=true;
        }
        resumePendingComicTurn();
    }

    void resumePendingComicTurn() {
        if(!comicPendingTurn_.active)return;
        if(!doc_ || !comic() || screen_!=ScreenState::Reader || quickMenu_ || appBackground_ || continuous() ||
           comicPendingTurn_.identity!=comicTextureIdentity() || comicPendingTurn_.anchorPage!=currentPage_) {
            comicPendingTurn_=PendingComicTurn{};return;
        }
        if(!ensureComicTurn(comicPendingTurn_.anchorPage,comicPendingTurn_.targetPage))return;
        const int steps=comicPendingTurn_.steps;comicPendingTurn_=PendingComicTurn{};
        const int direction=steps>0?1:-1;turnPage(direction);
        const int queued=steps-direction;
        if(queued) {
            if(pageTurnAnimation_.active)pageTurnAnimation_.queued=queued;
            else turnPage(queued);
        }
    }

    SDL_Texture *getOrRenderPageTexture(int pageIdx) {
        if (!doc_ || pageIdx < 0 || pageIdx >= pageCount_) return nullptr;

        int w = 1280, h = 720;
        getWindowSize(w, h);

        std::string key;
        if (px_document_is_epub(doc_)) {
            const auto style=presentedStyle();
            key = PageCache::makeEpubKey(currentBookId_,pageIdx,style.fontSize,style.theme,
                                         style.lineHeight,style.margin,style.textAlign,w,h,style.fontFamily);
        } else {
            const auto color=palette();
            key = PageCache::makePdfKey(currentBookId_, pageIdx, pdfZoom_, pdfPanX_, pdfPanY_, w, h)
                + ":comic:" + std::to_string(comicReadingColors())
                + ":paper:" + std::to_string((int(color.bg.r)<<16)|(int(color.bg.g)<<8)|color.bg.b)
                + ":ink:" + std::to_string((int(color.fg.r)<<16)|(int(color.fg.g)<<8)|color.fg.b);
        }

        if(px_document_is_epub(doc_))key+=":paper:"+std::to_string(appSettings_.get().paperTexture);
        SDL_Texture *tex = pageCache_.get(key);
        if (tex) {if(pageIdx==currentPage_)pageRenderError_.clear();return tex;}

        PxImage img{};
        char err[256] = {};
        int ok = 0;

        if (px_document_is_epub(doc_)) {
            const auto layout=readerLayout();
            ok = px_document_render(doc_, pageIdx, layout.contentWidth(), layout.contentHeight(), &img, err, sizeof(err));
        } else {
            // PDF: support viewport zoom and panning
            if(comic())++comicTextureBuilds_;
            if (pdfZoom_ != 1.0f) {
                ok = px_document_render_view(doc_, pageIdx, pdfZoom_, pdfPanX_, pdfPanY_,
                                             fixedViewportWidth(w), h - 144, &img, err, sizeof(err));
            } else {
                const auto layout=readerLayout();
                ok = px_document_render(doc_, pageIdx, layout.contentWidth(), layout.contentHeight(), &img, err, sizeof(err));
            }
        }

        if (ok && img.rgb) {
            if (!px_document_is_epub(doc_) && (!comic() || comicReadingColors())) {
                const auto color=palette();
                const unsigned paper=(unsigned(color.bg.r)<<16)|(unsigned(color.bg.g)<<8)|color.bg.b;
                const unsigned ink=(unsigned(color.fg.r)<<16)|(unsigned(color.fg.g)<<8)|color.fg.b;
                px_image_apply_reading_colors(&img,paper,ink);
            } else if(px_document_is_epub(doc_)) {const auto style=presentedStyle();auto bg=readingColor(style.backgroundColor,g_palettes[style.theme].bg);applyPaperTexture(img.rgb,img.width,img.height,img.stride,(bg.r<<16)|(bg.g<<8)|bg.b,appSettings_.get().paperTexture);}
            tex = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STATIC, img.width, img.height);
            if (tex) {
                if (SDL_UpdateTexture(tex, nullptr, img.rgb, img.stride) != 0) {
                    SDL_DestroyTexture(tex); tex = nullptr;
                } else if (!pageCache_.put(key, tex, img.width, img.height)) tex = nullptr;
            }
            px_image_free(&img);
        }
        if(pageIdx==currentPage_) {
            if(tex)pageRenderError_.clear();
            else pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(pageIdx+1)+" · "+(err[0]?err:catLabel("Görüntü belleği yetersiz","Not enough image memory"));
        }
        return tex;
    }

    SDL_Texture *getOrRenderThumbnail(const Book &b) {
        std::string key = "thumb_" + b.id;
        SDL_Texture *tex = thumbCache_.get(key);
        if (tex) return tex;

        // If not already in cache and not failed, enqueue for asynchronous background loading
        std::lock_guard<std::mutex> lock(queueMutex_);
        if(coverStates_[b.id]==CoverState::NoEmbeddedCover || coversPaused_) return nullptr;
        auto failed = failedThumbIds_.find(b.id);
        if (failed != failedThumbIds_.end() && failed->second <= std::chrono::steady_clock::now())
            failedThumbIds_.erase(failed);
        if (failedThumbIds_.find(b.id) == failedThumbIds_.end() && queuedThumbIds_.size() < 32 &&
            queuedThumbIds_.find(b.id) == queuedThumbIds_.end()) {
            queuedThumbIds_.insert(b.id);
            coverStates_[b.id]=CoverState::Loading;
            thumbQueue_.push_back(b);
            queueCv_.notify_one();
        }
        return nullptr;
    }

    void scanBrowserDirectory() {
        browserEntries_.clear();
        std::error_code ec;
        if (!fs::exists(browserPath_, ec)) return;

        fs::directory_iterator it(browserPath_, fs::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end && browserEntries_.size() < 10000; it.increment(ec)) {
            const auto &entry = *it;
            if (entry.is_symlink(ec)) continue;
            if (entry.is_directory(ec)) {
                browserEntries_.push_back({entry.path(), true, 0});
            } else if (entry.is_regular_file(ec)) {
                auto fmt = detectFormat(entry.path().string());
                if (fmt != Format::Unsupported) {
                    browserEntries_.push_back({entry.path(), false, entry.file_size(ec)});
                }
            }
        }
        std::sort(browserEntries_.begin(), browserEntries_.end(), [](const BrowserEntry &a, const BrowserEntry &b) {
            if (a.isDirectory != b.isDirectory) return a.isDirectory > b.isDirectory;
            int n=naturalCompare(a.path.filename().string(),b.path.filename().string());return n?n<0:a.path<b.path;
        });
        browserSelected_ = 0;
        browserOffset_ = 0; browserScroll_=0;
        dirty_ = true;
    }

    void openBrowser() {
        if(doc_)closeBook();
        globalSettings_=false; quickMenu_=false; listVelocity_=0;
#ifdef __SWITCH__
        browserRoot_ = "sdmc:/";
#else
        browserRoot_ = root_;
#endif
        browserPath_ = browserRoot_;
        if(!appSettings_.get().defaultBrowserPath.empty()) {
            std::error_code ec; auto candidate=fs::u8path(appSettings_.get().defaultBrowserPath);
            auto canonical=fs::weakly_canonical(candidate,ec); auto base=fs::weakly_canonical(browserRoot_,ec);
            if(!ec && fs::is_directory(canonical,ec) && !ec) {
                auto relative=canonical.lexically_relative(base);
                if(!relative.empty() && *relative.begin()!="..") browserPath_=canonical;
            }
        }
        scanBrowserDirectory();
        screen_ = ScreenState::FileBrowser;
        dirty_ = true;
    }

    void handleBrowserSelect() {
        if (browserSelected_ < 0 || browserSelected_ >= static_cast<int>(browserEntries_.size())) return;
        const auto &entry = browserEntries_[browserSelected_];
        if (entry.isDirectory) {
            browserPath_ = entry.path;
            scanBrowserDirectory();
        } else {
            // Open selected book directly
            Book b;
            b.path = entry.path.string();
            b.format = detectFormat(b.path);
            b.id = documentId(b.path, entry.size);
            b.title = entry.path.stem().string();
            b.bytes = entry.size;
            auto found = std::find_if(books_.begin(), books_.end(), [&](const Book &book) { return book.id == b.id; });
            if (found == books_.end()) books_.push_back(b);
            openBook(b);
        }
    }

    #include "catalog_ui.inc"
    #include "v04_focus.inc"
    #include "v04_choices.inc"
    #include "controller_input.inc"
    #include "series_library.inc"
    #include "reader_book_chrome.inc"
    #include "page_navigation.inc"
    #include "self_update_ui.inc"

    void changeBrightnessFromSlider(int row,int x,int width,int height,bool save) {
        ui::SettingsLayout layout(width,height);auto control=layout.control(row);control.y-=int(settingsScroll_);
        const auto track=ui::settingsSliderTrack(control);auto preferences=appSettings_.get();
        if(row==8)preferences.softwareBrightness=ui::settingsSliderValue(track,x,50,100);
        else preferences.systemBrightness=ui::settingsSliderValue(track,x,10,100);
        appSettings_.put(preferences);
        if(row==9) {
#ifdef __SWITCH__
            backlightAvailable_=setSystemBrightness(preferences.systemBrightness);
            if(!backlightAvailable_)currentErrorMessage_=catLabel("El cihazı parlaklık servisi kullanılamıyor; yazılımsal dimmer kullanılabilir.",
                "Handheld brightness control is unavailable; software dimming remains available.");
#endif
        }
        if(save)persistAppSettings();
        dirty_=true;
    }
    bool brightnessSliderEvent(const SDL_Event &event) {
        if(brightnessDragRow_>=0) {
            if(event.type!=SDL_FINGERMOTION && event.type!=SDL_FINGERUP && event.type!=SDL_FINGERDOWN)return false;
            if(event.tfinger.fingerId!=brightnessFinger_)return true;
            int width,height;getWindowSize(width,height);
            changeBrightnessFromSlider(brightnessDragRow_,int(event.tfinger.x*width),width,height,event.type==SDL_FINGERUP);
            ui::pointer(int(event.tfinger.x*width),int(event.tfinger.y*height),event.type!=SDL_FINGERUP);
            if(event.type==SDL_FINGERUP)brightnessDragRow_=-1;
            return true;
        }
        if(screen_!=ScreenState::AaModal || aaTab_!=2 || event.type!=SDL_FINGERDOWN)return false;
        int width,height;getWindowSize(width,height);const int x=int(event.tfinger.x*width),y=int(event.tfinger.y*height);
        const ui::SettingsLayout layout(width,height);
        for(int row:{8,9}) {
            auto control=layout.control(row);control.y-=int(settingsScroll_);
            const auto track=ui::settingsSliderTrack(control);
            const SDL_Rect hit{track.x-16,control.y,track.w+32,control.h};
            if(ui::contains(hit,x,y)) {
                brightnessDragRow_=row;brightnessFinger_=event.tfinger.fingerId;
                ui::pointer(x,y,true);changeBrightnessFromSlider(row,x,width,height,false);return true;
            }
        }
        return false;
    }

    // Input & Event Handling
    void handleEvent(const SDL_Event &incoming, bool &running) {
        SDL_Event e=incoming;
        trace("event_received");
        int orientation=appSettings_.get().displayOrientation;
        if(orientation && (e.type==SDL_FINGERDOWN || e.type==SDL_FINGERMOTION || e.type==SDL_FINGERUP)) {
            int pw,ph;SDL_GetWindowSize(window_,&pw,&ph);DisplayTransform transform{pw,ph,orientation};
            auto point=transform.toLogical(e.tfinger.x*pw,e.tfinger.y*ph);
            e.tfinger.x=point.first/transform.logicalWidth();e.tfinger.y=point.second/transform.logicalHeight();
        }
        if(orientation && e.type==SDL_MULTIGESTURE){float x=e.mgesture.x,y=e.mgesture.y;e.mgesture.x=orientation==1?y:1-y;e.mgesture.y=orientation==1?1-x:x;}
        if (e.type == SDL_QUIT) {
            running = false;
            return;
        }
        if(e.type==SDL_APP_WILLENTERBACKGROUND){restoreSystemBrightness();cancelPageTurn();appBackground_=true;comicPages_.setSuspended(true);readingStats_.reanchor();readingStats_.save();journal_.pause(SDL_GetTicks(),std::time(nullptr));journal_.save();}
        if(e.type==SDL_APP_DIDENTERFOREGROUND){appBackground_=false;comicPages_.setSuspended(false);backlightApplyPending_=true;
#ifdef __SWITCH__
            backlightUnavailable_=false;
#endif
            controllerFocused_=true;clearHeldControls();readingStats_.reanchor();}
        if(e.type==SDL_FINGERDOWN||e.type==SDL_FINGERMOTION||e.type==SDL_KEYDOWN||e.type==SDL_CONTROLLERBUTTONDOWN||e.type==SDL_CONTROLLERAXISMOTION){
            readingStats_.interaction(SDL_GetTicks());
            if(screen_==ScreenState::Reader&&!quickMenu_&&!appBackground_)journal_.interaction(SDL_GetTicks());
        }
        if((e.type==SDL_FINGERDOWN||e.type==SDL_FINGERUP||e.type==SDL_FINGERMOTION)&&e.tfinger.touchId!=-42){sawFinger_=true;lastFingerTick_=SDL_GetTicks();}
        if(e.type==SDL_MOUSEBUTTONDOWN&&e.button.button==SDL_BUTTON_RIGHT&&screen_==ScreenState::Library&&catalogPanel_==CatalogPanel::None&&choiceKind_<0&&colorEditing_<0){
            int pw,ph;SDL_GetWindowSize(window_,&pw,&ph);DisplayTransform transform{pw,ph,appSettings_.get().displayOrientation};
            const auto point=transform.toLogical(e.button.x,e.button.y);const int i=libraryBookAt(float(point.first)/transform.logicalWidth(),float(point.second)/transform.logicalHeight());
            if(i>=0)openBookActions(visibleBooks_[i]);
            return;
        }
        if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEMOTION) {
            if ((sawFinger_ && SDL_GetTicks() - lastFingerTick_ < 750)) return;
            const bool motion = e.type == SDL_MOUSEMOTION;
            if (!motion && e.button.button != SDL_BUTTON_LEFT) return;
            if (motion && !pointerActive_) return;
            if (!motion && e.type == SDL_MOUSEBUTTONDOWN) pointerActive_ = true;
            if (!motion && e.type == SDL_MOUSEBUTTONUP && !pointerActive_) return;
            int w, h; SDL_GetWindowSize(window_, &w, &h);
            SDL_Event finger{};
            finger.type = motion ? SDL_FINGERMOTION : e.type == SDL_MOUSEBUTTONDOWN ? SDL_FINGERDOWN : SDL_FINGERUP;
            finger.tfinger.touchId = -42; // Internal mouse fallback; do not mark as native finger input.
            finger.tfinger.fingerId = -42;
            finger.tfinger.x = float(motion ? e.motion.x : e.button.x) / std::max(1, w);
            finger.tfinger.y = float(motion ? e.motion.y : e.button.y) / std::max(1, h);
            handleEvent(finger, running);
            if (!motion && e.type == SDL_MOUSEBUTTONUP) pointerActive_ = false;
            return;
        }

        if(updateEvent(e))return;
        if(brightnessSliderEvent(e))return;
        if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_LOST) {controllerFocused_=false;clearHeldControls();}
        if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_FOCUS_GAINED) {controllerFocused_=true;clearHeldControls();}
        if(e.type==SDL_APP_WILLENTERBACKGROUND)clearHeldControls();
        if(trackHeldControl(e))return;
        // Held pan masks stay in physical coordinates so a live rotation also
        // redirects a held button. Discrete UI actions are mapped exactly once.
        orientDirectionalEvent(e);
        if(modalTouchGuard(e))return;
        if(choiceEvent(e))return;
        if(handleCatalogEvent(e))return;
        if (colorEditing_ >= 0) {
            if(e.type==SDL_FINGERUP || (e.type==SDL_MOUSEBUTTONUP && e.button.button==SDL_BUTTON_LEFT && !(sawFinger_ && SDL_GetTicks()-lastFingerTick_<750))) {
                int w,h;getWindowSize(w,h);auto panel=colorPanel(w,h);
                int x=int(e.tfinger.x*w),y=int(e.tfinger.y*h);
                if(e.type==SDL_MOUSEBUTTONUP){int pw,ph;SDL_GetWindowSize(window_,&pw,&ph);DisplayTransform transform{pw,ph,orientation};auto point=transform.toLogical(float(e.button.x),float(e.button.y));x=int(point.first);y=int(point.second);}
                if(e.type==SDL_FINGERUP) {lastFingerTick_=SDL_GetTicks();sawFinger_=true;}
                int cell=(panel.w-48)/4;
                for(int i=0;i<16;++i)if(ui::contains({panel.x+24+(i%4)*cell,panel.y+72+(i/4)*60,cell-8,52},x,y)) {swatchFocus_=i;previewSwatch();}
                if(y>=panel.y+348 && y<panel.y+408)closeColor(x>=panel.x+panel.w/2);
                ui::pointer(0,0,false);dirty_=true;return;
            }
            if(e.type==SDL_KEYDOWN || e.type==SDL_CONTROLLERBUTTONDOWN) {
                bool pad=e.type==SDL_CONTROLLERBUTTONDOWN;int k=pad?e.cbutton.button:e.key.keysym.sym;
                auto m=[&](int key,int b){return k==(pad?b:key);};
                if(m(SDLK_ESCAPE,SDL_CONTROLLER_BUTTON_B))closeColor(false);
                else if(m(SDLK_RETURN,SDL_CONTROLLER_BUTTON_A)) {previewSwatch();closeColor(true);}
                else {int d=m(SDLK_LEFT,SDL_CONTROLLER_BUTTON_DPAD_LEFT)?-1:m(SDLK_RIGHT,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)?1:m(SDLK_UP,SDL_CONTROLLER_BUTTON_DPAD_UP)?-4:m(SDLK_DOWN,SDL_CONTROLLER_BUTTON_DPAD_DOWN)?4:0;
                    swatchFocus_=std::clamp(swatchFocus_+d,0,15);previewSwatch();}
                return;
            }
            return;
        }
        if (e.type == SDL_TEXTINPUT && searchActive_) {
            if (searchQuery_.size() + std::char_traits<char>::length(e.text.text) <= 256)
                searchQuery_ += e.text.text;
            updateVisibleBooks();
            return;
        }

        if(pageNavigationEvent(e))return;

        // Touch Input
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            cancelPageTurn();
            pageCache_.clear();
            if (doc_ && px_document_is_epub(doc_)) { ++layoutGeneration_; layoutPending_=true; layoutDue_=SDL_GetTicks()+140; }
            else if (doc_) setPdfView(extraStyle_.pdfViewMode);
            dirty_ = true;
        }
        if (e.type == SDL_APP_WILLENTERBACKGROUND) {
            listTracking_=false; listVelocity_=0; ui::pointer(0,0,false);
            scrollTracking_=false; scrollVelocity_=0;
            saveCurrentPosition();
            flushProgress();
            gestureTracker_.reset(); activeFingers_.clear(); multiTouch_ = false; pointerActive_ = pdfDragging_ = false;zoomResetTracking_=false;panVelocityX_=panVelocityY_=0;
        }
        if (e.type == SDL_APP_DIDENTERFOREGROUND) { gestureTracker_.reset(); activeFingers_.clear(); multiTouch_ = false; pointerActive_ = pdfDragging_ = false;zoomResetTracking_=false;panVelocityX_=panVelocityY_=0; pageCache_.clear(); dirty_ = true; }
        if (e.type == SDL_MULTIGESTURE && !quickMenu_ && screen_ == ScreenState::Reader && e.mgesture.numFingers >= 2 && !continuous()) {
            zoomPdf(pdfZoom_ * (1.0f + e.mgesture.dDist * 4), e.mgesture.x, e.mgesture.y,true);
            pdfMoved_ = true;
            gestureTracker_.reset();
            return;
        }
        if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) {
            if (e.tfinger.touchId != -42) { lastFingerTick_ = SDL_GetTicks(); sawFinger_ = true; }
            if (debugTouch_) logMessage("touch type=" + std::to_string(e.type) + " finger=" + std::to_string(e.tfinger.fingerId) +
                " x=" + std::to_string(e.tfinger.x) + " y=" + std::to_string(e.tfinger.y));
        }
        if (e.type == SDL_FINGERDOWN) {
            trace("finger_down");
            int tw,th; getWindowSize(tw,th); ui::pointer(int(e.tfinger.x*tw),int(e.tfinger.y*th),true); dirty_=true;
            if (activeFingers_.empty()) { gestureTracker_.reset(); pdfDragging_ = false; multiTouch_ = false; }
            activeFingers_.insert(e.tfinger.fingerId);
            if (activeFingers_.size() > 1) { zoomResetTracking_=false;panVelocityX_=panVelocityY_=0;cancelPageTurn(); libraryHold_=false; listTracking_=false; listVelocity_=0; multiTouch_ = true; scrollTracking_=false; scrollVelocity_=0; gestureTracker_.reset(); pdfDragging_ = false; return; }
            pressX_ = e.tfinger.x; pressY_ = e.tfinger.y;
            if(screen_==ScreenState::Library&&!quickMenu_){libraryHoldIndex_=libraryBookAt(pressX_,pressY_);libraryHold_=libraryHoldIndex_>=0;libraryHoldConsumed_=false;libraryHoldStart_=SDL_GetTicks();libraryHoldX_=pressX_;libraryHoldY_=pressY_;}
            scrollVelocity_=0; listVelocity_=0;panVelocityX_=panVelocityY_=0;
            if(screen_==ScreenState::Reader && !quickMenu_ && catalogPanel_==CatalogPanel::None && showZoomReset() && (ui::contains(zoomResetRect(tw,th),int(pressX_*tw),int(pressY_*th)) ||
                (comic() && ui::contains(zoomTopRect(tw,th),int(pressX_*tw),int(pressY_*th))))) {
                zoomReturnTop_=comic() && ui::contains(zoomTopRect(tw,th),int(pressX_*tw),int(pressY_*th));
                zoomResetTracking_=true;zoomResetFinger_=e.tfinger.fingerId;gestureTracker_.reset();return;
            }
            if(beginListTouch(e.tfinger.x,e.tfinger.y)) return;
            if(!quickMenu_ && continuous() && screen_==ScreenState::Reader && (!toolbarVisible_ || (pressY_>.1f && pressY_<.9f))) {
                scrollTracking_=true; scrollMoved_=false; scrollLastX_=pressX_; scrollLastY_=pressY_; scrollTick_=SDL_GetTicks(); return;
            }
            if (screen_ == ScreenState::Reader && !quickMenu_ && !px_document_is_epub(doc_) && pdfZoom_ > 1.0f && (!toolbarVisible_ || (comic() && pressY_*th>=72 && pressY_*th<th-72))) {
                // A zoomed page still pans normally. A horizontal drag at its
                // pan boundary, or starting outside its paper, can turn it.
                if(comic() && comicCanGrip(pressX_,pressY_))beginPageTurnTouch(pressX_,pressY_,e.tfinger.fingerId);
                if (!pdfDragging_) {
                    pdfDragging_ = true; pdfMoved_ = false; pdfFinger_ = e.tfinger.fingerId;
                    pdfTouchX_ = e.tfinger.x; pdfTouchY_ = e.tfinger.y;
                    pdfGestureStart_=SDL_GetTicks();pdfStartPanX_=pdfPanX_;pdfStartPanY_=pdfPanY_;
                    pdfLastMotionX_ = e.tfinger.x; pdfLastMotionTick_ = pdfGestureStart_;
                    pdfReleaseVelocityX_ = 0.f;panLastY_=e.tfinger.y;
                }
                return;
            }
            beginPageTurnTouch(e.tfinger.x,e.tfinger.y,e.tfinger.fingerId);
            gestureTracker_.onFingerDown(e.tfinger.x, e.tfinger.y, e.tfinger.fingerId);
            return;
        }
        if (e.type == SDL_FINGERMOTION) {
            if(libraryHold_){int w,h;getWindowSize(w,h);if(std::hypot((e.tfinger.x-libraryHoldX_)*w,(e.tfinger.y-libraryHoldY_)*h)>12)libraryHold_=false;}
            trace("finger_motion");
            if (multiTouch_) { scrollTracking_=false; scrollVelocity_=0; listTracking_=false; listVelocity_=0; return; }
            if(zoomResetTracking_)return;
            if(listTracking_) { moveListTouch(e.tfinger.y); return; }
            if(scrollTracking_) {
                int w,h; getWindowSize(w,h); Uint32 now=SDL_GetTicks();
                float dy=(scrollLastY_-e.tfinger.y)*h*extraStyle_.scrollSensitivity/100.f;
                if(std::hypot(e.tfinger.x-pressX_,e.tfinger.y-pressY_)>.008f) scrollMoved_=true;
                if(scrollMoved_) { scrollBy(dy); scrollVelocity_=std::clamp(dy*1000/std::max(8u,now-scrollTick_),-2400.f,2400.f); }
                scrollLastX_=e.tfinger.x; scrollLastY_=e.tfinger.y; scrollTick_=now; return;
            }
#ifdef __SWITCH__
            if (activeFingers_.size() == 1 && !activeFingers_.count(e.tfinger.fingerId)) {
                SDL_Event corrected = incoming; corrected.tfinger.fingerId = *activeFingers_.begin();
                handleEvent(corrected, running); return;
            }
#endif
            if (pdfDragging_ && e.tfinger.fingerId == pdfFinger_) {
                int w, h; getWindowSize(w, h);
                if(comic() && pageTurnTouch_.tracking) {
                    if(movePageTurnTouch(e.tfinger.x,e.tfinger.y,e.tfinger.fingerId)) {
                        pdfDragging_=false;panVelocityX_=panVelocityY_=0;return;
                    }
                    if(std::abs(e.tfinger.y-pressY_)*h>12 && std::abs(e.tfinger.y-pressY_)*h>std::abs(e.tfinger.x-pressX_)*w)
                        pageTurnTouch_.tracking=false;
                }
                Uint32 now = SDL_GetTicks();
                Uint32 dt = now - pdfLastMotionTick_;
                if (dt >= 8) {
                    pdfReleaseVelocityX_ = ((e.tfinger.x - pdfLastMotionX_) * w) / float(dt);
                    panVelocityX_=.4f*panVelocityX_+.6f*std::clamp(-pdfReleaseVelocityX_*1000.f,-3600.f,3600.f);
                    panVelocityY_=.4f*panVelocityY_+.6f*std::clamp((panLastY_-e.tfinger.y)*h*1000.f/dt,-3600.f,3600.f);
                    panLastY_=e.tfinger.y;pdfLastMotionX_ = e.tfinger.x;
                    pdfLastMotionTick_ = now;
                }
                pdfPanX_=pdfStartPanX_-int(std::lround((e.tfinger.x-pressX_)*w));
                pdfPanY_=pdfStartPanY_-int(std::lround((e.tfinger.y-pressY_)*h));
                pdfTouchX_ = e.tfinger.x; pdfTouchY_ = e.tfinger.y;
                if (comic()?std::hypot((e.tfinger.x-pressX_)*w,(e.tfinger.y-pressY_)*h)>6.f:
                    std::hypot(e.tfinger.x-pressX_,e.tfinger.y-pressY_)>.015f)pdfMoved_=true;
                const int requestedX=pdfPanX_,requestedY=pdfPanY_;clampPdfPan();
                if(comic()){pdfStartPanX_+=pdfPanX_-requestedX;pdfStartPanY_+=pdfPanY_-requestedY;}
                dirty_ = true;
                return;
            }
            if(movePageTurnTouch(e.tfinger.x,e.tfinger.y,e.tfinger.fingerId)) return;
            gestureTracker_.onFingerMotion(e.tfinger.x, e.tfinger.y, e.tfinger.fingerId);
            return;
        }
        if (e.type == SDL_FINGERUP) {
            libraryHold_=false;
            if(libraryHoldConsumed_){libraryHoldConsumed_=false;listTracking_=false;ui::pointer(0,0,false);activeFingers_.erase(e.tfinger.fingerId);gestureTracker_.reset();return;}
            trace("finger_up");
            ui::pointer(0,0,false); dirty_=true;
            activeFingers_.erase(e.tfinger.fingerId);
            if(zoomResetTracking_ && e.tfinger.fingerId==zoomResetFinger_) {
                zoomResetTracking_=false;int w,h;getWindowSize(w,h);
                if(ui::contains(zoomReturnTop_?zoomTopRect(w,h):zoomResetRect(w,h),int(e.tfinger.x*w),int(e.tfinger.y*h))) {
                    if(zoomReturnTop_)returnComicToTop();else resetReaderZoom();
                }
                return;
            }
            if(listTracking_) {
                moveListTouch(e.tfinger.y); listTracking_=false;
                if(!listMoved_) { listVelocity_=0; handleTap(e.tfinger.x,e.tfinger.y); }
                else if(SDL_GetTicks()-listTick_>100) listVelocity_=0;
                if(listMoved_ && screen_==ScreenState::TocModal && tocCount()>0) {
                    int w,h;getWindowSize(w,h);tocSelected_=std::clamp(int(tocScroll_)/tocStep()*tocColumns(w),0,tocCount()-1);
                    rebuildFocus();navigationFocus_.select("toc."+std::to_string(tocSelected_));
                }
                gestureTracker_.reset(); return;
            }
            if(scrollTracking_) {
                if(std::hypot(e.tfinger.x-pressX_,e.tfinger.y-pressY_)>.008f) scrollMoved_=true;
                if(scrollMoved_ && e.tfinger.y!=scrollLastY_) {
                    int w,h; getWindowSize(w,h);
                    scrollBy((scrollLastY_-e.tfinger.y)*h*extraStyle_.scrollSensitivity/100.f);
                }
                scrollTracking_=false;
                if(!extraStyle_.scrollInertia || SDL_GetTicks()-scrollTick_>100) scrollVelocity_=0;
                if(!scrollMoved_) { scrollVelocity_=0; handleTap(e.tfinger.x,e.tfinger.y); }
                else saveCurrentPosition();
                physicsTick_=SDL_GetTicks(); return;
            }
            if (multiTouch_) { if (activeFingers_.empty()) multiTouch_ = false; return; }
            if (pdfDragging_) {
                pageTurnTouch_.tracking=false;
                if (e.tfinger.fingerId == pdfFinger_) {
                    pdfDragging_ = false;
                    int w,h;getWindowSize(w,h);
                    Uint32 now = SDL_GetTicks();
                    if (now - pdfLastMotionTick_ > 80) pdfReleaseVelocityX_ = 0.05f;
                    PdfGestureSample sample{(e.tfinger.x-pressX_)*w,(e.tfinger.y-pressY_)*h,float(w),std::max(1u,now-pdfGestureStart_),false,false,pdfReleaseVelocityX_};
                    auto classification=comic()?PdfGesture::Pan:classifyPdfGesture(sample);
                    if(classification==PdfGesture::PageNext || classification==PdfGesture::PagePrevious) {
                        pdfPanX_=pdfStartPanX_;pdfPanY_=pdfStartPanY_;turnPage(classification==PdfGesture::PageNext?1:-1);
                    } else if (!pdfMoved_) {panVelocityX_=panVelocityY_=0;handleTap(e.tfinger.x, e.tfinger.y);}
                    if(comic() && pdfMoved_) {
                        if(!extraStyle_.scrollInertia || now-pdfLastMotionTick_>80)panVelocityX_=panVelocityY_=0;
                        panInertiaX_=float(pdfPanX_);panInertiaY_=float(pdfPanY_);panPhysicsTick_=now;
                    }
                }
                return;
            }
            if(releasePageTurnTouch(e.tfinger.x,e.tfinger.y,e.tfinger.fingerId)) return;
            auto res = gestureTracker_.onFingerUp(e.tfinger.x, e.tfinger.y, e.tfinger.fingerId);
            handleGesture(res);
            return;
        }


        // Controller Hotplugging
        if (e.type == SDL_CONTROLLERDEVICEADDED) {
            prepareControllerMapping(e.cdevice.which);
            if (!controller_) {
                controller_ = SDL_GameControllerOpen(e.cdevice.which);
                if (controller_) logMessage("Controller attached dynamically");
            }
            return;
        }
        if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
            clearHeldControls();
            if (controller_ && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller_)) == e.cdevice.which) {
                SDL_GameControllerClose(controller_);
                controller_ = nullptr;
                logMessage("Controller detached dynamically");
                for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                    prepareControllerMapping(i);
                    if (SDL_IsGameController(i)) {
                        controller_ = SDL_GameControllerOpen(i);
                        if (controller_) {
                            logMessage("Switched to secondary controller");
                            break;
                        }
                    }
                }
            }
            return;
        }

        // Controller Axis Navigation
        if (e.type == SDL_CONTROLLERAXISMOTION) {
            return;
        }

        // Controller & Keyboard Keydown
        if (e.type == SDL_KEYDOWN || e.type == SDL_CONTROLLERBUTTONDOWN) {
            handleButtons(e, running);
        }
    }

    void handleGesture(const GestureResult &res) {
        if(quickMenu_) {
            if(res.type==GestureType::TapLeft || res.type==GestureType::TapCenter || res.type==GestureType::TapRight) handleTap(res.tapX,res.tapY);
            return;
        }
        if (debugTouch_) {
            logMessage("touch action=" + std::to_string(static_cast<int>(res.type)));
            lastTouchDebug_ = "Touch action=" + std::to_string(static_cast<int>(res.type)) +
                " x=" + std::to_string(res.tapX) + " y=" + std::to_string(res.tapY);
        }
        if (screen_ == ScreenState::Reader) {
            if (toolbarVisible_ && (pressY_ < 0.1f || pressY_ > 0.9f) &&
                (res.type == GestureType::SwipeLeft || res.type == GestureType::SwipeRight)) return;
            if (continuous() && (res.type == GestureType::SwipeLeft || res.type == GestureType::SwipeRight)) return;
            if (res.type == GestureType::SwipeLeft) {
                toolbarVisible_ = false;
                turnPage(readingDirection());
            } else if (res.type == GestureType::SwipeRight) {
                toolbarVisible_ = false;
                turnPage(-readingDirection());
            } else if (res.type == GestureType::TapLeft || res.type == GestureType::TapRight || res.type == GestureType::TapCenter) {
                handleTap(res.tapX, res.tapY);
            }
        } else if (res.type == GestureType::TapLeft || res.type == GestureType::TapRight || res.type == GestureType::TapCenter) {
            handleTap(res.tapX, res.tapY);
        } else if (res.type == GestureType::SwipeUp || res.type == GestureType::SwipeDown) {
            handleDirectional(0, res.type == GestureType::SwipeUp ? 1 : -1);
        }
    }

    void enterJournal(bool fromReader,const std::string &bookId={}) {
        journalReturn_=fromReader?ScreenState::Reader:ScreenState::Library;
        journalUI_.open(bookId);quickMenu_=false;helpMenu_=false;
        journalScroll_=0.f;
        screen_=ScreenState::ReadingJournal;dirty_=true;
    }
    void journalAction(ReadingJournalUI::Action action) {
        if(action==ReadingJournalUI::Action::OpenFull){enterJournal(true);return;}
        if(action==ReadingJournalUI::Action::ClosePreview){screen_=ScreenState::Reader;journalUI_.hidePreview();dirty_=true;return;}
        if(action==ReadingJournalUI::Action::Back){screen_=journalReturn_;journalUI_.back();dirty_=true;return;}
        dirty_=true;
    }
    void handleTap(float x, float y) {
        trace("tap_decision");
        int w = 1280, h = 720;
        getWindowSize(w, h);
        rebuildFocus();navigationFocus_.touch(int(x*w),int(y*h));
        int px = static_cast<int>(x * w);
        int py = static_cast<int>(y * h);
        if(catalogPanel_!=CatalogPanel::None){handleCatalogTap(px,py,w,h);return;}
        if((screen_==ScreenState::ReadingJournal||screen_==ScreenState::JournalPreview)&&!quickMenu_&&
           (screen_==ScreenState::JournalPreview || !(px>=w-80&&py<72))){
            journalAction(journalUI_.tap(px,py,w,h,journal_,&progressStore_,&catalog_));return;
        }

        if(quickMenu_) { handleQuickMenuTap(px,py,w,h); return; }
        listVelocity_=0;
        if(ui::contains({w-80,8,64,64},px,py)) { toggleQuickMenu(); return; }
        // Dismiss any active error banner on tap
        if (!currentErrorMessage_.empty()) {
            currentErrorMessage_.clear();
            dirty_ = true;
        }

        if (screen_ == ScreenState::Reader) {
            if(toolbarVisible_ || appSettings_.get().toolbar) {
                if(ui::contains(readerPrev(w,h),px,py)){turnPage(-readingDirection());return;}
                if(ui::contains(readerNext(w,h),px,py)){turnPage(readingDirection());return;}
                if(ui::contains(readerPages(w,h),px,py)){openContents(1);return;}
                if(ui::contains(readerSeekHit(w,h),px,py)){startReaderScrub(false);moveReaderScrub(x);finishReaderScrub();return;}
            }
            if(showZoomReset() && comic() && ui::contains(zoomTopRect(w,h),px,py)){returnComicToTop();return;}
            if(showZoomReset() && ui::contains(zoomResetRect(w,h),px,py)){resetReaderZoom();return;}
            if (toolbarVisible_ || appSettings_.get().toolbar) {
                if(py<72){
                    if(ui::contains(readerHeaderButton(0,w),px,py))closeBook();
                    else if(ui::contains(readerHeaderButton(1,w),px,py))openContents();
                    else if(ui::contains(readerHeaderButton(2,w),px,py)){screen_=ScreenState::BookmarksModal;dirty_=true;}
                    else if(ui::contains(readerHeaderButton(3,w),px,py))openSettings();
                    return;
                }
                // Tap in middle dismisses toolbar without turning page
                toolbarVisible_ = false;
                dirty_ = true;
                return;
            }
            // Standard reading tap zones (when toolbar is not visible)
            if(continuous()) { toolbarVisible_=!toolbarVisible_; dirty_=true; return; }
            if (x < extraStyle_.touchZone / 100.0f) turnPage(-readingDirection());
            else if (x > 1.0f - extraStyle_.touchZone / 100.0f) turnPage(readingDirection());
            else { toolbarVisible_ = true; dirty_ = true; }
        } else if (screen_ == ScreenState::AaModal) {
            handleAaModalTap(px, py, w, h);
        } else if (screen_ == ScreenState::TocModal) {
            handleTocModalTap(px, py, w, h);
        } else if (screen_ == ScreenState::BookmarksModal) {
            handleBookmarksModalTap(px, py, w, h);
        } else if (screen_ == ScreenState::Library) {
            if(!openSeriesKey_.empty() && ui::contains({16,16,152,56},px,py)){closeSeriesShelf();return;}
            if(openSeriesKey_.empty() && w>=900 && ui::contains({208,16,180,56},px,py)){setCatalogPanel(CatalogPanel::Browse);return;}
            if(ui::contains({w-440,16,200,56},px,py)){enterJournal(false);return;}
            ui::LibraryLayout l(w);
            for(int c=0;c<6;++c) if(ui::contains(categoryRect(c,w),px,py)) { category_=static_cast<Category>(c); updateVisibleBooks(); return; }
            if(ui::contains({w-216,16,112,56},px,py)) { toggleSearch(); return; }
            if(visibleBooks_.empty()) { if(books_.empty() && ui::contains({56,l.top+16+std::min(260,h-l.top-48)-72,260,56},px,py)) openBrowser(); return; }
            if(py<l.top || py>=h-28) return;
            int y=py-l.top+int(listScroll_),idx=-1; bool favorite=false;
            if(viewMode_==ViewMode::Grid) {
                int col=(px-l.left)/l.stepX;
                if(px<l.left || col>=l.columns || (px-l.left)%l.stepX>=l.cardWidth || y%l.stepY>=l.cardHeight) return;
                idx=(y/l.stepY)*l.columns+col;
                favorite=(px-l.left)%l.stepX>=l.cardWidth-72 && y%l.stepY>=10 && y%l.stepY<66;
            } else { idx=y/112; favorite=px>=w-124; }
            if(idx<0 || idx>=int(visibleBooks_.size())) return;
            selectedBookIdx_=idx;
            if(openSeriesShelf(visibleBooks_[idx]))return;
            if(favorite) {
                auto p=progressStore_.get(visibleBooks_[idx].id); p.favorite=!p.favorite;
                progressStore_.put(visibleBooks_[idx].id,p); progressDirty_=true;
                auto oldScroll=listScroll_; updateVisibleBooks(); listScroll_=std::clamp(oldScroll,0.f,maxUiScroll());
            } else openLibraryItem(idx);
        } else if (screen_ == ScreenState::FileBrowser) {
            if(ui::contains(browserFolderButton(w),px,py)){addBrowserFolder();return;}
            if(py<80) {
                if(px<180) { screen_=ScreenState::Library; dirty_=true; }
                else if(px<w-100 && browserPath_!=browserRoot_) { browserPath_=browserPath_.parent_path(); scanBrowserDirectory(); }
                return;
            }
            if(py>=112) {
                int idx=int(py-112+browserScroll_)/64;
                if(idx>=0 && idx<int(browserEntries_.size())) { browserSelected_=idx; handleBrowserSelect(); }
            }
        }
    }

    SDL_Rect navigationPanel(int w,int h) const { return {32,80,w-64,h-112}; }
    void handleTocModalTap(int px,int py,int w,int h) {
        contentsTap(px,py,w,h);
    }
    void handleBookmarksModalTap(int px,int py,int w,int h) {
        auto r=navigationPanel(w,h);
        if(!ui::contains(r,px,py) || ui::contains({w-184,88,112,56},px,py)) { screen_=ScreenState::Reader; dirty_=true; return; }
        if(ui::contains({48,h-112,w-96,56},px,py)) { toggleBookmark(); return; }
        if(py<152 || py>=h-128) return;
        auto p=progressStore_.get(currentBookId_); int idx=int(py-152+bookmarkScroll_)/56;
        if(idx<0 || idx>=int(p.bookmarks.size())) return;
        if(px>=w-184) {
            p.bookmarks.erase(p.bookmarks.begin()+idx); progressStore_.put(currentBookId_,p); progressDirty_=true;
            bookmarkScroll_=std::clamp(bookmarkScroll_,0.f,maxUiScroll()); dirty_=true; return;
        }
        bmSelected_=idx; openBookmark(p.bookmarks[idx]);
    }

    void handleAaModalTap(int px, int py, int w, int h) {
        ui::SettingsLayout l(w,h);
        if(ui::contains(l.close,px,py) || !ui::contains(l.sheet,px,py)) { dismissSettings(); return; }
        for(int i=0;i<6;++i) if(ui::contains(l.tab(i),px,py)) { settingsScroll_=0; aaTab_=i; dirty_=true; return; }
        if(ui::contains(l.scope,px,py) && doc_ && aaTab_!=4 && aaTab_!=5) {
            extraStyle_.styleOverride=1-extraStyle_.styleOverride;
            extraStyle_.modeOverride=extraStyle_.styleOverride;
            if(!extraStyle_.styleOverride) {
                auto resolved=resolveReadingPreferences(progressStore_.get("PAPERNX_DEFAULTS"),currentStyle());
                applyStyle(resolved);
            }
            queueStyleChange(true); return;
        }
        if(ui::contains(l.footer,px,py)) {
            if(px<l.footer.x+l.footer.w/2) { flushProgress(); persistAppSettings(); dismissSettings(); }
            else {
                if(aaTab_==5) {
                    appSettings_.put(AppPreferences{}); ui::language=appSettings_.get().language; viewMode_=ViewMode::Grid; sort_=Sort::Title;
                    persistAppSettings();applyReaderLayoutOptions();updateVisibleBooks();return;
                }
                if(aaTab_==4) {
                    extraStyle_.pdfViewMode=0; // appearance remains on the shared Appearance tab
                    if(doc_ && !px_document_is_epub(doc_)) { setPdfView(0); pageCache_.clear(); saveCurrentPosition(); }
                    else storeDefaults();
                    dirty_=true; return;
                }
                ReaderPreferences defaults; defaults.styleOverride=extraStyle_.styleOverride;
                defaults.modeOverride=extraStyle_.modeOverride;
                if(comic()) {defaults.pdfColorMode=extraStyle_.pdfColorMode&COMIC_RIGHT_TO_LEFT;defaults.pdfViewMode=extraStyle_.pdfViewMode;}
                applyStyle(defaults); queueStyleChange();
            }
            return;
        }
        int row=-1; for(int i=0;i<settingsRows();++i) if(ui::contains(l.control(i),px,py+int(settingsScroll_)) && ui::contains(l.content,px,py)) row=i;
        if(row<0) return;
        aaRow_=row;
        if(aaTab_!=5 || row!=16) coverClearConfirm_=false;
        bool right=px>=l.content.x+l.content.w/2;
        if(numericSetting()) {
            auto rect=l.control(row);rect.y-=int(settingsScroll_);int hit=ui::StepperLayout(rect).hit(px,py);
            if(hit==0) {if(!settingsEditing_){editOriginal_=currentStyle();editOriginalZoom_=pdfZoom_;settingsEditing_=true;dirty_=true;}return;}
            right=hit>0;
        }
        if(comic() && aaTab_==0) {
            if(row==0)extraStyle_.pdfColorMode^=COMIC_READING_COLORS;
        } else if(comic() && aaTab_==1) {
            if(row==0)currentMargin_=std::clamp(currentMargin_+(right?10:-10),10,120);
        } else if(aaTab_==0) {
            if(row==1){openChoice(0,currentFontFamily_);return;}
            if(row==0) currentFontSize_=std::clamp(currentFontSize_+(right?2:-2),14,44);
            if(row==1) currentFontFamily_=(currentFontFamily_+1)%3;
            if(row==2) currentLineHeight_=std::clamp(currentLineHeight_+(right?10:-10),100,220);
            if(row==3) extraStyle_.paragraphSpacing=std::clamp(extraStyle_.paragraphSpacing+(right?20:-20),0,200);
            if(row==4) extraStyle_.publisherStyles=!extraStyle_.publisherStyles;
            if(row==5) extraStyle_.indentation=extraStyle_.indentation?0:120;
        } else if(aaTab_==1) {
            if(row==0) currentMargin_=std::clamp(currentMargin_+(right?10:-10),10,120);
            if(row==1) currentTextAlign_=1-currentTextAlign_;
            if(row==2) extraStyle_.indentation=extraStyle_.indentation?0:120;
            if(row==3) extraStyle_.paragraphSpacing=std::clamp(extraStyle_.paragraphSpacing+(right?20:-20),0,200);
            if(row==4) {toggleLandscapeSpreads();return;}
        } else if(aaTab_==2) {
            if(row<5) { const int order[]={0,1,4,6,7,8,2,3,5,9};int index=order[std::min(9,row*2+int(right))]; auto preset=g_paperPresets[index];
                currentTheme_=preset.theme;extraStyle_.textColor=preset.text;extraStyle_.backgroundColor=preset.background; }
            if(row==5) { editColor(0); return; }
            if(row==6) { editColor(1); return; }
            if(row==7) {extraStyle_.textColor=extraStyle_.backgroundColor=-1;}
            if(row==8) {
                auto p=appSettings_.get();
                p.softwareBrightness=std::clamp(p.softwareBrightness+(right?5:-5),50,100);
                appSettings_.put(p);
                persistAppSettings();
                dirty_=true;return;
            }
            if(row==9) {
                auto p=appSettings_.get();
                p.systemBrightness=std::clamp(p.systemBrightness+(right?1:-1),10,100);
                appSettings_.put(p);
                persistAppSettings();
#ifdef __SWITCH__
                backlightAvailable_=setSystemBrightness(p.systemBrightness);
                if(!backlightAvailable_)currentErrorMessage_=catLabel("El cihazı parlaklık servisi kullanılamıyor; yazılımsal dimmer kullanılabilir.",
                    "Handheld brightness control is unavailable; software dimming remains available.");
#endif
                dirty_=true;return;
            }
            if(row==10) {
                auto p=appSettings_.get();p.softwareBrightness=100;p.systemBrightness=100;
                appSettings_.put(p);persistAppSettings();
#ifdef __SWITCH__
                backlightAvailable_=setSystemBrightness(p.systemBrightness);
#endif
                dirty_=true;return;
            }
        } else if(aaTab_==3) {
            if(row==0) extraStyle_.readingMode=0;
            if(row==1) extraStyle_.readingMode=1;
            if(row==2) extraStyle_.scrollSensitivity=std::clamp(extraStyle_.scrollSensitivity+(right?25:-25),50,200);
            if(row==3) extraStyle_.scrollInertia=!extraStyle_.scrollInertia;
            if(row==5){auto a=appSettings_.get();a.shoulderPageTurns=!a.shoulderPageTurns;appSettings_.put(a);persistAppSettings();return;}
            if(row==4) extraStyle_.touchZone=std::clamp(extraStyle_.touchZone+(right?5:-5),20,40);
            extraStyle_.modeOverride=doc_ && extraStyle_.styleOverride;
        } else if(aaTab_==4) {
            if(comic() && row==7) {toggleLandscapeSpreads();return;}
            if(row<3) {
                extraStyle_.pdfViewMode=row;
                if(doc_ && !px_document_is_epub(doc_)) setPdfView(row);
            }
            if(row==3 && doc_ && !px_document_is_epub(doc_)) zoomPdf(pdfZoom_+(right?.25f:-.25f));
            if(row==4) { auto a=appSettings_.get();a.preservePdfZoom=!a.preservePdfZoom;appSettings_.put(a);persistAppSettings(); }
            if(comic() && row==5) {cancelPageTurn();extraStyle_.pdfColorMode^=COMIC_READING_COLORS;pageCache_.clear();}
            if(comic() && row==6) {cancelPageTurn();extraStyle_.pdfColorMode^=COMIC_RIGHT_TO_LEFT;}
            if(globalSettings_ || !doc_) storeDefaults(); else saveCurrentPosition();
            dirty_=true; return;
        } else if(aaTab_==5) {
            auto p=appSettings_.get();
            if(row==9){openChoice(10,0);return;}
            if(row==11){if(libraryScanner_.pending()){libraryScanAgain_=false;libraryScanner_.cancel();dirty_=true;}else startLibraryScan();return;}
            if(row==12){openChoice(8,p.interfaceStyle);return;}
            if(row==13){openChoice(9,p.interfaceAccent);return;}
            if(row==14){p.automaticUpdateCheck=!p.automaticUpdateCheck;appSettings_.put(p);persistAppSettings();return;}
            if(row==15){checkForUpdate();return;}
            if(row==16){
                if(!coverClearConfirm_){coverClearConfirm_=true;dirty_=true;return;}
                coverClearConfirm_=false;requestCoverCacheClear();dirty_=true;return;
            }
            if(row==0){openChoice(7,globalPaperPresetIndex());return;}
            if(row==6){openChoice(1,p.displayOrientation);return;}
            if(row==7){openChoice(6,p.paperTexture);return;}
            if(row==1) { viewMode_=viewMode_==ViewMode::Grid?ViewMode::List:ViewMode::Grid; listScroll_=0; }
            if(row==2) { sort_=static_cast<Sort>((static_cast<int>(sort_)+1)%5); updateVisibleBooks(); }
            if(row==3) p.toolbar=1-p.toolbar;
            if(row==4) { p.defaultBrowserPath=browserPath_.u8string(); }
            if(row==5) { p.language=1-p.language; ui::language=p.language; }
            if(row==6) { p.displayOrientation=(p.displayOrientation+1)%3;appSettings_.put(p);pageCache_.clear();
                if(doc_ && px_document_is_epub(doc_)) {++layoutGeneration_;layoutPending_=true;layoutDue_=SDL_GetTicks()+100;}
                else if(doc_)setPdfView(extraStyle_.pdfViewMode);
            }
            if(row==7) p.paperTexture=(p.paperTexture+1)%3;
            if(row==8) p.expandedViewport=!p.expandedViewport;
            if(row==10) p.scanOnLaunch=!p.scanOnLaunch;
            appSettings_.put(p); persistAppSettings();
            if(row==8) {
                applyReaderLayoutOptions();
            }
            return;
        }
        if(!settingsEditing_)queueStyleChange();else dirty_=true;
    }

    void handleAnalog(int dx,int dy) {
        if(scrub_.active){if(scrub_.controller)seekDirection(dx,dy);return;}
        if(screen_==ScreenState::Reader && !quickMenu_ && !readerToolbarFocusActive_ && doc_ && !px_document_is_epub(doc_) && !continuous()) {
            panReaderBy(dx*64.f,dy*64.f);
        } else handleDirectional(dx,dy);
    }
    int settingsRows() const { if(comic()) {if(aaTab_==0 || aaTab_==1)return 1;if(aaTab_==4)return 8;}const int rows[]={6,5,11,6,5,17}; return rows[aaTab_]; }
    bool numericSetting() const { if(comic() && aaTab_==0)return false;return (aaTab_==0 && (aaRow_==0 || aaRow_==2 || aaRow_==3)) ||
        (aaTab_==1 && (aaRow_==0 || aaRow_==3)) || (aaTab_==2 && (aaRow_==8 || aaRow_==9)) || (aaTab_==3 && (aaRow_==2 || aaRow_==4)) || (aaTab_==4 && aaRow_==3); }
    void settingsActivate(int direction=1) {
        int w,h; getWindowSize(w,h); ui::SettingsLayout l(w,h);
        if(settingsColumn_==-1) { settingsColumn_=0; dirty_=true; return; }
        if(aaRow_==settingsRows()) { dismissSettings(); return; }
        auto r=l.control(aaRow_); r.y-=int(settingsScroll_);
        if(numericSetting())r=direction>0?ui::StepperLayout(r).plus:ui::StepperLayout(r).minus;
        handleAaModalTap(r.x+r.w/2,r.y+r.h/2,w,h);
    }
    void handleDirectional(int dx, int dy) {
        if(screen_==ScreenState::ReadingJournal&&!quickMenu_){journalUI_.navigate(dx,dy,journal_,&progressStore_,&catalog_);journalScroll_=float(journalUI_.scrollY());dirty_=true;return;}
        if(screen_==ScreenState::JournalPreview)return;
        if(moveFocus(dx,dy))return;
        if(quickMenu_) { quickFocus_=std::clamp(quickFocus_+dy,0,std::max(0,int(quickActions().size())-1));
            int w,h;getWindowSize(w,h);auto r=quickSheet(w,h);quickScroll_=std::clamp(float(quickFocus_*60-(r.h-QUICK_HEADER_HEIGHT-56)),0.f,maxUiScroll());dirty_=true;return; }
        if (screen_ == ScreenState::Reader) {
            if(dx) { if(continuous()) chapterJump(dx*readingDirection()); else turnPage(dx*readingDirection()); }
            else if(continuous()) { scrollBy(dy*80.f); saveCurrentPosition(); }
            else if(doc_ && !px_document_is_epub(doc_) && dy) { pdfPanY_+=dy*40;clampPdfPan();dirty_=true; }
        } else if (screen_ == ScreenState::TocModal) {
            if (dy != 0 && !tocEntries_.empty()) {
                int total = static_cast<int>(tocEntries_.size());
                tocSelected_ = std::clamp(tocSelected_ + dy, 0, total - 1);
                if (tocSelected_ < tocOffset_) tocOffset_ = tocSelected_;
                if (tocSelected_ >= tocOffset_ + 9) tocOffset_ = tocSelected_ - 8;
                tocScroll_=std::clamp(float(tocOffset_*56),0.f,maxUiScroll()); dirty_ = true;
            }
        } else if (screen_ == ScreenState::BookmarksModal) {
            auto p = progressStore_.get(currentBookId_);
            if (dy != 0 && !p.bookmarks.empty()) {
                int total = static_cast<int>(p.bookmarks.size());
                bmSelected_ = std::clamp(bmSelected_ + dy, 0, total - 1);
                if (bmSelected_ < bmOffset_) bmOffset_ = bmSelected_;
                if (bmSelected_ >= bmOffset_ + 8) bmOffset_ = bmSelected_ - 7;
                bookmarkScroll_=std::clamp(float(bmOffset_*56),0.f,maxUiScroll()); dirty_ = true;
            }
        } else if (screen_ == ScreenState::AaModal) {
            coverClearConfirm_ = false;
            if(settingsEditing_) { if(dx) settingsActivate(dx); return; }
            if(dx) { aaTab_=std::clamp(aaTab_+dx,0,5);aaRow_=0;settingsScroll_=0; }
            if(dy) aaRow_=std::clamp(aaRow_+dy,0,settingsRows());
            int w,h;getWindowSize(w,h);ui::SettingsLayout l(w,h);
            if(aaRow_<settingsRows()) {
                float top=aaRow_*64.f,bottom=top+56;
                if(top<settingsScroll_)settingsScroll_=top;
                if(bottom>settingsScroll_+l.content.h)settingsScroll_=bottom-l.content.h;
            }
            dirty_=true;
        } else if (screen_ == ScreenState::Library) {
            int w,h; getWindowSize(w,h); ui::LibraryLayout l(w);
            int columns=viewMode_==ViewMode::Grid?l.columns:1;
            if(visibleBooks_.empty()) return;
            selectedBookIdx_=std::clamp(selectedBookIdx_+dx+dy*columns,0,int(visibleBooks_.size())-1);
            int item=viewMode_==ViewMode::Grid?selectedBookIdx_/columns:selectedBookIdx_;
            int step=viewMode_==ViewMode::Grid?l.stepY:112;
            float top=float(item*step),bottom=top+(viewMode_==ViewMode::Grid?l.cardHeight:104);
            if(top<listScroll_) listScroll_=top;
            if(bottom>listScroll_+h-l.top-28) listScroll_=std::max(0.f,bottom-(h-l.top-28));
            listScroll_=std::clamp(listScroll_,0.f,maxUiScroll()); dirty_=true;
        } else if (screen_ == ScreenState::FileBrowser) {
            if(browserEntries_.empty()) return;
            browserSelected_=std::clamp(browserSelected_+dy,0,int(browserEntries_.size())-1);
            int w,h; getWindowSize(w,h);
            if(browserSelected_*64<browserScroll_) browserScroll_=float(browserSelected_*64);
            if(browserSelected_*64+56>browserScroll_+h-128) browserScroll_=float(browserSelected_*64+56-h+128);
            browserScroll_=std::clamp(browserScroll_,0.f,maxUiScroll()); dirty_=true;
        }
    }

    void handleButtons(const SDL_Event &e, bool &running) {
        bool pad = (e.type == SDL_CONTROLLERBUTTONDOWN);
        int key = pad ? e.cbutton.button : e.key.keysym.sym;
        auto match = [&](int k, int p) { return pad ? (key == p) : (key == k); };
        if (!pad && searchActive_ && key != SDLK_ESCAPE) {
            if (key == SDLK_BACKSPACE && !searchQuery_.empty()) {
                std::size_t pos = searchQuery_.size() - 1;
                while (pos > 0 && (static_cast<unsigned char>(searchQuery_[pos]) & 0xc0) == 0x80) --pos;
                searchQuery_.resize(pos);
                updateVisibleBooks();
            }
            return;
        }

        if (match(SDLK_ESCAPE, SDL_CONTROLLER_BUTTON_B)) {
            if(screen_==ScreenState::Library && !quickMenu_ && closeSeriesShelf())return;
            if(quickMenu_){quickMenu_=false;helpMenu_=false;dirty_=true;return;}
            if(screen_==ScreenState::JournalPreview){screen_=ScreenState::Reader;journalUI_.hidePreview();dirty_=true;return;}
            if(screen_==ScreenState::ReadingJournal){
                if(journalUI_.hasSubpage())journalUI_.back();
                else screen_=journalReturn_;
                dirty_=true;return;
            }
            if(catalogPanel_!=CatalogPanel::None){closeCatalogPanel();return;}
            if(screen_==ScreenState::AaModal) {
                if(settingsEditing_) { applyStyle(editOriginal_);pdfZoom_=editOriginalZoom_;settingsEditing_=false;queueStyleChange();dirty_=true;return; }
                dismissSettings(); return; }
            if(globalSettings_) { screen_=ScreenState::Library; globalSettings_=false; dirty_=true; return; }
            if (screen_ == ScreenState::AaModal || screen_ == ScreenState::TocModal || screen_ == ScreenState::BookmarksModal) {
                screen_ = ScreenState::Reader;
            } else if (screen_ == ScreenState::FileBrowser) {
                if (browserPath_ == browserRoot_) screen_ = ScreenState::Library;
                else { browserPath_ = browserPath_.parent_path(); scanBrowserDirectory(); }
            } else if (searchActive_) {
                searchActive_ = false;
                SDL_StopTextInput();
                searchQuery_.clear();
                updateVisibleBooks();
            } else if (screen_ == ScreenState::Reader) {
                if (readerToolbarFocusActive_) { readerToolbarFocusActive_=false;dirty_=true;return; }
                if (toolbarVisible_) { toolbarVisible_ = false; dirty_ = true; return; }
                closeBook();
            } else {
                running = false;
            }
            dirty_ = true;
            return;
        }

        if(match(SDLK_PLUS,SDL_CONTROLLER_BUTTON_START)) {toggleQuickMenu();return;}

        if (match(SDLK_RETURN, SDL_CONTROLLER_BUTTON_A)) {
            if(quickMenu_){
                rebuildFocus();const auto focused=navigationFocus_.active();
                if(focused=="quick.pages" || focused=="quick.orientation") {activateFocus();return;}
                int w,h;getWindowSize(w,h);auto rc=quickSheet(w,h);handleQuickMenuTap(rc.x+28,rc.y+QUICK_HEADER_HEIGHT+quickFocus_*60-int(quickScroll_)+24,w,h);return;
            }
            if(screen_==ScreenState::JournalPreview){enterJournal(true);return;}
            if(screen_==ScreenState::ReadingJournal){journalAction(journalUI_.activate(0,0,journal_));return;}
            if(activateFocus())return;
            if (screen_ == ScreenState::TocModal) {
                if (!tocEntries_.empty() && tocSelected_ >= 0 && tocSelected_ < static_cast<int>(tocEntries_.size()) && tocEntries_[tocSelected_].page >= 0) {
                    rememberPosition();
                    currentPage_ = std::clamp(tocEntries_[tocSelected_].page, 0, std::max(0, pageCount_ - 1));
                    if(comic() && !continuous())resetComicPagePan();
                scrollY_=scrollVelocity_=0;
                    screen_ = ScreenState::Reader;
                    toolbarVisible_ = false;
                    saveCurrentPosition();
                    dirty_ = true;
                }
                return;
            } else if (screen_ == ScreenState::BookmarksModal) {
                auto p = progressStore_.get(currentBookId_);
                if (!p.bookmarks.empty() && bmSelected_ >= 0 && bmSelected_ < static_cast<int>(p.bookmarks.size())) {
                    openBookmark(p.bookmarks[bmSelected_]);
                }
                return;
            } else if (screen_ == ScreenState::AaModal) {
                if(settingsEditing_) { settingsEditing_=false;queueStyleChange(); }
                else if(numericSetting()) { editOriginal_=currentStyle();editOriginalZoom_=pdfZoom_;settingsEditing_=true; }
                else settingsActivate();
                dirty_=true;return;
            } else if (screen_ == ScreenState::FileBrowser) {
                handleBrowserSelect();
            } else if (screen_ == ScreenState::Library) {
                if (selectedBookIdx_ >= 0 && selectedBookIdx_ < static_cast<int>(visibleBooks_.size())) {
                    openLibraryItem(selectedBookIdx_);
                }
            } else if (screen_ == ScreenState::Reader) {
                toolbarVisible_ = !toolbarVisible_;
                dirty_ = true;
            }
            return;
        }

        if (screen_ == ScreenState::Reader) {
            if (match(SDLK_PAGEDOWN, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) { if(appSettings_.get().shoulderPageTurns)turnPage(shoulderPageDirection(true)); return; }
            if (match(SDLK_PAGEUP, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) { if(appSettings_.get().shoulderPageTurns)turnPage(shoulderPageDirection(false)); return; }
            if (!px_document_is_epub(doc_)) {
                if (!pad && (key == SDLK_EQUALS || key == SDLK_PLUS)) { zoomPdf(pdfZoom_ + 0.25f); return; }
                if (!pad && key == SDLK_MINUS) { zoomPdf(pdfZoom_ - 0.25f); return; }
                if (match(SDLK_0, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) { resetReaderZoom(); return; }
            }
            if (match(SDLK_b, SDL_CONTROLLER_BUTTON_BACK)) { screen_ = ScreenState::BookmarksModal; dirty_ = true; return; }
        }
        if (screen_ == ScreenState::AaModal && match(SDLK_y, SDL_CONTROLLER_BUTTON_Y)) {
            if (doc_ && !px_document_is_epub(doc_)) { zoomPdf(1.0f); return; }
            aaRow_ = (aaRow_ + 1) % std::max(1,settingsRows());
            dirty_ = true; return;
        }
        if (screen_ == ScreenState::AaModal && doc_ && !px_document_is_epub(doc_) && match(SDLK_f, SDL_CONTROLLER_BUTTON_X)) {
            setPdfView((extraStyle_.pdfViewMode + 1) % 3); return;
        }
        if(screen_==ScreenState::AaModal && !settingsEditing_ && (match(SDLK_PAGEUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER)||match(SDLK_PAGEDOWN,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))){aaTab_=std::clamp(aaTab_+(match(SDLK_PAGEUP,SDL_CONTROLLER_BUTTON_LEFTSHOULDER)?-1:1),0,5);aaRow_=0;settingsScroll_=0;dirty_=true;return;}
        if (screen_ == ScreenState::ReadingJournal) {
            if (match(SDLK_PAGEUP, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) { journalUI_.prevTab(); dirty_ = true; return; }
            if (match(SDLK_PAGEDOWN, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) { journalUI_.nextTab(); dirty_ = true; return; }
            if (match(SDLK_x, SDL_CONTROLLER_BUTTON_X)) { journalUI_.openExport(); dirty_ = true; return; }
            if (match(SDLK_y, SDL_CONTROLLER_BUTTON_Y)) { journal_.interaction(SDL_GetTicks()); dirty_ = true; return; }
        }
        // Page turns
        if (match(SDLK_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || match(SDLK_PAGEDOWN, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
            handleDirectional(1, 0);
        } else if (match(SDLK_LEFT, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || match(SDLK_PAGEUP, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
            handleDirectional(-1, 0);
        } else if (match(SDLK_DOWN, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
            handleDirectional(0, 1);
        } else if (match(SDLK_UP, SDL_CONTROLLER_BUTTON_DPAD_UP)) {
            handleDirectional(0, -1);
        }

        if(screen_==ScreenState::FileBrowser && !quickMenu_ && match(SDLK_y,SDL_CONTROLLER_BUTTON_Y)){addBrowserFolder();return;}
        // Shortcuts
        if (screen_ == ScreenState::Reader) {
            if (match(SDLK_x, SDL_CONTROLLER_BUTTON_X)) { openContents(); }
            if (match(SDLK_y, SDL_CONTROLLER_BUTTON_Y)) {if(comic() && showZoomReset())returnComicToTop();else toggleBookmark();}
            if (match(SDLK_EQUALS, -1)) reflowEpub(currentFontSize_ + 2);
            if (match(SDLK_MINUS, -1)) reflowEpub(currentFontSize_ - 2);
        } else if (screen_ == ScreenState::Library) {
            if (match(SDLK_f, SDL_CONTROLLER_BUTTON_LEFTSTICK) && !visibleBooks_.empty()) {
                if(openSeriesShelf(visibleBooks_[selectedBookIdx_]))return;
                auto p = progressStore_.get(visibleBooks_[selectedBookIdx_].id);
                p.favorite = !p.favorite;
                progressStore_.put(visibleBooks_[selectedBookIdx_].id, p);
                progressDirty_ = true; updateVisibleBooks();
            }
            if (match(SDLK_PAGEUP, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
                category_ = static_cast<Category>((static_cast<int>(category_) + 5) % 6); updateVisibleBooks();
            }
            if (match(SDLK_PAGEDOWN, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
                category_ = static_cast<Category>((static_cast<int>(category_) + 1) % 6); updateVisibleBooks();
            }
            if (match(SDLK_x, SDL_CONTROLLER_BUTTON_X)) {
                if(!visibleBooks_.empty())openBookActions(visibleBooks_[selectedBookIdx_]);
            }
            if(match(SDLK_y,SDL_CONTROLLER_BUTTON_Y))setCatalogPanel(CatalogPanel::Browse);

            if (match(SDLK_t, -1)) { currentTheme_ = (currentTheme_ + 1) % g_paletteCount; dirty_ = true; }
            if (match(SDLK_MINUS, SDL_CONTROLLER_BUTTON_BACK)) {
                viewMode_ = (viewMode_ == ViewMode::Grid) ? ViewMode::List : ViewMode::Grid;
                dirty_ = true;
            }
        }

        if (!pad && key == SDLK_BACKSPACE && searchActive_ && !searchQuery_.empty()) {
            std::size_t pos = searchQuery_.size() - 1;
            while (pos > 0 && (static_cast<unsigned char>(searchQuery_[pos]) & 0xc0) == 0x80) --pos;
            searchQuery_.resize(pos);
            updateVisibleBooks();
        }
    }

    // Rendering Screens
    void render() {
        pollUpdates();
        trace("frame_begin");
        processLibraryScan();processPagePreviews();tickPageNavigation();
        int w = 1280, h = 720;
        getWindowSize(w, h);
        const auto readingPal=palette();
        const auto pal=interfacePalette();
        const int rotation=appSettings_.get().displayOrientation;
        if(rotation) {
            if(!orientationTarget_ || orientationWidth_!=w || orientationHeight_!=h) {
                if(orientationTarget_)SDL_DestroyTexture(orientationTarget_);
                orientationTarget_=SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,w,h);
                orientationWidth_=w;orientationHeight_=h;
            }
            if(!orientationTarget_ || SDL_SetRenderTarget(renderer_,orientationTarget_)!=0) {
                SDL_SetRenderTarget(renderer_,nullptr);auto a=appSettings_.get();a.displayOrientation=0;appSettings_.put(a);persistAppSettings();
                currentErrorMessage_="Portrait output unavailable; landscape restored";pageCache_.clear();
                if(doc_ && px_document_is_epub(doc_)){++layoutGeneration_;layoutPending_=true;layoutDue_=SDL_GetTicks()+100;}
                else if(doc_)setPdfView(extraStyle_.pdfViewMode);
                render();return;
            }
        }
        // Exactly the same resolved background feeds both the page surround
        // and all chrome in this frame (including during asynchronous reflow).
        const bool paper=doc_ && screen_!=ScreenState::Library && screen_!=ScreenState::FileBrowser && screen_!=ScreenState::ReadingJournal && !globalSettings_;
        SDL_Color canvas=paper?readingPal.bg:pal.bg;
        SDL_SetRenderDrawColor(renderer_, canvas.r, canvas.g, canvas.b, 255);
        SDL_RenderClear(renderer_);
        if(!paper)interfaceBackground(w,h,pal);

        if (screen_ == ScreenState::ReadingJournal) {
            journalUI_.setScrollY(int(journalScroll_));
            journalUI_.render(renderer_,*typography_,pal,journal_,progressStore_,catalog_,w,h,ui::language==1);
        } else if(screen_==ScreenState::JournalPreview) {
            renderReaderScreen(w,h,readingPal);
            journalUI_.renderPreview(renderer_,*typography_,pal,journal_,currentBookId_,w,h,ui::language==1);
        } else if (screen_ == ScreenState::Library) {
            renderLibraryScreen(w, h, pal);
        } else if (screen_ == ScreenState::Reader) {
            renderReaderScreen(w, h, readingPal);
        } else if (screen_ == ScreenState::FileBrowser) {
            renderFileBrowserScreen(w, h, pal);
        } else if (screen_ == ScreenState::AaModal || screen_ == ScreenState::TocModal || screen_ == ScreenState::BookmarksModal) {
            // Keep the reader page rendered in the background!
            if(globalSettings_) renderLibraryScreen(w,h,pal); else renderReaderScreen(w, h, readingPal);

            // Draw dimming backdrop scrim
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 150);
            SDL_Rect fullscreen{0, 0, w, h};
            SDL_RenderFillRect(renderer_, &fullscreen);

            // Render modal on top
            if (screen_ == ScreenState::AaModal) {
                renderAaModal(w, h, pal);
            } else if (screen_ == ScreenState::TocModal) {
                renderTocModal(w, h, pal);
            } else if (screen_ == ScreenState::BookmarksModal) {
                renderBookmarksModal(w, h, pal);
            }
        }

        // One discoverable touch affordance across every screen and modal.
        if(screen_!=ScreenState::JournalPreview)
            ui::button(renderer_,*typography_,{w-80,8,64,64},quickMenu_?"×":"+",pal,quickMenu_);
        if(quickMenu_) renderQuickMenu(w,h,pal);
        renderCatalogPanel(w,h,pal);
        if(colorEditing_<0 && choiceKind_<0)paintFocus();
        if(colorEditing_>=0) {
            auto panel=colorPanel(w,h);ui::elevated(renderer_,panel,pal);
            typography_->renderText(renderer_,typography_->titleFont(),ui::label(colorEditing_?"Arka plan rengi":"Metin rengi"),panel.x+24,panel.y+16,pal.fg);
            int cell=(panel.w-48)/4;
            for(int i=0;i<16;++i) {
                SDL_Rect r{panel.x+24+(i%4)*cell,panel.y+72+(i/4)*60,cell-8,52};
                ui::surface(renderer_,r,readingColor(swatchColors_[i],pal.bg),8);
                if(i==swatchFocus_)ui::focusRing(renderer_,r,pal);
            }
            if(readingContrast(readingColor(extraStyle_.backgroundColor,pal.bg),readingColor(extraStyle_.textColor,pal.fg))<4.5)
                typography_->renderText(renderer_,typography_->smallFont(),ui::label("Düşük kontrast"),panel.x+24,panel.y+316,pal.accent);
            ui::button(renderer_,*typography_,{panel.x+24,panel.y+348,(panel.w-64)/2,56},"Vazge\u00e7",pal);
            ui::button(renderer_,*typography_,{panel.x+panel.w/2+8,panel.y+348,(panel.w-64)/2,56},"Kaydet",pal,true);
        }
        renderChoices(w,h,pal);
        renderUpdateDialog(w,h,pal);
        if(rotation && orientationTarget_) {
            SDL_SetRenderTarget(renderer_,nullptr);
            SDL_SetRenderDrawColor(renderer_,pal.bg.r,pal.bg.g,pal.bg.b,255); SDL_RenderClear(renderer_);
            SDL_Rect dest{(h-w)/2,(w-h)/2,w,h};
            SDL_RenderCopyEx(renderer_,orientationTarget_,nullptr,&dest,rotation==1?90:270,nullptr,SDL_FLIP_NONE);
        }
        // Full-frame software dimming fallback, after all reader/UI elements
        // and after portrait composition. Each frame is cleared first; alpha
        // cannot accumulate across redraws.
        const int dimPercent = 100 - appSettings_.get().softwareBrightness;
        if (dimPercent > 0) {
            SDL_BlendMode oldBlend = SDL_BLENDMODE_NONE;
            SDL_GetRenderDrawBlendMode(renderer_, &oldBlend);
            SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
            Uint8 alpha = static_cast<Uint8>(std::clamp(dimPercent * 255 / 100, 0, 160));
            SDL_SetRenderDrawColor(renderer_, 0, 0, 0, alpha);
            int rw = 1280, rh = 720;
            SDL_GetRendererOutputSize(renderer_, &rw, &rh);
            SDL_Rect screenRect{0, 0, rw, rh};
            SDL_RenderFillRect(renderer_, &screenRect);
            SDL_SetRenderDrawBlendMode(renderer_, oldBlend);
        }
        trace("present_begin");
        SDL_RenderPresent(renderer_);
        trace("present_return");
    }

    std::string libraryEmptyMessage() const {
        std::error_code ec;
        if (!fs::is_directory(root_ / "books", ec)) return "SD kitap klasörü kullanılamıyor. SD kartı kontrol edin.";
        if (!searchQuery_.empty()) return "Aramanızla eşleşen kitap bulunamadı.";
        if (books_.empty()) return "Kitaplığınızda henüz kitap bulunmuyor.";
        switch (category_) {
            case Category::Completed: return "Henüz tamamladığınız bir kitap yok.";
            case Category::Favorites: return "Henüz favori kitabınız yok.";
            case Category::ContinueReading: return "Devam ettiğiniz bir kitap yok.";
            case Category::Recent: return "Henüz kitap açmadınız.";
            case Category::Unread: return "Okunmamış kitabınız yok.";
            default: return "Bu kategoride kitap bulunamadı.";
        }
    }
    void renderBookTitle(const std::string &title, SDL_Rect rect, SDL_Color color) {
        typography_->renderWrapped(renderer_,typography_->uiFont(),title,rect,color);
    }

    SDL_Rect categoryRect(int i,int w) const {
        int cols=w<900?3:6, cw=(w-64)/cols;
        return {32+(i%cols)*cw,98+(i/cols)*64,cw-12,56};
    }
    void placeholderCover(const Book &book,SDL_Rect r,const ThemePalette &pal) {
        CoverState state=CoverState::NotRequested;
        {std::lock_guard<std::mutex> lock(queueMutex_);auto found=coverStates_.find(book.id);if(found!=coverStates_.end())state=found->second;}
        ui::fallbackCover(renderer_,*typography_,r,book,state,pal);
    }
    void drawCover(const Book &book,SDL_Rect r,const ThemePalette &pal) {
        auto *thumb=getOrRenderThumbnail(book);
        if(!thumb) { placeholderCover(book,r,pal); return; }
        int tw=0,th=0; SDL_QueryTexture(thumb,nullptr,nullptr,&tw,&th);
        float scale=std::min(float(r.w)/std::max(1,tw),float(r.h)/std::max(1,th));
        int iw=std::max(1,int(tw*scale)),ih=std::max(1,int(th*scale));
        SDL_Rect dst{r.x+(r.w-iw)/2,r.y+(r.h-ih)/2,iw,ih};
        ui::surface(renderer_,r,pal.header,8); SDL_RenderCopy(renderer_,thumb,nullptr,&dst);
    }
    std::set<std::string> prioritizeCovers(int w,int h) {
        ui::LibraryLayout l(w); std::set<std::string> wanted;
        int first=viewMode_==ViewMode::Grid?int(listScroll_/l.stepY)*l.columns:int(listScroll_/112);
        int count=viewMode_==ViewMode::Grid?((h-l.top)/l.stepY+3)*l.columns:(h-l.top)/112+3;
        for(int i=first;i<std::min(int(visibleBooks_.size()),first+count);++i) wanted.insert(visibleBooks_[i].id);
        std::lock_guard<std::mutex> lock(queueMutex_);
        for(auto it=thumbQueue_.begin();it!=thumbQueue_.end();) {
            if(!wanted.count(it->id)) { queuedThumbIds_.erase(it->id); it=thumbQueue_.erase(it); } else ++it;
        }
        return wanted;
    }
    void renderLibraryScreen(int w, int h, const ThemePalette &pal) {
        ui::surface(renderer_,{0,0,w,88},pal.card,0);
        if(openSeriesKey_.empty()) {
            typography_->renderText(renderer_,typography_->titleFont(),ui::label("PaperNX"),32,24,pal.fg);
            if(w>=900)ui::button(renderer_,*typography_,{208,16,180,56},catLabel("Kitaplığı gez","Browse library"),pal,catalogFilter_.has_value());
        } else {
            ui::button(renderer_,*typography_,{16,16,152,56},catLabel("Geri","Back"),pal);
            typography_->renderText(renderer_,typography_->sectionFont(),openSeriesTitle_,184,24,pal.fg,std::max(1,w-644));
        }
        ui::button(renderer_,*typography_,{w-440,16,200,56},catLabel("Günlüğüm","My journal"),pal);
        ui::button(renderer_,*typography_,{w-216,16,112,56},searchActive_?"Temizle":"Ara",pal,searchActive_);
        if(searchActive_) typography_->renderText(renderer_,typography_->uiFont(),searchQuery_+"_",32,62,pal.accent,w-280);
        const char *cats[]={"Tüm kitaplar","Devam et","Son okunan","Favoriler","Okunmamış","Tamamlanan"};
        for(int i=0;i<6;++i) ui::button(renderer_,*typography_,categoryRect(i,w),cats[i],pal,static_cast<int>(category_)==i);
        ui::LibraryLayout l(w);
        if(visibleBooks_.empty()) {
            SDL_Rect empty{32,l.top+16,w-64,std::min(260,h-l.top-48)}; ui::elevated(renderer_,empty,pal);
            typography_->renderText(renderer_,typography_->titleFont(),ui::label(libraryScanner_.pending()?"Finding your books...":libraryEmptyMessage()),empty.x+24,empty.y+28,pal.fg,empty.w-48);
            typography_->renderText(renderer_,typography_->uiFont(),(books_.empty()?catLabel("SD’den EPUB, PDF, CBZ ve CBR açın.","Browse SD for EPUB, PDF, CBZ and CBR books."):ui::label("Başka bir kategori seçin veya aramanızı temizleyin.")),empty.x+24,empty.y+96,pal.muted,empty.w-48);
            if(books_.empty()) ui::button(renderer_,*typography_,{empty.x+24,empty.y+empty.h-72,260,56},ui::label("Add books folder"),pal,true);
        } else {
            if(screen_==ScreenState::Library) prioritizeCovers(w,h);
            SDL_Rect clip{0,l.top,w,h-l.top-28}; SDL_RenderSetClipRect(renderer_,&clip);
            if(viewMode_==ViewMode::Grid) renderGrid(w,h,pal); else renderList(w,h,pal);
            SDL_RenderSetClipRect(renderer_,nullptr);
        }
        if(libraryScanner_.pending())typography_->renderText(renderer_,typography_->smallFont(),ui::label("Scanning library..."),32,h-56,pal.accent,w-64);
        if(!currentErrorMessage_.empty()) ui::button(renderer_,*typography_,{32,h-96,w-64,56},currentErrorMessage_,pal);
        typography_->renderText(renderer_,typography_->smallFont(),(std::to_string(visibleBooks_.size())+(seriesShelves_.empty()?catLabel(" kitap · "," books · "):catLabel(" öğe · "," items · "))+"EPUB · PDF · CBZ · CBR"+catLabel(" · Çevrimdışı"," · Offline"))+" · "+APP_VERSION_FULL,32,h-25,pal.muted,w-64);
    }
    void renderGrid(int w,int h,const ThemePalette &pal) {
        ui::LibraryLayout l(w);
        int first=std::max(0,int(listScroll_/l.stepY)*l.columns);
        for(int i=first;i<int(visibleBooks_.size());++i) {
            int y=l.top+(i/l.columns)*l.stepY-int(listScroll_); if(y>=h-28) break;
            int x=l.left+(i%l.columns)*l.stepX;
            auto p=progressStore_.get(visibleBooks_[i].id);
            SDL_Rect card{x,y,l.cardWidth,l.cardHeight};
            bool pressed=ui::pointerDown && ui::contains(card,ui::pointerX,ui::pointerY);
            ui::elevated(renderer_,card,pal);
            if(pressed)ui::surface(renderer_,card,pal.header);

            drawCover(visibleBooks_[i],{x+10,y+10,l.cardWidth-20,206},pal);
            renderBookTitle(libraryItemTitle(visibleBooks_[i]),{x+12,y+228,l.cardWidth-24,56},pal.fg);
            typography_->renderText(renderer_,typography_->smallFont(),libraryItemSubtitle(visibleBooks_[i]),x+12,y+288,pal.muted,l.cardWidth-24);
            typography_->drawProgressBar(renderer_,x+12,y+318,l.cardWidth-24,4,libraryItemFraction(visibleBooks_[i]),pal.cardBorder,pal.accent);
            if(!seriesShelves_.count(visibleBooks_[i].id))ui::favorite(renderer_,{x+l.cardWidth-64,y+10,56,56},pal,p.favorite);
        }
    }
    void renderList(int w,int h,const ThemePalette &pal) {
        ui::LibraryLayout l(w); int first=std::max(0,int(listScroll_/112));
        for(int i=first;i<int(visibleBooks_.size());++i) {
            int y=l.top+i*112-int(listScroll_); if(y>=h-28) break;
            const auto &b=visibleBooks_[i]; auto p=progressStore_.get(b.id);
            SDL_Rect card{32,y,w-64,104};
            ui::elevated(renderer_,card,pal);

            drawCover(b,{40,y+6,66,92},pal);
            typography_->renderText(renderer_,typography_->uiFont(),libraryItemTitle(b),126,y+14,pal.fg,w-310);
            auto subtitle=libraryItemSubtitle(b);
            typography_->renderText(renderer_,typography_->smallFont(),subtitle.empty()?std::string(formatName(b.format))+catLabel(" kitap"," book"):subtitle,126,y+49,pal.muted,w-330);
            const auto fraction=libraryItemFraction(b);
            typography_->drawProgressBar(renderer_,126,y+82,w-350,5,fraction,pal.cardBorder,pal.accent);
            typography_->renderText(renderer_,typography_->smallFont(),ui::label("%")+std::to_string(int(fraction*100)),w-192,y+44,pal.muted);
            if(!seriesShelves_.count(b.id))ui::favorite(renderer_,{w-116,y+24,64,56},pal,p.favorite);
        }
    }

    void renderReaderScreen(int w, int h, const ThemePalette &pal) {
        bool comicSurfacePending=continuous()?renderScroll(w,h):false;
        trace("page_begin");
        tickPageTurn();
        const bool gpuComic=comic() && !continuous() && std::abs(pdfZoom_-1.f)>.0001f;
        if (pageTurnAnimation_.active) {
            const SDL_Rect viewport=pageTurnAnimation_.viewport;
            const SDL_Rect readerClip{currentMargin_,72,fixedViewportWidth(w),comicViewportHeight(h)};
            if(pageTurnAnimation_.comicSheet)SDL_RenderSetClipRect(renderer_,&readerClip);
            if(pageTurnAnimation_.spread) {
                PageCurlRenderer::renderSpread(renderer_,pageTurnAnimation_.previous.get(),pageTurnAnimation_.back.get(),
                    pageTurnAnimation_.under.get(),viewport,{0,0,w,h},pageTurnAnimation_.progress,
                    pageTurnAnimation_.direction,pageTurnAnimation_.anchorY,pageTurnAnimation_.dragY,
                    pageTurnAnimation_.comicSheet?&pageTurnAnimation_.oppositeViewport:nullptr,false,pageTurnAnimation_.paperBack.get());
            } else PageCurlRenderer::render(renderer_, pageTurnAnimation_.previous.get(),
                                     pageTurnAnimation_.next.get(), viewport,
                                     pageTurnAnimation_.progress, pageTurnAnimation_.direction,
                                     pageTurnAnimation_.anchorY,pageTurnAnimation_.dragY,pageTurnAnimation_.back.get(),
                                     pageTurnAnimation_.comicSheet?&pageTurnAnimation_.nextViewport:nullptr);
            if(pageTurnAnimation_.comicSheet)SDL_RenderSetClipRect(renderer_,nullptr);
        } else if(gpuComic) {
            if(comicSpreadActive())renderComicSpreadZoom(w,h);else renderComicZoom(w,h);
        } else if(!continuous()) {
            drawReaderSheets(currentPage_);
        }
        if(comic() && !continuous()) {
            const auto visible=comicSpreadPages(currentPage_);
            bool failed=false;
            for(int side=0;side<visible.second;++side) {
                const int page=visible.second==2?readerSidePage(visible.first,side):visible.first;
                if(pageCache_.get(comicTextureKey(page)))continue;
                if(comicBaseRetry_.count(page)) {
                    if(!failed)pageRenderError_=catLabel("Sayfa açılamadı: ","Could not render page: ")+std::to_string(page+1);
                    failed=true;
                } else comicSurfacePending=true;
            }
            if(!failed)pageRenderError_.clear();
        }
        if(comic() && (comicPendingTurn_.active || pageTurnTouch_.waitingForTexture || comicSurfacePending)) {
            const SDL_Rect loading{w/2-104,h/2-22,208,44};
            ui::surface(renderer_,loading,pal.card,12);
            typography_->renderText(renderer_,typography_->smallFont(),catLabel("Sayfa hazırlanıyor…","Preparing page…"),
                                    loading.x+12,loading.y+13,pal.muted,loading.w-24);
        }
        trace("page_end");
        if(showZoomReset()){
            ui::button(renderer_,*typography_,zoomResetRect(w,h),catLabel("Sayfaya sığdır","Fit page"),pal);
            if(comic())drawZoomTop(w,h,pal);
        }
        if (!currentErrorMessage_.empty() || !pageRenderError_.empty() || !comicTurnError_.empty()) {
            settingsButton({36, h - 120, w - 72, 48},!pageRenderError_.empty()?pageRenderError_:
                           (!comicTurnError_.empty()?comicTurnError_:currentErrorMessage_),pal);
        }
        if (debugTouch_) settingsButton({36, 72, w - 72, 48}, lastTouchDebug_, pal);
        // Toolbar Overlays
        if (toolbarVisible_ || appSettings_.get().toolbar) {
            const auto tools=readerChromePalette(presentedStyle(),false,0,0);
            ui::surface(renderer_,{12,6,128,60},tools.card,24);
            ui::surface(renderer_,{w-232,6,144,60},tools.card,24);
            for(int i=0;i<4;++i)drawBookHeaderButton(i,w,tools);
            if(comic())drawReaderCaption(journalTitle_,{160,0,std::max(1,w-416),72},tools);
            else drawReaderCaption(bookChapterCaption(h),{160,0,std::max(1,w-416),72},tools);

            renderReaderNavigation(w,h,pal);
        }
        if(doc_ && (continuous() || gpuComic || toolbarVisible_ || appSettings_.get().toolbar ||
                    (pageTurnAnimation_.active && pageTurnAnimation_.comicSheet))) {
            const auto caption=pageTurnAnimation_.active && pageTurnAnimation_.comicSheet
                ? pageTurnAnimation_.comicFolioCaption : readerFooterCaption(w,h);
            drawReaderCaption(caption,{96,h-58,std::max(1,w-192),44},pal);
        }
    }

    void renderFileBrowserScreen(int w,int h,const ThemePalette &pal) {
        ui::surface(renderer_,{0,0,w,88},pal.card,0);
        ui::button(renderer_,*typography_,{24,16,152,56},"Kitaplık",pal);
        ui::button(renderer_,*typography_,{192,16,168,56},"Üst klasör",pal);
        ui::button(renderer_,*typography_,browserFolderButton(w),ui::label("Add this folder"),pal,true);
        typography_->renderText(renderer_,typography_->smallFont(),browserPath_.u8string(),32,90,pal.muted,w-64);
        SDL_Rect clip{24,112,w-48,h-128}; SDL_RenderSetClipRect(renderer_,&clip);
        int first=int(browserScroll_/64);
        for(int i=first;i<int(browserEntries_.size());++i) {
            int y=112+i*64-int(browserScroll_); if(y>=h-16) break;
            const std::string itemPrefix = browserEntries_[i].isDirectory ? ui::label("Klasör   ") : ui::label("Kitap    ");
            ui::button(renderer_,*typography_,{32,y,w-64,56},
                itemPrefix+browserEntries_[i].path.filename().u8string(),pal,false,false,false,false);
        }
        SDL_RenderSetClipRect(renderer_,nullptr);
        if(browserEntries_.empty()) typography_->renderText(renderer_,typography_->uiFont(),catLabel("Bu klasörde EPUB, PDF, CBZ veya CBR bulunamadı.","No EPUB, PDF, CBZ or CBR books in this folder."),48,164,pal.muted,w-96);
    }

    void renderAaModal(int w, int h, const ThemePalette &pal) {
        trace("settings_draw");
        ui::SettingsLayout l(w,h);
        ui::elevated(renderer_,l.sheet,pal);
        ui::surface(renderer_,{l.sheet.x+8,l.sheet.y+72,l.sidebar,l.sheet.h-88},pal.bg,14);
        typography_->renderText(renderer_,typography_->titleFont(),ui::label("Okuma ve görünüm"),l.sheet.x+24,l.sheet.y+16,pal.fg);
        ui::button(renderer_,*typography_,l.close,"Kapat",pal);
        const char *tabs[]={"Okuma","Sayfa düzeni","Görünüm","Navigasyon","PDF","Uygulama"};
        if(comic()) {tabs[0]=ui::language?"Artwork":"Çizimler";tabs[1]=ui::language?"Borders":"Kenarlar";tabs[4]=ui::language?"Page view":"Sayfa görünümü";}
        for(int i=0;i<6;++i) ui::button(renderer_,*typography_,l.tab(i),tabs[i],pal,aaTab_==i);
        std::string scope=globalSettings_||!doc_?ui::label("Default reading style · Applies to books"):
            extraStyle_.styleOverride?"Bu kitabın teması · Genel temaya dön":"Genel temayı kullanıyor · Bu kitabı özelleştir";
        ui::button(renderer_,*typography_,l.scope,scope,pal,doc_ && extraStyle_.styleOverride);
        auto row=[&](int n,const std::string &label,bool selected=false) {
            auto r=l.control(n); r.y-=int(settingsScroll_);
            auto text=label;bool numeric=text.rfind("-    ",0)==0;
            if(numeric)text=text.substr(5);
            auto plus=text.find("    +");if(plus!=std::string::npos)text=text.substr(0,plus);
            auto split=text.find(": ");
            if(numeric && split!=std::string::npos) ui::settingsRow(renderer_,*typography_,r,text.substr(0,split),text.substr(split+2),pal,false,settingsEditing_ && n==aaRow_);
            else {
                ui::preferenceRow(renderer_,*typography_,r,text,pal,selected);
            }
        };
        const char *families[]={"Serif","Sans","Yayıncı"};
        SDL_RenderSetClipRect(renderer_,&l.content);
        if(comic() && aaTab_==0) {
            row(0,comicReadingColors()?catLabel("Çizim renkleri: Okuma paleti","Artwork colors: Reader palette"):catLabel("Çizim renkleri: Özgün","Artwork colors: Original"));
            typography_->renderText(renderer_,typography_->smallFont(),catLabel("Çizgi roman sayfaları görüntüdür; yazı boyutu yerine Sayfa görünümündeki yakınlaştırmayı kullanın.","Comic pages are artwork. Use zoom in Page view to enlarge text and panels."),l.content.x+16,l.content.y+88,pal.muted,l.content.w-32);
        } else if(aaTab_==0) {
            row(0,"-    Yazı boyutu: "+std::to_string(currentFontSize_)+" px    +");
            row(1,std::string("Yazı tipi: ")+families[currentFontFamily_]);
            row(2,"-    Satır aralığı: %"+std::to_string(currentLineHeight_)+"    +");
            row(3,"-    Paragraf aralığı: %"+std::to_string(extraStyle_.paragraphSpacing)+"    +");
            row(4,extraStyle_.publisherStyles?"Yayıncı stilleri: Açık":"Yayıncı stilleri: Kapalı");
            row(5,extraStyle_.indentation?"Paragraf girintisi: Açık":"Paragraf girintisi: Kapalı");
        } else if(comic() && aaTab_==1) {
            row(0,"-    Kenar boşluğu: "+std::to_string(currentMargin_)+" px    +");
            typography_->renderText(renderer_,typography_->smallFont(),catLabel("Özgün sayfa düzeni ve konuşma balonları korunur.","Original page layouts and speech balloons are preserved."),l.content.x+16,l.content.y+88,pal.muted,l.content.w-32);
        } else if(aaTab_==1) {
            row(0,"-    Kenar boşluğu: "+std::to_string(currentMargin_)+" px    +");
            row(1,currentTextAlign_?"Hizalama: Başlangıca yaslı":"Hizalama: İki yana yaslı");
            row(2,extraStyle_.indentation?"Paragraf girintisi: Açık":"Paragraf girintisi: Kapalı");
            row(3,"-    Paragraf aralığı: %"+std::to_string(extraStyle_.paragraphSpacing)+"    +");
            row(4,catLabel("Yatay ekranda çift sayfa: ","Double pages in landscape: ")+ui::label(appSettings_.get().bookLandscapeSpreads?"On":"Off"));
        } else if(aaTab_==2) {
            for(int i=0;i<10;++i) {
                auto r=l.control(i/2); r.y-=int(settingsScroll_);int half=(r.w-12)/2;r.x+=(i%2)*(half+12);r.w=half;
                const int order[]={0,1,4,6,7,8,2,3,5,9};auto preset=g_paperPresets[order[i]];auto colors=g_palettes[preset.theme];colors.bg=readingColor(preset.background,colors.bg);colors.header=colors.bg;colors.fg=readingColor(preset.text,colors.fg);
                ui::button(renderer_,*typography_,r,preset.name,colors,extraStyle_.backgroundColor==preset.background && extraStyle_.textColor==preset.text);
            }
            row(5,catLabel("Metin rengi · Renk paleti","Text color · Color palette"));
            row(6,catLabel("Arka plan · Renk paleti","Background · Color palette"));
            row(7,catLabel("Tema renklerine sıfırla","Reset theme colors"));
            auto softwareRow=l.control(8);softwareRow.y-=int(settingsScroll_);
            ui::settingsSliderRow(renderer_,*typography_,softwareRow,
                catLabel("Yazılımsal parlaklık","Software brightness"),appSettings_.get().softwareBrightness,50,100,pal,aaRow_==8);
            auto systemRow=l.control(9);systemRow.y-=int(settingsScroll_);
            const auto systemLabel=catLabel("Sistem parlaklığı (El cihazı)","System brightness (Handheld)")+" · "+
                (backlightAvailable_?catLabel("Etkin","Active"):catLabel("Yazılım fallback","Software fallback"));
            ui::settingsSliderRow(renderer_,*typography_,systemRow,systemLabel,appSettings_.get().systemBrightness,10,100,pal,aaRow_==9);
            row(10,catLabel("Parlaklığı sıfırla (%100)","Reset brightness (100%)"));
        } else if(aaTab_==3) {
            row(0,"Sayfalı Mod · Yatay gezinme",extraStyle_.readingMode==0);
            row(1,"Sürekli Kaydırma · Yukarı / aşağı",extraStyle_.readingMode==1);
            row(2,"-    Kaydırma hassasiyeti: %"+std::to_string(extraStyle_.scrollSensitivity)+"    +");
            row(3,extraStyle_.scrollInertia?"Kaydırma momentumu: Açık":"Kaydırma momentumu: Kapalı");
            row(5,ui::language?(appSettings_.get().shoulderPageTurns?"L/R page shortcuts: On":"L/R page shortcuts: Off"):(appSettings_.get().shoulderPageTurns?"L/R sayfa k\u0131sayolu: A\u00e7\u0131k":"L/R sayfa k\u0131sayolu: Kapal\u0131"));
            row(4,"-    Sayfa dokunma alanı: %"+std::to_string(extraStyle_.touchZone)+"    +");
            typography_->renderText(renderer_,typography_->smallFont(),ui::label("Dikey sürükleme menüyü açmaz. + her zaman erişilebilir."),l.content.x,l.content.y+326,pal.muted,l.content.w);
        } else if(aaTab_==4) {
            row(0,"Sayfaya sığdır",extraStyle_.pdfViewMode==0);
            row(1,"Genişliğe sığdır",extraStyle_.pdfViewMode==1);
            row(2,"Gerçek boyut · 100%",extraStyle_.pdfViewMode==2);
            row(4,appSettings_.get().preservePdfZoom?"Sayfalar aras\u0131 yak\u0131nla\u015ft\u0131rma: A\u00e7\u0131k":"Sayfalar aras\u0131 yak\u0131nla\u015ft\u0131rma: Kapal\u0131");
            if(doc_ && !px_document_is_epub(doc_)) row(3,"-    Yakınlaştırma: %"+std::to_string(int(pdfZoom_*100))+"    +");
            if(comic()) {
                row(5,comicReadingColors()?catLabel("Çizim renkleri: Okuma paleti","Artwork colors: Reader palette"):catLabel("Çizim renkleri: Özgün","Artwork colors: Original"));
                row(6,readingDirection()<0?catLabel("Okuma yönü: Sağdan sola","Reading direction: Right to left"):catLabel("Okuma yönü: Soldan sağa","Reading direction: Left to right"));
                row(7,catLabel("Yatay ekranda çift sayfa: ","Double pages in landscape: ")+ui::label(appSettings_.get().comicLandscapeSpreads?"On":"Off"));
            } else typography_->renderText(renderer_,typography_->smallFont(),
                 catLabel("PDF renkleri Görünüm sekmesinden ayarlanır.","PDF colors are adjusted in Appearance."),
                 l.content.x+16,l.content.y+320,pal.muted,l.content.w-32);
        } else {
            auto p=appSettings_.get();
            const int globalPreset=globalPaperPresetIndex();
            row(0,std::string(ui::language?"General theme: ":"Genel tema: ")+
                (globalPreset>=0?g_paperPresets[globalPreset].name:(ui::language?"Custom colors":"Özel renkler")));
            row(1,viewMode_==ViewMode::Grid?"Kitaplık: Kapak ızgarası":"Kitaplık: Ayrıntılı liste");
            const char *sorts[]={"Başlık","Yazar","Dosya adı","Değiştirilme","Son okunan"};
            row(2,std::string("Sıralama: ")+sorts[static_cast<int>(sort_)]);
            row(3,p.toolbar?"Okuma araçları: Sürekli görünür":"Okuma araçları: Dokunarak göster");
            if(!browserPath_.empty()) row(4,"Geçerli klasörü başlangıç olarak kaydet");
            row(5,p.language?"Dil: English":"Dil: Türkçe");
            const char *orientEn[]={"Landscape","Portrait clockwise","Portrait counterclockwise"};
            const char *orientTr[]={"Yatay","Dikey: saat y\u00f6n\u00fc","Dikey: ters y\u00f6n"};
            row(6,std::string(ui::language?"Display orientation: ":"Ekran yönü: ")+(ui::language?orientEn:orientTr)[p.displayOrientation]);
            const char *textureEn[]={"Off","Soft paper","Natural paper"};const char *textureTr[]={"Kapal\u0131","Yumu\u015fak ka\u011f\u0131t","Do\u011fal ka\u011f\u0131t"};
            row(7,std::string(ui::language?"Paper texture: ":"Kağıt dokusu: ")+(ui::language?textureEn:textureTr)[p.paperTexture]);
            row(8,ui::label("Expanded reading area")+": "+ui::label(p.expandedViewport?"On":"Off"));
            row(9,ui::label("Book folders")+": "+std::to_string(p.libraryFolders.size()+1));
            row(10,ui::label("Scan on launch")+": "+ui::label(p.scanOnLaunch?"On":"Off"));
            row(11,ui::label(libraryScanner_.pending()?"Cancel library scan":"Scan library now"));
            const char *styles[]={"Dark","Light","Follow reading theme"};
            const char *accents[]={"Purple","Blue","Teal","Green","Orange","Red","Pink"};
            row(12,ui::label("Interface appearance")+": "+ui::label(styles[p.interfaceStyle]));
            row(13,ui::label("Accent color")+": "+ui::label(accents[p.interfaceAccent]));
            row(14,ui::label("Automatic update check")+": "+ui::label(p.automaticUpdateCheck?"On":"Off"));
            row(15,ui::label("Check for updates"));
            std::string coverLabel;
            if(coverClearStatus_.load()==1) {
                coverLabel = ui::label("Clearing cover cache...");
            } else if(coverClearConfirm_) {
                coverLabel = ui::label("Confirm clear cover cache (A)");
            } else {
                coverLabel = ui::label("Cover cache") + ": " + std::to_string(coverDiskEntries_.load()) + " " +
                    catLabel("kapak","covers") + " (" + formatBytes(coverDiskBytes_.load()) + ") · " +
                    ui::label("Disk hits/misses") + ": " + std::to_string(coverDiskHits_.load()) + "/" +
                    std::to_string(coverDiskMisses_.load()) + " · " + ui::label("Clear");
            }
            row(16,coverLabel);

        }
        SDL_RenderSetClipRect(renderer_,nullptr);
        const float maxScroll=maxUiScroll();
        if(maxScroll>0) {
            const int thumb=std::max(32,int(float(l.content.h*l.content.h)/(settingsRows()*64)));
            const int y=l.content.y+int(settingsScroll_/maxScroll*(l.content.h-thumb));
            ui::surface(renderer_,{l.content.x+l.content.w+8,l.content.y,4,l.content.h},pal.header,2);
            ui::surface(renderer_,{l.content.x+l.content.w+8,y,4,thumb},pal.accent,2);
        }
        if(l.preview.w && (!doc_ || px_document_is_epub(doc_))) {
            auto rp=g_palettes[std::clamp(currentTheme_,0,5)];
            rp.bg=readingColor(extraStyle_.backgroundColor,rp.bg); rp.fg=readingColor(extraStyle_.textColor,rp.fg);
            ui::surface(renderer_,l.preview,rp.bg);
            typography_->renderText(renderer_,typography_->smallFont(),ui::label("CANLI ÖNİZLEME"),l.preview.x+20,l.preview.y+20,rp.muted);
            auto font=typography_->previewFont(currentFontFamily_,currentFontSize_);
            int margin=std::clamp(currentMargin_/2,8,64);
            int step=int(currentFontSize_*currentLineHeight_/100.f);
            SDL_Rect textArea{l.preview.x+margin,l.preview.y+60,l.preview.w-margin*2,l.preview.h-96};
            SDL_RenderSetClipRect(renderer_,&textArea);
            int y=textArea.y;
            y+=typography_->renderParagraph(renderer_,font,ui::label("Bir sayfa, bir dünya"),{textArea.x,y,textArea.w,textArea.h},rp.fg,step,true,0)+step/2;
            int indent=currentFontSize_*extraStyle_.indentation/100;
            y+=typography_->renderParagraph(renderer_,font,ui::label("İstanbul’da güneş doğarken, çığlık çığlığa uçan kuşlar şehrin üstünde süzülüyordu."),
                {textArea.x,y,textArea.w,textArea.y+textArea.h-y},rp.fg,step,currentTextAlign_!=0,indent);
            y+=currentFontSize_*extraStyle_.paragraphSpacing/100;
            typography_->renderParagraph(renderer_,font,ui::label("Öğrenmek, düşünmek ve hayal kurmak için bir sayfa daha çevir. Güzel bir gün!"),
                {textArea.x,y,textArea.w,std::max(0,textArea.y+textArea.h-y)},rp.fg,step,currentTextAlign_!=0,indent);
            SDL_RenderSetClipRect(renderer_,nullptr);
            if(readingContrast(rp.bg,rp.fg)<4.5) typography_->renderText(renderer_,typography_->smallFont(),ui::label("Düşük kontrast"),l.preview.x+20,l.preview.y+l.preview.h-34,pal.accent);
        }
        auto save=l.footer; save.w=(save.w-16)/2; ui::button(renderer_,*typography_,save,"Kaydet ve kapat",pal,true);

        save.x+=save.w+16; ui::button(renderer_,*typography_,save,"Ayarları sıfırla",pal);
        if(layoutPending_ || layoutWorker_.joinable()) typography_->renderText(renderer_,typography_->smallFont(),ui::label("Düzen hazırlanıyor…"),l.sheet.x+24,l.sheet.y+l.sheet.h-40,pal.accent,l.sidebar-16);
    }

    void settingsButton(SDL_Rect r, const std::string &label, const ThemePalette &pal) {
        ui::menuRow(renderer_,*typography_,r,label,pal);
    }

    void renderTocModal(int w,int h,const ThemePalette &pal) {renderContents(w,h,pal);}
    void renderBookmarksModal(int w,int h,const ThemePalette &pal) {
        auto r=navigationPanel(w,h); ui::elevated(renderer_,r,pal);
        typography_->renderText(renderer_,typography_->titleFont(),ui::label("Yer imleri"),56,96,pal.fg);
        ui::button(renderer_,*typography_,{w-184,88,112,56},"Kapat",pal);
        ui::button(renderer_,*typography_,{48,h-112,w-96,56},"Bu konuma yer imi ekle / kaldır",pal,true);
        auto p=progressStore_.get(currentBookId_);
        if(p.bookmarks.empty()) typography_->renderText(renderer_,typography_->uiFont(),ui::label("Henüz yer imi yok. Alttaki düğmeyle bu konumu kaydedin."),56,168,pal.muted,w-112);
        SDL_Rect clip{48,152,w-96,h-280}; SDL_RenderSetClipRect(renderer_,&clip);
        for(int i=int(bookmarkScroll_/56);i<int(p.bookmarks.size());++i) {
            int y=152+i*56-int(bookmarkScroll_); if(y>=h-128) break;
            ui::button(renderer_,*typography_,{56,y,w-256,56},p.bookmarks[i].title,pal,false,false,false,false);
            ui::button(renderer_,*typography_,{w-184,y,112,56},"Sil",pal);
        }
        SDL_RenderSetClipRect(renderer_,nullptr);
    }

};

} // namespace

int main(int argc, char **argv) {
#ifdef __SWITCH__
    fs::path root = "sdmc:/switch/PaperNX";
#else
    fs::path root = (argc > 1) ? argv[1] : "./PaperNX-data";
#endif
    std::string executable=argc>0&&argv&&argv[0]?argv[0]:"";
#ifdef __SWITCH__
    if(!executable.empty()&&executable[0]=='/')executable="sdmc:"+executable;
#endif
    auto updater=std::make_shared<SelfUpdater>();
#ifdef __SWITCH__
    updater->configure(executable,APP_VERSION,"romfs:/cacert.pem");
#else
    updater->configure(executable,APP_VERSION);
#endif
    std::string bootError;
    auto boot=updater->prepareBoot(bootError);
    auto scheduleRelaunch=[&] {
#ifdef __SWITCH__
        if(envHasNextLoad())envSetNextLoad(executable.c_str(),executable.c_str());
#endif
    };
    if(boot==SelfUpdater::BootRecovery::Error) {
        std::fprintf(stderr,"Update recovery: %s\n",bootError.c_str());return 1;
    }
    if(boot==SelfUpdater::BootRecovery::RolledBack) {
        std::fprintf(stderr,"Update recovery: %s\n",bootError.c_str());scheduleRelaunch();return 0;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "Failed to initialize SDL2: %s\n", SDL_GetError());
        if(boot==SelfUpdater::BootRecovery::Candidate){updater->rollbackBoot(bootError);scheduleRelaunch();}
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "Failed to initialize SDL2_ttf: %s\n", TTF_GetError());
        SDL_Quit();
        if(boot==SelfUpdater::BootRecovery::Candidate){updater->rollbackBoot(bootError);scheduleRelaunch();}
        return 1;
    }

    int result=0;
    {
        PaperNxApp app(root,updater);
        if(app.init()) {app.startUpdates(argc,argv);app.run();}
        else result=1;
    }

    if(boot==SelfUpdater::BootRecovery::Candidate) {
        const auto recovery=updater->prepareBoot(bootError);
        if(recovery==SelfUpdater::BootRecovery::RolledBack) {
            std::fprintf(stderr,"Update recovery: %s\n",bootError.c_str());scheduleRelaunch();result=0;
        } else if(recovery==SelfUpdater::BootRecovery::Error) {
            std::fprintf(stderr,"Update recovery: %s\n",bootError.c_str());result=1;
        }
    }

    TTF_Quit();
    SDL_Quit();
    return result;
}
