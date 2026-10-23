#include "papernx/self_update.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace papernx;
namespace fs=std::filesystem;
static std::string read(const fs::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
static UpdateStatus wait(SelfUpdater &updater){
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(std::chrono::steady_clock::now()<end){auto s=updater.status();if(s.state!=UpdateState::Checking&&s.state!=UpdateState::Downloading)return s;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    assert(false && "Updater timed out");return {};
}
static void unit(){
    assert(updateVersionNewer("0.13.0","0.12.0-reader.4"));
    assert(updateVersionNewer("0.13.1","0.13.0"));
    assert(updateVersionNewer("0.14.0","0.13.99"));
    assert(!updateVersionNewer("0.12.1","0.13.0"));
    for(auto p:{std::pair<const char*,const char*>{"v0.12.0-reader.10","0.12.0-reader.9"},{"0.12.0","0.12.0-reader.10"},{"0.13.0-reader.1","0.12.0"},{"1.0.0-beta","1.0.0-alpha"},{"1.0.0-alpha.1","1.0.0-alpha"}})assert(updateVersionNewer(p.first,p.second));
    for(const auto &bad:{"","vnope","1.2","01.2.3","1.2.3-reader.01","1.2.3-","1.2.3+","1.2.3-../bad"})assert(!updateVersionNewer(bad,"0.0.0"));
    assert(!updateVersionNewer("v0.12.0-reader.4","0.12.0-reader.4"));assert(!updateVersionNewer("1.0.0+new","1.0.0+old"));assert(!updateVersionNewer("1.0.0-beta","1.0.0"));
    UpdateRelease r;std::string error;UpdateSource source;
    const auto asset=source.assetPrefix+"v0.12.0-reader.10/";
    const auto json=std::string("[{\"tag_name\":\"v0.12.0-reader.10\",\"body\":\"\\u05e9\\u05dc\\u05d5\\u05dd \\\"notes\\\" \\ud83d\\ude00\",\"assets\":[{\"name\":\"PaperNX.nro\",\"size\":20000,\"browser_download_url\":\"")+asset+"PaperNX.nro\"},{\"name\":\"PaperNX.nro.sha256\",\"browser_download_url\":\""+asset+"PaperNX.nro.sha256\"}]}]";
    assert(selectUpdateRelease(json,"0.12.0-reader.4",source,r,error)&&r.installable());assert(r.notes==u8"שלום \"notes\" 😀");
    for(const auto &bad:{"{}","[{}]","[{\"tag_name\":\"1.0.0\",}]","[{\"tag_name\":\"1.0.0\",\"tag_name\":\"9.0.0\"}]","[", "[{\"body\":\"\\ud800\"}]"})assert(!selectUpdateRelease(bad,"0.12.0-reader.4",source,r,error));
    assert(selectUpdateRelease("[{\"tag_name\":\"0.12.0-reader.4\"},{\"tag_name\":\"9.0.0\",\"draft\":true}]","0.12.0-reader.4",source,r,error)&&r.version.empty());
    auto wrong=json;const auto where=wrong.find(source.assetPrefix);wrong.replace(where,source.assetPrefix.size(),"https://evil.invalid/");assert(selectUpdateRelease(wrong,"0.12.0-reader.4",source,r,error)&&!r.installable());
    std::cout<<"PASS: prerelease/stable version ordering, scoped bounded JSON, Unicode notes, malformed responses, draft filtering and asset origin\n";
}
static void network(int argc,char **argv){
    assert(argc>=5);const fs::path root=argv[2],path=root/u8"custom folder"/u8"קורא.nro";const std::string base=argv[3],scenario=argv[4],ca=argc>5?argv[5]:"";
    fs::create_directories(path.parent_path());{std::ofstream f(path);f<<"original application";}
    const auto original=read(path);UpdateSource source{base+"api",base+"download/"};
    SelfUpdater updater;updater.configure(path,"0.12.0-reader.4",ca,source);assert(updater.check());assert(!updater.check());
    auto state=wait(updater);
    if(scenario=="http"||scenario=="malformed"||scenario=="tls-reject"){assert(state.state==UpdateState::Error);assert(read(path)==original);return;}
    if(scenario=="current"){assert(state.state==UpdateState::Current);return;}
    assert(state.state==UpdateState::Available&&state.release.version=="0.13.0");
    if(scenario=="missing"){assert(!state.release.installable()&&!updater.download());return;}
    if(scenario=="write-error")fs::create_directory(path.string()+".part");
    assert(updater.download());assert(!updater.download());
    if(scenario=="cancel"){std::this_thread::sleep_for(std::chrono::milliseconds(70));updater.stop();state=updater.status();}
    else state=wait(updater);
    assert(read(path)==original); // No network outcome can overwrite the running app.
    const bool failed=scenario=="corrupt"||scenario=="badapp"||scenario=="badversion"||scenario=="truncated"||scenario=="oversized"||scenario=="cancel"||scenario=="write-error"||scenario=="downgrade";
    if(failed){assert(state.state==UpdateState::Error&&!state.error.empty());assert(!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready"));return;}
    if(state.state!=UpdateState::Ready)std::cerr<<"Update failed ("<<scenario<<"): "<<state.error<<'\n';
    assert(state.state==UpdateState::Ready);const auto downloaded=read(path.string()+".new");assert(downloaded.size()==state.release.bytes);
    std::string error;
    if(scenario=="tampered"){std::fstream f(path.string()+".new",std::ios::in|std::ios::out|std::ios::binary);f.seekp(0x200);f.put('!');f.close();assert(!updater.applyPending(error)&&!error.empty()&&read(path)==original);return;}
    if(scenario=="replace-failure"){fs::create_directory(path.string()+".old");std::ofstream(path.string()+".old/sentinel")<<"keep";assert(!updater.applyPending(error)&&!error.empty()&&read(path)==original);return;}
    if(scenario=="missing-marker"){fs::remove(path.string()+".ready");assert(!updater.applyPending(error)&&read(path)==original);return;}
    if(scenario=="recovery"){
        updater.stop();SelfUpdater recovered;recovered.configure(path,"0.12.0-reader.4",ca,{base+"unavailable-api",base+"download/"});assert(recovered.check());assert(wait(recovered).state==UpdateState::Ready);assert(recovered.applyPending(error));
    }else assert(updater.applyPending(error));
    assert(error.empty()&&read(path)==downloaded&&!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready")&&!fs::exists(path.string()+".old"));
    std::cout<<"PASS: streamed/verified download, unchanged running file, deferred installation and recovery ("<<scenario<<")\n";
}
int main(int argc,char **argv){
    if(argc>1&&std::string(argv[1])=="--network")network(argc,argv);
    else if(argc==4&&std::string(argv[1])=="--live"){
        SelfUpdater u;u.configure(fs::temp_directory_path()/"PaperNX-live-check.nro",argv[2],argv[3]);assert(u.check());const auto s=wait(u);
        if(s.state==UpdateState::Error)std::cerr<<s.error<<'\n';
        assert(s.state==UpdateState::Current||(s.state==UpdateState::Available&&s.release.installable()));
        std::cout<<"Verified live fork API over TLS; offered version: "<<s.release.version<<'\n';
    }else unit();
}
