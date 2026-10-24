#include "papernx/cover_repository.hpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

using namespace papernx;
using Clock = std::chrono::steady_clock;

int main() {
    const auto root = fs::temp_directory_path() / ("papernx-cover-perf-" + std::to_string(Clock::now().time_since_epoch().count()));
    const CoverImage cover{150, 210, 450, std::vector<std::uint8_t>(150 * 210 * 3, 0x7f)};

    for (const int count : {100, 500, 1000}) {
        const auto caseRoot = root / std::to_string(count);
        const auto bookRoot = caseRoot / "books";
        const auto cacheRoot = caseRoot / "covers";
        fs::create_directories(bookRoot);
        std::vector<Book> books;
        books.reserve(count);
        for (int i = 0; i < count; ++i) {
            const auto path = bookRoot / ("book-" + std::to_string(i) + ".epub");
            std::ofstream(path, std::ios::binary) << "fixture";
            Book book;
            book.id = documentId(path.string(), 7);
            book.path = path.string();
            book.bytes = 7;
            book.format = Format::Epub;
            books.push_back(std::move(book));
        }

        CoverRepository repository(cacheRoot);
        auto started = Clock::now();
        for (const auto &book : books) assert(!repository.read(book));
        const auto firstAccess = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
        const auto cold = repository.stats();
        assert(cold.misses == std::uint64_t(count) && cold.hits == 0);

        started = Clock::now();
        for (const auto &book : books) assert(repository.write(book, cover));
        const auto populate = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
        const auto populated = repository.stats();
        assert(populated.entries <= 512 && populated.bytes <= 48 * 1024 * 1024);

        started = Clock::now();
        for (const auto &book : books) (void)repository.read(book);
        const auto secondAccess = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started).count();
        const auto warm = repository.stats();
        assert(warm.hits + (warm.misses - cold.misses) == std::uint64_t(count));
        std::cout << "synthetic disk-cache IO (no document decode), " << count
                  << " books: cold cache lookup=" << firstAccess << " ms, population=" << populate
                  << " ms, warm cache lookup=" << secondAccess << " ms, warm hits=" << (warm.hits - cold.hits)
                  << ", warm misses=" << (warm.misses - cold.misses) << ", retained=" << warm.entries
                  << " entries/" << warm.bytes << " bytes\n";
    }

    std::error_code ec;
    fs::remove_all(root, ec);
    assert(!ec);
}
