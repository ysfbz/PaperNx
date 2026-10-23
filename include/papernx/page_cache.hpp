#pragma once
#include <SDL.h>
#include <list>
#include <string>
#include <unordered_map>
#include <vector>

namespace papernx {

struct RenderKey {
    std::string docId;
    int page{0};
    int zoomPercent{100}; // e.g. 100 = 1.0x, 150 = 1.5x
    int panX{0};
    int panY{0};
    int viewW{0};
    int viewH{0};
    int fontSize{0};
    int theme{0};
    int lineHeight{140};
    int margin{40};
    int textAlign{0};
    int fontFamily{0};

    std::string toString() const;
};

struct CachedTexture {
    std::string key;
    SDL_Texture *texture{nullptr};
    int width{0};
    int height{0};
    std::size_t bytes{0};
};

class PageCache {
public:
    explicit PageCache(std::size_t maxBytes = 48 * 1024 * 1024);
    ~PageCache();
    PageCache(const PageCache &) = delete;
    PageCache &operator=(const PageCache &) = delete;

    static std::string makeKey(const RenderKey &k);
    static std::string makeEpubKey(const std::string &docId, int page, int fontSize, int theme,
                                   int lineHeight, int margin, int textAlign, int viewW, int viewH, int fontFamily = 0);
    static std::string makePdfKey(const std::string &docId, int page, float zoom, int panX, int panY,
                                  int viewW, int viewH);

    SDL_Texture *get(const std::string &key, int *outW = nullptr, int *outH = nullptr);
    bool put(const std::string &key, SDL_Texture *texture, int w, int h, const std::vector<std::string> &protectedKeys = {});
    void evictDocument(const std::string &docId);
    void clear();

    std::size_t currentBytes() const { return currentBytes_; }
    std::size_t count() const { return items_.size(); }

private:
    std::size_t maxBytes_;
    std::size_t currentBytes_{0};

    std::list<CachedTexture> order_;
    std::unordered_map<std::string, std::list<CachedTexture>::iterator> items_;

    void evictOldest();
};

} // namespace papernx
