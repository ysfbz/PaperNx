#include "papernx/catalog.hpp"
#include "papernx/cover_repository.hpp"
#include <algorithm>
#include <set>

namespace papernx {
namespace {
constexpr const char *schemaId = "catalog000000001";
std::string packed(const std::vector<std::string> &values) {
    std::string out;
    for (const auto &v : values) out += std::to_string(v.size()) + ":" + v;
    return out;
}
bool unpack(const std::string &data, std::vector<std::string> &out) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const auto colon = data.find(':', offset);
        if (colon == std::string::npos || colon == offset || colon - offset > 5) return false;
        std::size_t size = 0;
        for (auto i = offset; i < colon; ++i) {
            if (data[i] < '0' || data[i] > '9') return false;
            size = size * 10 + static_cast<unsigned>(data[i] - '0');
        }
        offset = colon + 1;
        if (size > data.size() - offset || out.size() > 64) return false;
        out.push_back(data.substr(offset, size)); offset += size;
    }
    return true;
}
bool safeText(const std::string &s, std::size_t max = 1024) { return s.size() <= max && s.find('\0') == std::string::npos; }
std::string groupValue(const Book &b, const CatalogEntry &stored, CatalogGroup g) {
    if(g==CatalogGroup::Formats)return formatName(b.format);
    auto e=stored;if(!catalogMetadataCurrent(b,e)){e.sourceAuthor.clear();e.sourceGenre.clear();e.sourceSeries.clear();e.sourceSeriesIndex.clear();}
    if (g == CatalogGroup::Authors) return e.author.empty() ? (e.sourceAuthor.empty()?(b.author.empty() ? "Unknown Author" : b.author):e.sourceAuthor) : e.author;
    if (g == CatalogGroup::Genres) return e.genre.empty() ? (e.sourceGenre.empty()?"Uncategorized":e.sourceGenre) : e.genre;
    return e.series.empty() ? (e.sourceSeries.empty()?"No Series":e.sourceSeries) : e.series;
}
}
CatalogStore::CatalogStore(fs::path path) : path_(std::move(path)) {}
CatalogEntry CatalogStore::get(const std::string &id) const { const auto it = entries_.find(id); return it == entries_.end() ? CatalogEntry{} : it->second; }
bool CatalogStore::put(const std::string &id, CatalogEntry e) {
    if (id.size() != 16 || !safeText(id) || !safeText(e.title) || !safeText(e.author) ||
        !safeText(e.genre) || !safeText(e.series) || !safeText(e.tags) || !safeText(e.sourceTitle,512) || !safeText(e.sourceAuthor,512) || !safeText(e.sourceGenre,512) || !safeText(e.sourceSeries,512) || !safeText(e.sourceSeriesIndex,64) || !safeText(e.metadataFingerprint,128) || e.collections.size() > 32 ||
        static_cast<int>(e.status) < 0 || static_cast<int>(e.status) > 3) return false;
    std::sort(e.collections.begin(), e.collections.end());
    e.collections.erase(std::unique(e.collections.begin(), e.collections.end()), e.collections.end());
    for (const auto &c : e.collections) if (!collections_.count(c)) return false;
    if (packed({id,e.genre,e.series,e.tags,packed(e.collections),e.sourceTitle,e.sourceAuthor,e.sourceGenre,e.sourceSeries,e.metadataFingerprint,e.sourceSeriesIndex}).size() > 4096) return false;
    entries_[id] = std::move(e); return true;
}
bool CatalogStore::load() {
    ProgressStore store(path_);
    if (!store.load()) {std::error_code ec;for(const auto *suffix:{"",".bak",".bak.old",".tmp"})if(fs::exists(path_.string()+suffix,ec)){writable_=false;diagnostic_="Catalog is unreadable; existing files preserved";break;}return false;}
    const auto marker = store.get(schemaId);
    if (marker.bookTitle != "PAPERNX-CATALOG-1") { writable_=false;diagnostic_ = "Unsupported catalog schema; existing file preserved"; return false; }
    writable_=false;
    CatalogStore candidate(path_);
    for (const auto &r : store.all()) if (r.second.chapter == 2) {
        if (r.second.bookPath.size() != 16 || !safeText(r.second.bookTitle,128) || r.second.bookTitle.empty()) return false;
        candidate.collections_[r.second.bookPath] = r.second.bookTitle;
    }
    for (const auto &r : store.all()) if (r.second.chapter == 1) {
        std::vector<std::string> fields, shelves;
        if (!unpack(r.second.bookPath, fields) || (fields.size() != 5 && fields.size()!=10 && fields.size()!=11) || !unpack(fields[4], shelves)) return false;
        CatalogEntry e;
        e.title=r.second.bookTitle; e.author=r.second.bookAuthor; e.genre=fields[1]; e.series=fields[2]; e.tags=fields[3];
        e.collections=std::move(shelves); e.hidden=r.second.favorite; e.status=static_cast<ReadingStatus>(r.second.page);
        if(fields.size()>=10){e.sourceTitle=fields[5];e.sourceAuthor=fields[6];e.sourceGenre=fields[7];e.sourceSeries=fields[8];e.metadataFingerprint=fields[9];}
        if(fields.size()==11)e.sourceSeriesIndex=fields[10];
        if (!candidate.put(fields[0],std::move(e))) return false;
    }
    entries_.swap(candidate.entries_); collections_.swap(candidate.collections_); writable_=true;diagnostic_.clear(); return true;
}
bool CatalogStore::save() const { return save(ProgressStore::SaveOptions{}); }
bool CatalogStore::save(const ProgressStore::SaveOptions &options) const {
    if(!writable_){diagnostic_="Catalog save blocked to preserve unreadable or newer data";return false;}
    ProgressStore store(path_); // Dedicated file: no ProgressStore records are migrated or overwritten.
    Progress marker; marker.bookTitle="PAPERNX-CATALOG-1"; store.put(schemaId,marker);
    for (const auto &c : collections_) {
        Progress p; p.chapter=2; p.bookPath=c.first; p.bookTitle=c.second;
        store.put(documentId("catalog/collection/"+c.first,0),p);
    }
    for (const auto &entry : entries_) {
        const auto &e=entry.second;
        Progress p; p.chapter=1; p.bookTitle=e.title; p.bookAuthor=e.author; p.favorite=e.hidden; p.page=static_cast<int>(e.status);
        p.bookPath=packed({entry.first,e.genre,e.series,e.tags,packed(e.collections),e.sourceTitle,e.sourceAuthor,e.sourceGenre,e.sourceSeries,e.metadataFingerprint,e.sourceSeriesIndex});
        store.put(documentId("catalog/book/"+entry.first,0),p);
    }
    const bool ok=store.save(options); diagnostic_=store.saveDiagnostic(); return ok;
}
Book CatalogStore::apply(Book b) const { const auto e=get(b.id);const bool valid=catalogMetadataCurrent(b,e);
    if(valid&&!e.sourceTitle.empty())b.title=e.sourceTitle;
    if(valid&&!e.sourceAuthor.empty())b.author=e.sourceAuthor;
    if(!e.title.empty())b.title=e.title;
    if(!e.author.empty())b.author=e.author;
    return b; }
std::vector<Book> CatalogStore::visible(const std::vector<Book> &books) const {
    std::vector<Book> out; for (const auto &b:books) if(!get(b.id).hidden)out.push_back(apply(b)); return out;
}
std::map<std::string,std::size_t> CatalogStore::groups(const std::vector<Book> &books,CatalogGroup g) const {
    std::map<std::string,std::size_t> result;
    if(g==CatalogGroup::Collections)for(const auto &c:collections_)result[c.first]=0;
    for(const auto &b:books) { auto e=get(b.id); if(e.hidden)continue;
        if(g==CatalogGroup::Collections)for(const auto &c:e.collections)++result[c]; else ++result[groupValue(b,e,g)]; }
    return result;
}
std::vector<Book> CatalogStore::filter(const std::vector<Book>&books,CatalogGroup g,const std::string&value) const {
    std::vector<Book> out;
    for(const auto&b:books){const auto e=get(b.id);if(e.hidden)continue;
        if(g==CatalogGroup::Collections ? std::find(e.collections.begin(),e.collections.end(),value)!=e.collections.end() : groupValue(b,e,g)==value)out.push_back(apply(b));}
    return out;
}
std::string CatalogStore::createCollection(const std::string &name) {
    if(name.empty()||!safeText(name,128)||collections_.size()>=256)return {};
    for(const auto &c:collections_)if(turkishCaseFold(c.second)==turkishCaseFold(name))return c.first;
    unsigned serial=0; std::string id;
    do{id=documentId("collection/"+name,++serial);}while(collections_.count(id));
    collections_[id]=name;return id;
}
bool CatalogStore::renameCollection(const std::string&id,const std::string&name){
    if(!collections_.count(id)||name.empty()||!safeText(name,128))return false;
    for(const auto &c:collections_)if(c.first!=id&&turkishCaseFold(c.second)==turkishCaseFold(name))return false;
    collections_[id]=name;return true;
}
bool CatalogStore::deleteCollection(const std::string&id){
    if(!collections_.erase(id))return false;
    for(auto &e:entries_){auto &c=e.second.collections;c.erase(std::remove(c.begin(),c.end(),id),c.end());}return true;
}
bool CatalogStore::addToCollection(const std::string &book,const std::string &collection){auto e=get(book);e.collections.push_back(collection);return put(book,std::move(e));}
bool CatalogStore::removeFromCollection(const std::string &book,const std::string &collection){auto e=get(book);auto &c=e.collections;c.erase(std::remove(c.begin(),c.end(),collection),c.end());return put(book,std::move(e));}
bool CatalogStore::validateDeletion(const Book &b,const fs::path &allowedRoot,std::string &error)const{
    error.clear();std::error_code ec;
    auto reject=[&](const char *reason){error=reason;return false;};
    if(b.path.empty()||b.id.size()!=16||detectFormat(b.path)==Format::Unsupported||detectFormat(b.path)!=b.format)return reject("Not a supported selected book");
    constexpr bool nativePaths =
#ifdef __SWITCH__
        true;
#else
        false;
#endif
    const auto rootParts=cover_detail::rootInspectionPaths(allowedRoot,nativePaths);
    const auto targetParts=cover_detail::rootInspectionPaths(fs::path(b.path),nativePaths);
    if(rootParts.empty()||targetParts.empty())return reject("Invalid library path");
    const auto root=nativePaths?rootParts.back().lexically_normal():fs::canonical(rootParts.back(),ec);
    if(ec||!fs::is_directory(root,ec))return reject("Library root is unavailable");
    const auto absolute=targetParts.back();
    // Refuse traversal and symlinks at every path component, including parent folders.
    for(const auto &part:absolute)if(part=="..")return reject("Parent traversal is forbidden");
    for(const auto &walk:targetParts){
        const auto status=fs::symlink_status(walk,ec);if(ec)return reject("Book path is unavailable");if(fs::is_symlink(status))return reject("Symbolic links cannot be deleted");}
    const auto target=nativePaths?absolute.lexically_normal():fs::canonical(absolute,ec);if(ec)return reject("Book no longer exists");
    auto r=root.begin(),t=target.begin();for(;r!=root.end();++r,++t)if(t==target.end()||*r!=*t)return reject("Book is outside the library root");
    if(t==target.end()||!fs::is_regular_file(target,ec)||ec)return reject("Only regular book files may be deleted");
    const auto size=fs::file_size(target,ec);if(ec||size!=b.bytes||documentId(b.path,size)!=b.id)return reject("Book identity changed; rescan the library");
    // Require an exact current scanner entry, rather than trusting an arbitrary UI path.
    const auto selected=scan(allowedRoot);
    if(std::none_of(selected.begin(),selected.end(),[&](const Book &v){return v.id==b.id&&v.path==b.path&&v.bytes==b.bytes&&v.modified==b.modified;}))return reject("Book is not in the scanned library");
    return true;
}
bool CatalogStore::deleteBookFile(const Book &b,const fs::path &root,bool confirmed,std::string &error){
    if(!confirmed){error="Permanent deletion requires explicit confirmation";return false;}
    if(!validateDeletion(b,root,error))return false;
    std::error_code ec;if(!fs::remove(fs::path(b.path),ec)||ec){error="Unable to delete book: "+ec.message();return false;}
    // Keep overrides, memberships and reading position available for recovery/reimport.
    return true;
}
} // namespace papernx
