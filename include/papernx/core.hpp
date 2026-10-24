#pragma once
#include <cstdint>
#include <filesystem>
#include <map>
#include <functional>
#include <string>
#include <vector>

namespace papernx {
namespace fs = std::filesystem;

constexpr const char *APP_TITLE = "PaperNX";
constexpr const char *APP_AUTHOR = "Yusuf Buzrul";
constexpr const char *APP_VERSION = "0.9.1";
constexpr const char *APP_VERSION_FULL = "v0.9.1";

enum class Format { Epub, Pdf, Cbz, Cbr, Unsupported };
const char *formatName(Format format);
inline bool isComic(Format format) { return format==Format::Cbz || format==Format::Cbr; }
constexpr int COMIC_READING_COLORS=1, COMIC_RIGHT_TO_LEFT=2;
enum class Sort { Title, Author, Filename, Modified, LastRead };
enum class Category { All, ContinueReading, Recent, Favorites, Unread, Completed };

struct Book {
    std::string id;
    std::string path;
    std::string title;
    std::string author;
    Format format{Format::Unsupported};
    std::uintmax_t bytes{};
    std::int64_t modified{};
};

struct Bookmark {
    int page{0};
    std::string title;
    std::int64_t timestamp{0};
    std::int64_t muBookmark{-1};
};

struct ReaderPreferences {
    int styleOverride{0}; // Explicit EPUB appearance override; legacy records inherit global style.
    int readingMode{0}; // 0 paginated, 1 continuous
    int modeOverride{0}; // inherit global unless explicitly selected for this book
    int scrollSensitivity{100};
    int scrollInertia{1};
    int fontSize{22};
    int theme{0};
    int lineHeight{140};
    int margin{40};
    int textAlign{0};
    int fontFamily{0}; // Serif, Sans, Publisher
    int textColor{-1}; // -1 uses curated preset; otherwise RGB24
    int backgroundColor{-1};
    int paragraphSpacing{80}; // hundredths of em
    int indentation{120};
    int touchZone{30}; // percentage on each edge
    int publisherStyles{0};
    int pdfColorMode{0}; // Legacy PAPERNX-6 field; ignored by rendering. Both formats use Appearance.
    int pdfViewMode{0}; // fit page, fit width, 100%
};

// Resolves EPUB appearance and navigation independently. PDF settings remain book-specific.
ReaderPreferences resolveReadingPreferences(const ReaderPreferences &global, const ReaderPreferences &book);

struct Progress : ReaderPreferences {
    int anchorChapter{-1};
    int anchorCharacter{-1};
    double anchorOffset{0};
    int page{0};
    int pageCount{0};
    double fraction{0.0};
    bool favorite{false};
    std::int64_t muBookmark{-1};
    int chapter{-1};
    std::int64_t lastRead{0};
    std::vector<Bookmark> bookmarks;
    std::string bookPath;
    std::string bookTitle;
    std::string bookAuthor;
};

Format detectFormat(const fs::path &path);
std::string documentId(const std::string &path, std::uintmax_t size);
std::string defaultTitle(const fs::path &path);
// `complete` is false when traversal is cancelled, fails, or is truncated by a safety limit.
std::vector<Book> scan(const fs::path &root, std::size_t maxBooks = 10000,
                       const std::function<bool()> &cancelled = {}, bool *complete = nullptr);
// `complete` is false when a root fails, traversal is cancelled, or a safety limit truncates it.
std::vector<Book> scanRoots(const std::vector<fs::path> &roots, std::size_t maxBooks = 10000,
                            const std::function<bool()> &cancelled = {}, bool *complete = nullptr);

// Unicode & Turkish Support
std::string turkishCaseFold(const std::string &s);
std::string utf8SafeTruncate(const std::string &s, std::size_t maxBytes);
std::size_t utf8CodepointCount(const std::string &s);

// Overflow-safe natural order for numbered titles, filenames and series volumes.
// Negative/zero/positive result follows strcmp-style ordering after Turkish case folding.
int compareNatural(const std::string &left, const std::string &right);
int naturalCompare(const std::string &a, const std::string &b);

// Categorization & Filtering
std::vector<Book> filterByCategory(const std::vector<Book> &books, const class ProgressStore &store, Category category);
std::vector<Book> search(const std::vector<Book> &books, const std::string &needle, Sort sort = Sort::Title);
std::vector<Book> searchEx(const std::vector<Book> &books, const class ProgressStore &store,
                           const std::string &needle, Sort sort = Sort::Title, Category category = Category::All);

int restorePage(const Progress &progress, int newPageCount);

class ProgressStore {
public:
    struct SaveOptions {
        // Deterministic storage fault injection for regression tests. Empty in production.
        std::function<int(const char *)> fault;
        bool noReplaceRename{false};
        // If true at the publication boundary, keep the current primary untouched.
        std::function<bool()> cancelled;
    };
    explicit ProgressStore(fs::path path);
    bool load();
    bool save() const;
    bool save(const SaveOptions &options) const;
    const std::string &saveDiagnostic() const { return saveDiagnostic_; }
    Progress get(const std::string &id) const;
    void put(const std::string &id, Progress progress);
    std::size_t count() const;
    const std::map<std::string, Progress> &all() const { return records_; }

private:
    fs::path path_;
    std::map<std::string, Progress> records_;
    mutable std::string saveDiagnostic_;
    fs::path migrationSource_;
    int migrationVersion_{0};
};

} // namespace papernx
