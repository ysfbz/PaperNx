#pragma once
#include "papernx/mupdf_bridge.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#endif

namespace papernx {
// Six base pages fit below 48 MiB of the reader's 64 MiB cache. Detailed
// visible pages have their own two-slot budget and never displace base pages.
struct ComicResolution {
    static constexpr std::size_t basePixels = 2u * 1024u * 1024u;
    static std::pair<int, int> size(float w, float h, bool detail, int rendererLimit = 3072) {
        const int limit = std::min(detail ? 3072 : 2048, rendererLimit);
        const double pixels = detail ? 6.0 * 1024.0 * 1024.0 : double(basePixels);
        const double scale = std::min({1.0, double(limit) / w, double(limit) / h, std::sqrt(pixels / (double(w) * h))});
        return {std::max(1, int(std::floor(w * scale))), std::max(1, int(std::floor(h * scale)))};
    }
};
// The worker owns its MuPDF document; neither SDL nor the active reader's
// document crosses threads. Back pressure bounds completed RGB buffers to two.
class ComicPages {
public:
    struct Spec {
        std::string identity, path;
        bool recolor{false};
        unsigned paper{0xffffff}, ink{0};
        int textureLimit{3072};
    };
    struct Result {
        std::string identity;
        int page{}, width{}, height{}, stride{};
        float nativeWidth{}, nativeHeight{};
        bool detail{};
        std::vector<unsigned char> rgb;
    };
    ~ComicPages() { stop(); }
    void configure(Spec spec) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (spec.identity == spec_.identity) return;
        spec_ = std::move(spec);
        ++generation_;
        queue_.clear();
        results_.clear();
        requested_.clear();
        wake_.notify_one();
    }
    void request(int page, bool detail = false) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto key = std::make_pair(page, detail);
        if (stopped_ || spec_.identity.empty() || page < 0 || requested_.count(key) || queue_.size() >= 6) return;
        requested_.insert(key);
        if (detail) queue_.push_back(key);
        else queue_.insert(std::find_if(queue_.begin(), queue_.end(), [](auto job) { return job.second; }), key);
        if (!thread_.joinable()) thread_ = std::thread([this] { run(); });
        wake_.notify_one();
    }
    std::vector<Result> take() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Result> out;
        auto next = results_.begin();
        if (next != results_.end()) {
            requested_.erase({next->page, next->detail});
            out.push_back(std::move(*next));
            results_.erase(next);
        }
        wake_.notify_one();
        return out;
    }
    bool pending() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return active_ || !queue_.empty() || !results_.empty();
    }
    void clear() { configure({}); }
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
            queue_.clear();
            results_.clear();
            requested_.clear();
            wake_.notify_one();
        }
        if (thread_.joinable()) thread_.join();
    }
private:
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    Spec spec_;
    std::uint64_t generation_{};
    bool stopped_{false}, active_{false};
    std::deque<std::pair<int, bool>> queue_;
    std::set<std::pair<int, bool>> requested_;
    std::deque<Result> results_;

    void run() {
#ifdef __SWITCH__
        // Keep archive/codec work below the UI thread's normal priority.
        svcSetThreadPriority(CUR_THREAD_HANDLE, 0x3b);
#endif
        PxDocument *doc = nullptr;
        std::uint64_t opened = ~std::uint64_t{};
        for (;;) {
            Spec spec;
            std::uint64_t generation;
            int page;
            bool detail;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                if (!wake_.wait_for(lock, std::chrono::seconds(3), [&] {
                    return stopped_ || opened != generation_ || (!queue_.empty() && results_.size() < 2);
                })) {
                    lock.unlock();
                    if (doc) px_document_close(doc);
                    doc = nullptr;
                    continue;
                }
                if (stopped_) break;
                if (opened != generation_) {
                    opened = generation_;
                    lock.unlock();
                    if (doc) px_document_close(doc);
                    doc = nullptr;
                    lock.lock();
                    if (opened != generation_) continue;
                }
                if (queue_.empty() || results_.size() >= 2) continue;
                spec = spec_;
                generation = generation_;
                page = queue_.front().first;
                detail = queue_.front().second;
                queue_.pop_front();
                active_ = true;
            }
            char error[256]{};
            if (!doc) doc = px_document_open_limited(spec.path.c_str(), 0, 0, 16, "", error, sizeof error, 96u * 1024u * 1024u);
            Result result;
            result.identity = spec.identity;
            result.page = page;
            result.detail = detail;
            PxImage image{};
            if (doc && px_document_is_comic(doc) && px_document_page_size(doc, page, &result.nativeWidth, &result.nativeHeight)) {
                const auto size = ComicResolution::size(result.nativeWidth, result.nativeHeight, detail, spec.textureLimit);
                if (px_document_render(doc, page, size.first, size.second, &image, error, sizeof error)) {
                    if (spec.recolor) px_image_apply_reading_colors(&image, spec.paper, spec.ink);
                    result.width = image.width;
                    result.height = image.height;
                    result.stride = image.stride;
                    result.rgb.assign(image.rgb, image.rgb + std::size_t(image.stride) * image.height);
                }
            }
            px_image_free(&image);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                active_ = false;
                if (generation == generation_ && !stopped_) results_.push_back(std::move(result));
            }
        }
        if (doc) px_document_close(doc);
    }
};
} // namespace papernx
