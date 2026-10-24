#include "papernx/core.hpp"
#include "papernx/app_settings.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <system_error>
#include <cstdio>
#ifdef __SWITCH__
#include <switch.h>
#endif
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#else
#include <io.h>
#endif

namespace papernx {
namespace {

std::uint64_t fnv1a(const std::string &s) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

std::string hex(std::uint64_t h) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << h;
    return out.str();
}

std::string escapeString(const std::string &s) {
    std::string res;
    for (char c : s) {
        if (c == '\t') res += "\\t";
        else if (c == '\n') res += "\\n";
        else if (c == '\r') res += "\\r";
        else if (c == '\\') res += "\\\\";
        else res += c;
    }
    return res;
}

std::string unescapeString(const std::string &s) {
    std::string res;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            if (s[i + 1] == 't') { res += '\t'; ++i; }
            else if (s[i + 1] == 'n') { res += '\n'; ++i; }
            else if (s[i + 1] == 'r') { res += '\r'; ++i; }
            else if (s[i + 1] == '\\') { res += '\\'; ++i; }
            else res += s[i];
        } else {
            res += s[i];
        }
    }
    return res;
}

bool parseSafeInt(const std::string &s, int &out) {
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long val = std::strtol(s.c_str(), &end, 10);
    if (errno == ERANGE || end == s.c_str() || end != s.c_str() + s.size() ||
        val < std::numeric_limits<int>::min() || val > std::numeric_limits<int>::max()) return false;
    out = static_cast<int>(val);
    return true;
}

bool parseSafeLL(const std::string &s, std::int64_t &out) {
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    long long val = std::strtoll(s.c_str(), &end, 10);
    if (errno == ERANGE || end == s.c_str() || end != s.c_str() + s.size()) return false;
    out = static_cast<std::int64_t>(val);
    return true;
}

bool parseSafeDouble(const std::string &s, double &out) {
    if (s.empty()) return false;
    char *end = nullptr;
    errno = 0;
    double val = std::strtod(s.c_str(), &end);
    if (errno == ERANGE || end == s.c_str() || end != s.c_str() + s.size() || !std::isfinite(val)) return false;
    out = val;
    return true;
}

std::string payloadV3(const std::map<std::string, Progress> &records) {
    std::ostringstream out;
    out << "PAPERNX-6\n";
    for (const auto &[id, p] : records) {
        if (id.size() > 128 || id.find_first_of("\t\r\n") != std::string::npos) continue;
        out << id << '\t'
            << p.page << '\t'
            << p.pageCount << '\t'
            << std::setprecision(17) << p.fraction << '\t'
            << p.fontSize << '\t'
            << p.theme << '\t'
            << (p.favorite ? 1 : 0) << '\t'
            << p.muBookmark << '\t'
            << p.chapter << '\t'
            << p.lineHeight << '\t'
            << p.margin << '\t'
            << p.textAlign << '\t'
            << p.fontFamily << '\t'
            << p.lastRead << '\t'
            << p.bookmarks.size();
        for (const auto &bm : p.bookmarks) {
            out << '\t' << bm.page << '\t' << bm.timestamp << '\t'
                << bm.muBookmark << '\t' << escapeString(bm.title);
        }
        out << '\t' << escapeString(p.bookPath) << '\t' << escapeString(p.bookTitle) << '\t' << escapeString(p.bookAuthor)
            << '\t' << p.textColor << '\t' << p.backgroundColor << '\t' << p.paragraphSpacing
            << '\t' << p.indentation << '\t' << p.touchZone << '\t' << p.publisherStyles
            << '\t' << p.pdfColorMode << '\t' << p.pdfViewMode << '\t' << p.readingMode << '\t' << p.modeOverride
            << '\t' << p.scrollSensitivity << '\t' << p.scrollInertia
            << '\t' << p.anchorChapter << '\t' << p.anchorCharacter << '\t' << p.anchorOffset << '\t' << p.styleOverride << '\n';
    }
    return out.str();
}

} // namespace

std::string turkishCaseFold(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    const unsigned char *p = reinterpret_cast<const unsigned char*>(s.data());
    const std::size_t n = s.size();

    for (std::size_t i = 0; i < n; ) {
        // Turkish uppercase 'İ' is 0xC4 0xB0 -> becomes lowercase 'i' (0x69)
        if (p[i] == 0xC4 && i + 1 < n && p[i + 1] == 0xB0) {
            out.push_back('i');
            i += 2;
        }
        // ASCII uppercase 'I' (0x49) in Turkish becomes 'ı' (0xC4 0xB1)
        else if (p[i] == 'I') {
            out.push_back(static_cast<char>(0xC4));
            out.push_back(static_cast<char>(0xB1));
            i += 1;
        }
        // Turkish 'Ğ' is 0xC4 0x9E -> 'ğ' (0xC4 0x9F)
        else if (p[i] == 0xC4 && i + 1 < n && p[i + 1] == 0x9E) {
            out.push_back(static_cast<char>(0xC4));
            out.push_back(static_cast<char>(0x9F));
            i += 2;
        }
        // Turkish 'Ü' is 0xC3 0x9C -> 'ü' (0xC3 0xBC)
        else if (p[i] == 0xC3 && i + 1 < n && p[i + 1] == 0x9C) {
            out.push_back(static_cast<char>(0xC3));
            out.push_back(static_cast<char>(0xBC));
            i += 2;
        }
        // Turkish 'Ş' is 0xC5 0x9E -> 'ş' (0xC5 0x9F)
        else if (p[i] == 0xC5 && i + 1 < n && p[i + 1] == 0x9E) {
            out.push_back(static_cast<char>(0xC5));
            out.push_back(static_cast<char>(0x9F));
            i += 2;
        }
        // Turkish 'Ö' is 0xC3 0x96 -> 'ö' (0xC3 0xB6)
        else if (p[i] == 0xC3 && i + 1 < n && p[i + 1] == 0x96) {
            out.push_back(static_cast<char>(0xC3));
            out.push_back(static_cast<char>(0xB6));
            i += 2;
        }
        // Turkish 'Ç' is 0xC3 0x87 -> 'ç' (0xC3 0xA7)
        else if (p[i] == 0xC3 && i + 1 < n && p[i + 1] == 0x87) {
            out.push_back(static_cast<char>(0xC3));
            out.push_back(static_cast<char>(0xA7));
            i += 2;
        }
        // Standard ASCII A-Z -> a-z
        else if (p[i] >= 'A' && p[i] <= 'Z') {
            out.push_back(static_cast<char>(p[i] - 'A' + 'a'));
            i += 1;
        }
        else {
            out.push_back(static_cast<char>(p[i]));
            i += 1;
        }
    }
    return out;
}

// Compare entire digit runs without converting them to a fixed-width integer.
// A shorter significant run is always smaller; identical numbers prefer fewer
// leading zeros. This remains deterministic for arbitrarily long filenames.
int compareNatural(const std::string &left, const std::string &right) {
    const std::string a = turkishCaseFold(left), b = turkishCaseFold(right);
    const auto digit = [](unsigned char c) { return c >= '0' && c <= '9'; };
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (digit(static_cast<unsigned char>(a[i])) && digit(static_cast<unsigned char>(b[j]))) {
            const auto aStart = i, bStart = j;
            while (i < a.size() && digit(static_cast<unsigned char>(a[i]))) ++i;
            while (j < b.size() && digit(static_cast<unsigned char>(b[j]))) ++j;
            auto aSig = aStart, bSig = bStart;
            while (aSig < i && a[aSig] == '0') ++aSig;
            while (bSig < j && b[bSig] == '0') ++bSig;
            const auto aLen = i - aSig, bLen = j - bSig;
            if (aLen != bLen) return aLen < bLen ? -1 : 1;
            for (std::size_t k = 0; k < aLen; ++k) {
                if (a[aSig + k] != b[bSig + k]) return a[aSig + k] < b[bSig + k] ? -1 : 1;
            }
            if (i - aStart != j - bStart) return i - aStart < j - bStart ? -1 : 1;
            continue;
        }
        const auto ca = static_cast<unsigned char>(a[i++]);
        const auto cb = static_cast<unsigned char>(b[j++]);
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    return i == a.size() && j == b.size() ? 0 : i == a.size() ? -1 : 1;
}

std::string utf8SafeTruncate(const std::string &s, std::size_t maxBytes) {
    if (s.size() <= maxBytes) return s;
    if (maxBytes == 0) return "";
    std::size_t end = maxBytes;
    while (end > 0 && (static_cast<unsigned char>(s[end]) & 0xC0) == 0x80) {
        --end;
    }
    return s.substr(0, end);
}

std::size_t utf8CodepointCount(const std::string &s) {
    std::size_t count = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) ++count;
    }
    return count;
}

Format detectFormat(const fs::path &path) {
    std::string ext = path.extension().string();
    for (char &c : ext) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    if (ext == ".epub") return Format::Epub;
    if (ext == ".pdf") return Format::Pdf;
    if (ext == ".cbz") return Format::Cbz;
    if (ext == ".cbr") return Format::Cbr;
    return Format::Unsupported;
}

const char *formatName(Format format) {
    switch(format) {
        case Format::Epub:return "EPUB";
        case Format::Pdf:return "PDF";
        case Format::Cbz:return "CBZ";
        case Format::Cbr:return "CBR";
        default:return "Unknown";
    }
}

std::string defaultTitle(const fs::path &path) {
    std::string title = path.stem().u8string();
    for (char &c : title) {
        if (c == '_') c = ' ';
    }
    return title.empty() ? "Untitled" : title;
}

std::string documentId(const std::string &path, std::uintmax_t size) {
    return hex(fnv1a(path + "\n" + std::to_string(size)));
}

namespace {
void scanInto(const fs::path &root, std::size_t maxBooks, const std::function<bool()> &cancelled,
              std::vector<Book> &books, bool *complete = nullptr) {
    auto fail = [&] { if (complete) *complete = false; };
    std::error_code ec;
    if (!fs::is_directory(root, ec) || ec) { fail(); return; }

    fs::recursive_directory_iterator it(root, fs::directory_options::none, ec), end;
    if (ec) { fail(); return; }
    while (it != end && books.size() < maxBooks) {
        if (cancelled && cancelled()) break;
        if (it.depth() > 16) {
            it.disable_recursion_pending();
        } else {
            const bool symlink = it->is_symlink(ec);
            if (ec) { fail(); ec.clear(); }
            else if (symlink) it.disable_recursion_pending();
            else {
                const bool regular = it->is_regular_file(ec);
                if (ec) { fail(); ec.clear(); }
                else if (regular) {
                    const auto path = it->path();
                    const auto type = detectFormat(path);
                    if (type != Format::Unsupported) {
                        const auto size = it->file_size(ec);
                        if (ec) { fail(); ec.clear(); }
                        else {
                            Book b;
                            b.path = path.u8string();
                            b.bytes = size;
                            b.id = documentId(b.path, size);
                            b.title = defaultTitle(path);
                            b.format = type;
                            const auto mtime = it->last_write_time(ec);
                            if (!ec) b.modified = std::chrono::duration_cast<std::chrono::seconds>(mtime.time_since_epoch()).count();
                            else { fail(); ec.clear(); }
                            books.push_back(std::move(b));
                        }
                    }
                }
            }
        }
        it.increment(ec);
        if (ec) { fail(); break; }
    }
    if (complete && it != end && books.size() >= maxBooks) *complete = false;
}

std::size_t pathDepth(const fs::path &path) {
    return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

bool containsPath(const fs::path &parent, const fs::path &child) {
    for (auto current = child; !current.empty();) {
        std::error_code ec;
        if (fs::equivalent(parent, current, ec) && !ec) return true;
        auto next = current.parent_path();
        if (next == current) break;
        current = std::move(next);
    }
    return false;
}
}

std::vector<Book> scan(const fs::path &root, std::size_t maxBooks,
                       const std::function<bool()> &cancelled) {
    std::vector<Book> books;
    scanInto(root, maxBooks, cancelled, books);
    std::sort(books.begin(), books.end(), [](const Book &a, const Book &b) {
        const int order = compareNatural(a.title, b.title);
        return order ? order < 0 : a.id < b.id;
    });
    return books;
}

std::vector<Book> scanRoots(const std::vector<fs::path> &roots, std::size_t maxBooks,
                            const std::function<bool()> &cancelled, bool *complete) {
    if (complete) *complete = true;
    std::vector<fs::path> candidates;
    for (const auto &root : roots) {
        std::error_code ec;
        auto path = fs::weakly_canonical(root, ec);
        if (ec || !fs::is_directory(path, ec) || ec) {
            if (complete) *complete = false;
            continue;
        }
        candidates.push_back(std::move(path));
    }
    std::sort(candidates.begin(), candidates.end(), [](const fs::path &a, const fs::path &b) {
        const auto leftDepth = pathDepth(a), rightDepth = pathDepth(b);
        return leftDepth == rightDepth ? a < b : leftDepth < rightDepth;
    });

    std::vector<fs::path> uniqueRoots;
    uniqueRoots.reserve(candidates.size());
    for (const auto &candidate : candidates) {
        if (std::none_of(uniqueRoots.begin(), uniqueRoots.end(), [&](const fs::path &root) {
                return containsPath(root, candidate);
            })) uniqueRoots.push_back(candidate);
    }

    std::vector<Book> books;
    for (std::size_t i = 0; i < uniqueRoots.size(); ++i) {
        if (cancelled && cancelled()) break;
        if (books.size() >= maxBooks) {
            if (complete) *complete = false;
            break;
        }
        const auto &root = uniqueRoots[i];
        scanInto(root, maxBooks, cancelled, books, complete);
    }
    std::sort(books.begin(), books.end(), [](const Book &a, const Book &b) {
        const int order = naturalCompare(a.path, b.path);
        return order ? order < 0 : a.id < b.id;
    });
    return books;
}

std::vector<Book> filterByCategory(const std::vector<Book> &books, const ProgressStore &store, Category category) {
    if (category == Category::All) return books;
    std::vector<Book> result;
    result.reserve(books.size());

    for (const auto &b : books) {
        auto p = store.get(b.id);
        const bool continuousEpub = (b.format == Format::Epub || isComic(b.format)) && p.readingMode == 1;
        switch (category) {
            case Category::ContinueReading:
                if (p.lastRead > 0 && p.fraction < 0.98 &&
                    (continuousEpub ? p.pageCount > 0 : p.pageCount > 1 && p.page < p.pageCount - 1))
                    result.push_back(b);
                break;
            case Category::Recent:
                if (p.lastRead > 0)
                    result.push_back(b);
                break;
            case Category::Favorites:
                if (p.favorite)
                    result.push_back(b);
                break;
            case Category::Unread:
                if (p.lastRead == 0 && p.page == 0 && p.fraction <= 0.001)
                    result.push_back(b);
                break;
            case Category::Completed:
                if (p.pageCount > 0 && (p.fraction >= 0.98 || (!continuousEpub && p.page >= p.pageCount - 1)))
                    result.push_back(b);
                break;
            default:
                result.push_back(b);
                break;
        }
    }
    return result;
}

int naturalCompare(const std::string &a,const std::string &b) {
    const auto left=turkishCaseFold(a),right=turkishCaseFold(b);
    std::size_t i=0,j=0;
    while(i<left.size() && j<right.size()) {
        if(left[i]>='0' && left[i]<='9' && right[j]>='0' && right[j]<='9') {
            auto ie=i,je=j;
            while(ie<left.size() && left[ie]>='0' && left[ie]<='9') ++ie;
            while(je<right.size() && right[je]>='0' && right[je]<='9') ++je;
            auto iz=i,jz=j;
            while(iz<ie && left[iz]=='0') ++iz;
            while(jz<je && right[jz]=='0') ++jz;
            if(ie-iz!=je-jz) return ie-iz<je-jz?-1:1;
            int n=left.compare(iz,ie-iz,right,jz,je-jz); if(n) return n<0?-1:1;
            i=ie;j=je;continue;
        }
        if(left[i]!=right[j]) return static_cast<unsigned char>(left[i])<static_cast<unsigned char>(right[j])?-1:1;
        ++i;++j;
    }
    return i==left.size()?(j==right.size()?0:-1):1;
}

std::vector<Book> search(const std::vector<Book> &books, const std::string &needle, Sort sort) {
    std::vector<Book> result;
    const std::string q = turkishCaseFold(needle);

    for (const auto &b : books) {
        if (q.empty() ||
            turkishCaseFold(b.title).find(q) != std::string::npos ||
            turkishCaseFold(b.author).find(q) != std::string::npos ||
            turkishCaseFold(fs::path(b.path).filename().string()).find(q) != std::string::npos) {
            result.push_back(b);
        }
    }

    std::sort(result.begin(), result.end(), [sort](const Book &a, const Book &b) {
        if (sort == Sort::Modified) {
            return a.modified == b.modified ? a.id < b.id : a.modified > b.modified;
        }
        if (sort == Sort::Author) {
            const int byAuthor = compareNatural(a.author, b.author);
            if (byAuthor) return byAuthor < 0;
        }
        if (sort == Sort::Filename) {
            const int byFile = compareNatural(fs::path(a.path).filename().string(), fs::path(b.path).filename().string());
            return byFile ? byFile < 0 : a.id < b.id;
        }
        const int byTitle = compareNatural(a.title, b.title);
        return byTitle ? byTitle < 0 : a.id < b.id;
    });

    return result;
}

std::vector<Book> searchEx(const std::vector<Book> &books, const ProgressStore &store,
                           const std::string &needle, Sort sort, Category category) {
    auto filtered = filterByCategory(books, store, category);
    auto res = search(filtered, needle, sort);

    if (sort == Sort::LastRead || category == Category::Recent || category == Category::ContinueReading) {
        std::sort(res.begin(), res.end(), [&store](const Book &a, const Book &b) {
            auto pa = store.get(a.id), pb = store.get(b.id);
            if (pa.lastRead != pb.lastRead) return pa.lastRead > pb.lastRead;
            return a.id < b.id;
        });
    }
    return res;
}

int restorePage(const Progress &p, int n) {
    if (n <= 0) return 0;
    if (p.pageCount == n) return std::clamp(p.page, 0, n - 1);
    const double f = std::isfinite(p.fraction) ? std::clamp(p.fraction, 0.0, 1.0) : 0.0;
    return std::clamp(static_cast<int>(std::round(f * (n - 1))), 0, n - 1);
}

ReaderPreferences resolveReadingPreferences(const ReaderPreferences &global, const ReaderPreferences &book) {
    ReaderPreferences resolved = book.styleOverride ? book : global;
    resolved.styleOverride = book.styleOverride;
    resolved.modeOverride = book.modeOverride;
    const auto &navigation = book.modeOverride ? book : global;
    resolved.readingMode = navigation.readingMode;
    resolved.scrollSensitivity = navigation.scrollSensitivity;
    resolved.scrollInertia = navigation.scrollInertia;
    // Fixed-layout display choices must not be replaced by EPUB appearance inheritance.
    resolved.pdfColorMode = book.pdfColorMode;
    resolved.pdfViewMode = book.pdfViewMode;
    return resolved;
}

AppSettingsStore::AppSettingsStore(fs::path path) : store_(std::move(path)) {}

void AppSettingsStore::put(AppPreferences preferences) {
    preferences.uiTheme = std::clamp(preferences.uiTheme, 0, 5);
    preferences.language = std::clamp(preferences.language, 0, 1);
    preferences.libraryLayout = std::clamp(preferences.libraryLayout, 0, 1);
    preferences.sort = std::clamp(preferences.sort, 0, 4);
    preferences.toolbar = std::clamp(preferences.toolbar, 0, 1);
    // Migrate old independent UI-theme policies to a single global reading theme.
    // Keep the legacy fields on disk for file compatibility, but never reactivate
    // conflicting application/custom chrome after upgrading or restarting.
    preferences.chromePolicy = 0;
    preferences.customUiTheme = std::clamp(preferences.customUiTheme, 0, 5);
    preferences.displayOrientation = std::clamp(preferences.displayOrientation, 0, 2);
    preferences.paperTexture = std::clamp(preferences.paperTexture, 0, 2);
    preferences.softwareBrightness = std::clamp(preferences.softwareBrightness, 50, 100);
    preferences.systemBrightness = std::clamp(preferences.systemBrightness, 10, 100);
    preferences.interfaceStyle = std::clamp(preferences.interfaceStyle, 0, 2);
    preferences.interfaceAccent = std::clamp(preferences.interfaceAccent, 0, 6);
    std::vector<std::string> folders;
    for(const auto &folder:preferences.libraryFolders) {
        if(folder.empty())continue;
        const auto path=fs::u8path(utf8SafeTruncate(folder,4096)).lexically_normal().u8string();
        if(std::find(folders.begin(),folders.end(),path)==folders.end())folders.push_back(path);
        if(folders.size()==32)break;
    }
    preferences.libraryFolders = std::move(folders);
    preferences.defaultBrowserPath = utf8SafeTruncate(preferences.defaultBrowserPath, 4096);
    preferences_ = std::move(preferences);
}

bool AppSettingsStore::load() {
    if (!store_.load() || !store_.all().count("APP_PREFERENCES_")) return false;
    const auto record = store_.get("APP_PREFERENCES_");
    AppPreferences preferences;
    preferences.uiTheme = record.theme;
    preferences.language = record.fontFamily;
    preferences.libraryLayout = record.textAlign;
    preferences.sort = record.fontSize - 12;
    preferences.toolbar = record.pdfViewMode;
    preferences.defaultBrowserPath = record.bookPath;
    if(store_.all().count("APP_UI_THEME_016")) {
        const auto theme=store_.get("APP_UI_THEME_016");
        preferences.interfaceStyle=theme.fontFamily;preferences.interfaceAccent=theme.fontSize-12;
        preferences.scanOnLaunch=theme.scrollInertia!=0;
    }
    for(int index=0;index<32;++index) {
        char key[32];std::snprintf(key,sizeof key,"LIB_FOLDER_%05d",index);
        const auto folder=store_.get(key).bookPath;
        if(!folder.empty())preferences.libraryFolders.push_back(folder);
    }
    if(store_.all().count("APP_READER_V015_")) {
        const auto layout=store_.get("APP_READER_V015_");
        preferences.expandedViewport=layout.textAlign!=0;
    }
    // v015's true value was also its default, so it is not a reliable user preference.
    if(store_.all().count("APP_READER_V092_")) {
        const auto layout=store_.get("APP_READER_V092_");
        preferences.bookLandscapeSpreads=layout.textAlign!=0;
        preferences.comicLandscapeSpreads=layout.publisherStyles!=0;
    }
    if(store_.all().count("APP_DISPLAY_V04_")) {
        const auto display=store_.get("APP_DISPLAY_V04_");
        preferences.chromePolicy=display.fontFamily;
        preferences.customUiTheme=display.theme;
        preferences.displayOrientation=display.pdfViewMode;
        preferences.preservePdfZoom=display.scrollInertia!=0;
        preferences.shoulderPageTurns=display.publisherStyles==0;
        preferences.pageCurlAnimation=display.readingMode==0;
        preferences.paperTexture=display.pdfColorMode;
        preferences.softwareBrightness=display.scrollSensitivity; // Previously unused in APP_DISPLAY_V04_.
    }
    if(store_.all().count("APP_UPDATE_V092_")) {
        const auto updater=store_.get("APP_UPDATE_V092_");
        preferences.automaticUpdateCheck=updater.favorite;
        preferences.systemBrightness=updater.page;
    }
    put(std::move(preferences));
    return true;
}

bool AppSettingsStore::save() const {
    Progress record;
    record.theme = preferences_.uiTheme;
    record.fontFamily = preferences_.language;
    record.textAlign = preferences_.libraryLayout;
    record.fontSize = 12 + preferences_.sort;
    record.pdfViewMode = preferences_.toolbar;
    record.bookPath = preferences_.defaultBrowserPath;
    store_.put("APP_PREFERENCES_", std::move(record));
    Progress display;
    display.fontFamily=preferences_.chromePolicy;
    display.theme=preferences_.customUiTheme;
    display.pdfViewMode=preferences_.displayOrientation;
    display.scrollInertia=preferences_.preservePdfZoom?1:0;
    display.publisherStyles=preferences_.shoulderPageTurns?0:1;
    display.readingMode=preferences_.pageCurlAnimation?0:1;
    display.pdfColorMode=preferences_.paperTexture;
    display.scrollSensitivity=preferences_.softwareBrightness;
    store_.put("APP_DISPLAY_V04_",std::move(display));
    Progress layout;
    layout.textAlign=preferences_.expandedViewport?1:0;
    layout.publisherStyles=0;
    store_.put("APP_READER_V015_",std::move(layout));
    Progress spreads;
    spreads.textAlign=preferences_.bookLandscapeSpreads?1:0;
    spreads.publisherStyles=preferences_.comicLandscapeSpreads?1:0;
    store_.put("APP_READER_V092_",std::move(spreads));
    Progress theme;
    theme.fontFamily=preferences_.interfaceStyle;theme.fontSize=12+preferences_.interfaceAccent;
    theme.scrollInertia=preferences_.scanOnLaunch?1:0;
    store_.put("APP_UI_THEME_016",std::move(theme));
    Progress updater;
    updater.favorite=preferences_.automaticUpdateCheck;
    updater.page=preferences_.systemBrightness;
    store_.put("APP_UPDATE_V092_",std::move(updater));
    for(int index=0;index<32;++index) {
        char key[32];std::snprintf(key,sizeof key,"LIB_FOLDER_%05d",index);Progress folder;
        if(index<int(preferences_.libraryFolders.size()))folder.bookPath=preferences_.libraryFolders[index];
        store_.put(key,std::move(folder));
    }
    return store_.save();
}

ProgressStore::ProgressStore(fs::path p) : path_(std::move(p)) {}

Progress ProgressStore::get(const std::string &id) const {
    auto it = records_.find(id);
    return it == records_.end() ? Progress{} : it->second;
}

void ProgressStore::put(const std::string &id, Progress p) {
    p.page = std::max(0, p.page);
    p.pageCount = std::max(0, p.pageCount);
    p.fraction = std::isfinite(p.fraction) ? std::clamp(p.fraction, 0.0, 1.0) : 0.0;
    p.readingMode=std::clamp(p.readingMode,0,1);
    p.modeOverride=std::clamp(p.modeOverride,0,1);
    p.styleOverride=std::clamp(p.styleOverride,0,1);
    p.scrollSensitivity=std::clamp(p.scrollSensitivity,50,200);
    p.scrollInertia=std::clamp(p.scrollInertia,0,1);
    p.fontSize = std::clamp(p.fontSize, 12, 48);
    p.theme = std::clamp(p.theme, 0, 5);
    p.lineHeight = std::clamp(p.lineHeight, 100, 220);
    p.margin = std::clamp(p.margin, 10, 120);
    p.textAlign = std::clamp(p.textAlign, 0, 1);
    p.fontFamily = std::clamp(p.fontFamily, 0, 2);
    p.textColor = std::clamp(p.textColor, -1, 0xffffff);
    p.backgroundColor = std::clamp(p.backgroundColor, -1, 0xffffff);
    p.paragraphSpacing = std::clamp(p.paragraphSpacing, 0, 200);
    p.indentation = std::clamp(p.indentation, 0, 200);
    p.touchZone = std::clamp(p.touchZone, 20, 40);
    p.publisherStyles = std::clamp(p.publisherStyles, 0, 1);
    p.pdfColorMode = std::clamp(p.pdfColorMode, 0, 3);
    p.pdfViewMode = std::clamp(p.pdfViewMode, 0, 2);
    p.bookPath = utf8SafeTruncate(p.bookPath, 4096);
    p.bookTitle = utf8SafeTruncate(p.bookTitle, 1024);
    p.bookAuthor = utf8SafeTruncate(p.bookAuthor, 1024);
    if (p.bookmarks.size() > 1024) p.bookmarks.resize(1024);
    for (auto &bm : p.bookmarks) {
        bm.page = std::max(0, bm.page);
        bm.title = utf8SafeTruncate(bm.title, 1024);
    }
    records_[id] = std::move(p);
}

std::size_t ProgressStore::count() const {
    return records_.size();
}

bool ProgressStore::load() {
    auto readFile = [&](const fs::path &candidate) -> bool {
        std::ifstream in(candidate, std::ios::binary);
        if (!in) return false;

        std::error_code sizeError;
        auto size = fs::file_size(candidate, sizeError);
        if (sizeError || size > 8 * 1024 * 1024) return false;
        std::string encoded((std::istreambuf_iterator<char>(in)), {});
        if (encoded.size() < 20 || encoded.size() > 8 * 1024 * 1024) return false;

        const auto split = encoded.find('\n');
        if (split == std::string::npos) return false;

        const std::string header = encoded.substr(0, split);
        const std::string body = encoded.substr(split + 1);

        if (header != "CHECKSUM=" + hex(fnv1a(body))) return false;

        std::istringstream rows(body);
        std::string versionLine;
        if (!std::getline(rows, versionLine)) return false;

        std::map<std::string, Progress> restored;

        if (versionLine == "PAPERNX-2" || versionLine == "PAPERNX-3" || versionLine == "PAPERNX-4" || (versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6")) {
            std::string line;
            while (std::getline(rows, line)) {
                if (line.empty()) continue;
                std::vector<std::string> tokens;
                std::string token;
                std::istringstream tokenStream(line);
                while (std::getline(tokenStream, token, '\t')) {
                    tokens.push_back(token);
                }
                if (!line.empty() && line.back() == '\t') tokens.emplace_back();
                if (tokens.size() < 15) continue;

                Progress p;
                std::string id = tokens[0];
                int fav = 0, bmCount = 0;

                bool valid = parseSafeInt(tokens[1], p.page) &&
                             parseSafeInt(tokens[2], p.pageCount) &&
                             parseSafeDouble(tokens[3], p.fraction) &&
                             parseSafeInt(tokens[4], p.fontSize) &&
                             parseSafeInt(tokens[5], p.theme) &&
                             parseSafeInt(tokens[6], fav) &&
                             parseSafeLL(tokens[7], p.muBookmark) &&
                             parseSafeInt(tokens[8], p.chapter) &&
                             parseSafeInt(tokens[9], p.lineHeight) &&
                             parseSafeInt(tokens[10], p.margin) &&
                             parseSafeInt(tokens[11], p.textAlign) &&
                             parseSafeInt(tokens[12], p.fontFamily) &&
                             parseSafeLL(tokens[13], p.lastRead) &&
                             parseSafeInt(tokens[14], bmCount);

                if (!valid || bmCount < 0 || bmCount > 1024 ||
                    tokens.size() != 15 + static_cast<std::size_t>(bmCount) * 4 + ((versionLine == "PAPERNX-4" || (versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6")) ? ((versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6") ? (versionLine == "PAPERNX-6" ? 19 : 18) : 11) : versionLine == "PAPERNX-3" ? 3 : 0) ||
                    p.page < 0 || p.pageCount < 0 || p.fraction < 0 || p.fraction > 1 ||
                    p.fontSize < 12 || p.fontSize > 48 || p.theme < 0 || p.theme > 5 || fav < 0 || fav > 1 ||
                    p.lineHeight < 100 || p.lineHeight > 220 || p.margin < 10 || p.margin > 120 ||
                    p.textAlign < 0 || p.textAlign > 1 || p.fontFamily < 0 || p.fontFamily > ((versionLine == "PAPERNX-4" || (versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6")) ? 2 : 1)) continue;
                p.favorite = (fav != 0);

                std::size_t cursor = 15;
                bool bookmarksValid = true;
                for (int b = 0; b < bmCount && cursor + 3 < tokens.size(); ++b) {
                    Bookmark bm;
                    if (!parseSafeInt(tokens[cursor++], bm.page) ||
                        !parseSafeLL(tokens[cursor++], bm.timestamp) ||
                        !parseSafeLL(tokens[cursor++], bm.muBookmark)) {
                        bookmarksValid = false;
                        break;
                    }
                    bm.title = unescapeString(tokens[cursor++]);
                    if (bm.page < 0 || bm.timestamp < 0 || bm.title.size() > 1024) { bookmarksValid = false; break; }
                    p.bookmarks.push_back(std::move(bm));
                }

                if ((versionLine == "PAPERNX-3" || (versionLine == "PAPERNX-4" || (versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6"))) && bookmarksValid) {
                    p.bookPath = unescapeString(tokens[cursor++]);
                    p.bookTitle = unescapeString(tokens[cursor++]);
                    p.bookAuthor = unescapeString(tokens[cursor++]);
                    if (p.bookPath.size() > 4096 || p.bookTitle.size() > 1024 || p.bookAuthor.size() > 1024) continue;
                }
                if ((versionLine == "PAPERNX-4" || (versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6")) && bookmarksValid) {
                    if (!parseSafeInt(tokens[cursor++], p.textColor) || !parseSafeInt(tokens[cursor++], p.backgroundColor) ||
                        !parseSafeInt(tokens[cursor++], p.paragraphSpacing) || !parseSafeInt(tokens[cursor++], p.indentation) ||
                        !parseSafeInt(tokens[cursor++], p.touchZone) || !parseSafeInt(tokens[cursor++], p.publisherStyles) ||
                        !parseSafeInt(tokens[cursor++], p.pdfColorMode) || !parseSafeInt(tokens[cursor++], p.pdfViewMode) ||
                        p.textColor < -1 || p.textColor > 0xffffff || p.backgroundColor < -1 || p.backgroundColor > 0xffffff ||
                        p.paragraphSpacing < 0 || p.paragraphSpacing > 200 || p.indentation < 0 || p.indentation > 200 ||
                        p.touchZone < 20 || p.touchZone > 40 || p.publisherStyles < 0 || p.publisherStyles > 1 ||
                        p.pdfColorMode < 0 || p.pdfColorMode > 3 || p.pdfViewMode < 0 || p.pdfViewMode > 2) continue;
                }
                if ((versionLine == "PAPERNX-5" || versionLine == "PAPERNX-6") && bookmarksValid) {
                    if (!parseSafeInt(tokens[cursor++], p.readingMode) || !parseSafeInt(tokens[cursor++], p.modeOverride) ||
                        !parseSafeInt(tokens[cursor++], p.scrollSensitivity) || !parseSafeInt(tokens[cursor++], p.scrollInertia) ||
                        !parseSafeInt(tokens[cursor++], p.anchorChapter) || !parseSafeInt(tokens[cursor++], p.anchorCharacter) ||
                        !parseSafeDouble(tokens[cursor++], p.anchorOffset) || p.readingMode < 0 || p.readingMode > 1 ||
                        p.modeOverride < 0 || p.modeOverride > 1 || p.scrollSensitivity < 50 || p.scrollSensitivity > 200 ||
                        p.scrollInertia < 0 || p.scrollInertia > 1 || p.anchorChapter < -1 || p.anchorCharacter < -1 ||
                        !std::isfinite(p.anchorOffset) || p.anchorOffset < -4 || p.anchorOffset > 4) continue;
                }
                if (!bookmarksValid) continue;
                if (versionLine == "PAPERNX-6" && (!parseSafeInt(tokens[cursor++], p.styleOverride) || p.styleOverride < 0 || p.styleOverride > 1)) continue;
                if (bookmarksValid && id.size() == 16 && p.page >= 0 && std::isfinite(p.fraction)) {
                    restored[id] = p;
                }
            }
        } else if (versionLine == "PAPERNX-1") {
            // Backward-compatible schema migration
            std::string line;
            while (std::getline(rows, line)) {
                if (line.empty()) continue;
                std::replace(line.begin(), line.end(), '\t', ' ');
                std::istringstream row(line);
                std::string id;
                Progress p;
                int fav = 0;
                if (!(row >> id >> p.page >> p.pageCount >> p.fraction >> p.fontSize >> p.theme >> fav)) {
                    continue;
                }
                if (id.size() != 16 || p.page < 0 || p.pageCount < 0 || !std::isfinite(p.fraction)) {
                    continue;
                }
                p.favorite = (fav != 0);
                p.lastRead = 0;
                p.muBookmark = -1;
                p.chapter = -1;
                restored[id] = p;
            }
        } else {
            return false;
        }

        migrationSource_ = candidate;
        migrationVersion_ = versionLine == "PAPERNX-6" ? 0 : versionLine.back() - '0';
        records_.swap(restored);
        return true;
    };

    if (readFile(path_)) return true;

    fs::path backup = path_;
    backup += ".bak";
    if (readFile(backup)) return true;

    fs::path backupOld = path_;
    backupOld += ".bak.old";
    if (readFile(backupOld)) return true;

    fs::path temp = path_;
    temp += ".tmp";
    if (readFile(temp)) return true;
    for (int version = 5; version >= 1; --version)
        if (readFile(path_.string() + ".pre-v6-v" + std::to_string(version) + ".bak")) return true;
    return false;
}

bool ProgressStore::save() const { return save(SaveOptions{}); }

bool ProgressStore::save(const SaveOptions &options) const {
    saveDiagnostic_.clear();
    std::error_code ec;
    const fs::path temp = path_.string() + ".tmp", backup = path_.string() + ".bak", old = path_.string() + ".bak.old";
    auto read = [](const fs::path &p) {
        std::error_code e;
        const auto size = fs::file_size(p, e);
        if (e || size > 8 * 1024 * 1024) return std::string{};
        std::ifstream in(p, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(in)), {});
    };
    auto valid = [&](const fs::path &p) {
        const auto bytes = read(p);
        const auto split = bytes.find('\n');
        return split != std::string::npos && bytes.substr(0, split) == "CHECKSUM=" + hex(fnv1a(bytes.substr(split + 1)));
    };
    auto nativeError = []() -> unsigned {
#ifdef __SWITCH__
        return fsdevGetLastResult();
#else
        return 0;
#endif
    };
    auto fail = [&](const char *op, const fs::path &p, int error, unsigned native = 0) {
        const std::error_code code(error ? error : EIO, std::generic_category());
        std::ostringstream msg;
        msg << "save path=" << p.string() << " operation=" << op << " errno=" << error
            << " error_code=" << code.value() << " message=" << code.message()
            << " platform_result=0x" << std::hex << native << std::dec
            << " primary_valid=" << valid(path_) << " backup_valid=" << valid(backup)
            << " temp_valid=" << valid(temp) << " retry_possible=1";
        saveDiagnostic_ = msg.str();
        return false;
    };
    auto injected = [&](const char *op) { return options.fault ? options.fault(op) : 0; };
    auto writeVerified = [&](const fs::path &p, const std::string &bytes, const char *operation) {
        int err = injected(operation);
        if (err) return fail(operation, p, err);
        FILE *out = std::fopen(p.string().c_str(), "wb");
        if (!out) return fail("open", p, errno, nativeError());
        const char *failed = nullptr;
        unsigned native = 0;
        auto record = [&](const char *op) { if (!failed) { failed = op; err = errno; native = nativeError(); } };
        if (int fault = injected("write")) { errno = fault; record("write"); }
        else if (std::fwrite(bytes.data(), 1, bytes.size(), out) != bytes.size()) record("write");
        if (std::fflush(out) != 0) record("fflush");
        if (int fault = injected("fflush")) { errno = fault; record("fflush"); }
        const int syncFault = injected("fsync");
        if (syncFault) { errno = syncFault; record("fsync"); }
#ifdef _WIN32
        else if (_commit(_fileno(out)) != 0) record("fsync");
#else
        else if (fsync(fileno(out)) != 0) record("fsync");
#endif
        if (std::fclose(out) != 0) record("fclose");
        if (int fault = injected("fclose")) { errno = fault; record("fclose"); }
        // libnx fsync uses fsFileFlush. Unsupported flush is explicitly retryable, never success.
        if (failed) return fail(failed, p, err, native);
        if (injected("verify") || read(p) != bytes) return fail("verify", p, EIO);
        return true;
    };
    if (int err = injected("create_directories")) return fail("create_directories", path_.parent_path(), err);
    if (!path_.parent_path().empty()) fs::create_directories(path_.parent_path(), ec);
    if (ec) return fail("create_directories", path_.parent_path(), ec.value(), nativeError());
    // Keep the exact accepted pre-upgrade file forever, independent of rotating backups.
    // This runs before overwriting any temp/recovery source and aborts migration on failure.
    if (migrationVersion_ >= 1 && migrationVersion_ <= 5) {
        const fs::path original = path_.string() + ".pre-v6-v" + std::to_string(migrationVersion_) + ".bak";
        if (fs::exists(original, ec)) {
            if (!valid(original)) return fail("migration_backup_verify", original, EIO);
        } else {
            const auto originalBytes = read(migrationSource_);
            if (!valid(migrationSource_) || originalBytes.empty()) return fail("migration_source_verify", migrationSource_, EIO);
            const fs::path staging = original.string() + ".tmp";
            if (!writeVerified(staging, originalBytes, "migration_backup")) return false;
            fs::rename(staging, original, ec);
            if (ec) return fail("migration_backup_rename", original, ec.value(), nativeError());
            if (read(original) != originalBytes) return fail("migration_backup_verify", original, EIO);
        }
    }
    // Preserve the only recovery copy before truncating a leftover temp file.
    if (!valid(path_) && !valid(backup) && valid(temp)) {
        if (!writeVerified(backup, read(temp), "recovery_copy")) return false;
    }
    const auto data = payloadV3(records_);
    const auto encoded = "CHECKSUM=" + hex(fnv1a(data)) + "\n" + data;
    if (encoded.size() > 8 * 1024 * 1024) return fail("size_limit", path_, EFBIG);
    if (!writeVerified(temp, encoded, "temporary_write")) return false;
    if (valid(path_)) {
        if (valid(backup) && !writeVerified(old, read(backup), "backup_old")) return false;
        // A directory occupying a backup path must fail instead of being discarded.
        if (!writeVerified(backup, read(path_), "backup_copy")) return false;
    }
    bool noReplace = options.noReplaceRename;
#if defined(_WIN32) || defined(__SWITCH__)
    // Horizon fsFsRenameFile does not replace an existing destination.
    noReplace = true;
#endif
    if (noReplace && fs::exists(path_, ec)) {
        if (valid(path_) && !valid(backup)) return fail("backup_verify", backup, EIO);
        if (int err = injected("remove_primary")) return fail("remove_primary", path_, err);
        fs::remove(path_, ec);
        if (ec) return fail("remove_primary", path_, ec.value(), nativeError());
    }
    if (int err = injected("rename")) return fail("rename", temp, err);
    fs::rename(temp, path_, ec);
    if (ec) return fail("rename", path_, ec.value(), nativeError());
    if (read(path_) != encoded) return fail("promoted_verify", path_, EIO);
#if !defined(_WIN32) && !defined(__SWITCH__)
    const auto parent = path_.parent_path().empty() ? fs::path(".") : path_.parent_path();
    int dir = open(parent.string().c_str(), O_RDONLY);
    if (dir < 0) return fail("directory_open", parent, errno);
    const int result = fsync(dir), err = errno;
    close(dir);
    if (result != 0) return fail("directory_fsync", parent, err);
#endif
    return true;
}

} // namespace papernx
