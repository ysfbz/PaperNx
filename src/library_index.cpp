#include "papernx/library_index.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace papernx {
namespace {
constexpr const char *inventorySchemaId="libraryindex0001";
constexpr const char *inventorySchema="PAPERNX-LIBRARY-1";
}
std::optional<std::uintmax_t> regularBookFileSize(const fs::path &path) {
#if !defined(_WIN32) && defined(O_NOFOLLOW)
    int flags=O_RDONLY|O_NONBLOCK|O_NOFOLLOW;
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    const int file=::open(path.c_str(),flags);
    if(file<0)return std::nullopt;
    struct stat info{};
    const int status=::fstat(file,&info);
    ::close(file);
    if(status!=0 || !S_ISREG(info.st_mode) || info.st_size<0)return std::nullopt;
    return static_cast<std::uintmax_t>(info.st_size);
#else
    std::error_code ec;const auto status=fs::symlink_status(path,ec);
    if(ec || fs::is_symlink(status) || !fs::is_regular_file(status))return std::nullopt;
    const auto size=fs::file_size(path,ec);
    if(ec)return std::nullopt;
    return size;
#endif
}
std::vector<Book> loadLibraryInventory(const fs::path &path) {
    ProgressStore store(path);std::vector<Book> books;
    if(!store.load())return books;
    const auto marker=store.all().find(inventorySchemaId);
    if(marker!=store.all().end() && marker->second.bookTitle!=inventorySchema)return books;
    // An isolated inventory adapts the existing atomic/checksummed codec:
    // bookPath=path, bookTitle=title, bookAuthor=byte count, lastRead=file mtime.
    for(const auto &[id,p]:store.all()) {
        if(id==inventorySchemaId)continue;
        if(p.bookPath.empty() || p.bookAuthor.empty())continue;
        std::uintmax_t size=0;const auto parsed=std::from_chars(p.bookAuthor.data(),p.bookAuthor.data()+p.bookAuthor.size(),size);
        if(parsed.ec!=std::errc{} || parsed.ptr!=p.bookAuthor.data()+p.bookAuthor.size() || documentId(p.bookPath,size)!=id)continue;
        const auto bookPath=fs::u8path(p.bookPath);const auto actualSize=regularBookFileSize(bookPath);
        if(!actualSize || *actualSize!=size)continue;
        const auto type=detectFormat(bookPath);if(type==Format::Unsupported)continue;
        Book b;b.id=id;b.path=p.bookPath;b.bytes=size;b.title=p.bookTitle;b.modified=p.lastRead;b.format=type;
        std::error_code ec;const auto stamp=fs::last_write_time(bookPath,ec);
        if(!ec)b.modified=std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
        books.push_back(std::move(b));if(books.size()==10000)break;
    }
    return books;
}
bool LibraryScanner::start(std::vector<fs::path> roots,std::vector<Book> known,fs::path inventory) {
    if(pending())return false;
    cancel_=false;ready_=false;result_={};
    worker_=std::thread([this,roots=std::move(roots),known=std::move(known),inventory=std::move(inventory)] {
        LibraryScanResult result;
#if defined(__cpp_exceptions)
        try {
#endif
            std::unordered_map<std::string,Book> prior;prior.reserve(known.size());
            for(auto &book:known){const auto id=book.id;prior.emplace(id,std::move(book));}
            std::unordered_set<std::string> ids;ids.reserve(10000);
            auto add=[&](Book book) {
                if(result.books.size()>=10000 || !ids.insert(book.id).second)return;
                auto old=prior.find(book.id);if(old!=prior.end()){book.title=old->second.title;book.author=old->second.author;}
                result.books.push_back(std::move(book));
            };
            bool scanComplete=true;
            for(auto &book:scanRoots(roots,10000,[this]{return cancel_.load();},&scanComplete))add(std::move(book));
            if(!scanComplete) {
                result.error="Library scan incomplete";result.cancelled=cancel_.load();
                result_=std::move(result);ready_.store(true);return;
            }
            // Independently opened files remain available. Missing source files
            // leave the inventory without deleting their progress or metadata.
            for(const auto &[id,old]:prior) {
                if(cancel_)break;
                if(ids.count(id))continue;
                const auto path=fs::u8path(old.path);const auto size=regularBookFileSize(path);
                if(detectFormat(path)==Format::Unsupported || !size)continue;
                auto book=old;book.bytes=*size;book.id=documentId(book.path,*size);
                if(book.id!=id){book.title=defaultTitle(path);book.author.clear();}
                std::error_code ec;
                auto stamp=fs::last_write_time(path,ec);if(!ec)book.modified=std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
                add(std::move(book));
            }
            std::sort(result.books.begin(),result.books.end(),[](const Book &a,const Book &b){return naturalCompare(a.path,b.path)<0;});
            result.complete=true;
            if(!cancel_) {
                ProgressStore cache(inventory);
                Progress marker;marker.bookTitle=inventorySchema;cache.put(inventorySchemaId,std::move(marker));
                for(const auto &b:result.books){if(cancel_)break;Progress p;p.bookPath=b.path;p.bookTitle=b.title;p.bookAuthor=std::to_string(b.bytes);p.lastRead=b.modified;cache.put(b.id,std::move(p));}
                if(!cancel_){result.cached=cache.save();if(!result.cached)result.error="Library cache could not be saved";}
            }
#if defined(__cpp_exceptions)
        } catch(const std::exception &error) {result.error=error.what();}
#endif
        result.cancelled=cancel_.load();
        // A cancelled scan must never publish as a complete (possibly partial) inventory.
        if(result.cancelled)result.complete=false;
        result_=std::move(result);ready_.store(true);
    });
    return true;
}
std::optional<LibraryScanResult> LibraryScanner::take() {
    if(!ready_)return std::nullopt;
    worker_.join();
    auto result=std::move(result_);
    result.cancelled=result.cancelled||cancel_.load();
    ready_=false;cancel_=false;
    return result;
}
void LibraryScanner::stop() {cancel_=true;if(worker_.joinable())worker_.join();ready_=false;}
} // namespace papernx
