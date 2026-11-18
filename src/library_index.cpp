#include "papernx/library_index.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <unordered_map>
#include <unordered_set>

namespace papernx {
namespace {
constexpr const char *inventorySchemaId = "libraryindex0001";
constexpr const char *inventorySchema = "PAPERNX-LIBRARY-1";
constexpr const char *overlaySchemaId = "libraryoverlay01";
constexpr const char *overlaySchema = "PAPERNX-DISCOVERY-1";

std::string utcTimestampNow() {
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buf;
}

std::string formatOsError(int err, std::uint32_t nativeResult) {
    std::ostringstream out;
    if (err) {
        const std::error_code code(err, std::generic_category());
        out << code.message() << " (errno=" << err << ")";
    } else {
        out << "unknown error";
    }
    if (nativeResult != 0) {
        out << " [libnx=0x" << std::hex << nativeResult << std::dec << "]";
    }
    return out.str();
}

ScanIssue makeIssue(const std::string &rootPath, const std::string &path, const std::string &op,
                    int err, std::uint32_t nativeResult, bool recoverable, ScanErrorCode code,
                    std::string customMessage = {}) {
    ScanIssue issue;
    issue.timestampUtc = utcTimestampNow();
    issue.rootPath = rootPath;
    issue.path = path;
    issue.operation = op;
    issue.errorNumber = err;
    issue.nativeResult = nativeResult;
    issue.recoverable = recoverable;
    issue.code = code;
    issue.message = customMessage.empty() ? formatOsError(err, nativeResult) : std::move(customMessage);
    return issue;
}

std::size_t pathDepthCount(const fs::path &path) {
    return static_cast<std::size_t>(std::distance(path.begin(), path.end()));
}

std::vector<Book> loadInventoryWithSchema(const fs::path &path, const char *markerId, const char *markerSchema,
                                          bool allowLegacyNoMarker, const FsOps &ops) {
    ProgressStore store(path);
    std::vector<Book> books;
    if (!store.load()) return books;
    const auto marker = store.all().find(markerId);
    if (marker == store.all().end()) {
        if (!allowLegacyNoMarker) return books;
    } else if (marker->second.bookTitle != markerSchema) {
        return books;
    }
    // bookPath=path, bookTitle=title, bookAuthor=byte count, lastRead=file mtime.
    for (const auto &[id, p] : store.all()) {
        if (id == markerId) continue;
        if (p.bookPath.empty() || p.bookAuthor.empty()) continue;
        std::uintmax_t size = 0;
        const auto parsed = std::from_chars(p.bookAuthor.data(), p.bookAuthor.data() + p.bookAuthor.size(), size);
        if (parsed.ec != std::errc{} || parsed.ptr != p.bookAuthor.data() + p.bookAuthor.size() ||
            documentId(p.bookPath, size) != id) {
            continue;
        }
        const auto bookPath = fs::u8path(p.bookPath);
        const auto st = ops.statPath(bookPath, false);
        if (!st.exists || st.isSymlink || !st.isRegularFile || st.size != size) continue;
        const auto type = detectFormat(bookPath);
        if (type == Format::Unsupported) continue;
        Book b;
        b.id = id;
        b.path = p.bookPath;
        b.bytes = size;
        b.title = p.bookTitle.empty() ? defaultTitle(bookPath) : p.bookTitle;
        b.modified = p.lastRead;
        b.format = type;
        if (st.modifiedKnown) {
            b.modified = st.modified;
        } else {
            int mtimeErr = 0;
            std::uint32_t mtimeNative = 0;
            const auto stamp = ops.lastWriteTime(bookPath, mtimeErr, mtimeNative);
            if (!mtimeErr) b.modified = stamp;
        }
        books.push_back(std::move(b));
        if (books.size() == 10000) break;
    }
    return books;
}

bool saveInventoryWithSchema(const fs::path &path, const char *markerId, const char *markerSchema,
                             const std::vector<Book> &books, const std::function<bool()> &cancelled) {
    auto stop = [&] { return cancelled && cancelled(); };
    ProgressStore cache(path);
    Progress marker;
    marker.bookTitle = markerSchema;
    cache.put(markerId, std::move(marker));
    for (const auto &b : books) {
        if (stop()) return false;
        Progress p;
        p.bookPath = b.path;
        p.bookTitle = b.title;
        p.bookAuthor = std::to_string(b.bytes);
        p.lastRead = b.modified;
        cache.put(b.id, std::move(p));
    }
    if (stop()) return false;
    ProgressStore::SaveOptions options;
    options.cancelled = cancelled;
    return cache.save(options);
}

bool isOwnedByAnyRoot(const std::string &bookPath, const std::vector<RootScanStatus> &roots,
                      const FsOps &ops, bool &ownerCompletedCleanly) {
    ownerCompletedCleanly = false;
    const auto p = fs::u8path(bookPath);
    for (const auto &r : roots) {
        const fs::path rootPath = fs::u8path(r.normalizedPath.empty() ? r.rootPath : r.normalizedPath);
        if (containsPathSafe(rootPath, p, ops)) {
            ownerCompletedCleanly = r.completedCleanly;
            return true;
        }
    }
    return false;
}

} // namespace

std::optional<std::uintmax_t> regularBookFileSize(const fs::path &path) {
    const auto info = defaultFsOps().statPath(path, false);
    if (!info.exists || info.isSymlink || !info.isRegularFile) return std::nullopt;
    return info.size;
}

std::optional<std::uintmax_t> regularBookFileSize(const fs::path &root, const fs::path &path) {
    const auto info = defaultFsOps().inspectRegularFile(root, path);
    if (!info.exists || info.isSymlink || !info.isRegularFile) return std::nullopt;
    return info.size;
}

std::vector<Book> InventoryRepository::load(const FsOps &ops) const {
    return loadInventoryWithSchema(path_, inventorySchemaId, inventorySchema, true, ops);
}

bool InventoryRepository::save(const std::vector<Book> &books, const std::function<bool()> &cancelled) const {
    return saveInventoryWithSchema(path_, inventorySchemaId, inventorySchema, books, cancelled);
}

std::vector<Book> DiscoveryOverlayRepository::load(const FsOps &ops) const {
    return loadInventoryWithSchema(path_, overlaySchemaId, overlaySchema, false, ops);
}

bool DiscoveryOverlayRepository::save(const std::vector<Book> &books, const std::function<bool()> &cancelled) const {
    return saveInventoryWithSchema(path_, overlaySchemaId, overlaySchema, books, cancelled);
}

fs::path discoveryOverlayPath(const fs::path &inventoryPath) {
    if (inventoryPath.empty()) return {};
    if (inventoryPath.filename() == "library-index.txt") {
        return inventoryPath.parent_path() / "library-discoveries.txt";
    }
    return fs::u8path(inventoryPath.u8string() + ".discoveries");
}

std::vector<Book> loadLibraryInventory(const fs::path &path, const FsOps &ops) {
    return InventoryRepository(path).load(ops);
}

std::vector<Book> loadMergedLibraryInventory(const fs::path &inventoryPath, const FsOps &ops) {
    auto books = InventoryRepository(inventoryPath).load(ops);
    const auto overlayPath = discoveryOverlayPath(inventoryPath);
    if (overlayPath.empty()) return books;
    auto overlay = DiscoveryOverlayRepository(overlayPath).load(ops);
    if (overlay.empty()) return books;
    std::unordered_set<std::string> seenIds;
    std::unordered_set<std::string> seenPaths;
    seenIds.reserve(books.size() + overlay.size());
    seenPaths.reserve(books.size() + overlay.size());
    for (const auto &b : books) {
        seenIds.insert(b.id);
        seenPaths.insert(b.path);
    }
    for (auto &b : overlay) {
        if (books.size() >= 10000) break;
        if (!seenIds.insert(b.id).second || !seenPaths.insert(b.path).second) continue;
        books.push_back(std::move(b));
    }
    return books;
}

TraversalOutcome traverseRootsDetailed(const std::vector<fs::path> &roots, std::size_t maxBooks,
                                       const std::function<bool()> &cancelled, bool normalizeRoots,
                                       const FsOps &ops) {
    TraversalOutcome outcome;
    auto isCancelled = [&] { return cancelled && cancelled(); };

    struct RootCandidate {
        std::string raw;
        fs::path normalized;
    };
    std::vector<RootCandidate> validCandidates;

    for (const auto &root : roots) {
        if (isCancelled()) {
            outcome.cancelled = true;
            outcome.complete = false;
            return outcome;
        }
        std::error_code normEc;
        fs::path resolved = normalizeRoots
            ? normalizeScanRoot(root, normEc, ops.assumeDevicePaths)
            : (isDevicePath(root) || ops.assumeDevicePaths ? normalizeDevicePath(root, normEc) : root);
        if (normEc || resolved.empty()) {
            RootScanStatus st;
            st.rootPath = root.u8string();
            st.available = false;
            st.completedCleanly = false;
            st.errorCount = 1;
            outcome.rootStatuses.push_back(std::move(st));
            outcome.inaccessibleDirs.push_back(makeIssue(
                root.u8string(), root.u8string(), "normalize_root",
                normEc ? normEc.value() : EINVAL, 0, true, ScanErrorCode::RootMissing));
            outcome.rootMissing = true;
            outcome.complete = false;
            continue;
        }
        const auto rootStat = ops.statPath(resolved, true);
        if (!rootStat.exists || !rootStat.isDirectory) {
            RootScanStatus st;
            st.rootPath = root.u8string();
            st.normalizedPath = resolved.u8string();
            st.available = false;
            st.completedCleanly = false;
            st.errorCount = 1;
            outcome.rootStatuses.push_back(std::move(st));
            outcome.inaccessibleDirs.push_back(makeIssue(
                resolved.u8string(), resolved.u8string(), "stat_root",
                rootStat.errorNumber ? rootStat.errorNumber : ENOENT, rootStat.nativeResult,
                true, ScanErrorCode::RootMissing));
            outcome.rootMissing = true;
            outcome.complete = false;
            continue;
        }
        validCandidates.push_back({root.u8string(), std::move(resolved)});
    }

    if (normalizeRoots) {
        std::sort(validCandidates.begin(), validCandidates.end(), [](const RootCandidate &a, const RootCandidate &b) {
            const auto da = pathDepthCount(a.normalized), db = pathDepthCount(b.normalized);
            return da == db ? a.normalized.u8string() < b.normalized.u8string() : da < db;
        });
    }

    std::vector<RootCandidate> uniqueRoots;
    uniqueRoots.reserve(validCandidates.size());
    for (const auto &candidate : validCandidates) {
        if (!normalizeRoots || std::none_of(uniqueRoots.begin(), uniqueRoots.end(), [&](const RootCandidate &existing) {
                return containsPathSafe(existing.normalized, candidate.normalized, ops);
            })) {
            uniqueRoots.push_back(candidate);
        }
    }

    std::unordered_set<std::string> seenIds;
    seenIds.reserve(std::min<std::size_t>(maxBooks, 1024));

    for (const auto &rootEntry : uniqueRoots) {
        if (isCancelled()) {
            outcome.cancelled = true;
            outcome.complete = false;
            break;
        }
        RootScanStatus rootStatus;
        rootStatus.rootPath = rootEntry.raw;
        rootStatus.normalizedPath = rootEntry.normalized.u8string();
        rootStatus.available = true;
        rootStatus.completedCleanly = true;

        if (outcome.books.size() >= maxBooks) {
            rootStatus.completedCleanly = false;
            rootStatus.truncated = true;
            outcome.truncatedByLimit = true;
            outcome.complete = false;
            outcome.rootStatuses.push_back(std::move(rootStatus));
            break;
        }

        struct PendingDir {
            fs::path path;
            int depth;
        };
        std::vector<PendingDir> stack;
        stack.push_back({rootEntry.normalized, 0});

        while (!stack.empty()) {
            if (isCancelled()) {
                outcome.cancelled = true;
                outcome.complete = false;
                rootStatus.completedCleanly = false;
                break;
            }
            const auto current = std::move(stack.back());
            stack.pop_back();

            std::vector<FsDirEntry> entries;
            int listErr = 0;
            std::uint32_t listNative = 0;
            const bool listOk = ops.listDirectory(current.path, entries, listErr, listNative);
            if (!listOk) {
                rootStatus.completedCleanly = false;
                ++rootStatus.errorCount;
                outcome.complete = false;
                const ScanErrorCode code = entries.empty() ? ScanErrorCode::DirectoryOpenFailed : ScanErrorCode::DirectoryReadFailed;
                outcome.inaccessibleDirs.push_back(makeIssue(
                    rootStatus.normalizedPath, current.path.u8string(),
                    entries.empty() ? "opendir" : "readdir",
                    listErr, listNative, true, code));
                if (entries.empty()) continue;
            }

            // Preserve deterministic traversal ordering inside each directory while handling
            // entries iteratively so a single faulty subdirectory cannot abort siblings.
            std::sort(entries.begin(), entries.end(), [](const FsDirEntry &a, const FsDirEntry &b) {
                return a.name < b.name;
            });

            std::vector<PendingDir> childDirs;
            for (const auto &entry : entries) {
                if (isCancelled()) {
                    outcome.cancelled = true;
                    outcome.complete = false;
                    rootStatus.completedCleanly = false;
                    break;
                }
                const fs::path childPath = current.path / fs::u8path(entry.name);
                FsDirEntry::Kind kind = entry.kind;
                if (kind == FsDirEntry::Kind::Unknown) {
                    const auto st = ops.statPath(childPath, false);
                    if (!st.exists) {
                        rootStatus.completedCleanly = false;
                        ++rootStatus.errorCount;
                        outcome.complete = false;
                        outcome.unreadableFiles.push_back(makeIssue(
                            rootStatus.normalizedPath, childPath.u8string(), "lstat",
                            st.errorNumber, st.nativeResult, true, ScanErrorCode::FileInspectFailed));
                        continue;
                    }
                    if (st.isSymlink) kind = FsDirEntry::Kind::Symlink;
                    else if (st.isDirectory) kind = FsDirEntry::Kind::Directory;
                    else if (st.isRegularFile) kind = FsDirEntry::Kind::Regular;
                    else kind = FsDirEntry::Kind::Other;
                }

                if (kind == FsDirEntry::Kind::Symlink || kind == FsDirEntry::Kind::Other) {
                    continue;
                }

                if (kind == FsDirEntry::Kind::Directory) {
                    if (current.depth + 1 > 16) {
                        rootStatus.completedCleanly = false;
                        rootStatus.depthLimited = true;
                        ++rootStatus.warningCount;
                        outcome.depthLimited = true;
                        outcome.complete = false;
                        outcome.inaccessibleDirs.push_back(makeIssue(
                            rootStatus.normalizedPath, childPath.u8string(), "depth_limit",
                            0, 0, true, ScanErrorCode::DepthLimitReached, "Maximum directory depth (16) reached"));
                        continue;
                    }
                    if (outcome.books.size() >= maxBooks) {
                        rootStatus.completedCleanly = false;
                        rootStatus.truncated = true;
                        outcome.truncatedByLimit = true;
                        outcome.complete = false;
                        break;
                    }
                    childDirs.push_back({childPath, current.depth + 1});
                    continue;
                }

                if (kind == FsDirEntry::Kind::Regular) {
                    const Format type = detectFormat(childPath);
                    if (type == Format::Unsupported) continue;
                    if (outcome.books.size() >= maxBooks) {
                        rootStatus.completedCleanly = false;
                        rootStatus.truncated = true;
                        outcome.truncatedByLimit = true;
                        outcome.complete = false;
                        break;
                    }
                    const auto inspected = ops.inspectRegularFile(rootEntry.normalized, childPath);
                    if (!inspected.exists || inspected.isSymlink || !inspected.isRegularFile) {
                        rootStatus.completedCleanly = false;
                        ++rootStatus.errorCount;
                        outcome.complete = false;
                        outcome.unreadableFiles.push_back(makeIssue(
                            rootStatus.normalizedPath, childPath.u8string(), "inspect_file",
                            inspected.errorNumber ? inspected.errorNumber : EIO, inspected.nativeResult,
                            true, ScanErrorCode::FileInspectFailed));
                        continue;
                    }
                    Book b;
                    b.path = childPath.u8string();
                    b.bytes = inspected.size;
                    b.id = documentId(b.path, inspected.size);
                    b.title = defaultTitle(childPath);
                    b.format = type;
                    if (inspected.modifiedKnown) {
                        b.modified = inspected.modified;
                    } else if (ops.lastWriteTime) {
                        int mtimeErr = 0;
                        std::uint32_t mtimeNative = 0;
                        const auto stamp = ops.lastWriteTime(childPath, mtimeErr, mtimeNative);
                        if (mtimeErr == 0) {
                            b.modified = stamp;
                        } else {
                            // Metadata timestamp failure must NEVER discard a verified regular book
                            // or fail the entire library scan.
                            b.modified = 0;
                            ++rootStatus.warningCount;
                            outcome.metadataWarnings.push_back(makeIssue(
                                rootStatus.normalizedPath, b.path, "last_write_time",
                                mtimeErr, mtimeNative, true, ScanErrorCode::MetadataTimestampFailed));
                        }
                    }
                    if (seenIds.insert(b.id).second) {
                        outcome.books.push_back(std::move(b));
                        ++rootStatus.booksFound;
                    }
                }
            }

            if (outcome.cancelled || outcome.truncatedByLimit) break;
            for (auto it = childDirs.rbegin(); it != childDirs.rend(); ++it) {
                stack.push_back(std::move(*it));
            }
        }

        if (outcome.truncatedByLimit) {
            outcome.metadataWarnings.push_back(makeIssue(
                rootStatus.normalizedPath, rootStatus.normalizedPath, "book_limit",
                0, 0, true, ScanErrorCode::BookLimitReached, "Maximum book scan limit reached"));
        }
        outcome.rootStatuses.push_back(std::move(rootStatus));
        if (outcome.cancelled) break;
    }

    return outcome;
}

static const char *scanStatusLabel(ScanStatus s) {
    switch (s) {
        case ScanStatus::Failed: return "failed";
        case ScanStatus::Complete: return "complete";
        case ScanStatus::Partial: return "partial";
        case ScanStatus::Cancelled: return "cancelled";
        case ScanStatus::RootUnavailable: return "root_unavailable";
        case ScanStatus::Incomplete: return "incomplete";
        case ScanStatus::InventoryWriteFailed: return "inventory_write_failed";
    }
    return "unknown";
}

bool appendLibraryScanLog(const fs::path &logPath, const LibraryScanResult &result,
                          const std::string &appVersion, const std::string &sourceCommit,
                          std::uintmax_t maxBytes) {
    if (logPath.empty()) return false;
    std::error_code ec;
    if (!logPath.parent_path().empty()) {
        fs::create_directories(logPath.parent_path(), ec);
        if (ec) return false;
    }
    const auto currentSize = fs::file_size(logPath, ec);
    if (!ec && currentSize >= maxBytes) {
        const fs::path rotated = fs::u8path(logPath.u8string() + ".1");
        fs::remove(rotated, ec);
        ec.clear();
        fs::rename(logPath, rotated, ec);
    }
    std::ofstream out(logPath, std::ios::app | std::ios::out);
    if (!out) return false;
    const auto ts = utcTimestampNow();
    out << "=== SCAN timestamp=" << ts
        << " version=" << appVersion
        << " commit=" << sourceCommit
        << " generation=" << result.generation
        << " status=" << scanStatusLabel(result.status)
        << " complete=" << (result.complete ? 1 : 0)
        << " partial=" << (result.partial ? 1 : 0)
        << " cached=" << (result.cached ? 1 : 0)
        << " overlaySaved=" << (result.overlaySaved ? 1 : 0)
        << " total=" << result.books.size()
        << " added=" << result.addedCount
        << " updated=" << result.updatedCount
        << " removed=" << result.removedCount
        << " unreadable=" << result.unreadableFiles.size()
        << " dirErrors=" << result.inaccessibleDirs.size()
        << " warnings=" << result.metadataWarnings.size()
        << " ===\n";
    for (const auto &rs : result.rootStatuses) {
        out << "  ROOT path=" << rs.rootPath
            << " normalized=" << rs.normalizedPath
            << " available=" << (rs.available ? 1 : 0)
            << " clean=" << (rs.completedCleanly ? 1 : 0)
            << " found=" << rs.booksFound
            << " errors=" << rs.errorCount
            << " warnings=" << rs.warningCount
            << (rs.depthLimited ? " depth_limited=1" : "")
            << (rs.truncated ? " truncated=1" : "")
            << "\n";
    }
    auto writeIssues = [&](const char *kind, const std::vector<ScanIssue> &issues) {
        for (const auto &issue : issues) {
            out << "  " << kind
                << " ts=" << issue.timestampUtc
                << " version=" << appVersion
                << " commit=" << sourceCommit
                << " gen=" << result.generation
                << " root=" << issue.rootPath
                << " path=" << issue.path
                << " op=" << issue.operation
                << " code=" << static_cast<int>(issue.code)
                << " errno=" << issue.errorNumber
                << " libnx=0x" << std::hex << issue.nativeResult << std::dec
                << " recoverable=" << (issue.recoverable ? 1 : 0)
                << " continued=1"
                << " msg=" << issue.message << "\n";
        }
    };
    writeIssues("DIR_ERR", result.inaccessibleDirs);
    writeIssues("FILE_ERR", result.unreadableFiles);
    writeIssues("WARN", result.metadataWarnings);
    return static_cast<bool>(out);
}

LibraryScanResult scanLibrary(const ScanRequest &request, const std::function<bool()> &cancelled) {
    LibraryScanResult result;
    auto cancelledNow = [&] { return cancelled && cancelled(); };
    const FsOps &ops = request.fsOps ? *request.fsOps : defaultFsOps();
#if defined(__cpp_exceptions)
    try {
#endif
        std::unordered_map<std::string, Book> priorById;
        std::unordered_map<std::string, Book> priorByPath;
        priorById.reserve(request.known.size());
        priorByPath.reserve(request.known.size());
        for (const auto &book : request.known) {
            priorById.emplace(book.id, book);
            priorByPath.emplace(book.path, book);
        }

        const auto traversal = traverseRootsDetailed(request.roots, 10000, cancelled, true, ops);
        result.rootStatuses = traversal.rootStatuses;
        result.unreadableFiles = traversal.unreadableFiles;
        result.inaccessibleDirs = traversal.inaccessibleDirs;
        result.metadataWarnings = traversal.metadataWarnings;
        result.truncatedByLimit = traversal.truncatedByLimit;
        result.discoveredBooks = traversal.books;

        if (traversal.cancelled || cancelledNow()) {
            result.cancelled = true;
            result.status = ScanStatus::Cancelled;
            result.errorCode = ScanErrorCode::Cancelled;
            result.books.clear();
            result.error.clear();
            return result;
        }

        std::unordered_set<std::string> ids;
        std::unordered_set<std::string> paths;
        ids.reserve(10000);
        paths.reserve(10000);

        auto addBook = [&](Book book, bool countDelta) {
            if (result.books.size() >= 10000) return;
            if (!ids.insert(book.id).second || !paths.insert(book.path).second) return;
            auto oldId = priorById.find(book.id);
            if (oldId != priorById.end()) {
                if (!oldId->second.title.empty()) book.title = oldId->second.title;
                if (!oldId->second.author.empty()) book.author = oldId->second.author;
                if (countDelta) ++result.existingCount;
            } else {
                auto oldPath = priorByPath.find(book.path);
                if (oldPath != priorByPath.end()) {
                    if (countDelta) ++result.updatedCount;
                } else if (countDelta) {
                    ++result.addedCount;
                }
            }
            result.books.push_back(std::move(book));
        };

        for (auto book : traversal.books) {
            addBook(std::move(book), true);
        }

        const bool partial = !traversal.complete || traversal.rootMissing ||
                             !traversal.unreadableFiles.empty() || !traversal.inaccessibleDirs.empty() ||
                             traversal.truncatedByLimit || traversal.depthLimited;

        // Preserve independently opened books and books belonging to unavailable/incomplete roots
        // without erasing user library entries during partial failures.
        for (const auto &[id, old] : priorById) {
            if (cancelledNow()) {
                result.cancelled = true;
                result.status = ScanStatus::Cancelled;
                result.errorCode = ScanErrorCode::Cancelled;
                result.books.clear();
                return result;
            }
            if (ids.count(id) || paths.count(old.path)) continue;

            bool ownerClean = false;
            const bool underAnyRoot = isOwnedByAnyRoot(old.path, result.rootStatuses, ops, ownerClean);
            if (underAnyRoot && !ownerClean) {
                // Root or subtree was unavailable/incomplete: keep prior verified record safely.
                addBook(old, false);
                continue;
            }

            const auto path = fs::u8path(old.path);
            if (detectFormat(path) == Format::Unsupported) continue;
            const auto st = ops.statPath(path, false);
            if (!st.exists || st.isSymlink || !st.isRegularFile) {
                ++result.removedCount;
                continue;
            }
            auto book = old;
            book.bytes = st.size;
            book.id = documentId(book.path, st.size);
            if (book.id != id) {
                book.title = defaultTitle(path);
                book.author.clear();
            }
            if (st.modifiedKnown) {
                book.modified = st.modified;
            }
            addBook(std::move(book), false);
        }

        std::sort(result.books.begin(), result.books.end(), [](const Book &a, const Book &b) {
            const int order = naturalCompare(a.path, b.path);
            return order ? order < 0 : a.id < b.id;
        });

        if (cancelledNow()) {
            result.cancelled = true;
            result.status = ScanStatus::Cancelled;
            result.errorCode = ScanErrorCode::Cancelled;
            result.books.clear();
            return result;
        }

        if (!partial) {
            result.complete = true;
            result.partial = false;
            result.status = ScanStatus::Complete;
            result.errorCode = ScanErrorCode::None;
            if (!request.inventory.empty()) {
                result.cached = InventoryRepository(request.inventory).save(result.books, cancelled);
                if (!result.cached) {
                    if (cancelledNow()) {
                        result.cancelled = true;
                        result.complete = false;
                        result.status = ScanStatus::Cancelled;
                        result.errorCode = ScanErrorCode::Cancelled;
                        result.books.clear();
                        return result;
                    }
                    result.status = ScanStatus::InventoryWriteFailed;
                    result.errorCode = ScanErrorCode::InventoryWriteFailed;
                    result.error = "Library cache could not be saved";
                    result.metadataWarnings.push_back(makeIssue(
                        request.inventory.parent_path().u8string(), request.inventory.u8string(),
                        "inventory_save", EIO, 0, true, ScanErrorCode::InventoryWriteFailed,
                        result.error));
                } else {
                    const auto overlayPath = discoveryOverlayPath(request.inventory);
                    if (!overlayPath.empty()) {
                        // Remove overlay cleanly once the full snapshot is durably published.
                        std::error_code rmEc;
                        if (!fs::remove(overlayPath, rmEc)) {
                            DiscoveryOverlayRepository(overlayPath).save({}, cancelled);
                        }
                    }
                }
            }
        } else {
            // Partial / incomplete / root-unavailable scan:
            // Preserve the last complete library-index.txt untouched, while publishing verified
            // discovered books in memory and persisting them in the discovery overlay.
            result.complete = false;
            result.partial = true;
            if (traversal.rootMissing) {
                result.status = ScanStatus::RootUnavailable;
            } else if (traversal.truncatedByLimit || traversal.depthLimited || result.books.empty()) {
                result.status = ScanStatus::Incomplete;
            } else {
                result.status = ScanStatus::Partial;
            }
            if (traversal.rootMissing) {
                result.errorCode = ScanErrorCode::RootMissing;
                result.error = "Library folder unavailable";
            } else if (traversal.truncatedByLimit) {
                result.errorCode = ScanErrorCode::BookLimitReached;
                result.error = "Library scan incomplete";
            } else if (traversal.depthLimited) {
                result.errorCode = ScanErrorCode::DepthLimitReached;
                result.error = "Library scan incomplete";
            } else if (!traversal.inaccessibleDirs.empty()) {
                result.errorCode = traversal.inaccessibleDirs.front().code;
                result.error = "Library scan incomplete";
            } else if (!traversal.unreadableFiles.empty()) {
                result.errorCode = ScanErrorCode::FileInspectFailed;
                result.error = "Library scan incomplete";
            } else {
                result.errorCode = ScanErrorCode::None;
                result.error = "Library scan incomplete";
            }
            if (!request.inventory.empty() && !result.discoveredBooks.empty()) {
                const auto overlayPath = discoveryOverlayPath(request.inventory);
                if (!overlayPath.empty()) {
                    auto existingOverlay = DiscoveryOverlayRepository(overlayPath).load();
                    std::unordered_set<std::string> overlayIds, overlayPaths;
                    std::vector<Book> mergedOverlay;
                    for (auto &b : result.discoveredBooks) {
                        if (overlayIds.insert(b.id).second && overlayPaths.insert(b.path).second) {
                            mergedOverlay.push_back(b);
                        }
                    }
                    for (auto &b : existingOverlay) {
                        if (mergedOverlay.size() >= 10000) break;
                        if (overlayIds.insert(b.id).second && overlayPaths.insert(b.path).second) {
                            mergedOverlay.push_back(std::move(b));
                        }
                    }
                    result.overlaySaved = DiscoveryOverlayRepository(overlayPath).save(mergedOverlay, cancelled);
                }
            }
        }
#if defined(__cpp_exceptions)
    } catch (const std::exception &error) {
        result = LibraryScanResult{};
        result.status = ScanStatus::Failed;
        result.errorCode = ScanErrorCode::FatalIo;
        result.error = error.what();
    }
#endif
    return result;
}

std::uint64_t LibraryScanner::begin(ScanRequest request) {
    if (pending()) return 0;
    cancel_ = false;
    ready_ = false;
    result_ = {};
    const auto generation = ++generation_;
    worker_ = std::thread([this, generation, request = std::move(request)] {
        auto result = scanLibrary(request, [this] { return cancel_.load(); });
        result.generation = generation;
        result_ = std::move(result);
        ready_.store(true);
    });
    return generation;
}

std::optional<LibraryScanResult> LibraryScanner::take() {
    if (!ready_) return std::nullopt;
    worker_.join();
    auto result = std::move(result_);
    // scanLibrary owns the cancellation/commit boundary; a later cancel cannot undo publication.
    ready_ = false;
    cancel_ = false;
    return result;
}

void LibraryScanner::stop() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
    ready_ = false;
}

} // namespace papernx
