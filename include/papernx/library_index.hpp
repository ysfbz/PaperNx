#pragma once
#include "papernx/core.hpp"
#include <atomic>
#include <optional>
#include <thread>

namespace papernx {
// Disposable inventory; rescans never rewrite reading progress or user metadata.
std::vector<Book> loadLibraryInventory(const fs::path &path);
// Returns a size only for a direct regular file, without following symlinks.
std::optional<std::uintmax_t> regularBookFileSize(const fs::path &path);
// `complete` means the full bounded snapshot is available; `cached` reports persistence.
struct LibraryScanResult {std::vector<Book> books;std::string error;bool cached{false},cancelled{false},complete{false};};
class LibraryScanner {
public:
    ~LibraryScanner(){stop();}
    bool start(std::vector<fs::path> roots,std::vector<Book> known,fs::path inventory);
    bool pending() const {return worker_.joinable();}
    bool ready() const {return ready_.load();}
    void cancel() {if(!ready_.load())cancel_.store(true);}
    std::optional<LibraryScanResult> take();
    void stop();
private:
    std::thread worker_;
    std::atomic<bool> cancel_{false},ready_{false};
    LibraryScanResult result_;
};
} // namespace papernx
