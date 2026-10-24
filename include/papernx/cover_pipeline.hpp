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
#include <locale>
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
    std::uint64_t archiveOpens{}, thumbnailDecodeFailures{};
    std::uint64_t diskCacheHits{}, diskCacheMisses{};
    std::uint64_t textureUploadAttempts{}, successfulTextureUploads{}, uploadFailures{}, discardedStaleResults{};
    std::uint64_t coverRequests{};
    std::size_t pendingQueue{}, completedQueue{};
    std::uint64_t activeGeneration{};
    std::uint64_t epochRequested{}, epochLoaded{}, outstandingRequests{};
    std::uintmax_t cacheBudgetBytes{};
    std::size_t cacheEntryBudget{};
    // Elapsed milliseconds of the current/last request epoch; -1 means "not measured".
    double firstVisibleMs{-1}, fill50Ms{-1}, fill95Ms{-1};
};

class CoverTelemetry {
public:
    enum class Counter { MetadataOpen, MetadataAvoided, ThumbnailOpen, DiskHit, DiskMiss, UploadAttempt, UploadOk, UploadFail, StaleDiscard, Request, ArchiveOpen, DecodeFail };

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
    std::uint64_t requested(const std::string &id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto active=outstanding_.find(id);
        if(active!=outstanding_.end())return active->second;
        if (outstanding_.empty()) {
            epochStart_ = Clock::now();epochRequested_ = epochLoaded_ = 0;
            first_ = fill50_ = fill95_ = -1;
        }
        const auto token=++nextToken_;
        outstanding_.emplace(id,token);++epochRequested_;count(Counter::Request);
        return token;
    }
    bool uploaded(const std::string &id,std::uint64_t token) {
        std::lock_guard<std::mutex> lock(mutex_);
        return resolve(id,token,true);
    }
    bool discarded(const std::string &id,std::uint64_t token,bool failed) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(!resolve(id,token,false))return false;
        count(failed?Counter::UploadFail:Counter::StaleDiscard);
        return true;
    }
    // Resolved without a texture and without counting as stale/failed (e.g. EPUB has no cover).
    bool dropped(const std::string &id,std::uint64_t token) { std::lock_guard<std::mutex> lock(mutex_);return resolve(id,token,false); }
    bool unavailable(const std::string &id,std::uint64_t token) { std::lock_guard<std::mutex> lock(mutex_);return resolve(id,token,false,false); }
    // Time from a decoded/cache-ready result entering completedThumbs_ to renderer upload attempt.
    void uploadQueueWait(double ms) { if(!enabled_.load() || ms<0)return;std::lock_guard<std::mutex> lock(mutex_);push(waits_,ms); }
    // Queue flushed wholesale (pause/clear): forget outstanding work so the next request opens a fresh epoch.
    void abandonAll() { std::lock_guard<std::mutex> lock(mutex_);outstanding_.clear();epochRequested_=epochLoaded_=0;first_=fill50_=fill95_=-1; }
    void frameTime(double ms) {
        if (!enabled_.load()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        push(frames_, ms);
    }
    void decodeTime(double ms) {
        if (!enabled_.load()) return;
        std::lock_guard<std::mutex> lock(mutex_);
        push(decode_, ms);
    }

    CoverPipelineStats snapshot() const {
        CoverPipelineStats s;
        s.metadataDocumentOpens = get(Counter::MetadataOpen);s.metadataOpensAvoided = get(Counter::MetadataAvoided);
        s.thumbnailDocumentOpens = get(Counter::ThumbnailOpen);
        s.archiveOpens=get(Counter::ArchiveOpen);s.thumbnailDecodeFailures=get(Counter::DecodeFail);
        s.diskCacheHits = get(Counter::DiskHit);s.diskCacheMisses = get(Counter::DiskMiss);
        s.textureUploadAttempts = get(Counter::UploadAttempt);s.successfulTextureUploads = get(Counter::UploadOk);
        s.uploadFailures = get(Counter::UploadFail);s.discardedStaleResults = get(Counter::StaleDiscard);
        s.coverRequests = get(Counter::Request);
        std::lock_guard<std::mutex> lock(mutex_);
        s.firstVisibleMs = first_;s.fill50Ms = fill50_;s.fill95Ms = fill95_;
        s.epochRequested=epochRequested_;s.epochLoaded=epochLoaded_;s.outstandingRequests=outstanding_.size();
        return s;
    }

    // JSON without any book path or title. `renderer`/`sourceCommit` come from the caller; values
    // that were not measured are exported as null rather than guessed.
    std::string toJson(const CoverPipelineStats &s, const std::string &appVersion,
                       const std::string &sourceCommit, const std::string &renderer,
                       const std::string &sourceDirty="unknown", const std::string &sourceTreeSha256="unknown",
                       const std::string &sourceIdentity="unknown") const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << "{\"schema\":\"papernx-cover-perf-1\",\"appVersion\":\"" << sanitize(appVersion)
            << "\",\"sourceCommit\":\"" << sanitize(sourceCommit) << "\",\"sourceDirty\":\"" << sanitize(sourceDirty)
            << "\",\"sourceTreeSha256\":\"" << sanitize(sourceTreeSha256) << "\",\"sourceIdentity\":\"" << sanitize(sourceIdentity)
            << "\",\"dataset\":\"" << dataset_
            << "\",\"runType\":\"" << runType_ << "\",\"renderer\":\"" << sanitize(renderer) << "\",\n"
            << "\"cacheBudgetBytes\":" << s.cacheBudgetBytes << ",\"cacheEntryBudget\":" << s.cacheEntryBudget << ",\n"
            << "\"counters\":{\"metadataDocumentOpens\":" << s.metadataDocumentOpens
            << ",\"metadataOpensAvoided\":" << s.metadataOpensAvoided
            << ",\"thumbnailDocumentOpens\":" << s.thumbnailDocumentOpens
            << ",\"archiveOpens\":" << s.archiveOpens << ",\"thumbnailDecodeFailures\":" << s.thumbnailDecodeFailures
            << ",\"diskCacheHits\":" << s.diskCacheHits << ",\"diskCacheMisses\":" << s.diskCacheMisses
            << ",\"textureUploadAttempts\":" << s.textureUploadAttempts
            << ",\"successfulTextureUploads\":" << s.successfulTextureUploads
            << ",\"uploadFailures\":" << s.uploadFailures << ",\"discardedStaleResults\":" << s.discardedStaleResults
            << ",\"coverRequests\":" << s.coverRequests << ",\"pendingQueue\":" << s.pendingQueue
            << ",\"completedQueue\":" << s.completedQueue << ",\"activeGeneration\":" << s.activeGeneration
            << ",\"epochRequested\":" << s.epochRequested << ",\"epochLoaded\":" << s.epochLoaded
            << ",\"outstandingRequests\":" << s.outstandingRequests << "},\n"
            << "\"lastEpoch\":{\"firstVisibleMs\":" << num(s.firstVisibleMs) << ",\"fill50Ms\":" << num(s.fill50Ms)
            << ",\"fill95Ms\":" << num(s.fill95Ms) << "},\n\"uploadQueueWaitMs\":" << list(waits_)
            << ",\n\"frameTimeMs\":" << list(frames_) << ",\n\"thumbnailDecodeMs\":" << list(decode_) << "}\n";
        return out.str();
    }

private:
    using Clock = std::chrono::steady_clock;
    static constexpr std::size_t ringSize = 256;
    static constexpr int counterCount = 12;
    std::atomic<std::uint64_t> counters_[counterCount]{};
    std::atomic<bool> enabled_{false};
    mutable std::mutex mutex_;
    std::string dataset_{"unspecified"}, runType_{"unspecified"};
    Clock::time_point epochStart_{Clock::now()};
    std::uint64_t nextToken_{};
    std::unordered_map<std::string,std::uint64_t> outstanding_;
    std::size_t epochRequested_{0}, epochLoaded_{0};
    double first_{-1}, fill50_{-1}, fill95_{-1};
    std::deque<double> waits_, frames_, decode_;

    std::uint64_t get(Counter c) const { return counters_[static_cast<int>(c)].load(); }
    static void push(std::deque<double> &ring, double v) { if (ring.size() == ringSize) ring.pop_front(); ring.push_back(v); }
    static double ms(Clock::time_point from) { return std::chrono::duration<double, std::milli>(Clock::now() - from).count(); }
    static std::string num(double v) { if(v<0)return "null";std::ostringstream out;out.imbue(std::locale::classic());out<<v;return out.str(); }
    static std::string list(const std::deque<double> &ring) {
        std::ostringstream out;out.imbue(std::locale::classic());out<<"[";
        for(std::size_t i=0;i<ring.size();++i)out<<(i?",":"")<<ring[i];
        return out.str()+"]";
    }
    // Labels are caller supplied: keep them bounded, printable, and quote/backslash free.
    static std::string sanitize(std::string text) {
        std::string clean;
        for (const unsigned char c : text) if (c >= 0x20 && c < 0x7f && c != '"' && c != '\\' && clean.size() < 256) clean.push_back(char(c));
        return clean.empty() ? "unspecified" : clean;
    }
    bool resolve(const std::string &id,std::uint64_t token,bool loaded,bool eligible=true) {
        const auto active=outstanding_.find(id);
        if(active==outstanding_.end() || active->second!=token)return false;
        outstanding_.erase(active);
        if(!eligible && epochRequested_>0)--epochRequested_;
        if(loaded)++epochLoaded_;
        if(epochRequested_==0 || epochLoaded_==0)return true;
        if (first_ < 0) first_ = ms(epochStart_);
        const double fraction = double(epochLoaded_) / double(epochRequested_);
        if (fill50_ < 0 && fraction >= 0.5) fill50_ = ms(epochStart_);
        if (fill95_ < 0 && fraction >= 0.95) fill95_ = ms(epochStart_);
        return true;
    }
};

} // namespace papernx
