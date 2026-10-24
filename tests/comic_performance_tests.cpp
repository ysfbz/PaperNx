#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
namespace {
struct AppTest {
#include "library_fixture_helpers.inc"
 static void drain(PaperNxApp &app) {
  const auto deadline=SDL_GetTicks()+20000;
  while(app.comicPages_.pending() && SDL_GetTicks()<deadline){app.processComicPages();SDL_Delay(2);}
  app.processComicPages();assert(!app.comicPages_.pending());
 }
 static void run(const fs::path &root) {
  PaperNxApp app(root);assert(initReady(app));app.openBook(app.books_.front());assert(app.comic());
  SDL_RendererInfo info{};SDL_GetRendererInfo(app.renderer_,&info);
  std::cout<<"Renderer: "<<info.name<<"; pages="<<app.pageCount_<<"; bytes="<<fs::file_size(app.books_.front().path)<<std::endl;
  assert(app.pageCount_>=3);
  app.currentPage_=app.pageCount_>30?18:std::min(4,app.pageCount_-3);const int original=app.currentPage_;
  app.toolbarVisible_=false;app.setPdfView(0);
  auto builds=app.comicTextureBuilds_;app.render();
  assert(app.comicTextureBuilds_==builds && app.comicPages_.pending());
  drain(app);
  assert(app.pageCache_.get(app.comicTextureKey(original)) && app.comicTextureBuilds_==builds);
  app.extraStyle_.readingMode=1;app.pageCache_.clear();app.render();
  assert(app.comicTextureBuilds_==builds && app.comicPages_.pending());drain(app);
  assert(app.pageCache_.get(app.comicTextureKey(original)) && app.comicTextureBuilds_==builds);
  app.extraStyle_.readingMode=0;app.pageCache_.clear();app.render();drain(app);
  assert(app.pageCache_.get(app.comicTextureKey(original)) && app.comicTextureBuilds_==builds);
  app.configureComicPages();auto start=SDL_GetTicks();builds=app.comicTextureBuilds_;
  app.prefetchPage_=original;app.prefetchStep_=0;
  for(int i=0;i<4;++i){app.prefetchDue_=0;app.idlePrefetch();app.toolbarVisible_=!app.toolbarVisible_;app.render();}
  std::cout<<"Four idle/toolbar cycles: "<<SDL_GetTicks()-start<<" ms; UI rasterizations="<<app.comicTextureBuilds_-builds<<std::endl;
  assert(app.comicTextureBuilds_==builds); // Old 0.17.1 performed 12 here.
  drain(app);
  const auto visibleGroup=app.comicSpreadPages(original);
  for(int page=visibleGroup.first;page<visibleGroup.first+visibleGroup.second+2 && page<app.pageCount_;++page)
   assert(app.pageCache_.get(app.comicTextureKey(page)));
  assert(app.pageCache_.currentBytes()<36u*1024u*1024u);
  // A large fling must stop at the conservative viewport boundary while a
  // cold comic page's geometry is still being decoded by the worker.
  if(app.pageCount_>30) {
   constexpr int coldPage=30;
   float coldWidth=0,coldHeight=0;
   assert(!px_document_known_comic_size(app.doc_,coldPage,&coldWidth,&coldHeight));
   app.comicPages_.setSuspended(true);app.comicPages_.clear();app.comicPagesIdentity_.clear();
   app.currentPage_=coldPage;app.scrollY_=0;app.scrollVelocity_=0;app.extraStyle_.readingMode=1;
   const float fallback=app.segmentHeight(coldPage);app.scrollBy(fallback*3);
   assert(app.currentPage_==coldPage && app.scrollY_==fallback && app.scrollVelocity_==0);
   app.comicPages_.setSuspended(false);const auto geometryDeadline=SDL_GetTicks()+20000;
   while(!app.readerPageSize(coldPage,&coldWidth,&coldHeight) && SDL_GetTicks()<geometryDeadline) {
    app.processComicPages();SDL_Delay(2);
   }
   assert(coldWidth>0 && coldHeight>0);
   app.comicPages_.setSuspended(true);app.comicPages_.clear();app.comicPagesIdentity_.clear();
   app.currentPage_=original;app.scrollY_=0;app.extraStyle_.readingMode=0;app.pageCache_.clear();
   app.configureComicPages();app.comicPages_.setSuspended(false);
  }
  // Cold controller and touch turns wait for the worker instead of rasterizing
  // the target synchronously or saving a position before the page is ready.
  auto settings=app.appSettings_.get();settings.comicLandscapeSpreads=false;settings.pageCurlAnimation=true;app.appSettings_.put(settings);
  app.cancelPageTurn();app.zoomPdf(1);app.pdfPanX_=app.pdfPanY_=0;app.comicPages_.setSuspended(true);
  app.pageCache_.clear();app.currentPage_=original;app.render();
  const int anchor=app.currentPage_,savedPage=app.progressStore_.get(app.currentBookId_).page;
  const int controllerTarget=app.readerTurnTarget(anchor,1);assert(!app.pageCache_.get(app.comicTextureKey(controllerTarget)));
  builds=app.comicTextureBuilds_;app.turnPage(1);assert(app.currentPage_==anchor && app.comicPendingTurn_.active);
  app.turnPage(1);app.turnPage(-1);assert(app.comicPendingTurn_.steps==1 && app.currentPage_==anchor);
  assert(app.progressStore_.get(app.currentBookId_).page==savedPage && app.comicTextureBuilds_==builds);
  app.comicPages_.setSuspended(false);auto coldDeadline=SDL_GetTicks()+30000;
  while((app.comicPendingTurn_.active || app.comicPages_.pending()) && SDL_GetTicks()<coldDeadline) {
   app.processComicPages();SDL_Delay(2);
  }
  app.processComicPages();assert(!app.comicPendingTurn_.active && app.currentPage_==controllerTarget);
  assert(app.pageTurnAnimation_.active && app.comicTextureBuilds_==builds);
  assert(app.progressStore_.get(app.currentBookId_).page==controllerTarget);
  app.pageTurnAnimation_.started=SDL_GetTicks()-2000;app.tickPageTurn();

  app.comicPages_.setSuspended(true);app.pageCache_.clear();app.currentPage_=controllerTarget;app.render();
  builds=app.comicTextureBuilds_;
  const int touchAnchor=app.currentPage_,touchTarget=app.readerTurnTarget(touchAnchor,1);
  assert(!app.pageCache_.get(app.comicTextureKey(touchTarget)));
  const int touchSaved=app.progressStore_.get(app.currentBookId_).page;
  app.beginPageTurnTouch(.9f,.5f,91);assert(app.movePageTurnTouch(.4f,.5f,91));
  assert(app.pageTurnTouch_.waitingForTexture && app.currentPage_==touchAnchor);
  assert(app.releasePageTurnTouch(.4f,.5f,91) && app.currentPage_==touchAnchor && app.comicPendingTurn_.active);
  assert(app.progressStore_.get(app.currentBookId_).page==touchSaved);
  app.comicPages_.setSuspended(false);coldDeadline=SDL_GetTicks()+30000;
  while((app.comicPendingTurn_.active || app.comicPages_.pending()) && SDL_GetTicks()<coldDeadline) {
   app.processComicPages();SDL_Delay(2);
  }
  app.processComicPages();assert(!app.comicPendingTurn_.active && app.currentPage_==touchTarget);
  assert(app.pageTurnAnimation_.active && app.comicTextureBuilds_==builds);
  app.pageTurnAnimation_.started=SDL_GetTicks()-2000;app.tickPageTurn();
  app.comicPages_.setSuspended(true);app.pageCache_.clear();app.currentPage_=original;app.render();
  auto currentBook=std::find_if(app.books_.begin(),app.books_.end(),[&](const Book &book){return book.id==app.currentBookId_;});
  assert(currentBook!=app.books_.end());const auto archivePath=currentBook->path;
  const int failedAnchor=app.currentPage_,failedProgress=app.progressStore_.get(app.currentBookId_).page;
  currentBook->path=archivePath+".missing";app.comicPages_.clear();app.comicPagesIdentity_.clear();app.configureComicPages();
  app.turnPage(1);assert(app.comicPendingTurn_.active && app.currentPage_==failedAnchor);
  app.comicPages_.setSuspended(false);coldDeadline=SDL_GetTicks()+5000;
  while((app.comicPendingTurn_.active || app.comicPages_.pending()) && SDL_GetTicks()<coldDeadline) {
   app.processComicPages();SDL_Delay(2);
  }
  app.processComicPages();assert(!app.comicPendingTurn_.active && app.currentPage_==failedAnchor);
  assert(app.progressStore_.get(app.currentBookId_).page==failedProgress && !app.comicTurnError_.empty() && !app.pageRenderError_.empty());
  app.render();assert(!app.comicPages_.pending()); // A failed visible page observes the retry delay instead of reopening each frame.
  currentBook->path=archivePath;app.comicPages_.clear();app.comicPagesIdentity_.clear();app.configureComicPages();
  app.currentPage_=original;app.saveCurrentPosition();app.render();
  app.ensureComicGroup(app.readerTurnTarget(original,1));drain(app);
  // Once both neighbours are ready, rendering and a controller turn perform no
  // archive IO/rasterization on the UI thread, even with large native scans.
  const fs::path archive=app.books_.front().path,held=archive.string()+".held";
  fs::rename(archive,held); // A warm turn must not reopen even a missing archive.
  builds=app.comicTextureBuilds_;app.turnPage(1);assert(app.pageTurnAnimation_.active && app.pageTurnAnimation_.comicSheet);
  auto &turn=app.pageTurnAnimation_;turn.dragging=true;start=SDL_GetTicks();
  for(int frame=0;frame<30;++frame){turn.progress=frame/29.f;app.render();}
  std::cout<<"30 comic curl frames: "<<SDL_GetTicks()-start<<" ms; UI rasterizations="<<app.comicTextureBuilds_-builds<<std::endl;
  assert(app.comicTextureBuilds_==builds);turn.dragging=false;turn.started=SDL_GetTicks()-2000;app.tickPageTurn();
  app.toolbarVisible_=false;app.zoomPdf(3,.5f,.5f,true);start=SDL_GetTicks();
  for(int i=0;i<30;++i){app.pdfPanX_=100+i*3;app.pdfPanY_=100+i*2;app.render();}
  std::cout<<"30 spread pan frames: "<<SDL_GetTicks()-start<<" ms; UI rasterizations="<<app.comicTextureBuilds_-builds<<std::endl;
  assert(app.comicTextureBuilds_==builds);
  fs::rename(held,archive);
  // High zoom sharpens both physical pages asynchronously, without evicting
  // their base images or requiring a decode for each pan/pinch location.
  app.comicDetailDue_=0;app.refineComicZoom();drain(app);
  const auto detailGroup=app.comicSpreadPages(app.currentPage_);
  int limit=std::min(info.max_texture_width>0?info.max_texture_width:3072,
                     info.max_texture_height>0?info.max_texture_height:3072);
  std::size_t expectedDetails=0;
  for(int i=0;i<detailGroup.second;++i) {
   const int page=detailGroup.first+i;float nw=0,nh=0;
   assert(px_document_page_size(app.doc_,page,&nw,&nh));
   const auto baseSize=ComicResolution::size(nw,nh,false,limit);
   const auto detailSize=ComicResolution::size(nw,nh,true,limit);
   if(detailSize==baseSize){assert(!app.comicDetails_.count(page));continue;}
   ++expectedDetails;int bw,bh,dw,dh;auto base=app.pageCache_.get(app.comicTextureKey(page));assert(base);
   SDL_QueryTexture(base,nullptr,nullptr,&bw,&bh);SDL_QueryTexture(app.comicDetails_.at(page).texture.get(),nullptr,nullptr,&dw,&dh);
   assert(dw>bw && dh>bh);
  }
  assert(app.comicDetails_.size()==expectedDetails);
  for(int i=0;i<10;++i){app.zoomPdf(2+i*.1f,.6f,.4f,true);app.pdfPanY_+=3;app.render();}
  assert(app.comicTextureBuilds_==builds);
  app.closeBook();assert(app.comicDetails_.empty());drain(app);
  const auto path=app.books_.front().path;
  if(app.pageCount_>=7) {
   // A visible request displaces speculative work when the bounded queue is full.
   ComicPages priorityWorker;
   priorityWorker.configure({"priority",path});priorityWorker.setSuspended(true);
   for(int page=0;page<6;++page)priorityWorker.request(page,false,ComicPages::Priority::SpeculativeForward);
   priorityWorker.request(6,false,ComicPages::Priority::Visible);priorityWorker.setSuspended(false);
   std::vector<int> priorityOrder;const auto priorityDeadline=SDL_GetTicks()+20000;
   while(priorityWorker.pending() && SDL_GetTicks()<priorityDeadline) {
    for(auto &result:priorityWorker.take()) {
     assert(result.identity=="priority" && !result.rgb.empty());priorityOrder.push_back(result.page);
    }
    SDL_Delay(2);
   }
   assert(priorityOrder.size()==6 && priorityOrder.front()==6 &&
          std::find(priorityOrder.begin(),priorityOrder.end(),5)==priorityOrder.end());
   priorityWorker.stop();assert(priorityWorker.metrics().documentOpens==1 && priorityWorker.metrics().documentCloses==1);
  }
  // Configuration replacement discards old results and closes its worker-owned
  // document. The open document then survives idle longer than the old timeout.
  const int lifecyclePage=std::min(2,app.pageCount_-1);
  ComicPages configWorker;
  const ComicPages::Spec largeSpec{"same-identity",path,false,0xffffff,0,3072};
  const auto waitConfigPage=[&] {
   ComicPages::Result result;bool received=false;const auto deadline=SDL_GetTicks()+20000;
   while(!received && SDL_GetTicks()<deadline) {
    for(auto &item:configWorker.take()) {assert(item.identity==largeSpec.identity);result=std::move(item);received=true;}
    if(!received)SDL_Delay(2);
   }
   assert(received && !result.rgb.empty());return result;
  };
  configWorker.configure(largeSpec);configWorker.request(lifecyclePage,ComicPages::Priority::Visible);
  const auto largeResult=waitConfigPage();assert(std::max(largeResult.width,largeResult.height)>1);
  configWorker.configure(largeSpec);configWorker.request(lifecyclePage,ComicPages::Priority::Visible);
  const auto repeatedResult=waitConfigPage();assert(repeatedResult.width==largeResult.width && repeatedResult.height==largeResult.height);
  assert(configWorker.metrics().documentOpens==1 && configWorker.metrics().documentCloses==0);
  auto smallSpec=largeSpec;smallSpec.textureLimit=1;
  configWorker.configure(smallSpec);configWorker.request(lifecyclePage,ComicPages::Priority::Visible);
  const auto limitedResult=waitConfigPage();assert(limitedResult.width==1 && limitedResult.height==1);
  assert(configWorker.metrics().documentOpens==2 && configWorker.metrics().documentCloses==1);
  configWorker.stop();assert(configWorker.metrics().documentOpens==2 && configWorker.metrics().documentCloses==2);

  ComicPages worker;
  worker.configure({"old",path});for(int i=0;i<std::min(6,app.pageCount_);++i)worker.request(i);
  const auto openDeadline=SDL_GetTicks()+5000;
  while(worker.metrics().documentOpens==0 && SDL_GetTicks()<openDeadline)SDL_Delay(1);
  assert(worker.metrics().documentOpens==1);
  worker.configure({"new",path});worker.request(lifecyclePage);bool received=false;const auto deadline=SDL_GetTicks()+20000;
  while(worker.pending() && SDL_GetTicks()<deadline){for(auto &r:worker.take()){assert(r.identity=="new" && r.page==lifecyclePage && !r.rgb.empty());received=true;}SDL_Delay(2);}
  assert(received && !worker.pending());auto metrics=worker.metrics();assert(metrics.documentOpens==2 && metrics.documentCloses==1);
  const auto entryLoads=metrics.pageEntryLoads;assert(entryLoads>0);
  worker.request(lifecyclePage);received=false;const auto rereadDeadline=SDL_GetTicks()+20000;
  while(worker.pending() && SDL_GetTicks()<rereadDeadline){for(auto &r:worker.take()){assert(r.identity=="new" && r.page==lifecyclePage && !r.rgb.empty());received=true;}SDL_Delay(2);}
  metrics=worker.metrics();assert(received && !worker.pending() && metrics.pageEntryLoads==entryLoads);
  int idleSeconds=0;
  for(const int interval:{1,2,7,20}) {
   std::this_thread::sleep_for(std::chrono::seconds(interval));idleSeconds+=interval;
   metrics=worker.metrics();assert(metrics.documentOpens==2 && metrics.documentCloses==1);
   std::cout<<"Worker idle "<<idleSeconds<<"s: opens="<<metrics.documentOpens<<", closes="<<metrics.documentCloses<<std::endl;
  }
  worker.setSuspended(true);const auto closeDeadline=SDL_GetTicks()+5000;
  while(worker.metrics().documentCloses<2 && SDL_GetTicks()<closeDeadline)SDL_Delay(1);
  assert(worker.metrics().documentCloses==2);
  worker.setSuspended(false);worker.request(lifecyclePage);received=false;const auto resumeDeadline=SDL_GetTicks()+20000;
  while(worker.pending() && SDL_GetTicks()<resumeDeadline){for(auto &r:worker.take()){assert(r.identity=="new" && r.page==lifecyclePage && !r.rgb.empty());received=true;}SDL_Delay(2);}
  assert(received && worker.metrics().documentOpens==3 && worker.metrics().documentCloses==2);
  worker.stop();metrics=worker.metrics();assert(metrics.documentOpens==3 && metrics.documentCloses==3);
  worker.request(0);assert(worker.metrics().documentOpens==3);
  std::cout<<"Worker documents: opens="<<metrics.documentOpens<<", closes="<<metrics.documentCloses
           <<"; page entry loads="<<metrics.pageEntryLoads<<", archive cursor opens/reopens="
           <<metrics.archiveCursorOpens<<"/"<<metrics.archiveCursorReopens
           <<", skipped entries="<<metrics.skippedArchiveEntries<<"; idle session retained and suspend released it\n";
  std::cout<<"PASS: bounded base/detail memory; zero hot-path rasterizations; asynchronous CBR/CBZ neighbours; preserved two-page zoom; stale-result cancellation; idle and suspend lifecycle\n";
 }
};
}
int main(int argc,char **argv){assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);auto root=fs::temp_directory_path()/(argc>1?"papernx-comic-performance-rar":"papernx-comic-performance-zip");fs::remove_all(root);fs::create_directories(root/"books");fs::path source=argc>1?argv[1]:"tests/fixtures/comics/high-resolution.cbz";fs::copy_file(source,root/"books"/source.filename());fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();}
