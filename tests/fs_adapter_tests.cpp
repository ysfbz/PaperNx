#include "papernx/core.hpp"
#include "papernx/fs_adapter.hpp"
#include "papernx/library_index.hpp"
#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef NDEBUG
#error Tests require enabled assertions
#endif

using namespace papernx;
namespace fs = std::filesystem;

namespace {

struct MockNode {
    bool isDir{false};
    bool isSymlink{false};
    std::uintmax_t size{0};
    std::int64_t mtime{1700000000};
    bool mtimeKnown{true};
    int listErr{0};
    std::uint32_t listNative{0};
    int statErr{0};
    std::uint32_t statNative{0};
    int mtimeErr{0};
    std::uint32_t mtimeNative{0};
    bool unreadable{false};
    std::vector<std::string> children;
};

class MockSwitchFs {
public:
    std::unordered_map<std::string, MockNode> nodes;

    static std::string keyOf(const fs::path &p) {
        std::error_code ec;
        auto norm = normalizeDevicePath(p, ec);
        return ec ? p.generic_u8string() : norm.generic_u8string();
    }

    void addDir(const std::string &path, int listErr = 0, std::uint32_t nativeRes = 0) {
        std::error_code ec;
        const std::string k = normalizeDevicePath(fs::u8path(path), ec).generic_u8string();
        auto &n = nodes[k];
        n.isDir = true;
        n.listErr = listErr;
        n.listNative = nativeRes;
        attachParent(k);
    }

    void addFile(const std::string &path, std::uintmax_t size = 128, bool mtimeKnown = true,
                 bool unreadable = false, bool symlink = false) {
        std::error_code ec;
        const std::string k = normalizeDevicePath(fs::u8path(path), ec).generic_u8string();
        auto &n = nodes[k];
        n.isDir = false;
        n.isSymlink = symlink;
        n.size = size;
        n.mtimeKnown = mtimeKnown;
        n.unreadable = unreadable;
        attachParent(k);
    }

    void removeNode(const std::string &path) {
        std::error_code ec;
        const std::string k = normalizeDevicePath(fs::u8path(path), ec).generic_u8string();
        nodes.erase(k);
    }

    FsOps ops() const {
        FsOps f;
        f.assumeDevicePaths = true; // Simulate libnx Switch sdmc:/ semantics on host.
        f.statPath = [this](const fs::path &path, bool /*follow*/) -> FsStatInfo {
            FsStatInfo info;
            const std::string k = keyOf(path);
            auto it = nodes.find(k);
            if (it == nodes.end()) {
                info.errorNumber = ENOENT;
                return info;
            }
            if (it->second.statErr != 0) {
                info.errorNumber = it->second.statErr;
                info.nativeResult = it->second.statNative;
                return info;
            }
            info.exists = true;
            info.isSymlink = it->second.isSymlink;
            info.isDirectory = !it->second.isSymlink && it->second.isDir;
            info.isRegularFile = !it->second.isSymlink && !it->second.isDir;
            info.size = it->second.size;
            info.modified = it->second.mtime;
            info.modifiedKnown = it->second.mtimeKnown;
            // Exact libnx fsdev_stat behavior: st_dev = 0 and st_ino = 0 for all SD paths.
            info.dev = 0;
            info.ino = 0;
            return info;
        };
        f.inspectRegularFile = [this](const fs::path &root, const fs::path &path) -> FsStatInfo {
            FsStatInfo info;
            auto rel = relativeComponentsSafe(root, path, true);
            if (!rel) {
                info.errorNumber = EINVAL;
                return info;
            }
            const std::string k = keyOf(path);
            auto it = nodes.find(k);
            if (it == nodes.end()) {
                info.errorNumber = ENOENT;
                return info;
            }
            if (it->second.unreadable) {
                info.errorNumber = it->second.statErr ? it->second.statErr : EIO;
                info.nativeResult = it->second.statNative ? it->second.statNative : 0x202;
                return info;
            }
            if (it->second.isSymlink) {
                info.exists = true;
                info.isSymlink = true;
                info.errorNumber = ELOOP;
                return info;
            }
            if (it->second.isDir) {
                info.exists = true;
                info.isDirectory = true;
                info.errorNumber = EISDIR;
                return info;
            }
            info.exists = true;
            info.isRegularFile = true;
            info.size = it->second.size;
            info.modified = it->second.mtime;
            info.modifiedKnown = it->second.mtimeKnown;
            info.dev = 0;
            info.ino = 0;
            return info;
        };
        f.listDirectory = [this](const fs::path &dir, std::vector<FsDirEntry> &out,
                                 int &errorNumber, std::uint32_t &nativeResult) -> bool {
            out.clear();
            errorNumber = 0;
            nativeResult = 0;
            const std::string k = keyOf(dir);
            auto it = nodes.find(k);
            if (it == nodes.end() || !it->second.isDir) {
                errorNumber = ENOENT;
                return false;
            }
            if (it->second.listErr != 0) {
                errorNumber = it->second.listErr;
                nativeResult = it->second.listNative;
                return false;
            }
            for (const auto &childName : it->second.children) {
                const std::string ck = k == "sdmc:/" ? (k + childName) : (k + "/" + childName);
                auto cit = nodes.find(ck);
                FsDirEntry entry;
                entry.name = childName;
                if (cit == nodes.end()) {
                    entry.kind = FsDirEntry::Kind::Unknown; // Simulate readdir/raced delete
                } else if (cit->second.isSymlink) {
                    entry.kind = FsDirEntry::Kind::Symlink;
                } else if (cit->second.isDir) {
                    entry.kind = FsDirEntry::Kind::Directory;
                } else {
                    entry.kind = FsDirEntry::Kind::Regular;
                }
                out.push_back(std::move(entry));
            }
            return true;
        };
        f.lastWriteTime = [this](const fs::path &path, int &errorNumber, std::uint32_t &nativeResult) -> std::int64_t {
            errorNumber = 0;
            nativeResult = 0;
            const std::string k = keyOf(path);
            auto it = nodes.find(k);
            if (it == nodes.end()) {
                errorNumber = ENOENT;
                return 0;
            }
            if (!it->second.mtimeKnown) {
                errorNumber = it->second.mtimeErr ? it->second.mtimeErr : EINVAL;
                nativeResult = it->second.mtimeNative;
                return 0;
            }
            return it->second.mtime;
        };
        return f;
    }

private:
    void attachParent(const std::string &childKey) {
        if (childKey == "sdmc:/") return;
        const auto slash = childKey.rfind('/');
        if (slash == std::string::npos) return;
        std::string parentKey = childKey.substr(0, slash);
        if (parentKey == "sdmc:") parentKey = "sdmc:/";
        const std::string name = childKey.substr(slash + 1);
        if (name.empty()) return;
        if (nodes.find(parentKey) == nodes.end()) {
            addDir(parentKey);
        }
        auto &children = nodes[parentKey].children;
        if (std::find(children.begin(), children.end(), name) == children.end()) {
            children.push_back(name);
        }
    }
};

std::string readFileBytes(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), {});
}

void writeRealFile(const fs::path &p, const std::string &body) {
    fs::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out << body;
}

void testDevicePathNormalizationAndZeroInodeSafety() {
    assert(isDevicePathString("sdmc:/switch/PaperNX/books"));
    assert(isDevicePathString("romfs:/fonts"));
    assert(!isDevicePathString("/tmp/books"));

    std::error_code ec;
    assert(normalizeDevicePath("sdmc://switch//PaperNX/./books/", ec).generic_u8string() == "sdmc:/switch/PaperNX/books");
    assert(!ec);
    assert(normalizeDevicePath("sdmc:/switch/../books", ec).generic_u8string() == "sdmc:/books");
    assert(!ec);
    assert(normalizeDevicePath("sdmc:", ec).generic_u8string() == "sdmc:/");
    assert(!ec);
    assert(normalizeDevicePath("sdmc:/", ec).generic_u8string() == "sdmc:/");
    assert(!ec);

    const auto rel = relativeComponentsSafe("sdmc:/switch/PaperNX/books", "sdmc:/switch/PaperNX/books/sub/kitap.epub", true);
    assert(rel && rel->size() == 2 && (*rel)[0] == "sub" && (*rel)[1] == "kitap.epub");
    const auto relRoot = relativeComponentsSafe("sdmc:/", "sdmc:/kitap.epub", true);
    assert(relRoot && relRoot->size() == 1 && (*relRoot)[0] == "kitap.epub");
    assert(!relativeComponentsSafe("sdmc:/books", "sdmc:/other/kitap.epub", true).has_value());

    // Simulate libnx st_dev=0, st_ino=0 on two distinct folders.
    MockSwitchFs mfs;
    mfs.addDir("sdmc:/switch/PaperNX/books");
    mfs.addDir("sdmc:/comics");
    mfs.addDir("sdmc:/comics/sub");
    const FsOps ops = mfs.ops();

    assert(pathsEquivalentSafe("sdmc:/switch/PaperNX/books/", "sdmc://switch/PaperNX/./books", ops));
    assert(!pathsEquivalentSafe("sdmc:/switch/PaperNX/books", "sdmc:/comics", ops));
    assert(containsPathSafe("sdmc:/comics", "sdmc:/comics/sub", ops));
    assert(!containsPathSafe("sdmc:/comics", "sdmc:/switch/PaperNX/books", ops));
}

void testFaultInjectedPartialAndSwitchConditions() {
    const fs::path tempRoot = fs::temp_directory_path() / "papernx-fs-adapter-tests";
    fs::remove_all(tempRoot);
    fs::create_directories(tempRoot / "data");

    const fs::path invPath = tempRoot / "data" / "library-index.txt";
    const fs::path progressPath = tempRoot / "data" / "progress.txt";
    const fs::path logPath = tempRoot / "data" / "library-scan.log";

    // Seed 25 existing valid books on a simulated Switch sdmc:/ partition.
    MockSwitchFs mfs;
    mfs.addDir("sdmc:/switch/PaperNX/books");
    std::vector<Book> baseline25;
    for (int i = 1; i <= 25; ++i) {
        const std::string p = "sdmc:/switch/PaperNX/books/book_" + std::to_string(i) + ".epub";
        mfs.addFile(p, 1024 + std::uintmax_t(i));
        Book b;
        b.path = p;
        b.bytes = 1024 + std::uintmax_t(i);
        b.modified = 1700000000;
        b.format = Format::Epub;
        b.id = documentId(b.path, b.bytes);
        b.title = "Book " + std::to_string(i);
        baseline25.push_back(b);
    }

    ProgressStore progress(progressPath);
    Progress p1;
    p1.page = 14;
    p1.bookPath = baseline25[0].path;
    p1.bookTitle = baseline25[0].title;
    progress.put(baseline25[0].id, p1);
    assert(progress.save());

    // Initial clean complete scan of the 25 books writes complete library-index.txt.
    const FsOps opsClean = mfs.ops();
    const auto cleanRes = scanLibrary(ScanRequest{{"sdmc:/switch/PaperNX/books"}, baseline25, invPath, &opsClean}, nullptr);
    assert(cleanRes.complete);
    assert(cleanRes.status == ScanStatus::Complete);
    assert(cleanRes.cached);
    assert(cleanRes.books.size() == 25);
    const std::string savedCompleteBytes = readFileBytes(invPath);
    assert(!savedCompleteBytes.empty());

    // Add 4 newly discovered valid books (EPUB, PDF, CBZ, CBR) including Turkish Unicode & spaces,
    // PLUS 1 unreadable file alongside them -> reproduces partial scan tolerance.
    const std::string new1 = "sdmc:/switch/PaperNX/books/İstanbul’da Sabah — Böl. 01 [Özel Basım]!.cbz";
    const std::string new2 = "sdmc:/switch/PaperNX/books/Çizgi Roman Seri #2.CBR";
    const std::string new3 = "sdmc:/switch/PaperNX/books/Makale Denemesi 2026.PDF";
    const std::string new4 = "sdmc:/switch/PaperNX/books/no-mtime-kitap.epub";
    const std::string bad1 = "sdmc:/switch/PaperNX/books/bozuk-okunamaz.epub";
    const std::string link1 = "sdmc:/switch/PaperNX/books/symlink-skip.pdf";
    mfs.addFile(new1, 4096, true, false);
    mfs.addFile(new2, 8192, true, false);
    mfs.addFile(new3, 2048, true, false);
    mfs.addFile(new4, 3072, false, false); // Scenario 2: valid file whose last_write_time fails -> still added!
    mfs.addFile(bad1, 512, true, true);    // Scenario 1: unreadable single file
    mfs.addFile(link1, 512, true, false, true); // Symlink must be rejected

    // Also add an accessible extra root with 1 book + an unavailable extra root + an ENAMETOOLONG subfolder.
    mfs.addDir("sdmc:/switch/PaperNX/books/longname_fail", ENAMETOOLONG, 0x202);
    mfs.addDir("sdmc:/extra-valid");
    const std::string extraBook = "sdmc:/extra-valid/extra.epub";
    mfs.addFile(extraBook, 5000);
    // Keep a previously known book under an inaccessible dir to ensure it is NOT silently deleted!
    Book hiddenInBadSub;
    hiddenInBadSub.path = "sdmc:/switch/PaperNX/books/longname_fail/inside_bad_dir.epub";
    hiddenInBadSub.bytes = 999;
    hiddenInBadSub.format = Format::Epub;
    hiddenInBadSub.id = documentId(hiddenInBadSub.path, hiddenInBadSub.bytes);
    hiddenInBadSub.title = "Kept From Inaccessible Subdir";
    auto knownWithSub = baseline25;
    knownWithSub.push_back(hiddenInBadSub);

    const FsOps opsFault = mfs.ops();
    const auto partialRes = scanLibrary(ScanRequest{
        {
            "sdmc:/switch/PaperNX/books",
            "sdmc://switch//PaperNX/books/", // Duplicate unnormalized root -> must dedup safely
            "sdmc:/extra-valid",
            "sdmc:/missing-sd-folder"
        },
        knownWithSub,
        invPath,
        &opsFault
    }, nullptr);

    assert(partialRes.hasUsableBooks());
    assert(!partialRes.complete);
    assert(partialRes.partial);
    assert(partialRes.status == ScanStatus::RootUnavailable);
    assert(!partialRes.cached);
    assert(partialRes.overlaySaved);
    // 25 baseline + 4 new in primary + 1 hidden preserved + 1 in extra-valid = 31 books (symlink & unreadable skipped)
    assert(partialRes.books.size() == 31);
    assert(partialRes.addedCount == 5);
    assert(partialRes.unreadableFiles.size() == 1);
    assert(partialRes.unreadableFiles[0].code == ScanErrorCode::FileInspectFailed);
    // Inaccessible dir list has both longname_fail (ENAMETOOLONG) and missing-sd-folder (RootMissing)
    assert(partialRes.inaccessibleDirs.size() >= 2);
    assert(!partialRes.metadataWarnings.empty());
    assert(partialRes.metadataWarnings[0].code == ScanErrorCode::MetadataTimestampFailed);

    bool foundNoMtime = false, foundTurkish = false, foundKeptSub = false, foundExtra = false, foundSymlink = false;
    for (const auto &b : partialRes.books) {
        if (b.path == new4) { foundNoMtime = true; assert(b.modified == 0); }
        if (b.path == new1) foundTurkish = true;
        if (b.path == hiddenInBadSub.path) foundKeptSub = true;
        if (b.path == extraBook) foundExtra = true;
        if (b.path == link1) foundSymlink = true;
    }
    assert(foundNoMtime && foundTurkish && foundKeptSub && foundExtra && !foundSymlink);

    // Crucial contract: complete inventory snapshot library-index.txt bytes MUST be preserved untouched,
    // while library-discoveries.txt holds safely verified new discoveries across app restarts.
    assert(readFileBytes(invPath) == savedCompleteBytes);
    assert(loadLibraryInventory(invPath, opsFault).size() == 25);
    const auto mergedRestart = loadMergedLibraryInventory(invPath, opsFault);
    assert(mergedRestart.size() >= 30); // 25 baseline + 5 new discoveries merged without duplicates!

    // Log rotation & structured diagnostics verification
    assert(appendLibraryScanLog(logPath, partialRes, "v0.9.4", "testsha"));
    const std::string logContents = readFileBytes(logPath);
    assert(logContents.find("status=root_unavailable") != std::string::npos);
    assert(logContents.find("partial=1") != std::string::npos);
    assert(logContents.find("errno=") != std::string::npos);

    // Progress store untouched throughout partial scans
    ProgressStore checkProgress(progressPath);
    assert(checkProgress.load());
    assert(checkProgress.get(baseline25[0].id).page == 14);

    // Also verify healthy-roots ScanStatus::Partial (single unreadable file + ENAMETOOLONG subdir, no missing root)
    const auto partialOnly = scanLibrary(ScanRequest{
        {"sdmc:/switch/PaperNX/books", "sdmc:/extra-valid"},
        knownWithSub,
        invPath,
        &opsFault
    }, nullptr);
    assert(partialOnly.hasUsableBooks() && !partialOnly.complete && partialOnly.partial);
    assert(partialOnly.status == ScanStatus::Partial);
    assert(partialOnly.books.size() == 31);

    // Simulate directory-read race (file disappears between listDirectory and stat)
    mfs.nodes["sdmc:/switch/PaperNX/books"].children.push_back("raced_deleted.epub");
    const auto raceRes = scanLibrary(ScanRequest{{"sdmc:/switch/PaperNX/books"}, baseline25, invPath, &opsFault}, nullptr);
    assert(raceRes.hasUsableBooks());
    assert(!raceRes.unreadableFiles.empty());

    // Recovery scan when faulty file + missing root + bad subdir are resolved:
    mfs.nodes.erase("sdmc:/switch/PaperNX/books/longname_fail");
    mfs.removeNode(bad1);
    auto &rootChildren = mfs.nodes["sdmc:/switch/PaperNX/books"].children;
    rootChildren.erase(std::remove(rootChildren.begin(), rootChildren.end(), "longname_fail"), rootChildren.end());
    rootChildren.erase(std::remove(rootChildren.begin(), rootChildren.end(), "bozuk-okunamaz.epub"), rootChildren.end());
    rootChildren.erase(std::remove(rootChildren.begin(), rootChildren.end(), "raced_deleted.epub"), rootChildren.end());
    mfs.removeNode(link1);
    rootChildren.erase(std::remove(rootChildren.begin(), rootChildren.end(), "symlink-skip.pdf"), rootChildren.end());

    const auto recoveredRes = scanLibrary(ScanRequest{{"sdmc:/switch/PaperNX/books", "sdmc:/extra-valid"},
                                                      partialRes.books, invPath, &opsFault}, nullptr);
    assert(recoveredRes.complete);
    assert(recoveredRes.status == ScanStatus::Complete);
    assert(recoveredRes.cached);
    assert(recoveredRes.books.size() == 30); // 25 + 4 new + 1 extra (removed bad subdir entry safely pruned on clean scan)
    assert(!fs::exists(discoveryOverlayPath(invPath))); // Overlay pruned cleanly on full snapshot commit!

    fs::remove_all(tempRoot);
}

void testHostPosixAdapterAndScannerLimits() {
    const fs::path root = fs::temp_directory_path() / "papernx-fs-host-limits";
    fs::remove_all(root);
    fs::create_directories(root / "books" / "Alt Klasör");
    fs::create_directories(root / "data");

    const fs::path b1 = root / "books" / "Türkçe Kitap [2026].epub";
    const fs::path b2 = root / "books" / "Alt Klasör" / "Çizgi 01!.cbz";
    const fs::path b3 = root / "books" / "not-book.txt";
    writeRealFile(b1, "EPUB-DATA-123456");
    writeRealFile(b2, "CBZ-DATA-1234567890");
    writeRealFile(b3, "ignore");

    // Host defaultFsOps agreement between direct regularBookFileSize and traverseRootsDetailed.
    const auto sz1 = regularBookFileSize(root / "books", b1);
    assert(sz1 && *sz1 == 16);
    const auto out = traverseRootsDetailed({root / "books"}, 100, nullptr, true, defaultFsOps());
    assert(out.complete && out.books.size() == 2);

    // Bounded depth limit > 16 test
    fs::path deep = root / "deep";
    fs::path cur = deep;
    for (int d = 0; d < 20; ++d) cur /= ("d" + std::to_string(d));
    writeRealFile(cur / "deep_book.epub", "DEEP");
    const auto deepOut = traverseRootsDetailed({deep}, 100, nullptr, true, defaultFsOps());
    assert(!deepOut.complete && deepOut.depthLimited);
    assert(!deepOut.inaccessibleDirs.empty());

    // Bounded 10,000-book limit test using MockSwitchFs (fast in-memory scale check)
    MockSwitchFs scaleFs;
    scaleFs.addDir("sdmc:/huge");
    for (int i = 0; i < 10005; ++i) {
        scaleFs.addFile("sdmc:/huge/b_" + std::to_string(i) + ".epub", 64);
    }
    const FsOps scaleOps = scaleFs.ops();
    const auto scaleRes = scanLibrary(ScanRequest{{"sdmc:/huge"}, {}, root / "data" / "scale-inv.txt", &scaleOps}, nullptr);
    assert(!scaleRes.complete);
    assert(scaleRes.truncatedByLimit);
    assert(scaleRes.books.size() == 10000);
    assert(scaleRes.errorCode == ScanErrorCode::BookLimitReached);

    fs::remove_all(root);
}

} // namespace

int main() {
    testDevicePathNormalizationAndZeroInodeSafety();
    testFaultInjectedPartialAndSwitchConditions();
    testHostPosixAdapterAndScannerLimits();
    std::cout << "PASS: Switch FS adapter, zero-inode safety, fault-injected partial scans, overlay persistence and limits\n";
    return 0;
}
