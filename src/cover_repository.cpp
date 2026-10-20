#include "papernx/cover_repository.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace papernx {
namespace cover_detail {
std::vector<fs::path> rootInspectionPaths(const fs::path &root, bool nativeDevicePaths) {
    std::vector<fs::path> paths;
    const auto raw = root.generic_string();
    if (nativeDevicePaths && raw.find(':') != std::string::npos) {
        const auto colon = raw.find(':');
        if (!colon || colon + 1 >= raw.size() || raw[colon+1] != '/' ||
            raw.find(':', colon+1) != std::string::npos || raw.find('\\') != std::string::npos ||
            raw.find('\0') != std::string::npos) return {};
        for (std::size_t i = 0; i < colon; ++i) {
            const char c = raw[i];
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_')) return {};
        }
        fs::path part(raw.substr(0, colon+2)); // Retain the device root's slash.
        paths.push_back(part);
        std::size_t begin = colon+2;
        while (begin < raw.size()) {
            auto end = raw.find('/', begin); if (end == std::string::npos) end = raw.size();
            auto component = raw.substr(begin, end-begin);
            // Cache roots are never allowed to traverse above a device prefix.
            if (component == ".." || component == ".") return {};
            if (!component.empty()) { part /= component; paths.push_back(part); }
            begin = end+1;
        }
        return paths;
    }
    std::error_code ec;
    // Preserve '..' so symlinks are inspected before any parent traversal.
    const auto absolute = fs::absolute(root, ec);
    if (ec) return {};
    fs::path part;
    for (const auto &component : absolute) { part /= component; paths.push_back(part); }
    return paths;
}
}
namespace {
constexpr std::uintmax_t headerSize = 56;
std::uint32_t checksum(const std::vector<std::uint8_t> &bytes) {
    std::uint32_t h = 2166136261u;
    for (auto b : bytes) h = (h ^ b) * 16777619u;
    return h;
}
void put32(std::ostream &out, std::uint32_t n) {
    for (int i = 0; i < 4; ++i) out.put(static_cast<char>((n >> (i * 8)) & 255));
}
std::uint32_t get32(std::istream &in) {
    std::uint32_t n = 0;
    for (int i = 0; i < 4; ++i) n |= std::uint32_t(static_cast<unsigned char>(in.get())) << (i * 8);
    return n;
}
bool safeKey(const std::string &s) {
    return s.size() == 32 && std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool regular(const fs::path &p) {
    std::error_code ec;
    return fs::is_regular_file(fs::symlink_status(p, ec));
}
bool safeRoot(const fs::path &root) {
    std::error_code ec;
    // libnx fsdev strips the device prefix and aliases lstat to stat because SD
    // symlinks are unsupported (verification/libnx-fsdev-source.txt). Inspect
    // "sdmc:/" directly; fs::absolute would incorrectly prepend getcwd().
#ifdef __SWITCH__
    constexpr bool nativeDevicePaths = true;
#else
    constexpr bool nativeDevicePaths = false;
#endif
    const auto prefixes = cover_detail::rootInspectionPaths(root, nativeDevicePaths);
    if (prefixes.empty()) return false;
    for (const auto &part : prefixes) {
        auto status = fs::symlink_status(part, ec);
        if (fs::is_symlink(status)) return false;
        if (ec && ec != std::errc::no_such_file_or_directory) return false;
        ec.clear();
    }
    return true;
}
void removeOwned(const fs::path &p) {
    std::error_code ec;
    const auto status = fs::symlink_status(p, ec);
    if (!ec && (fs::is_regular_file(status) || fs::is_symlink(status))) fs::remove(p, ec);
}
bool renameOwned(const fs::path &a, const fs::path &b) {
    std::error_code ec; fs::rename(a, b, ec); return !ec;
}
}

CoverRepository::CoverRepository(fs::path root, std::uintmax_t maxBytes, std::size_t maxEntries)
    : root_(std::move(root)), maxBytes_(maxBytes), maxEntries_(maxEntries) {}

std::string CoverRepository::cacheKey(const Book &book) {
    // Restat on the worker: stale library metadata must not resurrect old artwork.
    std::error_code ec;
    auto bytes = fs::file_size(book.path, ec);
    if (ec) bytes = book.bytes;
    auto stamp = fs::last_write_time(book.path, ec);
    auto modified = ec ? book.modified : static_cast<std::int64_t>(stamp.time_since_epoch().count());
    const std::string identity = book.path + '\0' + book.id + '\0' +
        std::to_string(bytes) + ':' + std::to_string(modified) + (isComic(book.format)?":comic-cover-v2":"");
    std::uint64_t a = 14695981039346656037ull, b = 7809847782465536322ull;
    for (unsigned char c : identity) { a = (a ^ c) * 1099511628211ull; b = (b ^ c) * 14029467366897019727ull; }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << a << std::setw(16) << b;
    return out.str();
}

std::optional<CoverImage> CoverRepository::readFile(const fs::path &p, const std::string &key) const {
    std::error_code ec;
    if (!regular(p)) return {};
    auto size = fs::file_size(p, ec);
    if (ec || size < headerSize || size > headerSize + maxWidth * maxHeight * 3) return {};
    std::ifstream in(p, std::ios::binary);
    std::array<char, 8> magic{}; std::array<char, 32> savedKey{};
    in.read(magic.data(), magic.size()); in.read(savedKey.data(), savedKey.size());
    if (std::string(magic.data(), 8) != std::string("PNXC0001", 8) ||
        std::string(savedKey.data(), 32) != key) return {};
    auto w = get32(in), h = get32(in), length = get32(in), sum = get32(in);
    if (!in || !w || !h || w > maxWidth || h > maxHeight || length != w * h * 3 || size != headerSize + length) return {};
    CoverImage image{static_cast<int>(w), static_cast<int>(h), static_cast<int>(w * 3), {}};
    image.rgb.resize(length);
    in.read(reinterpret_cast<char *>(image.rgb.data()), length);
    if (!in || checksum(image.rgb) != sum) return {};
    return image;
}

void CoverRepository::initialize() {
    if (initialized_) return;
    initialized_ = true;
    if (!safeRoot(root_)) return;
    std::error_code ec; fs::create_directories(root_, ec);
    if (ec) return;
    available_ = true;
    // One bounded-storage inventory per repository, never once per rendered card.
    fs::directory_iterator it(root_, ec), end;
    while (!ec && it != end) {
        auto p = it->path(); auto name = p.filename().string();
        if (name.size() >= 36 && safeKey(name.substr(0, 32)) && regular(p)) {
            const auto key = name.substr(0, 32), suffix = name.substr(32);
            const auto final = root_ / (key + ".pnxc");
            if (suffix == ".tmp") removeOwned(p);
            if (suffix == ".bak") {
                if (readFile(final, key)) removeOwned(p);
                else if (readFile(p, key)) { removeOwned(final); renameOwned(p, final); }
                else removeOwned(p);
            }
        }
        it.increment(ec);
    }
    ec.clear();
    for (fs::directory_iterator scan(root_, ec); !ec && scan != end; scan.increment(ec)) {
        auto p = scan->path(); auto name = p.filename().string();
        if (name.size() != 37 || name.substr(32) != ".pnxc" || !safeKey(name.substr(0, 32)) || !regular(p)) continue;
        auto bytes = fs::file_size(p, ec); if (ec) break;
        if (bytes > headerSize + maxWidth * maxHeight * 3) { removeOwned(p); continue; }
        entries_[name.substr(0, 32)] = {bytes, ++clock_};
        trim(); // Bound the index even if an old cache directory contains millions of entries.
    }
    trim();
}

std::optional<CoverImage> CoverRepository::read(const Book &book) {
    initialize(); if (!available_ || !safeRoot(root_)) return {};
    auto key = cacheKey(book); auto p = root_ / (key + ".pnxc");
    auto image = readFile(p, key);
    if (image) entries_[key] = {headerSize + image->rgb.size(), ++clock_};
    else { removeOwned(p); entries_.erase(key); }
    return image;
}

bool CoverRepository::write(const Book &book, const CoverImage &image) {
    if (image.width <= 0 || image.height <= 0 || image.width > maxWidth || image.height > maxHeight ||
        image.stride != image.width * 3 || image.rgb.size() != std::size_t(image.stride * image.height) ||
        image.rgb.size() + headerSize > maxBytes_ || !maxEntries_) return false;
    initialize(); if (!available_ || !safeRoot(root_)) return false;
    const auto key = cacheKey(book);
    const auto final = root_ / (key + ".pnxc"), temp = root_ / (key + ".tmp"), backup = root_ / (key + ".bak");
    if (readFile(final, key)) { entries_[key].used = ++clock_; return true; } // No repeated SD writes.
    std::error_code ec;
    for (const auto &p : {final, temp, backup}) if (fs::is_symlink(fs::symlink_status(p, ec))) return false;
    // Reserve space before writing the temporary so even replacement does not
    // exceed the disk budget. A corrupt prior entry has no recovery value.
    removeOwned(final); entries_.erase(key);
    const auto needed = headerSize + image.rgb.size();
    std::uintmax_t total = 0; for (const auto &e : entries_) total += e.second.bytes;
    while (!entries_.empty() && (total > maxBytes_ - needed || entries_.size() >= maxEntries_)) {
        auto oldest = std::min_element(entries_.begin(), entries_.end(), [](const auto &a, const auto &b) { return a.second.used < b.second.used; });
        total -= oldest->second.bytes; removeOwned(root_ / (oldest->first + ".pnxc")); entries_.erase(oldest);
    }
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    out.write("PNXC0001", 8); out.write(key.data(), key.size());
    put32(out, image.width); put32(out, image.height); put32(out, image.rgb.size()); put32(out, checksum(image.rgb));
    out.write(reinterpret_cast<const char *>(image.rgb.data()), image.rgb.size()); out.flush();
    bool ok = bool(out); out.close(); ok = ok && !out.fail();
    if (!ok || !readFile(temp, key)) { removeOwned(temp); return false; }
    // FAT does not promise rename-over-existing. Keep a recoverable prior copy.
    removeOwned(backup);
    if (regular(final) && !renameOwned(final, backup)) { removeOwned(temp); return false; }
    if (!renameOwned(temp, final)) { renameOwned(backup, final); removeOwned(temp); return false; }
    removeOwned(backup);
    entries_[key] = {headerSize + image.rgb.size(), ++clock_}; trim();
    return entries_.find(key) != entries_.end();
}

void CoverRepository::trim() {
    if (!initialized_) { initialize(); return; }
    if (!available_ || !safeRoot(root_)) return;
    std::uintmax_t total = 0; for (const auto &entry : entries_) total += entry.second.bytes;
    while (!entries_.empty() && (total > maxBytes_ || entries_.size() > maxEntries_)) {
        auto oldest = std::min_element(entries_.begin(), entries_.end(), [](const auto &a, const auto &b) { return a.second.used < b.second.used; });
        total -= oldest->second.bytes; removeOwned(root_ / (oldest->first + ".pnxc")); entries_.erase(oldest);
    }
}
} // namespace papernx
