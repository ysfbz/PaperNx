#include "papernx/fs_adapter.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#ifdef __SWITCH__
#include <switch.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace papernx {
namespace {

std::uint32_t currentNativeResult() {
#ifdef __SWITCH__
    return fsdevGetLastResult();
#else
    return 0;
#endif
}

bool splitDevicePath(const std::string &raw, std::string &prefix, std::vector<std::string> &components) {
    prefix.clear();
    components.clear();
    const auto colon = raw.find(':');
    if (colon == std::string::npos || colon == 0) return false;
    if (colon + 1 < raw.size() && raw[colon + 1] != '/') return false;
    if (raw.find(':', colon + 1) != std::string::npos || raw.find('\\') != std::string::npos || raw.find('\0') != std::string::npos) {
        return false;
    }
    for (std::size_t i = 0; i < colon; ++i) {
        const char c = raw[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    prefix = raw.substr(0, colon) + ":/"; // "<dev>:/"
    std::size_t pos = (colon + 1 < raw.size()) ? (colon + 2) : raw.size();
    while (pos < raw.size()) {
        auto slash = raw.find('/', pos);
        if (slash == std::string::npos) slash = raw.size();
        auto part = raw.substr(pos, slash - pos);
        if (!part.empty() && part != ".") {
            if (part == "..") {
                if (components.empty()) return false;
                components.pop_back();
            } else {
                components.push_back(std::move(part));
            }
        }
        pos = slash + 1;
    }
    return true;
}

std::string asciiLower(std::string text) {
    for (char &c : text) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return text;
}

FsStatInfo realStatPath(const fs::path &path, bool followSymlinks) {
    FsStatInfo info;
#ifndef _WIN32
    struct stat st{};
    errno = 0;
#ifdef __SWITCH__
    (void)followSymlinks;
    const int rc = ::stat(path.c_str(), &st);
#else
    const int rc = followSymlinks ? ::stat(path.c_str(), &st) : ::lstat(path.c_str(), &st);
#endif
    if (rc != 0) {
        info.errorNumber = errno ? errno : EIO;
        info.nativeResult = currentNativeResult();
        return info;
    }
    info.exists = true;
    info.isSymlink = S_ISLNK(st.st_mode);
    info.isDirectory = S_ISDIR(st.st_mode);
    info.isRegularFile = S_ISREG(st.st_mode);
    info.size = st.st_size >= 0 ? static_cast<std::uintmax_t>(st.st_size) : 0;
    if (st.st_mtime != 0) {
        info.modified = static_cast<std::int64_t>(st.st_mtime);
        info.modifiedKnown = true;
    }
    info.dev = static_cast<std::uint64_t>(st.st_dev);
    info.ino = static_cast<std::uint64_t>(st.st_ino);
    return info;
#else
    std::error_code ec;
    const auto status = followSymlinks ? fs::status(path, ec) : fs::symlink_status(path, ec);
    if (ec) {
        info.errorNumber = ec.value();
        return info;
    }
    info.exists = fs::exists(status);
    info.isSymlink = fs::is_symlink(status);
    info.isDirectory = fs::is_directory(status);
    info.isRegularFile = fs::is_regular_file(status);
    if (info.isRegularFile) {
        const auto sz = fs::file_size(path, ec);
        if (!ec) info.size = sz;
    }
    return info;
#endif
}

FsStatInfo realInspectRegularFile(const fs::path &root, const fs::path &path) {
    FsStatInfo info;
    const bool device = isDevicePath(root) || isDevicePath(path);
    const auto relative = relativeComponentsSafe(root, path, device);
    if (!relative || relative->empty()) {
        info.errorNumber = EINVAL;
        return info;
    }

#if !defined(__SWITCH__) && !defined(_WIN32) && defined(O_NOFOLLOW) && defined(O_DIRECTORY)
    if (!device) {
        int flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW;
#ifdef O_CLOEXEC
        flags |= O_CLOEXEC;
#endif
        errno = 0;
        int directory = ::open(root.c_str(), flags);
        if (directory < 0) {
            info.errorNumber = errno ? errno : EIO;
            return info;
        }
        for (std::size_t idx = 0; idx < relative->size(); ++idx) {
            const auto &component = (*relative)[idx];
            const bool final = (idx + 1 == relative->size());
            int childFlags = O_RDONLY | O_NOFOLLOW | (final ? O_NONBLOCK : O_DIRECTORY);
#ifdef O_CLOEXEC
            childFlags |= O_CLOEXEC;
#endif
            errno = 0;
            const int child = ::openat(directory, component.c_str(), childFlags);
            ::close(directory);
            if (child < 0) {
                info.errorNumber = errno ? errno : EIO;
                return info;
            }
            if (final) {
                struct stat st{};
                errno = 0;
                const int rc = ::fstat(child, &st);
                const int savedErr = errno;
                ::close(child);
                if (rc != 0) {
                    info.errorNumber = savedErr ? savedErr : EIO;
                    return info;
                }
                info.exists = true;
                info.isSymlink = S_ISLNK(st.st_mode);
                info.isDirectory = S_ISDIR(st.st_mode);
                info.isRegularFile = S_ISREG(st.st_mode);
                if (!info.isRegularFile || st.st_size < 0) {
                    info.errorNumber = EINVAL;
                    return info;
                }
                info.size = static_cast<std::uintmax_t>(st.st_size);
                info.dev = static_cast<std::uint64_t>(st.st_dev);
                info.ino = static_cast<std::uint64_t>(st.st_ino);
                if (st.st_mtime != 0) {
                    info.modified = static_cast<std::int64_t>(st.st_mtime);
                    info.modifiedKnown = true;
                }
                return info;
            }
            directory = child;
        }
        ::close(directory);
        info.errorNumber = EINVAL;
        return info;
    }
#endif

#ifndef _WIN32
    if (!device) {
        fs::path current = root;
        for (const auto &component : *relative) {
            current /= fs::u8path(component);
            const auto st = realStatPath(current, false);
            if (!st.exists || st.isSymlink) {
                info.errorNumber = st.errorNumber ? st.errorNumber : EACCES;
                info.nativeResult = st.nativeResult;
                return info;
            }
        }
    }
    int flags = O_RDONLY;
#if !defined(__SWITCH__) && defined(O_NOFOLLOW)
    flags |= O_NOFOLLOW | O_NONBLOCK;
#endif
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    errno = 0;
    const int fd = ::open(path.c_str(), flags);
    if (fd < 0) {
        info.errorNumber = errno ? errno : EIO;
        info.nativeResult = currentNativeResult();
        return info;
    }
    struct stat st{};
    errno = 0;
    const int rc = ::fstat(fd, &st);
    const int savedErr = errno;
    const auto savedNative = currentNativeResult();
    ::close(fd);
    if (rc != 0) {
        info.errorNumber = savedErr ? savedErr : EIO;
        info.nativeResult = savedNative;
        return info;
    }
    info.exists = true;
    info.isSymlink = S_ISLNK(st.st_mode);
    info.isDirectory = S_ISDIR(st.st_mode);
    info.isRegularFile = S_ISREG(st.st_mode);
    if (!info.isRegularFile || st.st_size < 0) {
        info.errorNumber = EINVAL;
        return info;
    }
    info.size = static_cast<std::uintmax_t>(st.st_size);
    info.dev = static_cast<std::uint64_t>(st.st_dev);
    info.ino = static_cast<std::uint64_t>(st.st_ino);
    if (st.st_mtime != 0) {
        info.modified = static_cast<std::int64_t>(st.st_mtime);
        info.modifiedKnown = true;
    }
    return info;
#else
    std::error_code ec;
    const auto status = fs::symlink_status(path, ec);
    if (ec || fs::is_symlink(status) || !fs::is_regular_file(status)) {
        info.errorNumber = ec ? ec.value() : EINVAL;
        return info;
    }
    const auto sz = fs::file_size(path, ec);
    if (ec) {
        info.errorNumber = ec.value();
        return info;
    }
    info.exists = true;
    info.isRegularFile = true;
    info.size = sz;
    return info;
#endif
}

bool realListDirectory(const fs::path &dir, std::vector<FsDirEntry> &out, int &errorNumber, std::uint32_t &nativeResult) {
    out.clear();
    errorNumber = 0;
    nativeResult = 0;
#ifndef _WIN32
    errno = 0;
    DIR *dp = ::opendir(dir.c_str());
    if (!dp) {
        errorNumber = errno ? errno : EIO;
        nativeResult = currentNativeResult();
        return false;
    }
    bool ok = true;
    for (;;) {
        errno = 0;
        struct dirent *ent = ::readdir(dp);
        if (!ent) {
            if (errno != 0) {
                errorNumber = errno;
                nativeResult = currentNativeResult();
                ok = false;
            }
            break;
        }
        const std::string name(ent->d_name);
        if (name == "." || name == "..") continue;
        FsDirEntry item;
        item.name = name;
#ifdef _DIRENT_HAVE_D_TYPE
        switch (ent->d_type) {
            case DT_DIR: item.kind = FsDirEntry::Kind::Directory; break;
            case DT_REG: item.kind = FsDirEntry::Kind::Regular; break;
            case DT_LNK: item.kind = FsDirEntry::Kind::Symlink; break;
            case DT_UNKNOWN: item.kind = FsDirEntry::Kind::Unknown; break;
            default: item.kind = FsDirEntry::Kind::Other; break;
        }
#else
        item.kind = FsDirEntry::Kind::Unknown;
#endif
        out.push_back(std::move(item));
    }
    ::closedir(dp);
    return ok;
#else
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) {
        errorNumber = ec.value();
        return false;
    }
    while (it != end) {
        FsDirEntry item;
        item.name = it->path().filename().u8string();
        std::error_code entryEc;
        if (it->is_symlink(entryEc)) item.kind = FsDirEntry::Kind::Symlink;
        else if (it->is_directory(entryEc)) item.kind = FsDirEntry::Kind::Directory;
        else if (it->is_regular_file(entryEc)) item.kind = FsDirEntry::Kind::Regular;
        else item.kind = FsDirEntry::Kind::Unknown;
        out.push_back(std::move(item));
        it.increment(ec);
        if (ec) {
            errorNumber = ec.value();
            return false;
        }
    }
    return true;
#endif
}

std::int64_t realLastWriteTime(const fs::path &path, int &errorNumber, std::uint32_t &nativeResult) {
    errorNumber = 0;
    nativeResult = 0;
    std::error_code ec;
    const auto stamp = fs::last_write_time(path, ec);
    if (ec) {
        errorNumber = ec.value();
        nativeResult = currentNativeResult();
        return 0;
    }
    return std::chrono::duration_cast<std::chrono::seconds>(stamp.time_since_epoch()).count();
}

} // namespace

const FsOps &defaultFsOps() {
    static const FsOps ops{
        realStatPath,
        realInspectRegularFile,
        realListDirectory,
        realLastWriteTime,
#ifdef __SWITCH__
        true
#else
        false
#endif
    };
    return ops;
}

bool isDevicePathString(const std::string &raw) {
    std::string prefix;
    std::vector<std::string> components;
    return splitDevicePath(raw, prefix, components);
}

bool isDevicePath(const fs::path &path) {
    return isDevicePathString(path.generic_u8string());
}

fs::path normalizeDevicePath(const fs::path &path, std::error_code &ec) {
    ec.clear();
    std::string prefix;
    std::vector<std::string> components;
    if (!splitDevicePath(path.generic_u8string(), prefix, components)) {
        ec = std::make_error_code(std::errc::invalid_argument);
        return {};
    }
    std::string normalized = prefix;
    for (std::size_t i = 0; i < components.size(); ++i) {
        if (i > 0) normalized.push_back('/');
        normalized += components[i];
    }
    return fs::u8path(normalized);
}

fs::path normalizeScanRoot(const fs::path &root, std::error_code &ec, bool forceDeviceMode) {
    ec.clear();
    const bool device = forceDeviceMode || isDevicePath(root);
    if (device) {
        return normalizeDevicePath(root, ec);
    }
    return fs::weakly_canonical(root, ec);
}

std::optional<std::vector<std::string>> relativeComponentsSafe(const fs::path &root, const fs::path &path, bool deviceMode) {
    if (deviceMode || isDevicePath(root) || isDevicePath(path)) {
        std::string rootPrefix, pathPrefix;
        std::vector<std::string> rootParts, pathParts;
        if (!splitDevicePath(root.generic_u8string(), rootPrefix, rootParts) ||
            !splitDevicePath(path.generic_u8string(), pathPrefix, pathParts)) {
            return std::nullopt;
        }
        if (asciiLower(rootPrefix) != asciiLower(pathPrefix) || pathParts.size() < rootParts.size()) {
            return std::nullopt;
        }
        for (std::size_t i = 0; i < rootParts.size(); ++i) {
            if (asciiLower(rootParts[i]) != asciiLower(pathParts[i])) return std::nullopt;
        }
        return std::vector<std::string>(pathParts.begin() + static_cast<std::ptrdiff_t>(rootParts.size()), pathParts.end());
    }
    const auto relative = path.lexically_relative(root);
    if (relative.empty() || relative.is_absolute()) return std::nullopt;
    std::vector<std::string> parts;
    for (const auto &component : relative) {
        const auto s = component.u8string();
        if (s == "." || s == "..") return std::nullopt;
        if (!s.empty()) parts.push_back(s);
    }
    if (parts.empty()) return std::nullopt;
    return parts;
}

bool pathsEquivalentSafe(const fs::path &a, const fs::path &b, const FsOps &ops) {
    if (ops.assumeDevicePaths || isDevicePath(a) || isDevicePath(b)) {
        std::string pa, pb;
        std::vector<std::string> ca, cb;
        if (!splitDevicePath(a.generic_u8string(), pa, ca) || !splitDevicePath(b.generic_u8string(), pb, cb)) {
            return a.lexically_normal() == b.lexically_normal();
        }
        if (asciiLower(pa) != asciiLower(pb) || ca.size() != cb.size()) return false;
        for (std::size_t i = 0; i < ca.size(); ++i) {
            if (asciiLower(ca[i]) != asciiLower(cb[i])) return false;
        }
        return true;
    }

    const auto statA = ops.statPath ? ops.statPath(a, true) : FsStatInfo{};
    const auto statB = ops.statPath ? ops.statPath(b, true) : FsStatInfo{};
    if (!statA.exists || !statB.exists) return false;
    if (statA.isDirectory != statB.isDirectory || statA.isRegularFile != statB.isRegularFile) return false;
    // libnx / synthetic VFS drivers leave st_dev=0 and st_ino=0 for every file and directory.
    // Never treat distinct paths as equivalent solely because both report zeroed device/inode IDs.
    if (statA.dev == 0 && statA.ino == 0 && statB.dev == 0 && statB.ino == 0) {
        return a.lexically_normal() == b.lexically_normal();
    }
    return statA.dev == statB.dev && statA.ino == statB.ino;
}

bool containsPathSafe(const fs::path &parent, const fs::path &child, const FsOps &ops) {
    if (ops.assumeDevicePaths || isDevicePath(parent) || isDevicePath(child)) {
        std::string pp, cp;
        std::vector<std::string> pc, cc;
        if (!splitDevicePath(parent.generic_u8string(), pp, pc) || !splitDevicePath(child.generic_u8string(), cp, cc)) {
            return false;
        }
        if (asciiLower(pp) != asciiLower(cp) || cc.size() < pc.size()) return false;
        for (std::size_t i = 0; i < pc.size(); ++i) {
            if (asciiLower(pc[i]) != asciiLower(cc[i])) return false;
        }
        return true;
    }

    for (auto current = child; !current.empty();) {
        if (pathsEquivalentSafe(parent, current, ops)) return true;
        auto next = current.parent_path();
        if (next == current) break;
        current = std::move(next);
    }
    return false;
}

} // namespace papernx
