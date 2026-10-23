#define main papernx_application_entry
#include "../src/main.cpp"
#undef main
#include <cassert>
#include <iostream>
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
  app.currentPage_=app.pageCount_>30?18:4;const int original=app.currentPage_;
  app.toolbarVisible_=false;app.setPdfView(0);app.render();app.configureComicPages();
  auto builds=app.comicTextureBuilds_;auto start=SDL_GetTicks();
  app.prefetchPage_=original;app.prefetchStep_=0;
  for(int i=0;i<4;++i){app.prefetchDue_=0;app.idlePrefetch();app.toolbarVisible_=!app.toolbarVisible_;app.render();}
  std::cout<<"Four idle/toolbar cycles: "<<SDL_GetTicks()-start<<" ms; UI rasterizations="<<app.comicTextureBuilds_-builds<<std::endl;
  assert(app.comicTextureBuilds_==builds); // Old 0.17.1 performed 12 here.
  drain(app);
  for(int page=original;page<original+4;++page)assert(app.pageCache_.get(app.comicTextureKey(page)));
  assert(app.pageCache_.currentBytes()<36u*1024u*1024u);
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
  app.comicDetailDue_=0;app.refineComicZoom();drain(app);assert(app.comicDetails_.size()==2);
  for(int i=0;i<2;++i){const int page=app.currentPage_+i;int bw,bh,dw,dh;
   auto base=app.pageCache_.get(app.comicTextureKey(page));assert(base);
   SDL_QueryTexture(base,nullptr,nullptr,&bw,&bh);SDL_QueryTexture(app.comicDetails_.at(page).texture.get(),nullptr,nullptr,&dw,&dh);
   assert(dw>bw && dh>bh);
  }
  for(int i=0;i<10;++i){app.zoomPdf(2+i*.1f,.6f,.4f,true);app.pdfPanY_+=3;app.render();}
  assert(app.comicTextureBuilds_==builds);
  app.closeBook();assert(app.comicDetails_.empty());drain(app);
  // Configuration replacement discards old queued/completed data. Stop wakes
  // a producer blocked by the two-result back-pressure limit as well.
  ComicPages worker;const auto path=app.books_.front().path;
  worker.configure({"old",path});for(int i=0;i<6;++i)worker.request(i);SDL_Delay(20);
  worker.configure({"new",path});worker.request(2);bool received=false;const auto deadline=SDL_GetTicks()+20000;
  while(worker.pending() && SDL_GetTicks()<deadline){for(auto &r:worker.take()){assert(r.identity=="new" && r.page==2 && !r.rgb.empty());received=true;}SDL_Delay(2);}
  assert(received && !worker.pending());for(int i=0;i<6;++i)worker.request(i);SDL_Delay(20);worker.stop();
  std::cout<<"PASS: bounded base/detail memory; zero hot-path rasterizations; asynchronous CBR/CBZ neighbours; preserved two-page zoom; stale-result cancellation and stop\n";
 }
};
}
int main(int argc,char **argv){assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_GAMECONTROLLER)==0 && TTF_Init()==0);auto root=fs::temp_directory_path()/(argc>1?"papernx-comic-performance-rar":"papernx-comic-performance-zip");fs::remove_all(root);fs::create_directories(root/"books");fs::path source=argc>1?argv[1]:"tests/fixtures/comics/high-resolution.cbz";fs::copy_file(source,root/"books"/source.filename());fs::create_directory_symlink(fs::absolute("fonts"),root/"fonts");AppTest::run(root);fs::remove_all(root);TTF_Quit();SDL_Quit();}
