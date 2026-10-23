#pragma once
#include <SDL.h>
#include <algorithm>

namespace papernx {
// A leaf includes its margins and folio. Layout, snapshots and hit testing use
// the same dimensions; turning the sheet never leaves its number behind.
struct ReaderLayout {
    int width{1280}, height{720}, margin{40};
    bool spread{false};
    bool expanded{true};
    static constexpr int top = 72, bottom = 44;
    int leafWidth() const { return spread ? width / 2 : width; }
    int contentInset() const { return std::clamp(margin, 0, std::max(0, (leafWidth() - 101) / 2)); }
    int contentWidth() const { return std::max(101, leafWidth() - contentInset() * 2); }
    int footerHeight() const { return expanded ? bottom : 72; }
    int contentHeight() const { return std::max(101, height - top - footerHeight()); }
    SDL_Rect leaf(int side) const {
        const int x = spread ? side * (width / 2) : 0;
        return {x, 0, spread && side == 0 ? width / 2 : width - x, height};
    }
    SDL_Rect content(int side) const {
        auto r = leaf(side);
        return {r.x + contentInset(), top, std::max(1, r.w - contentInset() * 2), contentHeight()};
    }
    SDL_Rect folio(int side) const {
        auto r = leaf(side);
        return {r.x + 16, height - footerHeight(), std::max(1, r.w - 32), footerHeight()};
    }
};
} // namespace papernx
