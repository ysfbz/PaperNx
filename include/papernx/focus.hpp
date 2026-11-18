#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace papernx {
// Stable IDs keep focus independent of selection and survive rendering and list updates.
struct FocusRect {
    int x{0}, y{0}, w{0}, h{0};
    bool contains(int px, int py) const {
        return w > 0 && h > 0 && px >= x && px < x + w && py >= y && py < y + h;
    }
};

class FocusManager {
public:
    struct Item {
        std::string id;
        FocusRect rect;
        bool enabled{true};
        FocusRect clip{0, 0, 0, 0};
        bool hasClip{false};
        std::string alias;
    };

    void begin(const std::string &context) {
        context_ = context;
        items_.clear();
    }

    void clearContext(const std::string &context) {
        remembered_.erase(context);
    }

    void add(std::string id, FocusRect rect, bool enabled = true, std::string alias = {}) {
        items_.push_back({std::move(id), rect, enabled, {0, 0, 0, 0}, false, std::move(alias)});
        if (active().empty() && enabled) remembered_[context_] = items_.back().id;
    }

    void addClipped(std::string id, FocusRect rect, FocusRect clip, bool enabled = true, std::string alias = {}) {
        items_.push_back({std::move(id), rect, enabled, clip, true, std::move(alias)});
        if (active().empty() && enabled) remembered_[context_] = items_.back().id;
    }

    void finish() {
        auto found = std::find_if(items_.begin(), items_.end(), [&](const Item &i) {
            return i.enabled && i.id == active();
        });
        if (found == items_.end()) {
            remembered_[context_].clear();
            for (const auto &i : items_) {
                if (i.enabled) {
                    remembered_[context_] = i.id;
                    break;
                }
            }
        }
    }

    const std::string &active() const {
        auto i = remembered_.find(context_);
        return i == remembered_.end() ? empty_ : i->second;
    }

    const Item *activeItem() const {
        const auto &currentId = active();
        if (currentId.empty()) return nullptr;
        for (const auto &i : items_) {
            if (i.id == currentId) return &i;
        }
        return nullptr;
    }

    bool focused(const std::string &id) const {
        const auto &cur = active();
        if (cur == id) return true;
        for (const auto &i : items_) {
            if (i.id == cur && !i.alias.empty() && i.alias == id) return true;
        }
        return false;
    }

    bool select(const std::string &idOrAlias) {
        for (const auto &i : items_) {
            if (i.enabled && (i.id == idOrAlias || (!i.alias.empty() && i.alias == idOrAlias))) {
                remembered_[context_] = i.id;
                return true;
            }
        }
        return false;
    }

    bool touch(int x, int y) {
        for (const auto &i : items_) {
            if (!i.enabled) continue;
            if (i.hasClip && !i.clip.contains(x, y)) continue;
            if (i.rect.contains(x, y)) return select(i.id);
        }
        return false;
    }

    bool move(int dx, int dy) {
        finish();
        auto current = std::find_if(items_.begin(), items_.end(), [&](const Item &i) {
            return i.id == active();
        });
        if (current == items_.end()) return false;
        const double x = current->rect.x + current->rect.w * .5;
        const double y = current->rect.y + current->rect.h * .5;
        double best = std::numeric_limits<double>::max();
        const Item *target = nullptr;
        for (const auto &i : items_) {
            if (!i.enabled || i.id == current->id) continue;
            double rx = i.rect.x + i.rect.w * .5 - x;
            double ry = i.rect.y + i.rect.h * .5 - y;
            double along = dx ? rx * dx : ry * dy;
            double across = dx ? std::abs(ry) : std::abs(rx);
            if (along <= 0) continue;
            double score = along + across * 3 + across * across / std::max(1.0, along);
            if (score < best) {
                best = score;
                target = &i;
            }
        }
        return target ? select(target->id) : false;
    }

    const std::vector<Item> &items() const { return items_; }
    const std::string &context() const { return context_; }

private:
    std::string context_, empty_;
    std::map<std::string, std::string> remembered_;
    std::vector<Item> items_;
};
} // namespace papernx
