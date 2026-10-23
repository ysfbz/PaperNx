#include "papernx/library_index.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace papernx {
std::vector<Book> loadLibraryInventory(const fs::path &path) {
    ProgressStore store(path);std::vector<Book> books;
    if(!store.load())return books;
    // An isolated inventory adapts the existing atomic/checksummed codec:
    // bookPath=path, bookTitle=title, bookAuthor=byte count, lastRead=file mtime.
    for(const auto &[id,p]:store.all()) {
        if(p.bookPath.empty() || p.bookAuthor.empty())continue;
        std::uintmax_t size=0;const auto parsed=std::from_chars(p.bookAuthor.data(),p.bookAuthor.data()+p.bookAuthor.size(),size);
        if(parsed.ec!=std::errc{} || parsed.ptr!=p.bookAuthor.data()+p.bookAuthor.size() || documentId(p.bookPath,size)!=id)continue;
        const auto type=detectFormat(fs::u8path(p.bookPath));if(type==Format::Unsupported)continue;
        Book b;b.id=id;b.path=p.bookPath;b.bytes=size;b.title=p.bookTitle;b.modified=p.lastRead;b.format=type;
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
            for(auto &book:scanRoots(roots,10000,[this]{return cancel_.load();}))add(std::move(book));
            // Independently opened files remain available. Missing source files
            // leave the inventory without deleting their progress or metadata.
            for(const auto &[id,old]:prior) {
                if(cancel_)break;
                if(ids.count(id))continue;
                std::error_code ec;const auto path=fs::u8path(old.path);const auto size=fs::file_size(path,ec);if(ec)continue;
                auto book=old;book.bytes=size;book.id=documentId(book.path,size);
                if(book.id!=id){book.title=defaultTitle(path);book.author.clear();}
                auto stamp=fs::last_write_time(path,ec);if(!ec)book.modified=std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
                add(std::move(book));
            }
            std::sort(result.books.begin(),result.books.end(),[](const Book &a,const Book &b){return naturalCompare(a.path,b.path)<0;});
            if(!cancel_) {
                ProgressStore cache(inventory);
                for(const auto &b:result.books){Progress p;p.bookPath=b.path;p.bookTitle=b.title;p.bookAuthor=std::to_string(b.bytes);p.lastRead=b.modified;cache.put(b.id,std::move(p));}
                result.cached=cache.save();if(!result.cached)result.error="Library cache could not be saved";
            }
#if defined(__cpp_exceptions)
        } catch(const std::exception &error) {result.error=error.what();}
#endif
        if(!cancel_){result_=std::move(result);ready_.store(true);}
    });
    return true;
}
std::optional<LibraryScanResult> LibraryScanner::take() {
    if(!ready_)return std::nullopt;
    worker_.join();ready_=false;return std::move(result_);
}
void LibraryScanner::stop() {cancel_=true;if(worker_.joinable())worker_.join();ready_=false;}
} // namespace papernx
