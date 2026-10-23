#pragma once
#include "papernx/core.hpp"
#include <map>
#include <optional>

namespace papernx {
namespace cover_detail {
// Pure path-planning seam: native devoptab roots are absolute to libnx even
// though the POSIX std::filesystem parser considers "sdmc:/" relative.
std::vector<fs::path> rootInspectionPaths(const fs::path &root, bool nativeDevicePaths);
}
enum class CoverState { NotRequested, Loading, Loaded, NoEmbeddedCover, Failed, Cached };
struct CoverImage {
    int width{}, height{}, stride{};
    std::vector<std::uint8_t> rgb;
};
struct CoverCacheStats {
    std::size_t entries{};
    std::uintmax_t bytes{};
    std::uint64_t hits{}, misses{};
};

// Worker-thread only. Construction performs no disk IO. Never stores book data
// or changes progress. Cache failures are ordinary misses, never reader errors.
class CoverRepository {
public:
    explicit CoverRepository(fs::path root, std::uintmax_t maxBytes = 32 * 1024 * 1024,
                             std::size_t maxEntries = 512);
    std::optional<CoverImage> read(const Book &book);
    bool write(const Book &book, const CoverImage &image);
    void trim();
    // Worker-thread only, just like read/write. Never removes source books or
    // unknown files; callers must stop/drain pending cover jobs before clearing.
    bool clear();
    CoverCacheStats stats();
    static std::string cacheKey(const Book &book);
    static constexpr int maxWidth = 150, maxHeight = 210;
private:
    struct Entry { std::uintmax_t bytes{}; std::uint64_t used{}; };
    fs::path root_;
    std::uintmax_t maxBytes_;
    std::size_t maxEntries_;
    std::uint64_t clock_{};
    std::uint64_t hits_{}, misses_{};
    bool initialized_{};
    bool available_{};
    std::map<std::string, Entry> entries_;
    void initialize();
    std::optional<CoverImage> readFile(const fs::path &, const std::string &) const;
};
} // namespace papernx
