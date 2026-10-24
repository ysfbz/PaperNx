#pragma once
#include "papernx/core.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <optional>
#include <thread>

namespace papernx {
// Returns a size only for a direct regular file, without following symlinks.
std::optional<std::uintmax_t> regularBookFileSize(const fs::path &path);

// Why a scan did or did not publish a replacement snapshot. Only Complete and
// InventoryWriteFailed carry a trustworthy book list; every other status must leave the
// caller's current library and the previous disposable inventory untouched.
enum class ScanStatus {Failed,Complete,Cancelled,RootUnavailable,Incomplete,InventoryWriteFailed};
// Input owned by the worker for the duration of a scan.
struct ScanRequest {std::vector<fs::path> roots;std::vector<Book> known;fs::path inventory;};
// `complete` means the full bounded snapshot is available; `cached` reports persistence.
struct LibraryScanResult {
    std::vector<Book> books;std::string error;bool cached{false},cancelled{false},complete{false};
    ScanStatus status{ScanStatus::Failed};
    std::uint64_t generation{0}; // request that produced this result; stale results must be dropped
};

// Disposable inventory cache. It reuses the checksummed ProgressStore codec as its file format (so
// existing library-index.txt files stay readable) but is never merged with user progress, and a
// foreign/mismatched schema marker is treated as an empty cache instead of being rewritten in place.
class InventoryRepository {
public:
    explicit InventoryRepository(fs::path path):path_(std::move(path)){}
    std::vector<Book> load() const;
    // Atomic overwrite; returns false (and writes nothing) if cancelled before the write.
    bool save(const std::vector<Book> &books,const std::function<bool()> &cancelled={}) const;
private:
    fs::path path_;
};
std::vector<Book> loadLibraryInventory(const fs::path &path);

// Thread-free traversal + merge + inventory publication step. Cancellation or any traversal
// problem yields complete=false and never touches the inventory file.
LibraryScanResult scanLibrary(const ScanRequest &request,const std::function<bool()> &cancelled);

// Worker lifecycle only: one scan at a time, one immutable result per request, deterministic join.
class LibraryScanner {
public:
    ~LibraryScanner(){stop();}
    // Returns the request generation (>0), or 0 when a scan is already pending.
    std::uint64_t begin(ScanRequest request);
    bool start(std::vector<fs::path> roots,std::vector<Book> known,fs::path inventory) {
        return begin(ScanRequest{std::move(roots),std::move(known),std::move(inventory)})!=0;
    }
    bool pending() const {return worker_.joinable();}
    bool ready() const {return ready_.load();}
    void cancel() {if(!ready_.load())cancel_.store(true);}
    std::optional<LibraryScanResult> take();
    void stop();
private:
    std::thread worker_;
    std::atomic<bool> cancel_{false},ready_{false};
    std::uint64_t generation_{0};
    LibraryScanResult result_;
};} // namespace papernx
