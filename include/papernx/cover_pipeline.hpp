#pragma once
// Cover/metadata pipeline support: a bounded "already read" ledger and opt-in telemetry.
// Header-only, SDL-free and exception-free so it builds for both the host tests and the NRO.
// Ownership: MetadataLedger is guarded by the caller's queue mutex; CoverTelemetry is
// internally synchronised (relaxed atomics for counters, one mutex for the opt-in sample rings).
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace papernx {

// Bounded LRU of "id:fingerprint" keys whose metadata has already been extracted this session.
// Eviction drops only the least recently used key, never the whole set, so a long library cannot
// make already-read books reopen in bulk.
class MetadataLedger {
public:
    explicit MetadataLedger(std::size_t capacity = 4096) : capacity_(capacity ? capacity : 1) {}
    bool contains(const std::string &key) {
        const auto found = index_.find(key);
        if (found == index_.end()) return false;
        order_.splice(order_.begin(), order_, found->second);
        return true;
    }
    void insert(const std::string &key) {
        const auto found = index_.find(key);
        if (found != index_.end()) { order_.splice(order_.begin(), order_, found->second); return; }
        if (index_.size() >= capacity_) { index_.erase(order_.back()); order_.pop_back(); }
        order_.push_front(key);
        index_[key] = order_.begin();
    }
    std::size_t size() const { return index_.size(); }
private:
    std::size_t capacity_;
    std::list<std::string> order_;
    std::unordered_map<std::string, std::list<std::string>::iterator> index_;
};

struct CoverPipelineStats {
    std::uint64_t metadataDocumentOpens{}, metadataOpensAvoided{}, thumbnailDocumentOpens{};
    std::uint64_t diskCacheHits{}, diskCacheMisses{};
    std::uint64_t textureUploadAttempts{}, successfulTextureUploads{}, uploadFailures{}, discardedStaleResults{};
    std::uint64_t coverRequests{};
    std::size_t pendingQueue{}, completedQueue{};
    std::uint64_t activeGeneration{};
    // Elapsed milliseconds of the current/last request epoch; -1 means "not measured".
    double firstVisibleMs{-1}, fill50Ms{-1}, fill95Ms{-1};
};

class CoverTelemetry {
public:
    enum class Counter { MetadataOpen, MetadataAvoided, ThumbnailOpen, DiskHit, DiskMiss, UploadAttempt, UploadOk, UploadFail, StaleDiscard, Request };

    // Counters are always on (a relaxed increment). Sample rings and export need enable().
    void count(Counter c) { ++counters_[static_cast<int>(c)]; }
    void enable(std::string datasetLabel, std::string runType) {
        std::lock_guard<std::mutex> lock(mutex_);
        dataset_ = sanitize(std::move(datasetLabel));
        runType_ = sanitize(std::move(runType));
        enabled_.store(true);
    }
    bool enabled() const { return enabled_.load(); }

    // A cover was requested: the first request after everything resolved starts a new epoch.
    void requested(const std::string &id) {
        count(Counter::Request);
        std::lock_guard<std::mutex> lock(mutex_);
        if (outstanding_ == 0) {
            epochStart_ = Clock::now();epochRequested_ = epochLoaded_ = 0;
            first_ = fill50_ = fill95_ = -1;
        }
        ++outstanding_;++epochRequested_;
        if (enabled_.load() && queuedAt_.size() < 256) queuedAt_[id] = Clock::now();
    }
    void uploaded(const std::string &id) {
        std::lock_guard<std::mutex> lock(mutex_);
        resolve(id, true);
    }
    void discarded(const std::string &id, bool failed) {
        count(failed ? Counter::UploadFail : Counter::StaleDiscard);
        std::lock_guard<std::mutex> lock(mutex_);
        resolve(id, false);
    }
    // Resolved without a texture and without counting as stale/failed (e.g. EPUB has no cover).
    void dropped(const std::string &id) { std::lock_guard<std::mutex> lock(mutex_); resolve(id, false); }
    // Queue flushed wholesale (pause/clear): forget outstanding work so the next request opens a fresh epoch.
    void abandonAll() { std::lock_guard<std::mutex> lock(mutex_); outstanding_ = 0; queuedAt_.clear(); }
    void frameTime(double ms) {
        if (!enabled_.load()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        push(frames_, ms);
    }

    CoverPipelineStats snapshot() const {
        CoverPipelineStats s;
        s.metadataDocumentOpens = get(Counter::MetadataOpen);s.metadataOpensAvoided = get(Counter::MetadataAvoided);
        s.thumbnailDocumentOpens = get(Counter::ThumbnailOpen);
        s.diskCacheHits = get(Counter::DiskHit);s.diskCacheMisses = get(Counter::DiskMiss);
        s.textureUploadAttempts = get(Counter::UploadAttempt);s.successfulTextureUploads = get(Counter::UploadOk);
        s.uploadFailures = get(Counter::UploadFail);s.discardedStaleResults = get(Counter::StaleDiscard);
        s.coverRequests = get(Counter::Request);
        std::lock_guard<std::mutex> lock(mutex_);
        s.firstVisibleMs = first_;s.fill50Ms = fill50_;s.fill95Ms = fill95_;
        return s;
    }

    // JSON without any book path or title. `renderer`/`sourceCommit` come from the caller; values
    // that were not measured are exported as null rather than guessed.
    std::string toJson(const CoverPipelineStats &s, const std::string &appVersion,
                       const std::string &sourceCommit, const std::string &renderer) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream out;
        out << "{\"schema\":\"papernx-cover-perf-1\",\"appVersion\":\"" << sanitize(appVersion)
            << "\",\"sourceCommit\":\"" << sanitize(sourceCommit) << "\",\"dataset\":\"" << dataset_
            << "\",\"runType\":\"" << runType_ << "\",\"renderer\":\"" << sanitize(renderer) << "\",\n"
            << "\"counters\":{\"metadataDocumentOpens\":" << s.metadataDocumentOpens
            << ",\"metadataOpensAvoided\":" << s.metadataOpensAvoided
            << ",\"thumbnailDocumentOpens\":" << s.thumbnailDocumentOpens
            << ",\"diskCacheHits\":" << s.diskCacheHits << ",\"diskCacheMisses\":" << s.diskCacheMisses
            << ",\"textureUploadAttempts\":" << s.textureUploadAttempts
            << ",\"successfulTextureUploads\":" << s.successfulTextureUploads
            << ",\"uploadFailures\":" << s.uploadFailures << ",\"discardedStaleResults\":" << s.discardedStaleResults
            << ",\"coverRequests\":" << s.coverRequests << ",\"pendingQueue\":" << s.pendingQueue
            << ",\"completedQueue\":" << s.completedQueue << ",\"activeGeneration\":" << s.activeGeneration << "},\n"
            << "\"lastEpoch\":{\"firstVisibleMs\":" << num(s.firstVisibleMs) << ",\"fill50Ms\":" << num(s.fill50Ms)
            << ",\"fill95Ms\":" << num(s.fill95Ms) << "},\n\"uploadQueueWaitMs\":" << list(waits_)
            << ",\n\"frameTimeMs\":" << list(frames_) << "}\n";
        return out.str();
    }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t ringSize = 256;
    static constexpr int counterCount = 10;
    std::atomic<std::uint64_t> counters_[counterCount]{};
    std::atomic<bool> enabled_{false};
    mutable std::mutex mutex_;
    std::string dataset_{"unspecified"}, runType_{"unspecified"};
    Clock::time_point epochStart_{Clock::now()};
    std::size_t outstanding_{0}, epochRequested_{0}, epochLoaded_{0};
    double first_{-1}, fill50_{-1}, fill95_{-1};
    std::unordered_map<std::string, Clock::time_point> queuedAt_;
    std::deque<double> waits_, frames_;

    std::uint64_t get(Counter c) const { return counters_[static_cast<int>(c)].load(); }
    static void push(std::deque<double> &ring, double v) { if (ring.size() == ringSize) ring.pop_front(); ring.push_back(v); }
    static double ms(Clock::time_point from) { return std::chrono::duration<double, std::milli>(Clock::now() - from).count(); }
    static std::string num(double v) { return v < 0 ? "null" : std::to_string(v); }
    static std::string list(const std::deque<double> &ring) {
        std::string text = "[";
        for (std::size_t i = 0; i < ring.size(); ++i) text += (i ? "," : "") + std::to_string(ring[i]);
        return text + "]";
    }
    // Labels are caller supplied: keep them printable, short and quote/backslash free.
    static std::string sanitize(std::string text) {
        std::string clean;
        for (const unsigned char c : text) if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\' && clean.size() < 64) clean.push_back(char(c));
        return clean.empty() ? "unspecified" : clean;
    }
    void resolve(const std::string &id, bool loaded) {
        if (outstanding_) --outstanding_;
        const auto wait = queuedAt_.find(id);
        if (wait != queuedAt_.end()) { if (loaded) push(waits_, ms(wait->second)); queuedAt_.erase(wait); }
        if (!loaded || epochRequested_ == 0) return;
        ++epochLoaded_;
        if (first_ < 0) first_ = ms(epochStart_);
        const double fraction = double(epochLoaded_) / double(epochRequested_);
        if (fill50_ < 0 && fraction >= 0.5) fill50_ = ms(epochStart_);
        if (fill95_ < 0 && fraction >= 0.95) fill95_ = ms(epochStart_);
    }
};

} // namespace papernx
