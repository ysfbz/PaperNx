#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace papernx {
namespace fs = std::filesystem;

struct FsDirEntry {
    enum class Kind { Unknown, Regular, Directory, Symlink, Other };
    std::string name;
    Kind kind{Kind::Unknown};
};

struct FsStatInfo {
    bool exists{false};
    bool isDirectory{false};
    bool isRegularFile{false};
    bool isSymlink{false};
    std::uintmax_t size{0};
    std::int64_t modified{0};
    bool modifiedKnown{false};
    std::uint64_t dev{0};
    std::uint64_t ino{0};
    int errorNumber{0};
    std::uint32_t nativeResult{0};
};

struct FsOps {
    std::function<FsStatInfo(const fs::path &path, bool followSymlinks)> statPath;
    std::function<FsStatInfo(const fs::path &root, const fs::path &path)> inspectRegularFile;
    std::function<bool(const fs::path &dir, std::vector<FsDirEntry> &out, int &errorNumber, std::uint32_t &nativeResult)> listDirectory;
    std::function<std::int64_t(const fs::path &path, int &errorNumber, std::uint32_t &nativeResult)> lastWriteTime;
    bool assumeDevicePaths{false};
};

const FsOps &defaultFsOps();

bool isDevicePathString(const std::string &raw);
bool isDevicePath(const fs::path &path);
fs::path normalizeDevicePath(const fs::path &path, std::error_code &ec);
fs::path normalizeScanRoot(const fs::path &root, std::error_code &ec, bool forceDeviceMode = false);
std::optional<std::vector<std::string>> relativeComponentsSafe(const fs::path &root, const fs::path &path, bool deviceMode = false);
bool pathsEquivalentSafe(const fs::path &a, const fs::path &b, const FsOps &ops = defaultFsOps());
bool containsPathSafe(const fs::path &parent, const fs::path &child, const FsOps &ops = defaultFsOps());

} // namespace papernx
