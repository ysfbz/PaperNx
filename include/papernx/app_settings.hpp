#pragma once
#include "papernx/core.hpp"

namespace papernx {

struct AppPreferences {
    int uiTheme{0}; // Legacy on-disk compatibility; the global ReaderPreferences theme is authoritative.
    int language{1}; // 0 Turkish, 1 English (Default in v0.9.0: English)
    int libraryLayout{0}; // 0 grid, 1 list
    int sort{0}; // papernx::Sort
    int toolbar{0}; // 0 auto-hide, 1 persistent
    int chromePolicy{0}; // Legacy persisted value: normalized to 0; no longer a user-facing policy.
    int customUiTheme{0}; // Legacy persisted value, ignored by unified reader chrome.
    int displayOrientation{0}; // landscape, clockwise, counterclockwise
    bool preservePdfZoom{true};
    bool shoulderPageTurns{true};
    bool pageCurlAnimation{true};
    int paperTexture{0}; // off, soft, natural; EPUB canvas only
    int softwareBrightness{100}; // 50-100%; software dimmer, independent of the backlight
    int systemBrightness{100}; // 10-100%; handheld-only and restored when the app yields
    bool expandedViewport{true};
    bool bookLandscapeSpreads{false};
    bool comicLandscapeSpreads{false};
    int interfaceStyle{0}; // dark, light, follow reading palette
    int interfaceAccent{0}; // purple, blue, teal, green, orange, red, pink
    bool scanOnLaunch{true};
    bool automaticUpdateCheck{false};
    std::vector<std::string> libraryFolders;
    std::string defaultBrowserPath;
};

// Separate file from reading progress, using ProgressStore's checksummed, recoverable
// atomic writes. Reserved APP_PREFERENCES_ record maps theme/uiTheme, fontFamily/language,
// textAlign/layout, (fontSize-12)/sort, pdfViewMode/toolbar and bookPath/browser path.
// The wire schema is versioned by PAPERNX-6; no application preference alters a book.
class AppSettingsStore {
public:
    explicit AppSettingsStore(fs::path path);
    bool load();
    bool save() const;
    const AppPreferences &get() const { return preferences_; }
    void put(AppPreferences preferences);
    const std::string &saveDiagnostic() const { return store_.saveDiagnostic(); }
private:
    mutable ProgressStore store_;
    AppPreferences preferences_;
};

} // namespace papernx
