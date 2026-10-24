#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <locale>

namespace {
using Clock=std::chrono::steady_clock;
double elapsedMs(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
void jsonString(std::ostream &out,const std::string &value) {
    out<<'"';
    for(const unsigned char c:value) {
        if(c=='"' || c=='\\')out<<'\\'<<char(c);
        else if(c>=0x20 && c<0x7f)out<<char(c);
        else out<<'?';
    }
    out<<'"';
}

std::uint64_t peakRssKb() {
#if defined(__linux__)
    std::ifstream status("/proc/self/status");std::string key;std::uint64_t value{};std::string unit;
    while(status>>key) { if(key=="VmHWM:"){status>>value>>unit;return value;}std::string rest;std::getline(status,rest); }
#endif
    return 0;
}

struct AppTest {
    static void waitForCovers(PaperNxApp &app,std::uint32_t timeoutMs=30000) {
        const auto end=SDL_GetTicks()+timeoutMs;
        do {
            app.processCompletedThumbnails();app.render();
            bool active;
            {std::lock_guard<std::mutex> lock(app.queueMutex_);active=!app.activeCoverRequests_.empty();}
            if(!active) {
                std::lock_guard<std::mutex> lock(app.completedMutex_);
                if(app.completedThumbs_.empty())return;
            }
            SDL_Delay(1);
        } while(SDL_GetTicks()<end);
        assert(false && "cover worker timed out");
    }
    static std::string rendererName(PaperNxApp &app) {
        SDL_RendererInfo info{};
        return SDL_GetRendererInfo(app.renderer_,&info)==0 && info.name ? info.name : "unknown";
    }
    static std::size_t visibleRows(PaperNxApp &app) {
        int width{},height{};app.getWindowSize(width,height);ui::LibraryLayout layout(width);
        std::size_t count{};
        for(std::size_t i=0;i<app.visibleBooks_.size();++i) {
            const int y=layout.top+int(i)*112-int(app.listScroll_);
            if(y>=height-28)break;
            if(y+104>layout.top)++count;
        }
        return count;
    }
    static std::size_t visibleTextures(PaperNxApp &app,std::size_t count) {
        std::size_t loaded{};
        for(std::size_t i=0;i<std::min(count,app.visibleBooks_.size());++i)
            if(app.thumbCache_.get("thumb_"+app.visibleBooks_[i].id))++loaded;
        return loaded;
    }
    static void run(const fs::path &appRoot,const fs::path &dataset,const std::string &label,
                    const std::string &runType,const fs::path &resultPath) {
        fs::create_directories(appRoot/"books");fs::create_directories(appRoot/"data");
        std::error_code ec;fs::create_directory_symlink(fs::absolute("fonts"),appRoot/"fonts",ec);
        AppSettingsStore settings(appRoot/"data"/"settings.txt");settings.load();
        auto preferences=settings.get();preferences.scanOnLaunch=true;preferences.libraryLayout=1;
        preferences.libraryFolders={dataset.u8string()};settings.put(preferences);assert(settings.save());
        {std::ofstream flag(appRoot/"data"/"cover-perf.flag");flag<<label<<"\n"<<runType<<"\n";assert(flag.good());}

        double initMs{},scanMs{},startupMs{},firstViewportMs{},fastScrollMs{},fullSweepMs{},categoryMs{},openReturnMs{},cacheReloadMs{};
        std::size_t initialViewportBooks{},initialViewportTextures{};
        std::string firstViewportTelemetry,finalTelemetry,renderer;
        std::uint64_t rssKb{};
        {
            PaperNxApp app(appRoot);
            const auto appStart=Clock::now();auto start=appStart;assert(app.init());initMs=elapsedMs(start);
            app.render();startupMs=elapsedMs(appStart);
            start=Clock::now();
            const auto scanDeadline=SDL_GetTicks()+120000;
            while(app.libraryScanner_.pending() && SDL_GetTicks()<scanDeadline){app.processLibraryScan();SDL_Delay(1);}
            assert(!app.libraryScanner_.pending());scanMs=elapsedMs(start);
            assert(app.books_.size()==std::size_t(std::stoul(label.substr(label.find_last_of('-')+1))));
            app.screen_=ScreenState::Library;app.viewMode_=ViewMode::List;app.listScroll_=0;
            const auto metadataDeadline=SDL_GetTicks()+120000;
            for(;;) {
                app.processCompletedThumbnails();
                bool workerIdle;
                {std::lock_guard<std::mutex> lock(app.queueMutex_);workerIdle=app.metadataQueue_.empty() && app.metadataJobsInFlight_==0;}
                bool uiIdle;
                {std::lock_guard<std::mutex> lock(app.completedMutex_);uiIdle=app.completedMetadata_.empty();}
                if(workerIdle && uiIdle)break;
                assert(SDL_GetTicks()<metadataDeadline);SDL_Delay(1);
            }
            app.render();

            initialViewportBooks=visibleRows(app);
            start=Clock::now();waitForCovers(app);firstViewportMs=elapsedMs(start);
            initialViewportTextures=visibleTextures(app,initialViewportBooks);
            renderer=rendererName(app);
            auto stats=app.coverPipelineStats();
            firstViewportTelemetry=app.coverTelemetry_.toJson(stats,APP_VERSION_FULL,PAPERNX_SOURCE_COMMIT,renderer,
                PAPERNX_SOURCE_DIRTY,PAPERNX_SOURCE_TREE_SHA256,PAPERNX_SOURCE_IDENTITY);

            start=Clock::now();
            const int maxScroll=int(app.maxUiScroll());
            for(int step=1;step<=48;++step) {
                app.listScroll_=float(maxScroll*step/48);app.render();app.processCompletedThumbnails();SDL_Delay(1);
            }
            for(int step=47;step>=0;--step) {
                app.listScroll_=float(maxScroll*step/48);app.render();app.processCompletedThumbnails();SDL_Delay(1);
            }
            waitForCovers(app);fastScrollMs=elapsedMs(start);

            start=Clock::now();
            const std::size_t viewportStep=std::max<std::size_t>(1,initialViewportBooks);
            for(std::size_t first=0;first<app.visibleBooks_.size();first+=viewportStep) {
                app.listScroll_=float(first*112);app.render();waitForCovers(app);
            }
            fullSweepMs=elapsedMs(start);

            start=Clock::now();
            for(const auto category:{Category::Recent,Category::Unread,Category::All}) {
                app.category_=category;app.updateVisibleBooks();app.listScroll_=0;app.render();waitForCovers(app,15000);
            }
            categoryMs=elapsedMs(start);

            const auto book=std::find_if(app.books_.begin(),app.books_.end(),[](const Book &candidate){return candidate.format==Format::Epub || candidate.format==Format::Pdf;});
            assert(book!=app.books_.end());start=Clock::now();app.openBook(*book);app.render();app.closeBook();
            app.screen_=ScreenState::Library;app.coversPaused_=false;app.listScroll_=0;app.render();waitForCovers(app,30000);
            openReturnMs=elapsedMs(start);

            start=Clock::now();app.requestCoverCacheClear();
            const auto clearDeadline=SDL_GetTicks()+120000;
            while(app.coverClearStatus_.load()==1 && SDL_GetTicks()<clearDeadline)SDL_Delay(2);
            assert(app.coverClearStatus_.load()==2);app.listScroll_=0;app.render();waitForCovers(app,30000);
            cacheReloadMs=elapsedMs(start);

            app.exportCoverPerf();
            std::ifstream telemetryFile(appRoot/"logs"/"cover-performance.json",std::ios::binary);
            assert(telemetryFile.good());finalTelemetry.assign(std::istreambuf_iterator<char>(telemetryFile),{});
            rssKb=peakRssKb();
        }

        std::ostringstream json;json.imbue(std::locale::classic());json<<std::setprecision(9);
        json<<"{\"schema\":";jsonString(json,"papernx-cover-app-benchmark-1");
        json<<",\"dataset\":";jsonString(json,label);json<<",\"runType\":";jsonString(json,runType);
        json<<",\"renderer\":";jsonString(json,renderer);json<<",\"sampleCount\":"<<initialViewportBooks
            <<",\"initMs\":"<<initMs<<",\"libraryScanMs\":"<<scanMs<<",\"startupToFirstFrameMs\":"<<startupMs
            <<",\"firstViewportReadyMs\":"<<firstViewportMs<<",\"initialViewportTextures\":"<<initialViewportTextures
            <<",\"fastScrollMs\":"<<fastScrollMs<<",\"fullLibrarySweepMs\":"<<fullSweepMs<<",\"categoryChangeMs\":"<<categoryMs
            <<",\"openReturnMs\":"<<openReturnMs<<",\"cacheClearReloadMs\":"<<cacheReloadMs
            <<",\"peakRssKb\":"<<rssKb<<",\"firstViewportTelemetry\":"<<firstViewportTelemetry
            <<",\"finalTelemetry\":"<<finalTelemetry<<"}\n";
        fs::create_directories(resultPath.parent_path());std::ofstream out(resultPath,std::ios::binary);
        out<<json.str();assert(out.good());
    }
};
}

int main(int argc,char **argv) {
    assert(argc==6);
    assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);
    AppTest::run(fs::u8path(argv[1]),fs::u8path(argv[2]),argv[3],argv[4],fs::u8path(argv[5]));
    TTF_Quit();SDL_Quit();return 0;
}
