#include "papernx/cover_repository.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace papernx;
int main() {
    const auto nativePaths = cover_detail::rootInspectionPaths("sdmc:/switch/PaperNX/data/covers", true);
    assert(nativePaths.size() == 5);
    assert(nativePaths.front().generic_string() == "sdmc:/");
    assert(nativePaths.back().generic_string() == "sdmc:/switch/PaperNX/data/covers");
    for (const auto &p : nativePaths) assert(p.generic_string().find("sdmc:/", 1) == std::string::npos);
    assert(cover_detail::rootInspectionPaths("sdmc:relative/covers", true).empty());
    assert(cover_detail::rootInspectionPaths("sdmc:/switch/../covers", true).empty());
    assert(cover_detail::rootInspectionPaths("sdmc:/other:/covers", true).empty());
    auto root = fs::temp_directory_path() / ("papernx-covers-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    Book book; book.id = "../../never-a-filename"; book.path = (root / "book.epub").string();
    { std::ofstream out(book.path); out << "original"; }
    auto cache = root / "covers";
    CoverImage image{2, 3, 6, std::vector<std::uint8_t>(18, 123)};
    CoverRepository repo(cache);
    assert(!fs::exists(cache)); // Construction never scans or creates on the UI thread.
    assert(!repo.read(book));
    auto coldStats = repo.stats();
    assert(coldStats.misses == 1 && coldStats.hits == 0);
    assert(repo.write(book, image));
    auto populatedStats = repo.stats();
    assert(populatedStats.entries == 1 && populatedStats.bytes == 74);
    auto cached = repo.read(book);
    assert(cached && cached->rgb == image.rgb);
    auto warmStats = repo.stats();
    assert(warmStats.hits == 1 && warmStats.misses == 1);
    auto key = CoverRepository::cacheKey(book);
    auto file = cache / (key + ".pnxc");
    auto modified = fs::last_write_time(file);
    assert(repo.write(book, image)); assert(fs::last_write_time(file) == modified);
    { CoverRepository restarted(cache); assert(restarted.read(book)->rgb == image.rgb); }
    // Recover interrupted FAT replacement; ignore a half-written temporary.
    fs::rename(file, cache / (key + ".bak"));
    { std::ofstream out(cache / (key + ".tmp")); out << "interrupted"; }
    { CoverRepository restarted(cache); assert(restarted.read(book)); }
    assert(!fs::exists(cache / (key + ".tmp"))); assert(fs::exists(file));
    // Corruption and malformed headers never reach image allocation.
    { std::fstream out(file, std::ios::binary | std::ios::in | std::ios::out); out.seekp(56); out.put(0); }
    assert(!repo.read(book)); assert(repo.write(book, image));
    { std::fstream out(file, std::ios::binary | std::ios::in | std::ios::out); out.seekp(40); out.write("\xff\xff\xff\xff", 4); }
    assert(!repo.read(book)); assert(repo.write(book, image));
    { std::ofstream out(file, std::ios::binary | std::ios::app); out << std::string(100000, 'x'); }
    assert(!repo.read(book)); assert(repo.write(book, image));
    // Stale library Book snapshot invalidates using actual file stat.
    { std::ofstream out(book.path, std::ios::app); out << "changed"; }
    assert(CoverRepository::cacheKey(book) != key); assert(!repo.read(book));
    auto beforeMtime = CoverRepository::cacheKey(book);
    auto oldSize = fs::file_size(book.path);
    fs::last_write_time(book.path, fs::last_write_time(book.path) + std::chrono::seconds(5));
    assert(fs::file_size(book.path) == oldSize);
    assert(CoverRepository::cacheKey(book) != beforeMtime);
    CoverImage tooBig{151, 1, 453, std::vector<std::uint8_t>(453)};
    assert(!repo.write(book, tooBig));
    CoverImage badStride{2, 3, 8, std::vector<std::uint8_t>(24)}; assert(!repo.write(book, badStride));
    // Disk budget and entry bound; unrelated user files are untouched.
    { std::ofstream out(cache / "user-notes.txt"); out << "keep"; }
    Book other = book; other.id = "other";
    { CoverRepository bounded(cache, 74, 1); assert(bounded.write(book, image)); assert(bounded.write(other, image)); assert(!bounded.read(book)); assert(bounded.read(other)); }
    assert(fs::exists(cache / "user-notes.txt"));
    // Restart inventory bounds an oversized prior cache, including corrupt entries.
    { CoverRepository large(cache, 4096, 100);
      for (int i = 0; i < 20; ++i) { Book many = book; many.id = std::to_string(i); assert(large.write(many, image)); }
    }
    { CoverRepository small(cache, 148, 2); small.trim(); }
    std::size_t files = 0; std::uintmax_t bytes = 0;
    for (const auto &entry : fs::directory_iterator(cache)) if (entry.path().extension() == ".pnxc") { ++files; bytes += entry.file_size(); }
    assert(files <= 2 && bytes <= 148);
    { CoverRepository bounded(cache, 1, 1); assert(!bounded.write(book, image)); bounded.trim(); }
    assert(fs::exists(cache / "user-notes.txt"));
    // Primary wins over backup; corrupt backup is discarded without affecting book.
    { CoverRepository clean(cache); assert(clean.write(book, image)); }
    key = CoverRepository::cacheKey(book); file = cache / (key + ".pnxc");
    fs::copy_file(file, cache / (key + ".bak"));
    { CoverRepository restarted(cache); assert(restarted.read(book)); }
    assert(!fs::exists(cache / (key + ".bak")));
    assert(fs::exists(book.path));
    // A symlinked cache root or ancestor must never redirect cache deletion.
    auto outside = root / "outside"; fs::create_directories(outside);
    fs::copy_file(file, outside / (key + ".pnxc"));
    { std::ofstream out(outside / "sentinel.txt"); out << "keep"; }
    std::error_code linkError;
    fs::create_directory_symlink(outside, root / "linked", linkError);
    if (!linkError) {
        CoverRepository unsafe(root / "linked", 0, 0);
        assert(!unsafe.read(book)); assert(!unsafe.write(book, image)); unsafe.trim();
        CoverRepository unsafeAncestor(root / "linked" / "nested");
        assert(!unsafeAncestor.write(book, image));
        CoverRepository traversal(root / "linked" / ".." / "redirected");
        assert(!traversal.write(book, image));
        assert(!fs::exists(root / "redirected"));
        assert(!fs::exists(outside / "nested"));
        assert(fs::exists(outside / (key + ".pnxc")));
        assert(fs::exists(outside / "sentinel.txt"));
    }
    // A user-requested cleanup removes only generated artifacts. It retains
    // books, unknown files and all user data, and permits immediate regeneration.
    { CoverRepository cleanup(cache);
      const auto before = cleanup.stats();
      assert(before.entries > 0 && before.bytes > 0);
      assert(cleanup.read(book));
      assert(cleanup.read(book));
      const auto counters = cleanup.stats();
      assert(counters.hits >= 2 && counters.bytes > 0);
      { std::ofstream out(cache / (key + ".tmp")); out << "interrupted"; }
      assert(cleanup.clear());
      const auto after = cleanup.stats();
      assert(after.entries == 0 && after.bytes == 0);
      assert(!fs::exists(file));
      assert(!fs::exists(cache / (key + ".tmp")));
      assert(fs::exists(book.path));
      assert(fs::exists(cache / "user-notes.txt"));
      assert(fs::exists(outside / "sentinel.txt"));
      assert(cleanup.write(book, image));
      assert(cleanup.read(book)->rgb == image.rgb);
    }
    // Never traverse even a malicious filename-shaped symlink in the cache.
    if (!linkError) {
        const auto deceptive = cache / (std::string(32, 'a') + ".pnxc");
        std::error_code local;
        fs::create_symlink(outside / "sentinel.txt", deceptive, local);
        if (!local) {
            CoverRepository cleanup(cache);
            assert(!cleanup.clear());
            assert(fs::exists(outside / "sentinel.txt"));
            fs::remove(deceptive);
            assert(cleanup.clear());
        }
        CoverRepository unsafe(root / "linked");
        assert(!unsafe.clear());
    }
    fs::remove_all(root); // Dedicated unique test-owned directory only.
    std::cout << "cover repository tests passed\n";
}
