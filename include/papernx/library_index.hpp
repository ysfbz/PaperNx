#pragma once
#include "papernx/core.hpp"
#include "papernx/fs_adapter.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace papernx {
// Returns a size only for a direct regular file, without following symlinks.
std::optional<std::uintmax_t> regularBookFileSize(const fs::path &path);
// Scanning variant: reject symlinks in every component below the already selected root.
std::optional<std::uintmax_t> regularBookFileSize(const fs::path &root, const fs::path &path);

// Why a scan did or did not publish a full replacement snapshot.
// Complete and InventoryWriteFailed carry a full snapshot; Partial, RootUnavailable,
// and Incomplete carry safely verified merged books while keeping library-index.txt untouched.
enum class ScanStatus { Failed, Complete, Partial, Cancelled, RootUnavailable, Incomplete, InventoryWriteFailed };

enum class ScanErrorCode {
    None,
    RootMissing,
    DirectoryOpenFailed,
    DirectoryReadFailed,
    FileInspectFailed,
    MetadataTimestampFailed,
    DepthLimitReached,
    BookLimitReached,
    InventoryWriteFailed,
    Cancelled,
    FatalIo
};

struct ScanIssue {
    std::string timestampUtc;
    std::string rootPath;
    std::string path;
    std::string operation;
    int errorNumber{0};
    std::uint32_t nativeResult{0};
    bool recoverable{true};
    ScanErrorCode code{ScanErrorCode::None};
    std::string message;
};

struct RootScanStatus {
    std::string rootPath;
    std::string normalizedPath;
    bool available{false};
    bool completedCleanly{false};
    bool truncated{false};
    bool depthLimited{false};
    std::size_t booksFound{0};
    std::size_t errorCount{0};
    std::size_t warningCount{0};
};

// Input owned by the worker for the duration of a scan.
struct ScanRequest {
    std::vector<fs::path> roots;
    std::vector<Book> known;
    fs::path inventory;
    const FsOps *fsOps{nullptr};
};

// `complete` means the full bounded snapshot is available; `partial` means verified
// books were merged without discarding healthy roots; `cached` reports full snapshot persistence.
struct LibraryScanResult {
    std::vector<Book> books;
    std::vector<Book> discoveredBooks;
    std::string error;
    bool cached{false}, overlaySaved{false}, cancelled{false}, complete{false}, partial{false}, truncatedByLimit{false};
    ScanStatus status{ScanStatus::Failed};
    ScanErrorCode errorCode{ScanErrorCode::None};
    std::uint64_t generation{0}; // request that produced this result; stale results must be dropped
    std::size_t addedCount{0}, existingCount{0}, updatedCount{0}, removedCount{0};
    std::vector<RootScanStatus> rootStatuses;
    std::vector<ScanIssue> unreadableFiles;
    std::vector<ScanIssue> inaccessibleDirs;
    std::vector<ScanIssue> metadataWarnings;

    bool hasUsableBooks() const {
        return !cancelled && status != ScanStatus::Failed;
    }
};

// Disposable complete inventory cache. It reuses the checksummed ProgressStore codec as its file format (so
// existing library-index.txt files stay readable) but is never merged with user progress, and a
// foreign/mismatched schema marker is treated as an empty cache instead of being rewritten in place.
class InventoryRepository {
public:
    explicit InventoryRepository(fs::path path) : path_(std::move(path)) {}
    std::vector<Book> load(const FsOps &ops = defaultFsOps()) const;
    // Returns false without publishing when cancelled before ProgressStore's commit boundary.
    bool save(const std::vector<Book> &books, const std::function<bool()> &cancelled = {}) const;
private:
    fs::path path_;
};

// Separate checksummed discovery overlay used to persist safely verified books discovered
// during partial scans without weakening or replacing the last complete inventory snapshot.
class DiscoveryOverlayRepository {
public:
    explicit DiscoveryOverlayRepository(fs::path path) : path_(std::move(path)) {}
    std::vector<Book> load(const FsOps &ops = defaultFsOps()) const;
    bool save(const std::vector<Book> &books, const std::function<bool()> &cancelled = {}) const;
private:
    fs::path path_;
};

fs::path discoveryOverlayPath(const fs::path &inventoryPath);
std::vector<Book> loadLibraryInventory(const fs::path &path, const FsOps &ops = defaultFsOps());
std::vector<Book> loadMergedLibraryInventory(const fs::path &inventoryPath, const FsOps &ops = defaultFsOps());

// Shared iterative directory traversal with per-root & per-entry diagnostics.
struct TraversalOutcome {
    std::vector<Book> books;
    std::vector<RootScanStatus> rootStatuses;
    std::vector<ScanIssue> unreadableFiles;
    std::vector<ScanIssue> inaccessibleDirs;
    std::vector<ScanIssue> metadataWarnings;
    bool complete{true};
    bool cancelled{false};
    bool rootMissing{false};
    bool truncatedByLimit{false};
    bool depthLimited{false};
};

TraversalOutcome traverseRootsDetailed(const std::vector<fs::path> &roots, std::size_t maxBooks,
                                       const std::function<bool()> &cancelled, bool normalizeRoots,
                                       const FsOps &ops);

// Bounded, rotating structured scan diagnostic log (e.g. sdmc:/switch/PaperNX/data/library-scan.log).
bool appendLibraryScanLog(const fs::path &logPath, const LibraryScanResult &result,
                          const std::string &appVersion, const std::string &sourceCommit,
                          std::uintmax_t maxBytes = 256 * 1024);

// Thread-free traversal + merge + inventory publication step.
LibraryScanResult scanLibrary(const ScanRequest &request, const std::function<bool()> &cancelled);

// Worker lifecycle only: one scan at a time, one immutable result per request, deterministic join.
class LibraryScanner {
public:
    ~LibraryScanner() { stop(); }
    // Returns the request generation (>0), or 0 when a scan is already pending.
    std::uint64_t begin(ScanRequest request);
    bool start(std::vector<fs::path> roots, std::vector<Book> known, fs::path inventory) {
        return begin(ScanRequest{std::move(roots), std::move(known), std::move(inventory), nullptr}) != 0;
    }
    bool pending() const { return worker_.joinable(); }
    bool ready() const { return ready_.load(); }
    void cancel() { if (!ready_.load()) cancel_.store(true); }
    std::optional<LibraryScanResult> take();
    void stop();
private:
    std::thread worker_;
    std::atomic<bool> cancel_{false}, ready_{false};
    std::uint64_t generation_{0};
    LibraryScanResult result_;
};

} // namespace papernx
