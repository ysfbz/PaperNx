#include "papernx/self_update.hpp"
#include "update_json.hpp"
#include <curl/curl.h>
#include <mupdf/fitz.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <unistd.h>

namespace papernx {
namespace {
constexpr std::uint64_t maxNro=256ull*1024*1024;
struct Version {std::vector<std::string> base,pre;bool valid{false};};
bool digits(const std::string &s){return !s.empty() && s.find_first_not_of("0123456789")==std::string::npos;}
std::vector<std::string> split(const std::string &s,char separator){std::vector<std::string> out;std::size_t from=0,to;do{to=s.find(separator,from);out.push_back(s.substr(from,to==s.npos?s.npos:to-from));from=to+1;}while(to!=s.npos);return out;}
bool identifiers(const std::vector<std::string> &parts,bool numericOnly){
    for(const auto &p:parts){if(p.empty() || p.size()>32)return false;for(char c:p)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'))return false;if(numericOnly&&!digits(p))return false;if(digits(p)&&p.size()>1&&p[0]=='0')return false;}return true;
}
Version version(std::string s){
    Version v;if(s.empty()||s.size()>128)return v;if(s[0]=='v'||s[0]=='V')s.erase(0,1);
    const auto metadata=s.find('+');if(metadata!=s.npos){auto ids=split(s.substr(metadata+1),'.');for(const auto &id:ids)if(id.empty()||id.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-")!=id.npos)return v;s.resize(metadata);}
    const auto dash=s.find('-');if(dash!=s.npos){v.pre=split(s.substr(dash+1),'.');s.resize(dash);if(!identifiers(v.pre,false))return v;}
    v.base=split(s,'.');v.valid=v.base.size()==3&&identifiers(v.base,true);return v;
}
int numericCompare(const std::string &a,const std::string &b){if(a.size()!=b.size())return a.size()>b.size()?1:-1;return a==b?0:a>b?1:-1;}
int compare(const Version &a,const Version &b){
    for(int i=0;i<3;++i){int result=numericCompare(a.base[i],b.base[i]);if(result)return result;}
    if(a.pre.empty()!=b.pre.empty())return a.pre.empty()?1:-1;
    for(std::size_t i=0;i<std::min(a.pre.size(),b.pre.size());++i){const auto &x=a.pre[i],&y=b.pre[i];if(x==y)continue;bool xn=digits(x),yn=digits(y);if(xn!=yn)return xn?-1:1;return xn?numericCompare(x,y):x>y?1:-1;}
    return a.pre.size()==b.pre.size()?0:a.pre.size()>b.pre.size()?1:-1;
}
std::string stripV(const std::string &s){return !s.empty()&&(s[0]=='v'||s[0]=='V')?s.substr(1):s;}
std::string hashValue(std::string value){
    if(value.compare(0,7,"sha256:")==0)value.erase(0,7);
    if(value.size()!=64 || value.find_first_not_of("0123456789abcdefABCDEF")!=value.npos)return {};
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return char(std::tolower(c));});return value;
}
std::uint64_t integer(const update_json::Value &v){
    if(v.kind!=update_json::Value::Number || !digits(v.text) || v.text.size()>12)return 0;
    std::uint64_t out=0;for(char c:v.text)out=out*10+c-'0';return out;
}
struct CurlRuntime {CURLcode result{curl_global_init(CURL_GLOBAL_DEFAULT)};~CurlRuntime(){curl_global_cleanup();}};
struct Transfer {
    std::atomic<bool> *cancel;std::string *body{nullptr};FILE *file{nullptr};std::size_t limit{0},written{0};
    std::function<void(int)> progress;
};
std::size_t receive(char *data,std::size_t size,std::size_t count,void *user){
    auto &t=*static_cast<Transfer*>(user);if(size && count>t.limit/size)return 0;auto bytes=size*count;
    if(t.cancel->load()||bytes>t.limit-t.written)return 0;
    if(t.file){if(std::fwrite(data,1,bytes,t.file)!=bytes)return 0;}else t.body->append(data,bytes);
    t.written+=bytes;return bytes;
}
int progress(void *user,curl_off_t total,curl_off_t now,curl_off_t,curl_off_t){auto &t=*static_cast<Transfer*>(user);if(t.cancel->load())return 1;if(t.progress&&total>0)t.progress(int(std::min<curl_off_t>(100,now*100/total)));return 0;}
bool request(const std::string &url,const std::string &ca,Transfer &t,std::string &error){
    static CurlRuntime runtime;if(runtime.result!=CURLE_OK){error="Network initialization failed";return false;}
    auto *curl=curl_easy_init();if(!curl){error="Network initialization failed";return false;}
    char message[CURL_ERROR_SIZE]{};
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_USERAGENT,"PaperNX updater");
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L);curl_easy_setopt(curl,CURLOPT_MAXREDIRS,5L);
    // HTTP is only used by an explicitly injected loopback test source.
    const bool https=url.compare(0,8,"https://")==0;
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS_STR,https?"https":"http,https");
    curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS_STR,https?"https":"http,https");
#else
    const long protocols=https?CURLPROTO_HTTPS:CURLPROTO_HTTP|CURLPROTO_HTTPS;
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS,protocols);curl_easy_setopt(curl,CURLOPT_REDIR_PROTOCOLS,protocols);
#endif
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    if(!ca.empty())curl_easy_setopt(curl,CURLOPT_CAINFO,ca.c_str());
    curl_easy_setopt(curl,CURLOPT_FAILONERROR,1L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,t.file?600L:20L);
    curl_easy_setopt(curl,CURLOPT_LOW_SPEED_LIMIT,1024L);curl_easy_setopt(curl,CURLOPT_LOW_SPEED_TIME,30L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&t);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&t);
    curl_easy_setopt(curl,CURLOPT_ERRORBUFFER,message);
    const auto result=curl_easy_perform(curl);curl_easy_cleanup(curl);
    if(result!=CURLE_OK){error=t.cancel->load()?"Update cancelled":result==CURLE_PEER_FAILED_VERIFICATION?"Certificate verification failed; check the console date and time":std::string(message[0]?message:curl_easy_strerror(result));return false;}return true;
}
bool fetch(const std::string &url,const std::string &ca,std::atomic<bool> &cancel,std::string &body,std::string &error,std::size_t limit=4*1024*1024){Transfer t{&cancel,&body,nullptr,limit,0,{}};return request(url,ca,t,error);}
bool durable(FILE *file){return std::fflush(file)==0 && ::fsync(::fileno(file))==0;}
std::uint32_t le32(const unsigned char *p){return std::uint32_t(p[0])|(std::uint32_t(p[1])<<8)|(std::uint32_t(p[2])<<16)|(std::uint32_t(p[3])<<24);}
std::uint64_t le64(const unsigned char *p){return std::uint64_t(le32(p))|(std::uint64_t(le32(p+4))<<32);}
bool validateNro(const std::filesystem::path &path,const std::string &expectedHash,std::uint64_t bytes,const std::string &expectedVersion,std::string &error){
    std::error_code ec;const auto size=std::filesystem::file_size(path,ec);
    if(ec||size!=bytes||size<0x1000||size>maxNro){error="Update download size does not match";return false;}
    std::ifstream file(path,std::ios::binary);std::array<unsigned char,0x80> header{};file.read(reinterpret_cast<char*>(header.data()),header.size());
    if(!file||std::memcmp(header.data()+0x10,"NRO0",4)){error="Downloaded file is not a Switch application";return false;}
    const auto declared=le32(header.data()+0x18);if(declared<0x80||declared>size-0x38){error="Invalid NRO header";return false;}
    std::uint64_t end=0;for(int i=0;i<3;++i){auto offset=le32(header.data()+0x20+i*8),length=le32(header.data()+0x24+i*8);if(offset<end||std::uint64_t(offset)+length>declared){error="Invalid NRO segments";return false;}end=std::uint64_t(offset)+length;}
    std::array<unsigned char,0x38> assets{};file.seekg(declared);file.read(reinterpret_cast<char*>(assets.data()),assets.size());
    if(!file||std::memcmp(assets.data(),"ASET",4)){error="Update is missing its resources";return false;}
    const auto nacp=le64(assets.data()+0x18),nacpSize=le64(assets.data()+0x20),romfs=le64(assets.data()+0x28),romfsSize=le64(assets.data()+0x30);
    const auto available=size-declared;
    if(nacp<0x38||nacpSize!=0x4000||nacp>available||nacpSize>available-nacp||romfs<0x38||romfs>available||romfsSize<0x50||romfsSize>available-romfs){error="Invalid update resources";return false;}
    std::array<char,512> title{};file.seekg(declared+nacp);file.read(title.data(),title.size());
    std::array<char,16> appVersion{};file.seekg(declared+nacp+0x3060);file.read(appVersion.data(),appVersion.size());
    if(!file||std::string(title.data(),strnlen(title.data(),title.size()))!="PaperNX"||std::string(appVersion.data(),strnlen(appVersion.data(),appVersion.size()))!=stripV(expectedVersion).substr(0,15)){error="Update application or version does not match";return false;}
    file.clear();file.seekg(0);fz_sha256 sha;fz_sha256_init(&sha);std::array<unsigned char,32768> buffer{};
    while(file){file.read(reinterpret_cast<char*>(buffer.data()),buffer.size());fz_sha256_update(&sha,buffer.data(),std::size_t(file.gcount()));}
    if(!file.eof()){error="Cannot read the update";return false;}
    unsigned char digest[32];fz_sha256_final(&sha,digest);std::string actual;const char *hex="0123456789abcdef";for(auto c:digest){actual+=hex[c>>4];actual+=hex[c&15];}
    if(actual!=expectedHash){error="Update checksum does not match";return false;}return true;
}
} // namespace

bool updateVersionNewer(const std::string &candidate,const std::string &current){auto a=version(candidate),b=version(current);return a.valid&&b.valid&&compare(a,b)>0;}
bool selectUpdateRelease(const std::string &json,const std::string &current,const UpdateSource &source,UpdateRelease &release,std::string &error){
    update_json::Value root;if(!update_json::Parser(json).parse(root)||root.kind!=update_json::Value::Array){error="Invalid release response";return false;}
    if(!version(current).valid){error="Invalid installed version";return false;}
    bool published=false;release={};
    for(const auto &item:root.items){
        if(item.kind!=update_json::Value::Object||item.get("draft").yes())continue;
        auto tag=item.get("tag_name").str();if(!version(tag).valid)continue;published=true;
        if(!updateVersionNewer(tag,release.version.empty()?current:release.version))continue;
        release={};release.version=stripV(tag);release.title=item.get("name").str().substr(0,256);release.notes=item.get("body").str().substr(0,32768);
        const auto &assets=item.get("assets");if(assets.kind!=update_json::Value::Array)continue;
        for(const auto &asset:assets.items){
            const auto name=asset.get("name").str(),url=asset.get("browser_download_url").str();
            if(url!=source.assetPrefix+tag+"/"+name)continue;
            if(name=="PaperNX.nro") {release.bytes=integer(asset.get("size"));if(release.bytes>=0x1000&&release.bytes<=maxNro){release.url=url;release.sha256=hashValue(asset.get("digest").str());}}
            if(name=="PaperNX.nro.sha256")release.checksumUrl=url;
        }
    }
    if(!published){error="No published release found";return false;}return true;
}

SelfUpdater::~SelfUpdater(){stop();}
void SelfUpdater::configure(std::filesystem::path executable,std::string current,std::string certificates,UpdateSource source){stop();executable_=std::move(executable);current_=std::move(current);certificates_=std::move(certificates);source_=std::move(source);std::lock_guard<std::mutex> lock(mutex_);status_={};}
UpdateStatus SelfUpdater::status() const {std::lock_guard<std::mutex> lock(mutex_);return status_;}
bool SelfUpdater::busy() const {auto state=status().state;return state==UpdateState::Checking||state==UpdateState::Downloading;}
void SelfUpdater::publish(UpdateState state,const std::string &error){std::lock_guard<std::mutex> lock(mutex_);status_.state=state;status_.error=error;if(state==UpdateState::Downloading)status_.percent=0;}
void SelfUpdater::stop(){cancel_=true;if(worker_.joinable())worker_.join();}
bool SelfUpdater::check(){
    if(busy())return false;
    if(worker_.joinable())worker_.join();
    cancel_=false;
    if(executable_.empty()||executable_.extension()!=".nro"||(!executable_.is_absolute()&&executable_.string().compare(0,6,"sdmc:/")!=0)){publish(UpdateState::Error,"Cannot determine the running NRO path");return false;}
    publish(UpdateState::Checking);
    worker_=std::thread([this]{
        std::string error;
        // A fully verified download may have survived an interrupted shutdown.
        if(validatePending(error)){publish(UpdateState::Ready);return;}
        std::string body;error.clear();if(!fetch(source_.api,certificates_,cancel_,body,error)){publish(UpdateState::Error,error);return;}
        UpdateRelease release;if(!selectUpdateRelease(body,current_,source_,release,error)){publish(UpdateState::Error,error);return;}
        std::lock_guard<std::mutex> lock(mutex_);status_.release=std::move(release);status_.state=status_.release.version.empty()?UpdateState::Current:UpdateState::Available;status_.error.clear();
    });return true;
}
bool SelfUpdater::download(){
    auto snapshot=status();if(snapshot.state!=UpdateState::Available||!snapshot.release.installable())return false;
    if(worker_.joinable())worker_.join();
    cancel_=false;publish(UpdateState::Downloading);
    worker_=std::thread([this,release=snapshot.release]()mutable{
        std::string error;
        if(release.sha256.empty()){
            std::string body;if(!fetch(release.checksumUrl,certificates_,cancel_,body,error,1024)){publish(UpdateState::Error,error);return;}
            std::istringstream line(body);std::string hash,name;line>>hash>>name;if(!name.empty()&&name!="PaperNX.nro"&&name!="*PaperNX.nro"){publish(UpdateState::Error,"Invalid update checksum file");return;}release.sha256=hashValue(hash);
            if(release.sha256.empty()){publish(UpdateState::Error,"Invalid update checksum file");return;}
        }
        const auto part=executable_.string()+".part",pending=executable_.string()+".new",ready=executable_.string()+".ready",temp=ready+".tmp";
        auto *file=std::fopen(part.c_str(),"wb");if(!file){publish(UpdateState::Error,"Cannot write update to the SD card");return;}
        Transfer transfer{&cancel_,nullptr,file,std::size_t(release.bytes),0,[this](int percent){std::lock_guard<std::mutex> lock(mutex_);status_.percent=percent;}};
        bool ok=request(release.url,certificates_,transfer,error);if(ok&&!durable(file)){ok=false;error="Cannot save update to the SD card";}if(std::fclose(file)!=0){ok=false;error="Cannot save update to the SD card";}
        if(ok)ok=validateNro(part,release.sha256,release.bytes,release.version,error);
        if(ok){std::remove(ready.c_str());if(std::rename(part.c_str(),pending.c_str())!=0){ok=false;error="Cannot stage update on the SD card";}}
        if(ok){
            file=std::fopen(temp.c_str(),"wb");if(!file){ok=false;}else{ok=std::fprintf(file,"%s\n%s\n%llu\n",release.sha256.c_str(),release.version.c_str(),static_cast<unsigned long long>(release.bytes))>0&&durable(file);if(std::fclose(file)!=0)ok=false;}
            if(ok)ok=std::rename(temp.c_str(),ready.c_str())==0;
            if(!ok)error="Cannot save pending update";
        }
        if(!ok){std::remove(part.c_str());std::remove(temp.c_str());std::remove(pending.c_str());publish(UpdateState::Error,error);return;}
        {std::lock_guard<std::mutex> lock(mutex_);status_.release=std::move(release);status_.percent=100;status_.state=UpdateState::Ready;}
    });return true;
}
bool SelfUpdater::validatePending(std::string &error) const {
    if(executable_.empty())return false;
    std::ifstream ready(executable_.string()+".ready");std::string hash,tag;std::uint64_t bytes=0;
    if(!(ready>>hash>>tag>>bytes)||hashValue(hash)!=hash||!updateVersionNewer(tag,current_))return false;
    return validateNro(executable_.string()+".new",hash,bytes,tag,error);
}
bool SelfUpdater::applyPending(std::string &error){
    stop();error.clear();std::error_code ec;
    if(executable_.empty()||!std::filesystem::exists(executable_.string()+".ready",ec))return false;
    if(!validatePending(error)){if(error.empty())error="Pending update is invalid";return false;}
    const auto path=executable_.string(),backup=path+".old";
    // FAT/newlib cannot always replace an existing file with rename. Retain a
    // backup until installation succeeds, and restore it after a failed swap.
    std::remove(backup.c_str());if(std::rename(path.c_str(),backup.c_str())!=0){error="Cannot move the installed NRO";return false;}
    if(std::rename((path+".new").c_str(),path.c_str())!=0){const auto reason=errno;if(std::rename(backup.c_str(),path.c_str())!=0)error="Update failed; restore the NRO from its .old backup";else error=std::string("Cannot install update: ")+std::strerror(reason);return false;}
    std::remove((path+".ready").c_str());std::remove(backup.c_str());return true;
}
} // namespace papernx
