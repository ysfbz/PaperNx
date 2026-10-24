#define main papernx_application_entry
#include "../src/main.cpp"
#include "test_evidence.hpp"
#undef main
#include <cassert>
#include <iostream>
namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
    static void settle(PaperNxApp &app) {
        const auto deadline=SDL_GetTicks()+10000;
        while(app.libraryScanner_.pending() && SDL_GetTicks()<deadline){app.processLibraryScan();SDL_Delay(2);}
        assert(!app.libraryScanner_.pending());
    }
    static void shot(PaperNxApp &app,const std::string &name) {
        app.render();int w,h;app.getWindowSize(w,h);
        auto *s=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(s);
        assert(SDL_RenderReadPixels(app.renderer_,nullptr,s->format->format,s->pixels,s->pitch)==0);
        const auto folder=test_evidence::path("modern-ui");fs::create_directories(folder);assert(SDL_SaveBMP(s,(folder/(name+".bmp")).string().c_str())==0);SDL_FreeSurface(s);
    }
    static void setting(PaperNxApp &app,int row) {
        int w,h;app.getWindowSize(w,h);ui::SettingsLayout l(w,h);
        app.settingsScroll_=std::clamp(float(row*64-l.content.h+56),0.f,app.maxUiScroll());
        auto r=l.control(row);r.y-=int(app.settingsScroll_);app.handleAaModalTap(r.x+r.w/2,r.y+r.h/2,w,h);
    }
    static void run(const fs::path &root) {
        {
            PaperNxApp app(root);assert(app.init());
            // init starts a worker and does not synchronously traverse book folders.
            assert(app.libraryScanner_.pending() && app.books_.empty());settle(app);assert(app.books_.size()==1);
            auto prefs=app.appSettings_.get();prefs.libraryFolders.push_back((root/"missing-sd-library").u8string());app.appSettings_.put(prefs);
            const auto beforeUnavailableScan=app.books_.size();app.startLibraryScan();settle(app);
            assert(app.books_.size()==beforeUnavailableScan && !app.currentErrorMessage_.empty());
            prefs=app.appSettings_.get();prefs.libraryFolders.pop_back();app.appSettings_.put(prefs);
            const auto before=app.completedLibraryScans_;
            app.openBrowser();app.browserPath_=root/"import";app.scanBrowserDirectory();shot(app,"folder-picker");
            const auto add=app.browserFolderButton(1280);app.handleTap(float(add.x+add.w/2)/1280,float(add.y+add.h/2)/720);
            assert(app.screen_==ScreenState::Library && app.libraryScanner_.pending());
            app.activeFingers_.insert(99);
            const auto deadline=SDL_GetTicks()+10000;while(!app.libraryScanner_.ready() && SDL_GetTicks()<deadline)SDL_Delay(1);
            app.processLibraryScan();assert(app.completedLibraryScans_==before);app.activeFingers_.clear();settle(app);
            assert(app.books_.size()==4 && app.appSettings_.get().libraryFolders.size()==1);
            // Duplicate folder selection and nested roots do not duplicate books.
            app.browserPath_=root/"import";app.addBrowserFolder();settle(app);assert(app.books_.size()==4 && app.appSettings_.get().libraryFolders.size()==1);
            app.browserPath_=root/"import"/"nested";app.addBrowserFolder();settle(app);assert(app.books_.size()==4);
            for(int i=0;i<40;++i){app.processCompletedThumbnails();SDL_Delay(5);}
            for(int variant:{0,1,2}) {
                auto p=app.appSettings_.get();p.interfaceStyle=variant;p.interfaceAccent=variant==1?1:0;app.appSettings_.put(p);
                const auto suffix=std::to_string(variant);shot(app,"library-"+suffix);
                app.quickMenu_=true;shot(app,"plus-"+suffix);app.quickMenu_=false;
                app.openSettings(true,5);shot(app,"settings-"+suffix);app.settingsScroll_=app.maxUiScroll();shot(app,"settings-library-"+suffix);app.dismissSettings();
                app.openChoice(8,variant);shot(app,"appearance-"+suffix);app.choiceKind_=-1;
                app.setCatalogPanel(PaperNxApp::CatalogPanel::Browse);shot(app,"catalog-"+suffix);app.closeCatalogPanel();
            }
            app.openSettings(true,5);setting(app,10);assert(!app.appSettings_.get().scanOnLaunch);
            setting(app,12);assert(app.choiceKind_==8);app.choiceIndex_=1;app.commitChoice();
            setting(app,13);assert(app.choiceKind_==9);app.choiceIndex_=2;app.commitChoice();
            setting(app,9);assert(app.choiceKind_==10 && app.choiceOptions().size()==4);shot(app,"folders");app.choiceKind_=-1;app.dismissSettings();
            const auto item=app.books_.front();app.openBook(item);assert(app.doc_);const int page=app.currentPage_;auto *doc=app.doc_;
            fs::copy_file("tests/fixtures/sample_book.epub",root/"import"/"new.epub");
            app.startLibraryScan();settle(app);assert(app.doc_==doc && app.currentPage_==page && app.books_.size()==5);
            app.openSettings(false,0);shot(app,"reading-settings");app.dismissSettings();
            app.openContents();shot(app,"contents");app.screen_=ScreenState::Reader;
            app.screen_=ScreenState::BookmarksModal;shot(app,"bookmarks");app.screen_=ScreenState::Reader;
            // Folder management can also be entered from an open book. Save and
            // close that reader before giving the library/browser ownership.
            app.openSettings(false,5);setting(app,9);assert(app.choiceKind_==10);
            app.choiceIndex_=0;app.commitChoice();assert(app.screen_==ScreenState::FileBrowser && !app.doc_);
            assert(app.progressStore_.get(item.id).page==page);
            app.screen_=ScreenState::Library;SDL_SetWindowSize(app.window_,720,1280);app.openSettings(true,5);shot(app,"portrait-settings");
            ui::language=0;shot(app,"portrait-settings-turkish");ui::language=1;app.dismissSettings();
            app.persistAppSettings();
        }
        fs::copy_file("tests/fixtures/sample_book.epub",root/"import"/"after-restart.epub");
        {
            PaperNxApp app(root);assert(app.init());assert(!app.libraryScanner_.pending());
            assert(app.books_.size()==5);assert(!app.appSettings_.get().scanOnLaunch);
            assert(app.appSettings_.get().interfaceStyle==1 && app.appSettings_.get().interfaceAccent==2);
            assert(app.appSettings_.get().libraryFolders.size()==2);
            const auto scansBeforeQueue=app.completedLibraryScans_;
            app.startLibraryScan();assert(app.libraryScanner_.pending());
            app.startLibraryScan();assert(app.libraryScanAgain_);
            const auto queuedDeadline=SDL_GetTicks()+10000;while(!app.libraryScanner_.ready() && SDL_GetTicks()<queuedDeadline)SDL_Delay(1);
            app.processLibraryScan();assert(app.libraryScanner_.pending() && !app.libraryScanAgain_ && app.completedLibraryScans_==scansBeforeQueue);
            settle(app);assert(app.completedLibraryScans_==scansBeforeQueue+1 && app.books_.size()==6);
            app.openSettings(true,5);const auto completed=app.completedLibraryScans_;
            setting(app,11);assert(app.libraryScanner_.pending());setting(app,11);
            const auto cancelDeadline=SDL_GetTicks()+10000;while(!app.libraryScanner_.ready() && SDL_GetTicks()<cancelDeadline)SDL_Delay(2);
            app.processLibraryScan();assert(app.completedLibraryScans_==completed && app.books_.size()==6);
            setting(app,11);assert(app.libraryScanner_.pending());settle(app);assert(app.books_.size()==6);app.dismissSettings();
            // Removing a folder only disables scanning; it never deletes sources.
            app.openChoice(10,2);app.commitChoice();assert(app.choiceKind_==11);app.choiceIndex_=1;app.commitChoice();
            assert(app.appSettings_.get().libraryFolders.size()==1 && fs::exists(root/"import"/"new.epub"));
        }
        std::cout<<"PASS: async startup, batch folder import, nested roots, gesture-safe publication, manual/launch scans, settings persistence, reader continuity and modern modal screenshots\n";
    }
    static void runHistoryRestore(const fs::path &root) {
        fs::create_directories(root/"books");fs::create_directories(root/"data");fs::create_directories(root/"external");
        const auto path=root/"external"/"history.epub";fs::copy_file("tests/fixtures/sample_book.epub",path);
        ProgressStore seed(root/"data"/"progress.txt");Progress p;p.page=3;p.pageCount=10;p.bookPath=path.u8string();p.bookTitle="Recovered title";p.bookAuthor="Recovered author";
        const auto id=documentId(path.u8string(),fs::file_size(path));seed.put(id,p);assert(seed.save());
        PaperNxApp app(root);assert(app.init());assert(app.books_.empty() && app.libraryScanner_.pending());
        settle(app);const auto found=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &book){return book.id==id;});
        assert(found!=app.books_.end() && found->title==p.bookTitle && found->author==p.bookAuthor);
        assert(app.progressStore_.get(id).page==3 && app.progressStore_.get(id).pageCount==10);
    }
    static void runStaleLibraryResult(const fs::path &root) {
        fs::create_directories(root/"books");fs::create_directories(root/"data");fs::create_directories(root/"external");
        const auto scanned=root/"external"/"stale.epub";fs::copy_file("tests/fixtures/sample_book.epub",scanned);
        AppSettingsStore settings(root/"data"/"settings.txt");auto prefs=settings.get();prefs.scanOnLaunch=false;settings.put(prefs);assert(settings.save());
        PaperNxApp app(root);assert(app.init()&&!app.libraryScanner_.pending());
        Book retained;retained.id="retained-snapshot";retained.title="Current snapshot";retained.format=Format::Epub;
        app.books_.push_back(retained);app.rebuildLibraryLookup();
        const auto generation=app.libraryScanner_.begin(ScanRequest{{root/"external"},{},root/"data"/"library-index.txt"});
        assert(generation>0);app.libraryScanGeneration_=generation+1;
        const auto deadline=SDL_GetTicks()+10000;while(!app.libraryScanner_.ready()&&SDL_GetTicks()<deadline)SDL_Delay(1);
        assert(app.libraryScanner_.ready());app.processLibraryScan();
        assert(app.books_.size()==1&&app.books_.front().id==retained.id&&app.books_.front().title==retained.title);
    }
    static void runCoverTelemetryHandoff() {
        PaperNxApp app(fs::temp_directory_path()/"papernx-cover-telemetry-handoff");
        constexpr std::uint64_t total=20000;
        std::atomic<std::uint64_t> queued{0},finished{0};
        const std::string id="same-cover-id";
        std::thread producer([&] {
            while(queued.load(std::memory_order_relaxed)<total) {
                bool added=false;
                {
                    std::lock_guard<std::mutex> lock(app.queueMutex_);
                    if(app.activeCoverRequests_.find(id)==app.activeCoverRequests_.end()) {
                        const auto token=app.coverTelemetry_.requested(id);
                        app.activeCoverRequests_[id]={token,1};app.queuedThumbIds_.insert(id);
                        queued.fetch_add(1,std::memory_order_relaxed);added=true;
                    }
                }
                if(!added)std::this_thread::yield();
            }
        });
        std::thread consumer([&] {
            while(finished.load(std::memory_order_relaxed)<total) {
                CoverRequestState request{};bool found=false;
                {
                    std::lock_guard<std::mutex> lock(app.queueMutex_);
                    const auto active=app.activeCoverRequests_.find(id);
                    if(active!=app.activeCoverRequests_.end()){request=active->second;found=true;}
                }
                if(found && app.finishCoverRequest(id,request,CoverState::Loaded,1))
                    finished.fetch_add(1,std::memory_order_relaxed);
                else std::this_thread::yield();
            }
        });
        producer.join();consumer.join();
        const auto stats=app.coverTelemetry_.snapshot();
        assert(queued.load()==total&&finished.load()==total);
        assert(stats.coverRequests==total&&stats.outstandingRequests==0&&stats.epochLoaded<=stats.epochRequested);
    }
    static void runSymlinkHistoryRestore(const fs::path &root) {
        fs::create_directories(root/"books");fs::create_directories(root/"data");fs::create_directories(root/"external");
        const auto target=root/"outside.epub",path=root/"external"/"history.epub";
        fs::copy_file("tests/fixtures/sample_book.epub",target);fs::create_symlink(target,path);
        ProgressStore progress(root/"data"/"progress.txt");Progress entry;entry.bookPath=path.u8string();
        entry.bookTitle="Recovered title";const auto id=documentId(entry.bookPath,fs::file_size(target));
        progress.put(id,entry);assert(progress.save());
        AppSettingsStore settings(root/"data"/"settings.txt");auto preferences=settings.get();preferences.scanOnLaunch=false;
        settings.put(preferences);assert(settings.save());
        PaperNxApp app(root);assert(app.init());
        assert(!app.libraryScanner_.pending() && app.books_.empty());
        assert(app.progressStore_.get(id).bookTitle==entry.bookTitle);
    }
    static void runCoverQueuePriority(const fs::path &root) {
        fs::create_directories(root/"books");
        PaperNxApp app(root);assert(initReady(app));
        app.visibleBooks_.clear();
        for(int i=0;i<24;++i){Book book;book.id="cover-"+std::to_string(i);book.title=book.id;book.format=Format::Epub;app.visibleBooks_.push_back(std::move(book));}
        app.viewMode_=ViewMode::List;app.listScroll_=1120;
        const auto visible=app.visibleBooks_[10].id,stale=app.visibleBooks_[0].id;
        std::uint64_t visibleToken{},staleToken{};
        {std::lock_guard<std::mutex> lock(app.queueMutex_);
            app.coverStates_[visible]=CoverState::Loading;app.coverStates_[stale]=CoverState::Loading;
            app.queuedThumbIds_.insert(visible);app.queuedThumbIds_.insert(stale);
            visibleToken=app.coverTelemetry_.requested(visible);staleToken=app.coverTelemetry_.requested(stale);
            app.activeCoverRequests_[visible]={visibleToken,app.libraryScanGeneration_};
            app.activeCoverRequests_[stale]={staleToken,app.libraryScanGeneration_};}
        {std::lock_guard<std::mutex> lock(app.completedMutex_);
            app.completedThumbs_.push_back({visible,1,1,3,{1,2,3},CoverState::Loaded,visibleToken,app.libraryScanGeneration_});
            app.completedThumbs_.push_back({stale,1,1,3,{4,5,6},CoverState::Loaded,staleToken,app.libraryScanGeneration_});}
        app.processCompletedThumbnails();
        assert(app.thumbCache_.get("thumb_"+visible) && !app.thumbCache_.get("thumb_"+stale));
        {std::lock_guard<std::mutex> lock(app.queueMutex_);assert(app.coverStates_[stale]==CoverState::NotRequested && app.coverStates_[visible]==CoverState::Loaded);assert(!app.queuedThumbIds_.count(visible) && !app.queuedThumbIds_.count(stale));}

        app.books_.clear();
        for(int i=0;i<24;++i){const auto number=std::to_string(i);Book book;book.id="cover"+std::string(11-number.size(),'0')+number;book.path=(root/("book_"+number+".epub")).u8string();book.title="Book "+std::string(2-number.size(),'0')+number;book.format=Format::Epub;book.bytes=100;book.modified=1;app.books_.push_back(std::move(book));}
        app.rebuildLibraryLookup();app.sort_=Sort::Title;app.updateVisibleBooks();app.viewMode_=ViewMode::List;app.listScroll_=1120;
        const auto newlyVisible=app.books_.front().id;
        assert(app.visibleBooks_.front().id==newlyVisible);
        std::uint64_t newlyVisibleToken{};
        {std::lock_guard<std::mutex> lock(app.queueMutex_);
            newlyVisibleToken=app.coverTelemetry_.requested(newlyVisible);
            app.queuedThumbIds_.insert(newlyVisible);
            app.activeCoverRequests_[newlyVisible]={newlyVisibleToken,app.libraryScanGeneration_};}
        {std::lock_guard<std::mutex> lock(app.completedMutex_);
            app.completedMetadata_.push_back({newlyVisible,"Book 12","","","",PaperNxApp::bookMetadataFingerprint(app.books_.front())});
            app.completedThumbs_.push_back({newlyVisible,1,1,3,{7,8,9},CoverState::Loaded,newlyVisibleToken,app.libraryScanGeneration_});}
        app.processCompletedThumbnails();
        const auto moved=std::find_if(app.visibleBooks_.begin(),app.visibleBooks_.end(),[&](const Book &book){return book.id==newlyVisible;});
        assert(moved!=app.visibleBooks_.end() && moved-app.visibleBooks_.begin()>=10);
        assert(app.thumbCache_.get("thumb_"+newlyVisible));
    }
    static void runMetadataIndexRetry(const fs::path &root) {
        fs::create_directories(root/"books");
        PaperNxApp app(root);assert(initReady(app));
        Book book;book.id="eeeeeeeeeeeeeeee";book.path=(root/"books"/"missing.epub").u8string();book.title="Retry";book.format=Format::Epub;book.bytes=123;book.modified=1;
        app.books_.push_back(book);app.rebuildLibraryLookup();
        const auto fingerprint=PaperNxApp::bookMetadataFingerprint(book);
        const auto key=book.id+":"+fingerprint;
        app.scheduleMetadataIndex();
        const auto deadline=SDL_GetTicks()+10000;
        bool released=false;
        while(SDL_GetTicks()<deadline) {
            {std::lock_guard<std::mutex> lock(app.queueMutex_);released=!app.metadataIndexedIds_.count(key);}
            if(released)break;
            SDL_Delay(2);
        }
        assert(released);
        const auto validPath=root/"books"/"valid.epub";
        fs::copy_file("tests/fixtures/sample_book.epub",validPath);
        app.books_.front().path=validPath.u8string();app.scheduleMetadataIndex();
        const auto retryDeadline=SDL_GetTicks()+10000;
        while(SDL_GetTicks()<retryDeadline) {
            app.processCompletedThumbnails();
            if(app.catalog_.get(book.id).metadataFingerprint==fingerprint)break;
            SDL_Delay(2);
        }
        assert(app.catalog_.get(book.id).metadataFingerprint==fingerprint);
        app.books_.front().modified=2;
        const auto updatedFingerprint=PaperNxApp::bookMetadataFingerprint(app.books_.front());
        {std::lock_guard<std::mutex> lock(app.completedMutex_);app.completedMetadata_.push_back({book.id,"Stale title","","","",fingerprint});}
        app.processCompletedThumbnails();
        assert(app.catalog_.get(book.id).metadataFingerprint==fingerprint && app.catalog_.get(book.id).sourceTitle!="Stale title");
        app.scheduleMetadataIndex();
        const auto refreshDeadline=SDL_GetTicks()+10000;
        while(SDL_GetTicks()<refreshDeadline) {
            app.processCompletedThumbnails();
            if(app.catalog_.get(book.id).metadataFingerprint==updatedFingerprint)break;
            SDL_Delay(2);
        }
        assert(app.catalog_.get(book.id).metadataFingerprint==updatedFingerprint);
    }
};
}
int main() {
    assert(shouldProcessCoverMetadata(false,true,0));
    assert(!shouldProcessCoverMetadata(true,false,9));
    assert(!shouldProcessCoverMetadata(true,true,0));
    assert(!shouldProcessCoverMetadata(true,true,2));
    assert(shouldProcessCoverMetadata(true,true,3));
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    const auto root=fs::temp_directory_path()/"papernx-modern-library";fs::remove_all(root);fs::create_directories(root/"books");fs::create_directories(root/"import"/"nested");
    fs::copy_file("tests/fixtures/sample_book.epub",root/"books"/"base.epub");
    fs::copy_file("tests/fixtures/hebrew_navigation.epub",root/"import"/"first.epub");
    fs::copy_file("tests/fixtures/sample_doc.pdf",root/"import"/"second.pdf");
    fs::copy_file("tests/fixtures/comics/navigation.cbz",root/"import"/"nested"/"third.cbz");
    fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");
    AppTest::run(root);const auto historyRoot=fs::temp_directory_path()/"papernx-history-restore";fs::remove_all(historyRoot);
    AppTest::runHistoryRestore(historyRoot);fs::remove_all(historyRoot);
    const auto staleRoot=fs::temp_directory_path()/"papernx-stale-library-result";fs::remove_all(staleRoot);
    AppTest::runStaleLibraryResult(staleRoot);fs::remove_all(staleRoot);
    AppTest::runCoverTelemetryHandoff();
    const auto symlinkHistoryRoot=fs::temp_directory_path()/"papernx-symlink-history";fs::remove_all(symlinkHistoryRoot);
    AppTest::runSymlinkHistoryRestore(symlinkHistoryRoot);fs::remove_all(symlinkHistoryRoot);
    const auto coverRoot=fs::temp_directory_path()/"papernx-cover-queue-priority";fs::remove_all(coverRoot);
    AppTest::runCoverQueuePriority(coverRoot);fs::remove_all(coverRoot);
    const auto metadataRoot=fs::temp_directory_path()/"papernx-metadata-index-retry";fs::remove_all(metadataRoot);
    AppTest::runMetadataIndexRetry(metadataRoot);fs::remove_all(metadataRoot);fs::remove_all(root);TTF_Quit();SDL_Quit();
}
