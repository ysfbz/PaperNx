#include "papernx/self_update.hpp"
#include "../src/update_json.hpp"
#include <atomic>
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
    while(std::chrono::steady_clock::now()<end){auto s=updater.status();if(s.state!=UpdateState::Checking&&s.state!=UpdateState::Downloading&&s.state!=UpdateState::Verifying)return s;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
    assert(false && "Updater timed out");return {};
}
static void unit(){
    assert(updateVersionNewer("0.13.0","0.12.0-reader.4"));
    assert(updateVersionNewer("0.9.3","0.9.2"));
    assert(!updateVersionNewer("0.9.2","0.9.3"));
    assert(updateVersionNewer("0.13.1","0.13.0"));
    assert(updateVersionNewer("0.14.0","0.13.99"));
    assert(updateVersionNewer("0.14.0+switch.001","0.13.99"));
    assert(!updateVersionNewer("0.12.1","0.13.0"));
    for(auto p:{std::pair<const char*,const char*>{"v0.12.0-reader.10","0.12.0-reader.9"},{"0.12.0","0.12.0-reader.10"},{"0.13.0-reader.1","0.12.0"},{"1.0.0-beta","1.0.0-alpha"},{"1.0.0-alpha.1","1.0.0-alpha"}})assert(updateVersionNewer(p.first,p.second));
    for(const auto &bad:{"","vnope","1.2","01.2.3","1.2.3-reader.01","1.2.3-","1.2.3+","1.2.3-../bad"})assert(!updateVersionNewer(bad,"0.0.0"));
    assert(!updateVersionNewer("v0.12.0-reader.4","0.12.0-reader.4"));assert(!updateVersionNewer("1.0.0+new","1.0.0+old"));assert(!updateVersionNewer("1.0.0-beta","1.0.0"));
    UpdateRelease r;std::string error;UpdateSource source;
    const auto asset=source.assetPrefix+"v0.12.0-reader.10/";
    const auto json=std::string("[{\"tag_name\":\"v0.12.0-reader.10\",\"body\":\"\\u05e9\\u05dc\\u05d5\\u05dd \\\"notes\\\" \\ud83d\\ude00\",\"assets\":[{\"name\":\"PaperNX.nro\",\"size\":20000,\"browser_download_url\":\"")+asset+"PaperNX.nro\"},{\"name\":\"PaperNX.nro.sha256\",\"browser_download_url\":\""+asset+"PaperNX.nro.sha256\"}]}]";
    assert(selectUpdateRelease(json,"0.12.0-reader.4",source,r,error)&&r.installable());assert(r.notes==u8"שלום \"notes\" 😀");
    const auto spaced=std::string(" \n[ { \"tag_name\" : \"v0.12.0-reader.10\", \"name\" : \"Release\", \"draft\" : false, \"prerelease\" : false, \"body\" : null, \"extra\" : {\"nested\" : [true, null, 3]}, \"assets\" : [ { \"name\" : \"PaperNX.nro\", \"size\" : 20000, \"browser_download_url\" : \"")+asset+"PaperNX.nro\", \"digest\" : \"sha256:"+std::string(64,'a')+"\" } ] } ] \t";
    assert(selectUpdateRelease(spaced,"0.12.0-reader.4",source,r,error)&&r.installable()&&r.title=="Release"&&r.notes.empty()&&r.sha256==std::string(64,'a'));
    assert(selectUpdateRelease("[{\"tag_name\":\"9.0.0\",\"name\":null,\"body\":null,\"assets\":null}]",
                               "0.12.0-reader.4",source,r,error)&&!r.installable());
    std::string manyAssets="[{\"tag_name\":\"v0.12.0-reader.10\",\"assets\":[";
    for(int i=0;i<10000;++i){if(i)manyAssets+=',';manyAssets+="{\"name\":\"page.png\"}";}
    manyAssets+=",{\"name\":\"PaperNX.nro\",\"size\":20000,\"browser_download_url\":\""+asset+"PaperNX.nro\"},{\"name\":\"PaperNX.nro.sha256\",\"browser_download_url\":\""+asset+"PaperNX.nro.sha256\"}]}]";
    std::vector<update_json::Release> parsed;
    assert(update_json::Parser(manyAssets).parse(parsed)&&parsed.size()==1&&parsed[0].assets.size()==2);
    assert(selectUpdateRelease(manyAssets,"0.12.0-reader.4",source,r,error)&&r.installable());
    const auto duplicateAsset=std::string("[{\"tag_name\":\"v0.12.0-reader.10\",\"assets\":[{\"name\":\"PaperNX.nro\",\"size\":20000,\"browser_download_url\":\"")+asset+"PaperNX.nro\"},{\"name\":\"PaperNX.nro\",\"size\":20000,\"browser_download_url\":\""+asset+"PaperNX.nro\"}]}]";
    assert(selectUpdateRelease(duplicateAsset,"0.12.0-reader.4",source,r,error)&&!r.installable());
    for(const auto &bad:{"{}","[{}]","[{\"tag_name\":\"1.0.0\",}]","[{\"tag_name\":\"1.0.0\",\"tag_name\":\"9.0.0\"}]","[", "[{\"body\":\"\\ud800\"}]"})assert(!selectUpdateRelease(bad,"0.12.0-reader.4",source,r,error));
    for(const std::string &bytes:{std::string(1,char(0xc0)),std::string("\xed\xa0\x80",3),std::string("\xf4\x90\x80\x80",4),std::string("\xe2\x82",2)}) {
        std::string malformed="[{\"tag_name\":\"1.0.0\",\"body\":\"";malformed+=bytes;malformed+="\"}]";
        assert(!selectUpdateRelease(malformed,"0.12.0-reader.4",source,r,error));
    }
    r.version="stale";r.title="stale";r.notes="stale";r.url="stale";r.checksumUrl="stale";r.sha256="stale";r.bytes=123;
    assert(!selectUpdateRelease("[{\"tag_name\":\"9.0.0\"},{}]","0.12.0-reader.4",source,r,error));
    assert(r.version.empty()&&r.title.empty()&&r.notes.empty()&&r.url.empty()&&r.checksumUrl.empty()&&r.sha256.empty()&&r.bytes==0);
    r.version="stale";
    assert(!selectUpdateRelease("[]","invalid",source,r,error)&&r.version.empty());
    assert(!selectUpdateRelease("[{\"tag_name\":\"1.0.0\",\"extra\":1,\"extra\":2}]","0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease("[{\"tag_name\":\"1.0.0\",\"extra\":{\"nested\":[{\"key\":1,\"key\":2}]}}]","0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease("[{\"tag_name\":12}]","0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease("[{\"tag_name\":\"9.0.0\",\"draft\":\"false\"}]","0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease("[{\"tag_name\":\"9.0.0\",\"prerelease\":0}]","0.12.0-reader.4",source,r,error));
    for(const auto &bad:{
        "[{\"tag_name\":\"9.0.0\",\"name\":12}]",
        "[{\"tag_name\":\"9.0.0\",\"body\":{}}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":{}}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[\"not-an-object\"]}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[null]}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[{\"name\":12}]}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[{\"size\":-1}]}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[{\"size\":1.5}]}]",
        "[{\"tag_name\":\"9.0.0\",\"assets\":[{\"size\":18446744073709551616}]}]"
    })assert(!selectUpdateRelease(bad,"0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease("[{\"tag_name\":\"1.0.0\",\"assets\":[{\"name\":\"a\",\"name\":\"b\"}]}]","0.12.0-reader.4",source,r,error));
    std::string tooDeep="[{\"tag_name\":\"1.0.0\",\"extra\":";tooDeep.append(26,'[');tooDeep+="0";tooDeep.append(26,']');tooDeep+="}]";
    assert(!selectUpdateRelease(tooDeep,"0.12.0-reader.4",source,r,error));
    std::string tooManyNodes="[{\"tag_name\":\"1.0.0\",\"extra\":[";
    for(int i=0;i<150001;++i){if(i)tooManyNodes+=',';tooManyNodes+="null";}
    tooManyNodes+="]}]";assert(!selectUpdateRelease(tooManyNodes,"0.12.0-reader.4",source,r,error));
    std::string releasesAtLimit="[";
    for(int i=0;i<30;++i){if(i)releasesAtLimit+=',';releasesAtLimit+="{\"tag_name\":\"1.0.0\"}";}
    releasesAtLimit+=']';assert(selectUpdateRelease(releasesAtLimit,"0.12.0-reader.4",source,r,error));
    std::string tooManyReleases="[";
    for(int i=0;i<31;++i){if(i)tooManyReleases+=',';tooManyReleases+="{\"tag_name\":\"1.0.0\"}";}
    tooManyReleases+=']';assert(!selectUpdateRelease(tooManyReleases,"0.12.0-reader.4",source,r,error));
    assert(!selectUpdateRelease(std::string(4*1024*1024+1,' '),"0.12.0-reader.4",source,r,error));
    assert(selectUpdateRelease("[{\"tag_name\":\"0.12.0-reader.4\"},{\"tag_name\":\"9.0.0\",\"draft\":true}]","0.12.0-reader.4",source,r,error)&&r.version.empty());
    auto wrong=json;const auto where=wrong.find(source.assetPrefix);wrong.replace(where,source.assetPrefix.size(),"https://evil.invalid/");assert(selectUpdateRelease(wrong,"0.12.0-reader.4",source,r,error)&&!r.installable());
    const auto recoveryRoot=fs::temp_directory_path()/("papernx-updater-recovery-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(recoveryRoot);
    const auto executable=recoveryRoot/"PaperNX.nro";
    {std::ofstream(executable)<<"candidate";std::ofstream(executable.string()+".old")<<"previous";std::ofstream(executable.string()+".swap")<<"staged";}
    SelfUpdater recovery;recovery.configure(executable,"0.12.0-reader.4");
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::RolledBack);
    assert(read(executable)=="previous"&&!fs::exists(executable.string()+".old")&&!fs::exists(executable.string()+".swap"));
    {std::ofstream(executable.string()+".old")<<"previous after interrupted rename";
     std::ofstream(executable.string()+".swap")<<"staged";}
    fs::remove(executable);
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::RolledBack);
    assert(read(executable)=="previous after interrupted rename"&&!fs::exists(executable.string()+".old")&&!fs::exists(executable.string()+".swap"));
    {std::ofstream(executable.string()+".swap")<<"staged";}
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::Error&&!error.empty());
    fs::remove(executable.string()+".swap");
    {std::ofstream(executable)<<"current";std::ofstream(executable.string()+".old")<<"user backup";}
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::Normal);
    assert(read(executable)=="current"&&read(executable.string()+".old")=="user backup");
    fs::remove(executable);
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::Error&&!error.empty());
    assert(read(executable.string()+".old")=="user backup");
    std::ofstream(executable)<<"current";
    {std::ofstream(executable)<<"candidate";std::ofstream(executable.string()+".old")<<"previous";
     std::ofstream(executable.string()+".boot")<<std::string(64,'a')<<"\n0.13.0\n4096\n";
     std::ofstream(executable.string()+".boot.started");}
    assert(recovery.prepareBoot(error)==SelfUpdater::BootRecovery::RolledBack);
    assert(read(executable)=="previous"&&!fs::exists(executable.string()+".old")&&!fs::exists(executable.string()+".boot.started"));
    const auto missingRoot=recoveryRoot/"missing";fs::create_directories(missingRoot);
    const auto missingExe=missingRoot/"PaperNX.nro";
    std::ofstream(missingExe.string()+".new")<<"candidate retained";
    std::ofstream(missingExe.string()+".ready")<<"recovery record retained";
    SelfUpdater missing;missing.configure(missingExe,"0.12.0-reader.4");
    assert(missing.prepareBoot(error)==SelfUpdater::BootRecovery::Error&&!error.empty());
    assert(read(missingExe.string()+".new")=="candidate retained"&&read(missingExe.string()+".ready")=="recovery record retained");
    {std::ofstream(missingExe.string()+".old")<<"previous version";
     std::ofstream(missingExe.string()+".boot")<<"legacy boot record";
     std::ofstream(missingExe.string()+".boot.started")<<"started";
     std::ofstream(missingExe.string()+".boot.ok")<<"confirmed";}
    assert(missing.prepareBoot(error)==SelfUpdater::BootRecovery::Error&&!error.empty());
    assert(read(missingExe.string()+".old")=="previous version"&&read(missingExe.string()+".boot") == "legacy boot record" &&
           read(missingExe.string()+".boot.started")=="started"&&read(missingExe.string()+".boot.ok")=="confirmed" &&
           read(missingExe.string()+".new")=="candidate retained"&&read(missingExe.string()+".ready")=="recovery record retained");
    assert(!missing.confirmBoot(error)&&!error.empty());
    assert(read(missingExe.string()+".old")=="previous version"&&fs::exists(missingExe.string()+".boot")&&
           fs::exists(missingExe.string()+".boot.started")&&fs::exists(missingExe.string()+".boot.ok"));
    fs::remove_all(recoveryRoot);
    std::cout<<"PASS: prerelease/stable version ordering, scoped bounded JSON, Unicode notes, malformed responses, draft filtering and asset origin\n";
}
static void network(int argc,char **argv){
    assert(argc>=5);const fs::path root=argv[2],path=root/u8"custom folder"/u8"קורא.nro";const std::string base=argv[3],scenario=argv[4],ca=argc>5?argv[5]:"";
    fs::create_directories(path.parent_path());{std::ofstream f(path);f<<"original application";}
    for(const auto &relative:{"books/keep.epub","data/progress.dat","logs/journal.dat","settings.ini"}) {
        const auto file=root/relative;fs::create_directories(file.parent_path());std::ofstream(file)<<"user data";
    }
    auto userDataUnchanged=[&]{for(const auto &relative:{"books/keep.epub","data/progress.dat","logs/journal.dat","settings.ini"})assert(read(root/relative)=="user data");};
    if(scenario=="stale-part") {std::ofstream(path.string()+".part")<<"partial";std::ofstream(path.string()+".ready.tmp")<<"partial";}
    if(scenario=="orphan-new") std::ofstream(path.string()+".new")<<"incomplete";
    if(scenario=="orphan-old") std::ofstream(path.string()+".old")<<"user backup";
    if(scenario=="legacy-state") std::ofstream(path.string()+".swap")<<"legacy transaction";
    if(scenario=="stale-part-error") {fs::create_directories(path.string()+".part");std::ofstream(path.string()+".part/sentinel")<<"preserve";}
    const auto original=read(path),priorCandidate=read(path.string()+".new"),priorSecond=read(path.string()+".new.1");UpdateSource source{base+"api",base+"download/"};
    SelfUpdater updater;updater.configureForTesting(path,"0.12.0-reader.4",ca,source);
    if(scenario=="stale-part-error") {
        assert(!updater.check()&&updater.status().state==UpdateState::Error&&read(path)==original);
        assert(fs::exists(path.string()+".part/sentinel"));userDataUnchanged();return;
    }
    assert(updater.check());assert(!updater.check());
    auto state=wait(updater);
    if(scenario=="stale-part")assert(!fs::exists(path.string()+".part")&&!fs::exists(path.string()+".ready.tmp"));
    if(scenario=="orphan-new")assert(!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready"));
    if(scenario=="resume-ready") {
        assert(state.state==UpdateState::Ready&&read(path)==original&&fs::exists(path.string()+".new")&&fs::exists(path.string()+".ready"));
        userDataUnchanged();return;
    }
    if(scenario=="http"||scenario=="malformed"||scenario=="tls-reject"){assert(state.state==UpdateState::Error);assert(read(path)==original);return;}
    if(scenario=="current"||scenario=="downgrade"){assert(state.state==UpdateState::Current);return;}
    assert(state.state==UpdateState::Available&&state.release.version=="0.13.0");
    if(scenario=="missing"||scenario=="oversized"){assert(!state.release.installable()&&!updater.download());return;}
    if(scenario=="space-error") {
        fs::remove(path);assert(fs::remove(path.parent_path()));
        assert(!updater.download()&&updater.status().state==UpdateState::Error);
        assert(!fs::exists(path)&&!fs::exists(path.string()+".part"));userDataUnchanged();return;
    }
    if(scenario=="preserve-nospace") {
        const auto prior=read(path.string()+".new");assert(!prior.empty()&&state.staged.version=="0.12.5");
        SelfUpdater::failNextOperationForTesting(SelfUpdater::FaultPoint::NoSpace);
        assert(!updater.download()&&updater.status().state==UpdateState::Error);
        assert(read(path.string()+".new")==prior&&fs::exists(path.string()+".ready"));userDataUnchanged();return;
    }
    if(scenario=="legacy-state") {
        assert(!updater.download()&&updater.status().state==UpdateState::Error);
        assert(read(path)==original&&fs::exists(path.string()+".swap"));
        if(!priorCandidate.empty())assert(read(path.string()+".new")==priorCandidate&&fs::exists(path.string()+".ready"));
        userDataUnchanged();return;
    }
    if(scenario=="preserve-rename"||scenario=="preserve-record"||scenario=="preserve-payload-fsync"||scenario=="preserve-record-fsync"||scenario=="preserve-verify-cancel"||scenario=="preserve-cancel"||scenario=="preserve-stage-cancel") {
        const auto prior=read(path.string()+".new");assert(!prior.empty()&&state.staged.version=="0.12.5");
        const auto fault=scenario=="preserve-rename"?SelfUpdater::FaultPoint::CandidateRename:
            scenario=="preserve-record"?SelfUpdater::FaultPoint::CandidateRecord:
            scenario=="preserve-payload-fsync"?SelfUpdater::FaultPoint::PayloadFsync:
            scenario=="preserve-record-fsync"?SelfUpdater::FaultPoint::CandidateRecordFsync:
            scenario=="preserve-verify-cancel"?SelfUpdater::FaultPoint::CancelAfterVerify:
            scenario=="preserve-stage-cancel"?SelfUpdater::FaultPoint::CancelAfterStage:SelfUpdater::FaultPoint::None;
        if(fault!=SelfUpdater::FaultPoint::None)SelfUpdater::failNextOperationForTesting(fault);
        assert(updater.download());assert(!updater.download());
        if(scenario=="preserve-cancel") {std::this_thread::sleep_for(std::chrono::milliseconds(70));updater.cancel();}
        state=wait(updater);
        assert(state.state==(scenario=="preserve-cancel"||scenario=="preserve-verify-cancel"||scenario=="preserve-stage-cancel"?UpdateState::Cancelled:UpdateState::Error));
        assert(read(path)==original&&read(path.string()+".new")==prior&&fs::exists(path.string()+".ready"));
        if(scenario=="preserve-stage-cancel")assert(!fs::exists(path.string()+".new.1")&&!fs::exists(path.string()+".ready.1"));
        assert(state.staged.stagedFile==path.string()+".new"&&state.staged.version=="0.12.5");userDataUnchanged();return;
    }
    const bool preserveInvalid=scenario=="preserve-corrupt"||scenario=="preserve-badapp"||scenario=="preserve-badversion"||scenario=="preserve-truncated";
    if(scenario=="write-error"){fs::create_directories(path.string()+".part");std::ofstream(path.string()+".part/sentinel")<<"keep";}
    assert(updater.download());assert(!updater.download());
    if(scenario=="cancel"){std::this_thread::sleep_for(std::chrono::milliseconds(70));updater.cancel();state=wait(updater);}
    else if(scenario=="cancel-near-completion") {
        std::atomic<bool> triggered{false};
        std::thread canceller([&]{
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
            while(std::chrono::steady_clock::now()<deadline) {
                const auto current=updater.status();
                if(current.state!=UpdateState::Downloading)return;
                if(current.percent>=100){triggered=true;updater.cancel();return;}
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        state=wait(updater);canceller.join();assert(triggered.load());
    }
    else state=wait(updater);
    assert(read(path)==original); // No network outcome can overwrite the running app.
    const bool failed=scenario=="corrupt"||scenario=="badapp"||scenario=="badversion"||scenario=="truncated"||preserveInvalid||scenario=="cancel"||scenario=="cancel-near-completion"||scenario=="write-error";
    if(failed){assert((state.state==UpdateState::Error||state.state==UpdateState::Cancelled)&&!state.error.empty());
        if(scenario=="cancel"||scenario=="cancel-near-completion") {
            updater.reportError("Network services are unavailable");
            const auto reported=updater.status();assert(reported.state==UpdateState::Error&&reported.error=="Network services are unavailable");
        }
        if(scenario=="write-error")assert(fs::exists(path.string()+".part/sentinel"));else assert(!fs::exists(path.string()+".part"));
        if(preserveInvalid) {
            assert(read(path.string()+".new")==priorCandidate&&fs::exists(path.string()+".ready"));
            assert(!fs::exists(path.string()+".new.1")&&!fs::exists(path.string()+".ready.1"));
            assert(state.staged.version=="0.12.5");
        } else assert(!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready"));
        return;}
    if(state.state!=UpdateState::Ready)std::cerr<<"Update failed ("<<scenario<<"): "<<state.error<<'\n';
    assert(state.state==UpdateState::Ready);const fs::path stagedPath=state.release.stagedFile;const auto downloaded=read(stagedPath);assert(downloaded.size()==state.release.bytes);
    assert(read(path)==original&&downloaded.size()==state.release.bytes);
    assert(fs::exists(stagedPath)&&fs::exists(stagedPath.string()==path.string()+".new"?path.string()+".ready":path.string()+".ready.1"));
    if(scenario=="two-valid-candidates")assert(read(path.string()+".new.1")==priorSecond&&state.staged.version=="0.13.0");
    if(scenario=="two-valid-tie")assert(stagedPath==path.string()+".new.1"&&read(path.string()+".new")==priorCandidate);
    if(scenario=="invalid-new-fallback")assert(read(path.string()+".new")==priorCandidate&&stagedPath==path.string()+".new.1");
    assert((scenario=="orphan-old" ? read(path.string()+".old")=="user backup" : !fs::exists(path.string()+".old")) &&
           !fs::exists(path.string()+".swap")&&!fs::exists(path.string()+".boot"));
    if(scenario=="tampered"){
        std::fstream f(path.string()+".new",std::ios::in|std::ios::out|std::ios::binary);f.seekp(0x200);f.put('!');f.close();
        updater.stop();SelfUpdater recheck;recheck.configureForTesting(path,"0.12.0-reader.4",ca,source);
        assert(recheck.check()&&wait(recheck).state==UpdateState::Available&&read(path)==original);
        assert(!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready"));userDataUnchanged();return;
    }
    if(scenario=="missing-marker"){
        fs::remove(path.string()+".ready");updater.stop();SelfUpdater recheck;recheck.configureForTesting(path,"0.12.0-reader.4",ca,source);
        assert(recheck.check()&&wait(recheck).state==UpdateState::Available&&read(path)==original);
        assert(!fs::exists(path.string()+".new")&&!fs::exists(path.string()+".ready"));userDataUnchanged();return;
    }
    userDataUnchanged();
    std::cout<<"PASS: streamed/verified update staged beside unchanged running NRO ("<<scenario<<")\n";
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
