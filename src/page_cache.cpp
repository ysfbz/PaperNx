#include "papernx/page_cache.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>

namespace papernx {

std::string RenderKey::toString() const {
    std::ostringstream ss;
    ss << docId << "_p" << page
       << "_z" << zoomPercent
       << "_x" << panX
       << "_y" << panY
       << "_w" << viewW
       << "_h" << viewH
       << "_f" << fontSize
       << "_t" << theme
       << "_lh" << lineHeight
       << "_m" << margin
       << "_ta" << textAlign << "_ff" << fontFamily;
    return ss.str();
}

std::string PageCache::makeKey(const RenderKey &k) {
    return k.toString();
}

std::string PageCache::makeEpubKey(const std::string &docId, int page, int fontSize, int theme,
                                   int lineHeight, int margin, int textAlign, int viewW, int viewH, int fontFamily) {
    RenderKey k;
    k.docId = docId;
    k.page = page;
    k.zoomPercent = 100;
    k.panX = 0;
    k.panY = 0;
    k.viewW = viewW;
    k.viewH = viewH;
    k.fontSize = fontSize;
    k.theme = theme;
    k.lineHeight = lineHeight;
    k.margin = margin;
    k.textAlign = textAlign;
    k.fontFamily = fontFamily;
    return k.toString();
}

std::string PageCache::makePdfKey(const std::string &docId, int page, float zoom, int panX, int panY,
                                  int viewW, int viewH) {
    RenderKey k;
    k.docId = docId;
    k.page = page;
    k.zoomPercent = static_cast<int>(std::round(zoom * 100.0f));
    k.panX = panX;
    k.panY = panY;
    k.viewW = viewW;
    k.viewH = viewH;
    return k.toString();
}

PageCache::PageCache(std::size_t maxBytes) : maxBytes_(maxBytes) {}

PageCache::~PageCache() {
    clear();
}

SDL_Texture *PageCache::get(const std::string &key, int *outW, int *outH) {
    auto it = items_.find(key);
    if (it == items_.end()) return nullptr;

    // Move to front of LRU order list
    order_.splice(order_.begin(), order_, it->second);

    if (outW) *outW = it->second->width;
    if (outH) *outH = it->second->height;
    return it->second->texture;
}

bool PageCache::put(const std::string &key, SDL_Texture *texture, int w, int h, const std::vector<std::string> &protectedKeys) {
    if (!texture) return false;
    Uint32 format = 0;
    int actualW = 0, actualH = 0;
    if (SDL_QueryTexture(texture, &format, nullptr, &actualW, &actualH) != 0 ||
        actualW != w || actualH != h || w <= 0 || h <= 0) {
        SDL_DestroyTexture(texture);
        return false;
    }
    // Conservative four-byte accounting includes RGB24 backend expansion.
    const std::size_t bytes = static_cast<std::size_t>(w) * static_cast<std::size_t>(h) *
        std::max(4u, SDL_BYTESPERPIXEL(format));
    if (bytes > maxBytes_) { SDL_DestroyTexture(texture); return false; }
    auto protectedKey=[&](const std::string &candidate){return std::find(protectedKeys.begin(),protectedKeys.end(),candidate)!=protectedKeys.end();};
    std::size_t pinned=0;for(const auto &entry:order_)if(entry.key!=key && protectedKey(entry.key))pinned+=entry.bytes;
    if(pinned>maxBytes_-bytes){SDL_DestroyTexture(texture);return false;}
    auto it = items_.find(key);
    if (it != items_.end()) {
        currentBytes_ -= it->second->bytes;
        if (it->second->texture != texture) SDL_DestroyTexture(it->second->texture);
        order_.erase(it->second);
        items_.erase(it);
    }
    while (currentBytes_ > maxBytes_ - bytes && !order_.empty()) {
        auto victim=order_.end();do{--victim;}while(victim!=order_.begin() && protectedKey(victim->key));
        currentBytes_-=victim->bytes;SDL_DestroyTexture(victim->texture);items_.erase(victim->key);order_.erase(victim);
    }
    order_.push_front({key, texture, w, h, bytes});
    items_[key] = order_.begin();
    currentBytes_ += bytes;
    return true;
}

void PageCache::evictOldest() {
    if (order_.empty()) return;
    auto last = std::prev(order_.end());
    if (last->texture) {
        SDL_DestroyTexture(last->texture);
    }
    currentBytes_ -= last->bytes;
    items_.erase(last->key);
    order_.pop_back();
}

void PageCache::evictDocument(const std::string &docId) {
    auto it = order_.begin();
    while (it != order_.end()) {
        if (it->key.rfind(docId + "_p", 0) == 0) { // starts with docId
            if (it->texture) {
                SDL_DestroyTexture(it->texture);
            }
            currentBytes_ -= it->bytes;
            items_.erase(it->key);
            it = order_.erase(it);
        } else {
            ++it;
        }
    }
}

void PageCache::clear() {
    for (auto &item : order_) {
        if (item.texture) {
            SDL_DestroyTexture(item.texture);
        }
    }
    order_.clear();
    items_.clear();
    currentBytes_ = 0;
}

} // namespace papernx
