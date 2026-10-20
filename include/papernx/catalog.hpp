#pragma once
#include "papernx/core.hpp"
#include <optional>

namespace papernx {
enum class ReadingStatus { Automatic, NotStarted, Reading, Completed };
// Explicit library status has user-visible position semantics, not just a filter tag.
// Preserve bookmarks, appearance overrides, and reading journal history.
inline void applyManualReadingStatus(Progress &p, ReadingStatus status, std::int64_t nowUtc=0) {
    if(status==ReadingStatus::Automatic)return;
    if(status==ReadingStatus::NotStarted) {
        p.page=0;p.fraction=0.0;p.lastRead=0;
        p.muBookmark=-1;p.anchorChapter=-1;p.anchorCharacter=-1;p.anchorOffset=0;
        p.chapter=-1;
    } else if(status==ReadingStatus::Completed) {
        p.fraction=1.0;
        p.page=p.pageCount>0?p.pageCount-1:0;
        p.muBookmark=-1;p.anchorChapter=-1;p.anchorCharacter=-1;p.anchorOffset=0;
        if(nowUtc>0 && p.lastRead<=0)p.lastRead=nowUtc;
    } else if(status==ReadingStatus::Reading) {
        // Returning a completed book to "Reading" starts a new read rather than
        // inventing 99% progress. A real intermediate position is preserved.
        if(p.fraction>=1.0) {
            p.fraction=0.0;p.page=0;
            p.muBookmark=-1;p.anchorChapter=-1;p.anchorCharacter=-1;p.anchorOffset=0;
        }
        if(nowUtc>0 && p.lastRead<=0)p.lastRead=nowUtc;
    }
}

enum class CatalogGroup { Authors, Genres, Collections, Series, Formats };
struct CatalogEntry {
    std::string title, author, genre, series, tags;
    std::string sourceTitle, sourceAuthor, sourceGenre, sourceSeries, metadataFingerprint, sourceSeriesIndex;
    std::vector<std::string> collections;
    bool hidden{false};
    ReadingStatus status{ReadingStatus::Automatic};
};
// Independent, versioned sidecar. Never writes the user's progress/bookmark file.
// Reuses its checksummed, flushed, backup/recovery persistence transport.
class CatalogStore {
public:
    explicit CatalogStore(fs::path path);
    bool load();
    bool save() const;
    bool save(const ProgressStore::SaveOptions &) const;
    const std::string &diagnostic() const { return diagnostic_; }
    CatalogEntry get(const std::string &id) const;
    bool put(const std::string &id, CatalogEntry entry);
    Book apply(Book book) const;
    std::vector<Book> visible(const std::vector<Book> &) const;
    std::map<std::string, std::size_t> groups(const std::vector<Book> &, CatalogGroup) const;
    std::vector<Book> filter(const std::vector<Book> &, CatalogGroup, const std::string &value) const;
    const std::map<std::string, CatalogEntry> &entries() const { return entries_; }
    const std::map<std::string, std::string> &collections() const { return collections_; }
    std::string createCollection(const std::string &name);
    bool renameCollection(const std::string &id, const std::string &name);
    bool deleteCollection(const std::string &id);
    bool addToCollection(const std::string &book, const std::string &collection);
    bool removeFromCollection(const std::string &book, const std::string &collection);
    // Selection must be a fresh scanner Book. Rejects links, directories, root
    // escapes, changed identity/size and formats not supported by the scanner.
    bool validateDeletion(const Book &, const fs::path &allowedRoot, std::string &error) const;
    bool deleteBookFile(const Book &, const fs::path &allowedRoot, bool confirmed, std::string &error);
private:
    fs::path path_;
    std::map<std::string, CatalogEntry> entries_;
    std::map<std::string, std::string> collections_;
    mutable std::string diagnostic_;
    bool writable_{true};
};
} // namespace papernx
