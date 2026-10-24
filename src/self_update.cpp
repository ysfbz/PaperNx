#include "papernx/self_update.hpp"
#include "update_json.hpp"
#include <curl/curl.h>
#include <mupdf/fitz.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <unistd.h>

namespace papernx {
namespace {
constexpr std::uint64_t maxNro = 256ull * 1024 * 1024;
constexpr std::uint64_t stagingReserve = 1024ull * 1024;

struct Version {
    std::array<std::uint64_t, 3> core{};
    std::vector<std::string> prerelease;
    bool valid{false};
};

bool digits(const std::string &value) {
    return !value.empty() && value.find_first_not_of("0123456789") == std::string::npos;
}

std::vector<std::string> split(const std::string &value, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto end = value.find(separator, start);
        parts.push_back(value.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return parts;
}

bool validIdentifiers(const std::vector<std::string> &parts, bool numericOnly,
                      bool rejectNumericLeadingZero = false) {
    for (const auto &part : parts) {
        if (part.empty() || part.size() > 32) return false;
        for (unsigned char c : part) {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-')) return false;
        }
        const bool numeric = digits(part);
        if (numericOnly && !numeric) return false;
        if (rejectNumericLeadingZero && numeric && part.size() > 1 && part.front() == '0') return false;
    }
    return true;
}

Version parseVersion(std::string value) {
    Version parsed;
    if (value.empty() || value.size() > 128) return parsed;
    if (value.front() == 'v' || value.front() == 'V') value.erase(value.begin());

    const auto metadata = value.find('+');
    if (metadata != std::string::npos) {
        if (!validIdentifiers(split(value.substr(metadata + 1), '.'), false)) return parsed;
        value.resize(metadata);
    }

    const auto dash = value.find('-');
    if (dash != std::string::npos) {
        parsed.prerelease = split(value.substr(dash + 1), '.');
        if (!validIdentifiers(parsed.prerelease, false, true)) return Version{};
        value.resize(dash);
    }

    const auto components = split(value, '.');
    if (components.size() != parsed.core.size() || !validIdentifiers(components, true, true)) return Version{};
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto result = std::from_chars(components[i].data(), components[i].data() + components[i].size(), parsed.core[i]);
        if (result.ec != std::errc{} || result.ptr != components[i].data() + components[i].size()) return Version{};
    }
    parsed.valid = true;
    return parsed;
}

int compareNumericIdentifier(const std::string &left, const std::string &right) {
    if (left.size() != right.size()) return left.size() < right.size() ? -1 : 1;
    return left == right ? 0 : left < right ? -1 : 1;
}

int compareVersion(const Version &left, const Version &right) {
    for (std::size_t i = 0; i < left.core.size(); ++i) {
        if (left.core[i] != right.core[i]) return left.core[i] < right.core[i] ? -1 : 1;
    }
    if (left.prerelease.empty() != right.prerelease.empty()) return left.prerelease.empty() ? 1 : -1;
    for (std::size_t i = 0; i < std::min(left.prerelease.size(), right.prerelease.size()); ++i) {
        const auto &a = left.prerelease[i];
        const auto &b = right.prerelease[i];
        if (a == b) continue;
        const bool aNumeric = digits(a), bNumeric = digits(b);
        if (aNumeric != bNumeric) return aNumeric ? -1 : 1;
        return aNumeric ? compareNumericIdentifier(a, b) : a < b ? -1 : 1;
    }
    if (left.prerelease.size() == right.prerelease.size()) return 0;
    return left.prerelease.size() < right.prerelease.size() ? -1 : 1;
}

std::string withoutPrefix(std::string value) {
    if (!value.empty() && (value.front() == 'v' || value.front() == 'V')) value.erase(value.begin());
    return value;
}

std::string normalizedHash(std::string value) {
    if (value.compare(0, 7, "sha256:") == 0) value.erase(0, 7);
    if (value.size() != 64 || value.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return {};
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return value;
}

struct CurlRuntime {
    CURLcode result{curl_global_init(CURL_GLOBAL_DEFAULT)};
    ~CurlRuntime() { curl_global_cleanup(); }
};

struct Transfer {
    std::atomic<bool> *cancel{nullptr};
    std::string *body{nullptr};
    FILE *file{nullptr};
    std::size_t limit{0};
    std::size_t written{0};
    std::function<void(int)> progress;
};

std::size_t receive(char *data, std::size_t size, std::size_t count, void *context) {
    auto &transfer = *static_cast<Transfer *>(context);
    if (size && count > transfer.limit / size) return 0;
    const auto bytes = size * count;
    if (transfer.cancel->load() || transfer.written > transfer.limit || bytes > transfer.limit - transfer.written) return 0;
    if (transfer.file) {
        if (std::fwrite(data, 1, bytes, transfer.file) != bytes) return 0;
    } else {
        transfer.body->append(data, bytes);
    }
    transfer.written += bytes;
    return bytes;
}

int reportTransfer(void *context, curl_off_t total, curl_off_t current, curl_off_t, curl_off_t) {
    auto &transfer = *static_cast<Transfer *>(context);
    if (transfer.cancel->load()) return 1;
    if (transfer.progress && total > 0) {
        transfer.progress(int(std::min<curl_off_t>(100, current * 100 / total)));
    }
    return 0;
}

bool loopbackHttp(const std::string &url) {
#ifdef PAPERNX_UPDATE_TESTING
    return url.compare(0, 17, "http://127.0.0.1:") == 0 || url.compare(0, 17, "http://localhost:") == 0;
#else
    (void)url;
    return false;
#endif
}

bool request(const std::string &url, const std::string &certificates, Transfer &transfer, std::string &error) {
    static CurlRuntime runtime;
    if (runtime.result != CURLE_OK) {
        error = "Network initialization failed";
        return false;
    }
    const bool testHttp = loopbackHttp(url);
    const bool https = url.compare(0, 8, "https://") == 0;
    if (!https && !testHttp) {
        error = "The updater only accepts HTTPS URLs";
        return false;
    }
    if (https && !certificates.empty()) {
        std::ifstream ca(certificates, std::ios::binary);
        if (!ca) {
            error = "The RomFS CA certificate bundle is missing or unreadable";
            return false;
        }
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        error = "Network initialization failed";
        return false;
    }
    char message[CURL_ERROR_SIZE]{};
    auto set = [curl](CURLoption option, auto value) { return curl_easy_setopt(curl, option, value); };
    bool configured = set(CURLOPT_URL, url.c_str()) == CURLE_OK &&
        set(CURLOPT_USERAGENT, "PaperNX updater") == CURLE_OK &&
        set(CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK && set(CURLOPT_MAXREDIRS, 5L) == CURLE_OK &&
        set(CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK && set(CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
        set(CURLOPT_FAILONERROR, 1L) == CURLE_OK && set(CURLOPT_NOSIGNAL, 1L) == CURLE_OK &&
        set(CURLOPT_CONNECTTIMEOUT, 8L) == CURLE_OK && set(CURLOPT_TIMEOUT, transfer.file ? 600L : 20L) == CURLE_OK &&
        set(CURLOPT_LOW_SPEED_LIMIT, 1024L) == CURLE_OK && set(CURLOPT_LOW_SPEED_TIME, 30L) == CURLE_OK &&
        set(CURLOPT_WRITEFUNCTION, receive) == CURLE_OK && set(CURLOPT_WRITEDATA, &transfer) == CURLE_OK &&
        set(CURLOPT_NOPROGRESS, 0L) == CURLE_OK && set(CURLOPT_XFERINFOFUNCTION, reportTransfer) == CURLE_OK &&
        set(CURLOPT_XFERINFODATA, &transfer) == CURLE_OK && set(CURLOPT_ERRORBUFFER, message) == CURLE_OK;
    if (configured && !certificates.empty()) configured = set(CURLOPT_CAINFO, certificates.c_str()) == CURLE_OK;
#if LIBCURL_VERSION_NUM >= 0x075500
    if (configured) configured = set(CURLOPT_PROTOCOLS_STR, testHttp ? "http,https" : "https") == CURLE_OK &&
                                 set(CURLOPT_REDIR_PROTOCOLS_STR, testHttp ? "http,https" : "https") == CURLE_OK;
#else
    const long protocols = testHttp ? (CURLPROTO_HTTP | CURLPROTO_HTTPS) : CURLPROTO_HTTPS;
    if (configured) configured = set(CURLOPT_PROTOCOLS, protocols) == CURLE_OK &&
                                 set(CURLOPT_REDIR_PROTOCOLS, protocols) == CURLE_OK;
#endif
    if (!configured) {
        error = "Could not configure secure network transfer";
        curl_easy_cleanup(curl);
        return false;
    }

    const auto result = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (result == CURLE_OK) return true;
    if (transfer.cancel->load()) error = "Update cancelled";
    else if (result == CURLE_PEER_FAILED_VERIFICATION) error = "Certificate verification failed; check the console date and time";
    else error = message[0] ? message : curl_easy_strerror(result);
    return false;
}

bool fetch(const std::string &url, const std::string &certificates, std::atomic<bool> &cancel,
           std::string &body, std::string &error, std::size_t limit = 4 * 1024 * 1024) {
    Transfer transfer{&cancel, &body, nullptr, limit, 0, {}};
    return request(url, certificates, transfer, error);
}

bool durable(FILE *file) {
    return std::fflush(file) == 0 && ::fsync(::fileno(file)) == 0;
}

bool writeAtomicText(const std::filesystem::path &path, const std::string &text, std::string &error) {
    const auto temporary = path.string() + ".tmp";
    FILE *file = std::fopen(temporary.c_str(), "wb");
    if (!file) {
        error = "Cannot write update recovery data";
        return false;
    }
    bool ok = std::fwrite(text.data(), 1, text.size(), file) == text.size() && durable(file);
    if (std::fclose(file) != 0) ok = false;
    if (ok && std::rename(temporary.c_str(), path.string().c_str()) == 0) return true;
    std::remove(temporary.c_str());
    error = "Cannot save update recovery data";
    return false;
}

std::uint32_t readLe32(const unsigned char *data) {
    return std::uint32_t(data[0]) | (std::uint32_t(data[1]) << 8) |
           (std::uint32_t(data[2]) << 16) | (std::uint32_t(data[3]) << 24);
}

std::uint64_t readLe64(const unsigned char *data) {
    return std::uint64_t(readLe32(data)) | (std::uint64_t(readLe32(data + 4)) << 32);
}

std::string boundedString(const std::array<char, 512> &data) {
    std::size_t length = 0;
    while (length < data.size() && data[length]) ++length;
    return std::string(data.data(), length);
}

std::string sha256File(const std::filesystem::path &path, std::string &error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "Cannot read the update";
        return {};
    }
    fz_sha256 sha;
    fz_sha256_init(&sha);
    std::array<unsigned char, 32768> buffer{};
    while (file) {
        file.read(reinterpret_cast<char *>(buffer.data()), buffer.size());
        const auto count = file.gcount();
        if (count > 0) fz_sha256_update(&sha, buffer.data(), static_cast<std::size_t>(count));
    }
    if (!file.eof()) {
        error = "Cannot read the update";
        return {};
    }
    unsigned char digest[32];
    fz_sha256_final(&sha, digest);
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (unsigned char byte : digest) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 15]);
    }
    return result;
}

bool validateNro(const std::filesystem::path &path, const std::string &expectedHash,
                 std::uint64_t expectedBytes, const std::string &expectedVersion, std::string &error) {
    std::error_code ec;
    const auto size = std::filesystem::file_size(path, ec);
    if (ec || size != expectedBytes || size < 0x1000 || size > maxNro) {
        error = "Update download size does not match";
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 0x80> header{};
    file.read(reinterpret_cast<char *>(header.data()), header.size());
    if (!file || std::memcmp(header.data() + 0x10, "NRO0", 4) != 0) {
        error = "Downloaded file is not a Switch application";
        return false;
    }
    const auto declared = readLe32(header.data() + 0x18);
    if (declared < header.size() || declared > size - 0x38) {
        error = "Invalid NRO header";
        return false;
    }
    std::uint64_t segmentEnd = 0;
    for (int i = 0; i < 3; ++i) {
        const auto offset = readLe32(header.data() + 0x20 + i * 8);
        const auto length = readLe32(header.data() + 0x24 + i * 8);
        if (offset < segmentEnd || std::uint64_t(offset) + length > declared) {
            error = "Invalid NRO segments";
            return false;
        }
        segmentEnd = std::uint64_t(offset) + length;
    }

    std::array<unsigned char, 0x38> assets{};
    file.seekg(declared);
    file.read(reinterpret_cast<char *>(assets.data()), assets.size());
    if (!file || std::memcmp(assets.data(), "ASET", 4) != 0) {
        error = "Update is missing its resources";
        return false;
    }
    const auto nacp = readLe64(assets.data() + 0x18);
    const auto nacpSize = readLe64(assets.data() + 0x20);
    const auto romfs = readLe64(assets.data() + 0x28);
    const auto romfsSize = readLe64(assets.data() + 0x30);
    const auto available = size - declared;
    if (nacp < 0x38 || nacpSize != 0x4000 || nacp > available || nacpSize > available - nacp ||
        romfs < 0x38 || romfs > available || romfsSize < 0x50 || romfsSize > available - romfs) {
        error = "Invalid update resources";
        return false;
    }

    std::array<char, 512> title{};
    std::array<char, 16> version{};
    file.seekg(declared + nacp);
    file.read(title.data(), title.size());
    file.seekg(declared + nacp + 0x3060);
    file.read(version.data(), version.size());
    std::size_t versionLength = 0;
    while (versionLength < version.size() && version[versionLength]) ++versionLength;
    if (!file || boundedString(title) != "PaperNX" ||
        std::string(version.data(), versionLength) != withoutPrefix(expectedVersion).substr(0, version.size() - 1)) {
        error = "Update application or version does not match";
        return false;
    }
    const auto actualHash = sha256File(path, error);
    if (actualHash.empty()) return false;
    if (actualHash != expectedHash) {
        error = "Update checksum does not match";
        return false;
    }
    return true;
}

struct BootRecord {
    std::string hash;
    std::string version;
    std::uint64_t bytes{0};
};

bool readBootRecord(const std::filesystem::path &path, BootRecord &record) {
    std::ifstream file(path);
    std::string hash, version, bytesText, extra;
    if (!(file >> hash >> version >> bytesText) || (file >> extra)) return false;
    record.hash = normalizedHash(hash);
    record.version = version;
    if (record.hash.empty() || !parseVersion(record.version).valid || !digits(bytesText)) return false;
    const auto parsed = std::from_chars(bytesText.data(), bytesText.data() + bytesText.size(), record.bytes);
    return parsed.ec == std::errc{} && record.bytes >= 0x1000 && record.bytes <= maxNro;
}

bool pathExists(const std::filesystem::path &path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

bool removeUpdaterFile(const std::filesystem::path &path) {
    return std::remove(path.string().c_str()) == 0 || errno == ENOENT;
}

bool restoreBackup(const std::filesystem::path &executable, std::string &error) {
    const auto backup = executable.string() + ".old";
    const auto failed = executable.string() + ".failed";
    if (!pathExists(backup)) {
        error = "The previous PaperNX NRO backup is missing";
        return false;
    }
    if (pathExists(failed)) {
        error = "Cannot restore the previous NRO while a .failed file exists";
        return false;
    }
    bool movedCandidate = false;
    if (pathExists(executable)) {
        if (std::rename(executable.string().c_str(), failed.c_str()) != 0) {
            error = "Cannot move the failed PaperNX NRO aside";
            return false;
        }
        movedCandidate = true;
    }
    if (std::rename(backup.c_str(), executable.string().c_str()) != 0) {
        const auto reason = errno;
        if (movedCandidate) std::rename(failed.c_str(), executable.string().c_str());
        error = std::string("Cannot restore the previous PaperNX NRO: ") + std::strerror(reason);
        return false;
    }
    if (movedCandidate) std::remove(failed.c_str());
    std::remove((executable.string() + ".boot").c_str());
    std::remove((executable.string() + ".boot.started").c_str());
    std::remove((executable.string() + ".boot.ok").c_str());
    std::remove((executable.string() + ".swap").c_str());
    std::remove((executable.string() + ".ready").c_str());
    std::remove((executable.string() + ".part").c_str());
    return true;
}

bool hasSpaceFor(const std::filesystem::path &directory, std::uint64_t required) {
    std::error_code ec;
    const auto space = std::filesystem::space(directory, ec);
    return !ec && space.available >= required;
}
} // namespace

bool updateVersionNewer(const std::string &candidate, const std::string &current) {
    const auto newer = parseVersion(candidate);
    const auto installed = parseVersion(current);
    return newer.valid && installed.valid && compareVersion(newer, installed) > 0;
}

bool selectUpdateRelease(const std::string &json, const std::string &current, const UpdateSource &source,
                         UpdateRelease &release, std::string &error) {
    release = {};
    std::vector<update_json::Release> releases;
    if (!update_json::Parser(json).parse(releases)) {
        error = "Invalid release response";
        return false;
    }
    if (!parseVersion(current).valid) {
        error = "Invalid installed version";
        return false;
    }

    for (const auto &item : releases) {
        if (item.draft || item.prerelease) continue;
        const auto &tag = item.tag;
        if (!updateVersionNewer(tag, current) || (!release.version.empty() && !updateVersionNewer(tag, release.version))) continue;

        UpdateRelease candidate;
        candidate.version = withoutPrefix(tag);
        candidate.title = item.name.substr(0, 256);
        candidate.notes = item.notes.substr(0, 32768);
        bool nroSeen = false, checksumSeen = false;
        for (const auto &asset : item.assets) {
            if (asset.name == "PaperNX.nro") {
                if (nroSeen) { candidate.url.clear(); break; }
                nroSeen = true;
                if (asset.url == source.assetPrefix + tag + "/PaperNX.nro" && asset.bytes >= 0x1000 && asset.bytes <= maxNro) {
                    candidate.url = asset.url;
                    candidate.bytes = asset.bytes;
                    candidate.sha256 = normalizedHash(asset.digest);
                }
            } else if (asset.name == "PaperNX.nro.sha256") {
                if (checksumSeen) { candidate.checksumUrl.clear(); break; }
                checksumSeen = true;
                if (asset.url == source.assetPrefix + tag + "/PaperNX.nro.sha256") candidate.checksumUrl = asset.url;
            }
        }
        release = std::move(candidate);
    }
    error.clear();
    return true;
}

SelfUpdater::~SelfUpdater() { stop(); }

void SelfUpdater::configure(std::filesystem::path executable, std::string version, std::string certificates) {
    configureSource(std::move(executable), std::move(version), std::move(certificates), UpdateSource{});
}

#ifdef PAPERNX_UPDATE_TESTING
void SelfUpdater::configureForTesting(std::filesystem::path executable, std::string version,
                                      std::string certificates, UpdateSource source) {
    configureSource(std::move(executable), std::move(version), std::move(certificates), std::move(source));
}
#endif

void SelfUpdater::configureSource(std::filesystem::path executable, std::string version,
                                  std::string certificates, UpdateSource source) {
    stop();
    executable_ = std::move(executable);
    current_ = std::move(version);
    certificates_ = std::move(certificates);
    source_ = std::move(source);
    cancel_ = false;
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = {};
}

UpdateStatus SelfUpdater::status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

bool SelfUpdater::busy() const {
    const auto state = status().state;
    return state == UpdateState::Checking || state == UpdateState::Downloading;
}

void SelfUpdater::publish(UpdateState state, const std::string &error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const bool operationActive=status_.state==UpdateState::Checking || status_.state==UpdateState::Downloading;
    if(state==UpdateState::Error && operationActive && cancel_) {state=UpdateState::Cancelled;status_.error="Update cancelled";}
    else status_.error=error;
    status_.state = state;
    if (state == UpdateState::Downloading || state == UpdateState::Cancelled) status_.percent = 0;
}

void SelfUpdater::reportError(std::string error) {
    publish(UpdateState::Error, error);
}

void SelfUpdater::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(status_.state==UpdateState::Checking || status_.state==UpdateState::Downloading) cancel_=true;
}

void SelfUpdater::stop() {
    cancel_ = true;
    if (worker_.joinable()) worker_.join();
}

bool SelfUpdater::check() {
    if (busy()) return false;
    if (worker_.joinable()) worker_.join();
    cancel_ = false;
    const auto path = executable_.string();
    if (executable_.extension() != ".nro" ||
        (!executable_.is_absolute() && path.compare(0, 6, "sdmc:/") != 0)) {
        publish(UpdateState::Error, "Cannot determine the running NRO path");
        return false;
    }
    if (!removeUpdaterFile(path + ".part") || !removeUpdaterFile(path + ".ready.tmp")) {
        publish(UpdateState::Error, "Cannot clean incomplete update files");
        return false;
    }
    publish(UpdateState::Checking);
    worker_ = std::thread([this] {
        std::string error;
        if (validatePending(error)) {
            publish(UpdateState::Ready);
            return;
        }
        if (!removeUpdaterFile(executable_.string() + ".new") ||
            !removeUpdaterFile(executable_.string() + ".ready")) {
            publish(UpdateState::Error, "Cannot clean an invalid pending update");
            return;
        }
        std::string response;
        if (!fetch(source_.api, certificates_, cancel_, response, error)) {
            publish(cancel_ ? UpdateState::Cancelled : UpdateState::Error, error);
            return;
        }
        UpdateRelease release;
        if (!selectUpdateRelease(response, current_, source_, release, error)) {
            publish(UpdateState::Error, error);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if(cancel_) {
            status_.release={};status_.state=UpdateState::Cancelled;status_.error="Update cancelled";status_.percent=0;
            return;
        }
        status_.release = std::move(release);
        status_.state = status_.release.version.empty() ? UpdateState::Current : UpdateState::Available;
        status_.error.clear();
    });
    return true;
}

bool SelfUpdater::download() {
    const auto snapshot = status();
    if (snapshot.state != UpdateState::Available || !snapshot.release.installable()) return false;
    if (worker_.joinable()) worker_.join();
    const auto path = executable_.string();
    if (pathExists(path + ".boot") || pathExists(path + ".boot.started") ||
        pathExists(path + ".boot.ok") || pathExists(path + ".swap")) {
        publish(UpdateState::Error, "A previous update still needs boot recovery");
        return false;
    }
    if (snapshot.release.bytes > maxNro || snapshot.release.bytes > std::numeric_limits<std::uint64_t>::max() - stagingReserve ||
        !hasSpaceFor(executable_.parent_path(), snapshot.release.bytes + stagingReserve)) {
        publish(UpdateState::Error, "There is not enough free space on the SD card");
        return false;
    }

    cancel_ = false;
    publish(UpdateState::Downloading);
    worker_ = std::thread([this, release = snapshot.release]() mutable {
        std::string error;
        if (release.sha256.empty()) {
            std::string body;
            if (!fetch(release.checksumUrl, certificates_, cancel_, body, error, 1024)) {
                publish(cancel_ ? UpdateState::Cancelled : UpdateState::Error, error);
                return;
            }
            std::istringstream checksum(body);
            std::string hash, name, extra;
            checksum >> hash >> name;
            if (checksum >> extra || (!name.empty() && name != "PaperNX.nro" && name != "*PaperNX.nro")) {
                publish(UpdateState::Error, "Invalid update checksum file");
                return;
            }
            release.sha256 = normalizedHash(hash);
            if (release.sha256.empty()) {
                publish(UpdateState::Error, "Invalid update checksum file");
                return;
            }
        }
        if(cancel_) {publish(UpdateState::Cancelled,"Update cancelled");return;}

        const auto part = executable_.string() + ".part";
        const auto pending = executable_.string() + ".new";
        const auto ready = executable_.string() + ".ready";
        const auto temporary = ready + ".tmp";
        std::remove(part.c_str());
        std::remove(pending.c_str());
        std::remove(ready.c_str());
        std::remove(temporary.c_str());

        FILE *file = std::fopen(part.c_str(), "wb");
        if (!file) {
            publish(UpdateState::Error, "Cannot write update to the SD card");
            return;
        }
        Transfer transfer{&cancel_, nullptr, file, static_cast<std::size_t>(release.bytes), 0,
            [this](int percent) {
                std::lock_guard<std::mutex> lock(mutex_);
                status_.percent = percent;
            }};
        bool ok = request(release.url, certificates_, transfer, error);
        if(ok && cancel_) {ok=false;error="Update cancelled";}
        if (ok && !durable(file)) { ok = false; error = "Cannot save update to the SD card"; }
        if (std::fclose(file) != 0) { ok = false; error = "Cannot save update to the SD card"; }
        if (ok) ok = validateNro(part, release.sha256, release.bytes, release.version, error);
        if(ok && cancel_) {ok=false;error="Update cancelled";}
        if (ok && std::rename(part.c_str(), pending.c_str()) != 0) {
            ok = false;
            error = "Cannot stage update on the SD card";
        }
        if(ok && cancel_) {ok=false;error="Update cancelled";}
        if (ok) ok = writeAtomicText(ready, release.sha256 + "\n" + release.version + "\n" +
                                     std::to_string(release.bytes) + "\n", error);
        if (!ok) {
            std::remove(part.c_str());
            std::remove(temporary.c_str());
            std::remove(pending.c_str());
            std::remove(ready.c_str());
            publish(cancel_ ? UpdateState::Cancelled : UpdateState::Error,
                    error.empty() ? "Update download failed" : error);
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if(cancel_) {
            std::remove(part.c_str());std::remove(pending.c_str());std::remove(temporary.c_str());std::remove(ready.c_str());
            status_.state=UpdateState::Cancelled;status_.error="Update cancelled";status_.percent=0;
            return;
        }
        status_.release = std::move(release);
        status_.percent = 100;
        status_.state = UpdateState::Ready;
    });
    return true;
}

bool SelfUpdater::validatePending(std::string &error) const {
    if (executable_.empty()) return false;
    std::ifstream ready(executable_.string() + ".ready");
    std::string hash, version, bytesText, extra;
    if (!(ready >> hash >> version >> bytesText) || (ready >> extra) || normalizedHash(hash) != hash || !digits(bytesText)) return false;
    std::uint64_t bytes = 0;
    const auto parsed = std::from_chars(bytesText.data(), bytesText.data() + bytesText.size(), bytes);
    if (parsed.ec != std::errc{} || parsed.ptr != bytesText.data() + bytesText.size() || !updateVersionNewer(version, current_)) return false;
    return validateNro(executable_.string() + ".new", hash, bytes, version, error);
}

SelfUpdater::BootRecovery SelfUpdater::prepareBoot(std::string &error) {
    error.clear();
    const auto marker = std::filesystem::path(executable_.string() + ".boot");
    const auto started = std::filesystem::path(executable_.string() + ".boot.started");
    const auto confirmed = std::filesystem::path(executable_.string() + ".boot.ok");
    if (pathExists(confirmed)) {
        // Never drop the only known-good backup while the canonical NRO is absent.
        if (!pathExists(executable_) && pathExists(executable_.string() + ".old")) {
            error = "PaperNX.nro is missing; restore the retained .old backup manually";
            return BootRecovery::Error;
        }
        if (pathExists(executable_.string() + ".old") && std::remove((executable_.string() + ".old").c_str()) != 0) {
            error = "Update started, but the old NRO backup could not be cleaned up";
            return BootRecovery::Error;
        }
        std::remove(marker.string().c_str());
        std::remove(started.string().c_str());
        std::remove(confirmed.string().c_str());
        std::remove((executable_.string() + ".ready").c_str());
        return BootRecovery::Normal;
    }
    if (!pathExists(marker)) {
        const auto backup = executable_.string() + ".old";
        const bool installMayHaveStarted = pathExists(started) || pathExists(executable_.string() + ".swap");
        if (installMayHaveStarted && pathExists(backup)) {
            error = "Update recovery data is missing; restoring the previous PaperNX version";
            return rollbackBoot(error) ? BootRecovery::RolledBack : BootRecovery::Error;
        }
        if (installMayHaveStarted) {
            error = "Update recovery data is incomplete; preserve the NRO files and restore PaperNX.nro manually";
            return BootRecovery::Error;
        }
        if (pathExists(backup) && !pathExists(executable_)) {
            error = "PaperNX.nro is missing; restore the retained .old backup manually";
            return BootRecovery::Error;
        }
        return BootRecovery::Normal;
    }

    BootRecord record;
    if (!readBootRecord(marker, record)) {
        error = "Update boot record is damaged";
        return pathExists(executable_.string() + ".old") && rollbackBoot(error) ? BootRecovery::RolledBack : BootRecovery::Error;
    }
    if (pathExists(started)) {
        error = "The updated PaperNX did not complete startup; restoring the previous version";
        return rollbackBoot(error) ? BootRecovery::RolledBack : BootRecovery::Error;
    }
    if (!validateNro(executable_, record.hash, record.bytes, record.version, error)) {
        error = "The installed update failed its startup check; restoring the previous version";
        return rollbackBoot(error) ? BootRecovery::RolledBack : BootRecovery::Error;
    }

    FILE *file = std::fopen(started.string().c_str(), "wb");
    const bool saved = file && durable(file);
    const bool closed = file && std::fclose(file) == 0;
    if (!saved || !closed) {
        error = "Cannot save update startup recovery state";
        rollbackBoot(error);
        return BootRecovery::Error;
    }
    return BootRecovery::Candidate;
}

bool SelfUpdater::confirmBoot(std::string &error) {
    error.clear();
    const auto marker = std::filesystem::path(executable_.string() + ".boot");
    if (!pathExists(marker)) return true;
    const auto confirmed = std::filesystem::path(executable_.string() + ".boot.ok");
    if (!writeAtomicText(confirmed, "PaperNX startup confirmed\n", error)) return false;
    if (pathExists(executable_.string() + ".old") && std::remove((executable_.string() + ".old").c_str()) != 0) {
        error = "PaperNX started, but the old NRO backup could not be cleaned up";
        return false;
    }
    std::remove(marker.string().c_str());
    std::remove((executable_.string() + ".boot.started").c_str());
    std::remove(confirmed.string().c_str());
    return true;
}

bool SelfUpdater::rollbackBoot(std::string &error) {
    error.clear();
    if (!pathExists(executable_.string() + ".old")) {
        error = "The previous PaperNX NRO backup is missing";
        return false;
    }
    if (!restoreBackup(executable_, error)) return false;
    error = "The previous PaperNX version was restored";
    return true;
}

} // namespace papernx
