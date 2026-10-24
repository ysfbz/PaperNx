#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace papernx::update_json {
constexpr std::size_t maxReleaseCount = 30;
struct Asset {
    std::string name, url, digest;
    std::uint64_t bytes{};
};
struct Release {
    std::string tag, name, notes;
    std::vector<Asset> assets;
    bool draft{}, prerelease{};
};

class Parser {
    const std::string &input_;
    std::size_t pos_{}, nodes_{};

    void space() {
        while (pos_ < input_.size() && (input_[pos_] == ' ' || input_[pos_] == '\n' || input_[pos_] == '\r' || input_[pos_] == '\t')) ++pos_;
    }
    bool take(char c) {
        space();
        if (pos_ == input_.size() || input_[pos_] != c) return false;
        ++pos_;
        return true;
    }
    bool account(int depth) { space(); return depth <= 24 && ++nodes_ <= 150000 && pos_ < input_.size(); }
    static void appendUtf8(std::string &out, unsigned value) {
        if (value < 0x80) out.push_back(char(value));
        else if (value < 0x800) { out.push_back(char(0xc0 | (value >> 6))); out.push_back(char(0x80 | (value & 63))); }
        else if (value < 0x10000) { out.push_back(char(0xe0 | (value >> 12))); out.push_back(char(0x80 | ((value >> 6) & 63))); out.push_back(char(0x80 | (value & 63))); }
        else { out.push_back(char(0xf0 | (value >> 18))); out.push_back(char(0x80 | ((value >> 12) & 63))); out.push_back(char(0x80 | ((value >> 6) & 63))); out.push_back(char(0x80 | (value & 63))); }
    }
    int hex4() {
        if (input_.size() - pos_ < 4) return -1;
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = input_[pos_++];
            const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) return -1;
            value = value * 16 + unsigned(digit);
        }
        return int(value);
    }
    bool string(std::string &out) {
        if (!take('"')) return false;
        while (pos_ < input_.size()) {
            unsigned char c = input_[pos_++];
            if (c == '"') return true;
            if (c < 0x20) return false;
            if (c != '\\') { out.push_back(char(c)); continue; }
            if (pos_ == input_.size()) return false;
            c = input_[pos_++];
            if (c == '"' || c == '\\' || c == '/') out.push_back(char(c));
            else if (c == 'b') out.push_back('\b');
            else if (c == 'f') out.push_back('\f');
            else if (c == 'n') out.push_back('\n');
            else if (c == 'r') out.push_back('\r');
            else if (c == 't') out.push_back('\t');
            else if (c == 'u') {
                int value = hex4();
                if (value < 0) return false;
                if (value >= 0xd800 && value <= 0xdbff) {
                    if (input_.size() - pos_ < 2 || input_[pos_] != '\\' || input_[pos_ + 1] != 'u') return false;
                    pos_ += 2;
                    const int low = hex4();
                    if (low < 0xdc00 || low > 0xdfff) return false;
                    value = 0x10000 + ((value - 0xd800) << 10) + low - 0xdc00;
                } else if (value >= 0xdc00 && value <= 0xdfff) return false;
                appendUtf8(out, unsigned(value));
            } else return false;
        }
        return false;
    }
    bool number(std::string *text = nullptr) {
        space();
        const auto start = pos_;
        if (pos_ < input_.size() && input_[pos_] == '-') ++pos_;
        if (pos_ == input_.size() || input_[pos_] < '0' || input_[pos_] > '9') return false;
        if (input_[pos_] == '0') ++pos_;
        else while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
        if (pos_ < input_.size() && input_[pos_] == '.') {
            ++pos_;
            const auto fraction = pos_;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
            if (fraction == pos_) return false;
        }
        if (pos_ < input_.size() && (input_[pos_] == 'e' || input_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < input_.size() && (input_[pos_] == '+' || input_[pos_] == '-')) ++pos_;
            const auto exponent = pos_;
            while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') ++pos_;
            if (exponent == pos_) return false;
        }
        if (text) *text = input_.substr(start, pos_ - start);
        return true;
    }
    bool literal(const char *word) {
        const std::string token(word);
        if (input_.compare(pos_, token.size(), token) != 0) return false;
        pos_ += token.size();
        return true;
    }
    bool skipBody(int depth) {
        space();
        if (pos_ == input_.size()) return false;
        const char c = input_[pos_];
        if (c == '"') { std::string ignored; return string(ignored); }
        if (c == '{' || c == '[') {
            const bool object = c == '{';
            ++pos_;
            if (take(object ? '}' : ']')) return true;
            std::unordered_set<std::string> keys;
            do {
                if (object) {
                    std::string key;
                    if (!string(key) || !keys.insert(std::move(key)).second || !take(':')) return false;
                }
                if (!skipValue(depth + 1)) return false;
                if (take(object ? '}' : ']')) return true;
            } while (take(','));
            return false;
        }
        if (literal("true") || literal("false") || literal("null")) return true;
        return number();
    }
    bool skipValue(int depth) { return account(depth) && skipBody(depth); }
    bool optionalString(std::string &out, int depth) {
        if (!account(depth)) return false;
        if (input_[pos_] == '"') return string(out);
        return skipBody(depth);
    }
    bool optionalBool(bool &out, int depth) {
        if (!account(depth)) return false;
        if (literal("true")) { out = true; return true; }
        if (literal("false")) { out = false; return true; }
        return false;
    }
    bool assetArray(std::vector<Asset> &assets, int depth) {
        if (!account(depth)) return false;
        if (input_[pos_] != '[') return skipBody(depth);
        ++pos_;
        if (take(']')) return true;
        do {
            space();
            if (pos_ == input_.size()) return false;
            if (input_[pos_] == '{') {
                Asset asset;
                if (!assetObject(asset, depth + 1)) return false;
                assets.push_back(std::move(asset));
            } else if (!skipValue(depth + 1)) return false;
            if (take(']')) return true;
        } while (take(','));
        return false;
    }
    bool assetObject(Asset &asset, int depth) {
        if (!account(depth) || !take('{')) return false;
        std::unordered_set<std::string> keys;
        if (take('}')) return true;
        do {
            std::string key;
            if (!string(key) || !keys.insert(key).second || !take(':')) return false;
            if (key == "name") { if (!optionalString(asset.name, depth + 1)) return false; }
            else if (key == "browser_download_url") { if (!optionalString(asset.url, depth + 1)) return false; }
            else if (key == "digest") { if (!optionalString(asset.digest, depth + 1)) return false; }
            else if (key == "size") {
                if (!account(depth + 1)) return false;
                std::string value;
                if (number(&value)) {
                    std::uint64_t bytes = 0;
                    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), bytes);
                    if (!value.empty() && value.front() != '-' && parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size()) asset.bytes = bytes;
                } else if (!skipBody(depth + 1)) return false;
            } else if (!skipValue(depth + 1)) return false;
            if (take('}')) return true;
        } while (take(','));
        return false;
    }
    bool releaseObject(Release &release) {
        if (!account(1) || !take('{')) return false;
        std::unordered_set<std::string> keys;
        bool tagIsString = false;
        if (take('}')) return false;
        do {
            std::string key;
            if (!string(key) || !keys.insert(key).second || !take(':')) return false;
            if (key == "tag_name") {
                if (!account(2)) return false;
                if (input_[pos_] != '"') return false;
                tagIsString = string(release.tag);
            } else if (key == "name") { if (!optionalString(release.name, 2)) return false; }
            else if (key == "body") { if (!optionalString(release.notes, 2)) return false; }
            else if (key == "draft") { if (!optionalBool(release.draft, 2)) return false; }
            else if (key == "prerelease") { if (!optionalBool(release.prerelease, 2)) return false; }
            else if (key == "assets") { if (!assetArray(release.assets, 2)) return false; }
            else if (!skipValue(2)) return false;
            if (take('}')) return tagIsString && !release.tag.empty();
        } while (take(','));
        return false;
    }

public:
    explicit Parser(const std::string &input) : input_(input) {}
    bool parse(std::vector<Release> &releases) {
        releases.clear();
        if (input_.size() > 4 * 1024 * 1024 || !account(0) || !take('[')) return false;
        if (!take(']')) {
            do {
                Release release;
                if (!releaseObject(release)) return false;
                if (releases.size() == maxReleaseCount) return false;
                releases.push_back(std::move(release));
                if (take(']')) break;
            } while (take(','));
            if (pos_ == 0 || input_[pos_ - 1] != ']') return false;
        }
        space();
        return pos_ == input_.size();
    }
};
} // namespace papernx::update_json
