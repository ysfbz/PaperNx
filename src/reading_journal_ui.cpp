#include "papernx/reading_journal_ui.hpp"
#include "papernx/ui.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace papernx {
namespace {

inline std::string label(bool en, const char *tr, const char *english) {
    return en ? english : tr;
}

std::string duration(std::uint64_t ms, bool en) {
    if (ms < 60000) return std::to_string(ms / 1000) + (en ? " sec" : " sn");
    auto m = ms / 60000;
    if (m >= 60) {
        return std::to_string(m / 60) + (en ? " h " : " sa ") + std::to_string(m % 60) + (en ? " min" : " dk");
    }
    return std::to_string(m) + (en ? " min" : " dk");
}

std::string key(int year, int month, int day) {
    char b[32];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d", year, month, day);
    return b;
}

std::tm nowDate() {
    const auto n = std::time(nullptr);
    const auto *ptr = std::localtime(&n);
    return ptr ? *ptr : std::tm{};
}

std::tm dateAt(int monthDelta) {
    auto d = nowDate();
    d.tm_mday = 1;
    d.tm_mon += monthDelta;
    std::mktime(&d);
    return d;
}

int daysInMonth(int year, int month) {
    std::tm d{};
    d.tm_year = year - 1900;
    d.tm_mon = month + 1;
    d.tm_mday = 0;
    std::mktime(&d);
    return d.tm_mday;
}

std::string localDate(std::int64_t epoch) {
    const auto raw = static_cast<std::time_t>(epoch);
    const auto *d = std::localtime(&raw);
    if (!d) return "?";
    char b[32];
    std::strftime(b, sizeof b, "%Y-%m-%d", d);
    return b;
}

std::string localTime(std::int64_t epoch) {
    const auto raw = static_cast<std::time_t>(epoch);
    const auto *d = std::localtime(&raw);
    if (!d) return "??:??";
    char b[16];
    std::snprintf(b, sizeof b, "%02d:%02d", d->tm_hour, d->tm_min);
    return b;
}

void text(SDL_Renderer *r, Typography &font, const std::string &s, int x, int y,
          SDL_Color color, int maxw = 0, bool large = false) {
    font.renderText(r, large ? font.titleFont() : font.uiFont(), s, x, y, color, maxw);
}

void textSmall(SDL_Renderer *r, Typography &font, const std::string &s, int x, int y,
               SDL_Color color, int maxw = 0) {
    font.renderText(r, font.smallFont(), s, x, y, color, maxw);
}

void line(SDL_Renderer *r, SDL_Color color, int x, int y, int w) {
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, 255);
    SDL_RenderDrawLine(r, x, y, x + w, y);
}

void panel(SDL_Renderer *r, const ThemePalette &pal, SDL_Rect rc) {
    ui::surface(r, rc, pal.card, 16);
    SDL_SetRenderDrawColor(r, pal.cardBorder.r, pal.cardBorder.g, pal.cardBorder.b, 255);
    SDL_RenderDrawRect(r, &rc);
}

void drawRing(SDL_Renderer *r, const ThemePalette &pal, int cx, int cy, int radius, int thickness, float fraction) {
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    SDL_Color track = blendColor(pal.card, pal.cardBorder, 60);
    SDL_SetRenderDrawColor(r, track.r, track.g, track.b, 255);
    for (int angle = 0; angle < 360; ++angle) {
        float rad = angle * 3.14159265f / 180.0f;
        for (int t = 0; t < thickness; ++t) {
            int px = cx + static_cast<int>((radius - t) * std::cos(rad));
            int py = cy + static_cast<int>((radius - t) * std::sin(rad));
            SDL_RenderDrawPoint(r, px, py);
        }
    }
    if (fraction > 0.01f) {
        SDL_SetRenderDrawColor(r, pal.accent.r, pal.accent.g, pal.accent.b, 255);
        int maxAngle = static_cast<int>(fraction * 360.0f);
        for (int angle = -90; angle < -90 + maxAngle; ++angle) {
            float rad = angle * 3.14159265f / 180.0f;
            for (int t = 0; t < thickness; ++t) {
                int px = cx + static_cast<int>((radius - t) * std::cos(rad));
                int py = cy + static_cast<int>((radius - t) * std::sin(rad));
                SDL_RenderDrawPoint(r, px, py);
            }
        }
    }
}

void drawCover(SDL_Renderer *r, const ThemePalette &pal, Typography &font, SDL_Rect rc,
               const std::string &title, const std::string &author, int seed = 0) {
    (void)pal;
    SDL_Color cBg = (seed % 3 == 0) ? SDL_Color{112, 75, 59, 255} :
                    (seed % 3 == 1) ? SDL_Color{174, 132, 89, 255} :
                                      SDL_Color{81, 104, 117, 255};
    ui::surface(r, rc, cBg, 8);
    SDL_SetRenderDrawColor(r, 255, 255, 255, 40);
    SDL_Rect spine{rc.x, rc.y, 6, rc.h};
    SDL_RenderFillRect(r, &spine);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 60);
    SDL_RenderDrawRect(r, &rc);
    SDL_Color tCol{245, 229, 201, 255};
    if (rc.w >= 45 && rc.h >= 65) {
        font.renderText(r, font.smallFont(), author, rc.x + 8, rc.y + 8, tCol, rc.w - 12);
        font.renderText(r, font.buttonFont(), title, rc.x + 8, rc.y + rc.h / 2 - 8, tCol, rc.w - 12);
    }
}

std::uint64_t dayMs(const std::map<std::string, std::uint64_t> &cal, int y, int m, int d) {
    const auto it = cal.find(key(y, m, d));
    return it == cal.end() ? 0 : it->second;
}

BookStateResolver makeResolver(const ProgressStore &progress, const CatalogStore &catalog) {
    BookStateResolver res;
    res.progressResolver = [&progress, &catalog](const std::string &bookId, double historic) -> double {
        const auto status = catalog.get(bookId).status;
        if (status == ReadingStatus::NotStarted) return 0.0;
        if (status == ReadingStatus::Completed) return 1.0;
        const auto &all = progress.all();
        const auto it = all.find(bookId);
        if (it == all.end()) return std::clamp(historic, 0.0, 1.0);
        return std::clamp(it->second.fraction, 0.0, 1.0);
    };
    res.isCompleted = [&progress, &catalog](const std::string &bookId, double historic) -> bool {
        const auto status = catalog.get(bookId).status;
        if (status == ReadingStatus::Completed) return true;
        if (status == ReadingStatus::NotStarted) return false;
        const auto &all = progress.all();
        const auto it = all.find(bookId);
        if (it != all.end() && it->second.fraction >= 0.99) return true;
        return historic >= 0.99;
    };
    res.knownBookIds = [&progress, &catalog]() -> std::vector<std::string> {
        std::vector<std::string> ids;
        for (const auto &pr : progress.all()) ids.push_back(pr.first);
        for (const auto &entry : catalog.entries()) {
            if (std::find(ids.begin(), ids.end(), entry.first) == ids.end()) {
                ids.push_back(entry.first);
            }
        }
        return ids;
    };
    return res;
}

// -----------------------------------------------------------------------------
// Unified Journal Layout Model
// -----------------------------------------------------------------------------
struct JournalLayout {
    int w{0}, h{0};
    bool sidebar{false};
    SDL_Rect headerBar{0, 0, 0, 0};
    SDL_Rect libraryBtn{0, 0, 0, 0};
    SDL_Rect goalsBtn{0, 0, 0, 0};
    SDL_Rect exportBtn{0, 0, 0, 0};

    SDL_Rect navPanel{0, 0, 0, 0};
    SDL_Rect navTabs[4]{};

    SDL_Rect contentArea{0, 0, 0, 0};

    // Tab 0: Overview
    SDL_Rect periodBtns[3]{};
    SDL_Rect chartCard{0, 0, 0, 0};
    SDL_Rect goalCard{0, 0, 0, 0};
    SDL_Rect metricTiles[4]{};
    SDL_Rect currentCard{0, 0, 0, 0};
    SDL_Rect currentDetailBtn{0, 0, 0, 0};
    SDL_Rect currentBooksBtn{0, 0, 0, 0};
    SDL_Rect recentCard{0, 0, 0, 0};
    SDL_Rect recentHistoryBtn{0, 0, 0, 0};

    // Tab 1: Calendar
    SDL_Rect calCard{0, 0, 0, 0};
    SDL_Rect prevMonthBtn{0, 0, 0, 0};
    SDL_Rect nextMonthBtn{0, 0, 0, 0};
    int calCellW{0}, calCellH{0}, calGridX{0}, calGridY{0};
    SDL_Rect dayDetailCard{0, 0, 0, 0};

    // Tab 2: Books
    SDL_Rect backToBooksBtn{0, 0, 0, 0};
    SDL_Rect bookDetailHead{0, 0, 0, 0};
    int bookRowH{110};
    int bookRowGap{12};

    // Tab 3: History
    int historyRowH{84};
    int historyRowGap{10};

    int totalContentHeight{0};
    int maxScrollY{0};
};

JournalLayout computeLayout(int w, int h, int tab, bool inBookDetail,
                           int bookCount, int sessionCount, bool en) {
    (void)en;
    JournalLayout l;
    l.w = w;
    l.h = h;
    l.sidebar = (w >= 900);

    l.headerBar = {0, 0, w, 76};
    if (l.sidebar) {
        l.libraryBtn = {16, 10, 150, 56};
        l.goalsBtn = {w - 240, 10, 150, 56};
        l.exportBtn = {w - 400, 10, 150, 56};

        l.navPanel = {14, 86, 214, h - 98};
        for (int i = 0; i < 4; ++i) {
            l.navTabs[i] = {24, 190 + i * 66, 194, 56};
        }
        l.contentArea = {244, 86, w - 262, h - 98};
    } else {
        int btnW = std::clamp((w - 180) / 4, 84, 126);
        l.libraryBtn = {16, 10, btnW, 56};
        // Always place rightmost header button before w - 88 so global quick menu '+' at {w - 80, 8, 64, 64} is never covered!
        l.goalsBtn = {w - 88 - btnW, 10, btnW, 56};
        l.exportBtn = {l.goalsBtn.x - btnW - 10, 10, btnW, 56};

        int tabW = (w - 32) / 4;
        for (int i = 0; i < 4; ++i) {
            l.navTabs[i] = {16 + i * tabW, 86, tabW - 4, 56};
        }
        l.contentArea = {16, 152, w - 32, h - 164};
    }

    const int cx = l.contentArea.x;
    const int cy = l.contentArea.y;
    const int cw = l.contentArea.w;
    const int ch = l.contentArea.h;

    if (tab == 0) {
        // Tab 0: Overview
        int segW = std::clamp(cw / 6, 74, 94);
        int segX = cx + cw - 3 * segW;
        for (int p = 0; p < 3; ++p) {
            l.periodBtns[p] = {segX + p * segW, cy, segW - 2, 52};
        }

        const bool stackTop = (!l.sidebar || cw < 680);
        int topY = cy + 68;
        int topCardH = 224;

        if (!stackTop) {
            int wide = (cw * 60) / 100;
            l.chartCard = {cx, topY, wide, topCardH};
            l.goalCard = {cx + wide + 14, topY, cw - wide - 14, topCardH};
            topY += topCardH + 14;
        } else {
            l.chartCard = {cx, topY, cw, topCardH};
            topY += topCardH + 12;
            l.goalCard = {cx, topY, cw, 200};
            topY += 200 + 14;
        }

        // Metrics
        int tileCols = (l.sidebar && cw >= 680) ? 4 : 2;
        int tileW = (cw - (tileCols - 1) * 12) / tileCols;
        int tileH = 88;
        for (int i = 0; i < 4; ++i) {
            l.metricTiles[i] = {cx + (i % tileCols) * (tileW + 12),
                               topY + (i / tileCols) * (tileH + 12),
                               tileW, tileH};
        }
        int metricRows = (4 + tileCols - 1) / tileCols;
        topY += metricRows * (tileH + 12) + 4;

        // Bottom row
        int botH = 156;
        int btnW = 120;
        if (l.sidebar && cw >= 680) {
            int halfW = (cw - 14) / 2;
            l.currentCard = {cx, topY, halfW, botH};
            l.currentBooksBtn = {cx + halfW - btnW - 14, topY + 8, btnW, 56};
            l.currentDetailBtn = {cx + halfW - btnW - 14, topY + 68, btnW, 56};

            l.recentCard = {cx + halfW + 14, topY, halfW, botH};
            l.recentHistoryBtn = {cx + cw - btnW - 14, topY + 8, btnW, 56};
            topY += botH + 20;
        } else {
            l.currentCard = {cx, topY, cw, botH};
            l.currentBooksBtn = {cx + cw - btnW - 14, topY + 8, btnW, 56};
            l.currentDetailBtn = {cx + cw - btnW - 14, topY + 68, btnW, 56};
            topY += botH + 12;

            l.recentCard = {cx, topY, cw, botH};
            l.recentHistoryBtn = {cx + cw - btnW - 14, topY + 8, btnW, 56};
            topY += botH + 20;
        }

        l.totalContentHeight = topY - cy;
    } else if (tab == 1) {
        // Tab 1: Calendar
        const bool tallCal = (!l.sidebar || cw < 680);
        int calW = (!tallCal && cw >= 680) ? (cw * 62 / 100) : cw;
        int calH = tallCal ? 510 : ch - 58;
        l.calCard = {cx, cy + 54, calW, calH};
        l.prevMonthBtn = {cx + calW - 148, cy + 60, 64, 50};
        l.nextMonthBtn = {cx + calW - 76, cy + 60, 64, 50};

        l.calCellW = std::max(48, (calW - 24) / 7);
        l.calCellH = tallCal ? 50 : 54;
        l.calGridX = cx + 12;
        l.calGridY = cy + 136;

        if (!tallCal) {
            int detX = cx + calW + 14;
            int detW = cw - calW - 14;
            l.dayDetailCard = {detX, cy + 54, detW, calH};
            l.totalContentHeight = calH + 64;
        } else {
            l.dayDetailCard = {cx, cy + 54 + calH + 14, cw, 280};
            l.totalContentHeight = 54 + calH + 14 + 280 + 20;
        }
    } else if (tab == 2) {
        // Tab 2: Books
        if (inBookDetail) {
            l.backToBooksBtn = {cx, cy, 200, 56};
            l.bookDetailHead = {cx, cy + 68, cw, 140};
            l.totalContentHeight = 68 + 140 + 24 + 40 + sessionCount * 76 + 24;
        } else {
            l.totalContentHeight = 60 + bookCount * (l.bookRowH + l.bookRowGap) + 20;
        }
    } else if (tab == 3) {
        // Tab 3: History
        l.totalContentHeight = 60 + sessionCount * (l.historyRowH + l.historyRowGap) + 20;
    }

    l.maxScrollY = std::max(0, l.totalContentHeight - ch);
    return l;
}

} // namespace

void ReadingJournalUI::open(const std::string &bookId) {
    tab_ = bookId.empty() ? 0 : 2;
    detailId_ = bookId;
    period_ = 0;
    offset_ = 0;
    scrollY_ = 0;
    maxScrollY_ = 0;
    monthDelta_ = 0;
    selectedDay_ = 0;
    focus_ = 0;
    goals_ = false;
    exportModal_ = false;
    deleteModal_ = false;
    sessionToDelete_ = 0;
    exportMessage_.clear();
    preview_ = false;
}

void ReadingJournalUI::nextTab() {
    tab_ = (tab_ + 1) % 4;
    offset_ = 0;
    scrollY_ = 0;
    focus_ = tab_ + 1;
    detailId_.clear();
}

void ReadingJournalUI::prevTab() {
    tab_ = (tab_ + 3) % 4;
    offset_ = 0;
    scrollY_ = 0;
    focus_ = tab_ + 1;
    detailId_.clear();
}

void ReadingJournalUI::back() {
    if (deleteModal_) { deleteModal_ = false; sessionToDelete_ = 0; }
    else if (exportModal_) { exportModal_ = false; exportMessage_.clear(); }
    else if (goals_) { goals_ = false; }
    else if (!detailId_.empty()) { detailId_.clear(); offset_ = 0; scrollY_ = 0; }
}

void ReadingJournalUI::scroll(int dy) {
    scrollY_ = std::clamp(scrollY_ + dy, 0, maxScrollY_);
}

void ReadingJournalUI::navigate(int dx, int dy, const ReadingJournal &journal,
                               const ProgressStore *progress, const CatalogStore *catalog) {
    (void)journal;
    if (goals_ || exportModal_ || deleteModal_) return;

    BookStateResolver resolver;
    if (progress && catalog) resolver = makeResolver(*progress, *catalog);

    if (!detailId_.empty()) {
        if (dy) scroll(dy * 64);
        return;
    }

    if (tab_ == 1 && selectedDay_ > 0 && dx) {
        const auto d = dateAt(monthDelta_);
        selectedDay_ = std::clamp(selectedDay_ + dx, 1, daysInMonth(d.tm_year + 1900, d.tm_mon));
        return;
    }

    if (dx) {
        tab_ = std::clamp(tab_ + dx, 0, 3);
        offset_ = 0;
        scrollY_ = 0;
        detailId_.clear();
        focus_ = tab_ + 1;
        return;
    }

    if (dy && tab_ == 1) {
        const auto d = dateAt(monthDelta_);
        selectedDay_ = std::clamp(std::max(1, selectedDay_) + dy * 7, 1, daysInMonth(d.tm_year + 1900, d.tm_mon));
        return;
    }

    if (dy) {
        scroll(dy * 64);
    }
}

ReadingJournalUI::Action ReadingJournalUI::activate(int w, int h, ReadingJournal &j,
                                                  const ProgressStore *progress, const CatalogStore *catalog) {
    (void)w; (void)h;
    if (deleteModal_) {
        if (sessionToDelete_) j.deleteSession(sessionToDelete_);
        deleteModal_ = false; sessionToDelete_ = 0;
        return Action::None;
    }
    if (exportModal_) { exportModal_ = false; return Action::None; }
    if (goals_) { goals_ = false; return Action::None; }
    if (preview_) return Action::OpenFull;
    if (!detailId_.empty()) { back(); return Action::None; }
    if (tab_ == 2) {
        BookStateResolver resolver;
        if (progress && catalog) resolver = makeResolver(*progress, *catalog);
        const auto b = j.books(resolver);
        if (offset_ < int(b.size())) { detailId_ = b[offset_].id; offset_ = 0; scrollY_ = 0; }
    }
    return Action::None;
}

ReadingJournalUI::Action ReadingJournalUI::tap(int x, int y, int w, int h, ReadingJournal &journal,
                                              const ProgressStore *progress, const CatalogStore *catalog) {
    BookStateResolver resolver;
    if (progress && catalog) resolver = makeResolver(*progress, *catalog);

    // 1. Modals Tap Handling
    if (deleteModal_) {
        const int pw = std::min(520, w - 24), ph = 270, px = (w - pw) / 2, py = (h - ph) / 2;
        int btnW = (pw - 60) / 2;
        if (y >= py + 190 && y <= py + 246) {
            if (x >= px + 24 && x <= px + 24 + btnW) {
                if (sessionToDelete_) journal.deleteSession(sessionToDelete_);
                deleteModal_ = false; sessionToDelete_ = 0;
                return Action::None;
            } else if (x >= px + 36 + btnW && x <= px + 36 + 2 * btnW) {
                deleteModal_ = false; sessionToDelete_ = 0;
                return Action::None;
            }
        }
        if (x < px || x > px + pw || y < py || y > py + ph) {
            deleteModal_ = false; sessionToDelete_ = 0;
        }
        return Action::None;
    }

    if (exportModal_) {
        const int pw = std::min(540, w - 24), ph = 330, px = (w - pw) / 2, py = (h - ph) / 2;
        int btnW = (pw - 60) / 2;
        if (y >= py + 180 && y <= py + 236) {
            auto today = nowDate();
            char dateBuf[32];
            std::snprintf(dateBuf, sizeof dateBuf, "%04d-%02d-%02d", today.tm_year + 1900, today.tm_mon + 1, today.tm_mday);
            if (x >= px + 24 && x <= px + 24 + btnW) {
                auto outPath = journal.exportDirectory() / ("reading_journal_" + std::string(dateBuf) + ".json");
                if (journal.exportJson(outPath)) exportMessage_ = "JSON: " + outPath.string();
                else exportMessage_ = "Hata: JSON aktarılamadı";
                return Action::None;
            } else if (x >= px + 36 + btnW && x <= px + 36 + 2 * btnW) {
                auto outPath = journal.exportDirectory() / ("reading_journal_" + std::string(dateBuf) + ".csv");
                if (journal.exportCsv(outPath)) exportMessage_ = "CSV: " + outPath.string();
                else exportMessage_ = "Hata: CSV aktarılamadı";
                return Action::None;
            }
        }
        if (y >= py + 250 && y <= py + 306) { exportModal_ = false; return Action::None; }
        if (x < px || x > px + pw || y < py || y > py + ph) { exportModal_ = false; }
        return Action::None;
    }

    if (goals_) {
        const int pw = std::min(540, w - 24), ph = 340, px = (w - pw) / 2, py = (h - ph) / 2;
        // Steppers row: [-] and [+]
        if (y >= py + 114 && y <= py + 170) {
            if (x >= px + 24 && x <= px + 104) {
                journal.setDailyGoalMinutes(journal.dailyGoalMinutes() >= 5 ? journal.dailyGoalMinutes() - 5 : 0);
                return Action::None;
            } else if (x >= px + pw - 104 && x <= px + pw - 24) {
                journal.setDailyGoalMinutes(std::min(720u, journal.dailyGoalMinutes() + 5));
                return Action::None;
            }
        }
        // Presets row: 15, 30, 45, Off
        int preW = (pw - 48 - 3 * 10) / 4;
        if (y >= py + 182 && y <= py + 234) {
            for (int k = 0; k < 4; ++k) {
                int bx = px + 24 + k * (preW + 10);
                if (x >= bx && x <= bx + preW) {
                    if (k == 0) journal.setDailyGoalMinutes(15);
                    else if (k == 1) journal.setDailyGoalMinutes(30);
                    else if (k == 2) journal.setDailyGoalMinutes(45);
                    else journal.setDailyGoalMinutes(0);
                    return Action::None;
                }
            }
        }
        if (y >= py + 250 && y <= py + 306) { goals_ = false; return Action::None; }
        if (x < px || x > px + pw || y < py || y > py + ph) { goals_ = false; }
        return Action::None;
    }

    if (preview_) {
        const int pw = std::min(620, w - 24), px = (w - pw) / 2, py = (h - 320) / 2;
        if (x < px || x > px + pw || y < py || y > py + 320) return Action::ClosePreview;
        if (y > py + 244) return x < px + pw / 2 ? Action::OpenFull : Action::ClosePreview;
        return Action::None;
    }

    // Compute layout for exact synchronized coordinate testing
    const auto allBooks = journal.books(resolver);
    const auto &allSessions = journal.sessions();
    const auto bookSessions = detailId_.empty() ? std::vector<JournalSession>{} : journal.sessionsForBook(detailId_);
    const auto l = computeLayout(w, h, tab_, !detailId_.empty(),
                                int(allBooks.size()),
                                detailId_.empty() ? int(allSessions.size()) : int(bookSessions.size()),
                                false);
    maxScrollY_ = l.maxScrollY;

    // 2. Header Bar Actions
    if (y < 76) {
        if (ui::contains(l.libraryBtn, x, y)) {
            if (!detailId_.empty()) { detailId_.clear(); scrollY_ = 0; return Action::None; }
            return Action::Back;
        }
        if (ui::contains(l.goalsBtn, x, y)) { openGoals(); return Action::None; }
        if (tab_ == 3 && ui::contains(l.exportBtn, x, y)) { openExport(); return Action::None; }
        return Action::None;
    }

    // 3. Navigation Tabs
    if (l.sidebar) {
        for (int i = 0; i < 4; ++i) {
            if (ui::contains(l.navTabs[i], x, y)) {
                tab_ = i; offset_ = 0; scrollY_ = 0; focus_ = i + 1; detailId_.clear();
                return Action::None;
            }
        }
    } else {
        if (y >= 86 && y <= 146) {
            for (int i = 0; i < 4; ++i) {
                if (ui::contains(l.navTabs[i], x, y)) {
                    tab_ = i; offset_ = 0; scrollY_ = 0; focus_ = i + 1; detailId_.clear();
                    return Action::None;
                }
            }
        }
    }

    // 4. Content Area Actions
    if (!ui::contains(l.contentArea, x, y)) return Action::None;
    const int cy = y + scrollY_;

    if (!detailId_.empty()) {
        if (ui::contains(l.backToBooksBtn, x, cy)) {
            detailId_.clear(); scrollY_ = 0; return Action::None;
        }
        return Action::None;
    }

    if (tab_ == 0) {
        // Overview Period
        for (int p = 0; p < 3; ++p) {
            if (ui::contains(l.periodBtns[p], x, cy)) {
                period_ = p; return Action::None;
            }
        }
        // Currently Reading [Detay]
        if (ui::contains(l.currentDetailBtn, x, cy)) {
            const auto cr = journal.currentlyReading(resolver);
            if (!cr.first.empty()) { detailId_ = cr.first; tab_ = 2; scrollY_ = 0; }
            return Action::None;
        }
        // Currently Reading [Kitaplarım]
        if (ui::contains(l.currentBooksBtn, x, cy)) {
            tab_ = 2; scrollY_ = 0; return Action::None;
        }
        // Recent Activity [Tümü]
        if (ui::contains(l.recentHistoryBtn, x, cy)) {
            tab_ = 3; scrollY_ = 0; return Action::None;
        }
    } else if (tab_ == 1) {
        // Calendar Month navigation
        if (ui::contains(l.prevMonthBtn, x, cy)) { monthDelta_--; selectedDay_ = 0; return Action::None; }
        if (ui::contains(l.nextMonthBtn, x, cy)) { monthDelta_++; selectedDay_ = 0; return Action::None; }

        // Day cells
        const auto d = dateAt(monthDelta_);
        int first = (d.tm_wday + 6) % 7;
        int maxD = daysInMonth(d.tm_year + 1900, d.tm_mon);
        for (int k = 1; k <= maxD; ++k) {
            int z = first + k - 1, row = z / 7, col = z % 7;
            SDL_Rect box{l.calGridX + col * l.calCellW, l.calGridY + row * l.calCellH, l.calCellW - 4, l.calCellH - 4};
            if (ui::contains(box, x, cy)) {
                selectedDay_ = k; return Action::None;
            }
        }
    } else if (tab_ == 2) {
        // Books list
        int n = 0;
        for (std::size_t i = 0; i < allBooks.size(); ++i) {
            int rowY = l.contentArea.y + 60 + (n++) * (l.bookRowH + l.bookRowGap);
            SDL_Rect bRc{l.contentArea.x, rowY, l.contentArea.w, l.bookRowH};
            SDL_Rect btnRc{bRc.x + bRc.w - 140, bRc.y + 24, 124, 56};
            if (ui::contains(btnRc, x, cy) || ui::contains(bRc, x, cy)) {
                detailId_ = allBooks[i].id; scrollY_ = 0; return Action::None;
            }
        }
    } else if (tab_ == 3) {
        // History list
        int n = 0;
        for (auto it = allSessions.rbegin(); it != allSessions.rend(); ++it) {
            int rowY = l.contentArea.y + 60 + (n++) * (l.historyRowH + l.historyRowGap);
            SDL_Rect sRc{l.contentArea.x, rowY, l.contentArea.w, l.historyRowH};
            SDL_Rect delBtn{sRc.x + sRc.w - 90, sRc.y + 14, 76, 56};
            if (ui::contains(delBtn, x, cy)) {
                openDeleteConfirm(it->id);
                return Action::None;
            }
        }
    }

    return Action::None;
}

void ReadingJournalUI::render(SDL_Renderer *r, Typography &font, const ThemePalette &pal,
                             const ReadingJournal &journal, const ProgressStore &progress,
                             const CatalogStore &catalog, int w, int h, bool en) {
    const auto resolver = makeResolver(progress, catalog);
    const auto allBooks = journal.books(resolver);
    const auto &allSessions = journal.sessions();
    const auto bookSessions = detailId_.empty() ? std::vector<JournalSession>{} : journal.sessionsForBook(detailId_);

    JournalLayout l = computeLayout(w, h, tab_, !detailId_.empty(),
                                   int(allBooks.size()),
                                   detailId_.empty() ? int(allSessions.size()) : int(bookSessions.size()),
                                   en);
    maxScrollY_ = l.maxScrollY;
    scrollY_ = std::clamp(scrollY_, 0, maxScrollY_);

    // 1. Header Bar
    ui::surface(r, l.headerBar, pal.card, 0);
    ui::button(r, font, l.libraryBtn, label(en, "Kitaplık", "Library"), pal, false, false, false);

    // Compute title bounds avoiding overlap with right buttons
    int titleX = l.libraryBtn.x + l.libraryBtn.w + 16;
    int rightLimit = l.goalsBtn.x - 16;
    if (tab_ == 3) rightLimit = l.exportBtn.x - 16;
    int titleMaxW = std::max(120, rightLimit - titleX);

    text(r, font, label(en, "Kütüphane / Günlüğüm", "Library / Journal"),
         titleX, 12, pal.muted, titleMaxW);
    text(r, font, label(en, "Okuma Günlüğüm", "My Reading Journal"),
         titleX, 36, pal.fg, titleMaxW, true);

    ui::button(r, font, l.goalsBtn, (w < 800 ? label(en, "Hedef", "Goals") : label(en, "Hedeflerim", "Goals")), pal, false, false, false);
    if (tab_ == 3) {
        ui::button(r, font, l.exportBtn, label(en, "Dışa aktar", "Export"), pal, false, false, false);
    }
    line(r, pal.cardBorder, 0, 76, w);

    // 2. Navigation Tabs
    if (l.sidebar) {
        panel(r, pal, l.navPanel);
        // Brand mark
        SDL_Rect brandMark{28, 102, 38, 38};
        ui::surface(r, brandMark, blendColor(pal.card, pal.accent, 25), 10);
        text(r, font, "P", brandMark.x + 12, brandMark.y + 6, pal.accent, 24, true);
        text(r, font, "PaperNx", 74, 102, pal.fg, 140, true);
        textSmall(r, font, label(en, "OKUMA ALANI", "READING SPACE"), 74, 144, pal.muted, 140);
        line(r, pal.cardBorder, 24, 174, 194);

        const char *trNav[] = {"Genel Bakış", "Takvim", "Kitaplarım", "Geçmiş"};
        const char *enNav[] = {"Overview", "Calendar", "My books", "History"};
        for (int i = 0; i < 4; ++i) {
            ui::button(r, font, l.navTabs[i], en ? enNav[i] : trNav[i], pal,
                       i == tab_, false, false, focus_ == i + 1);
            const auto ink = (i == tab_) ? pal.bg : pal.fg;
            SDL_SetRenderDrawColor(r, ink.r, ink.g, ink.b, 255);
            const int bx = l.navTabs[i].x + 12, by = l.navTabs[i].y + 18;
            if (i == 0) {
                for (int k = 0; k < 4; ++k) {
                    SDL_Rect q{bx + (k % 2) * 9, by + (k / 2) * 9, 6, 6};
                    SDL_RenderDrawRect(r, &q);
                }
            } else if (i == 1) {
                SDL_Rect q{bx, by, 20, 19};
                SDL_RenderDrawRect(r, &q);
                SDL_RenderDrawLine(r, bx, by + 6, bx + 19, by + 6);
            } else if (i == 2) {
                SDL_Rect q{bx, by, 9, 18};
                SDL_RenderDrawRect(r, &q);
                q.x += 10;
                SDL_RenderDrawRect(r, &q);
            } else {
                SDL_Rect q{bx, by, 17, 19};
                SDL_RenderDrawRect(r, &q);
                for (int k = 0; k < 3; ++k) {
                    SDL_RenderDrawLine(r, bx + 4, by + 5 + 5 * k, bx + 13, by + 5 + 5 * k);
                }
            }
        }

        // Offline status footer
        SDL_Rect foot{24, h - 90, 194, 68};
        panel(r, pal, foot);
        SDL_Color dotCol{78, 160, 98, 255};
        ui::surface(r, {foot.x + 12, foot.y + 16, 8, 8}, dotCol, 4);
        textSmall(r, font, label(en, "Çevrimdışı kayıt", "Offline journal"),
                  foot.x + 26, foot.y + 12, pal.fg, foot.w - 32);
        textSmall(r, font, label(en, "microSD kartında", "Saved on microSD"),
                  foot.x + 12, foot.y + 38, pal.muted, foot.w - 24);
    } else {
        const char *trNav[] = {"Genel", "Takvim", "Kitaplar", "Geçmiş"};
        const char *enNav[] = {"Overview", "Calendar", "Books", "History"};
        for (int i = 0; i < 4; ++i) {
            ui::button(r, font, l.navTabs[i], en ? enNav[i] : trNav[i], pal,
                       i == tab_, false, false, focus_ == i + 1);
        }
    }

    // 3. Content Viewport (Clipped and Scrolled)
    SDL_RenderSetClipRect(r, &l.contentArea);
    const int sY = scrollY_;

    if (!detailId_.empty()) {
        // Book Details Drilldown
        auto bTotal = journal.bookTotal(detailId_, resolver);
        SDL_Rect backBtnRc = l.backToBooksBtn;
        backBtnRc.y -= sY;
        ui::button(r, font, backBtnRc, label(en, "Kitaplarıma dön", "Back to books"), pal, false, false, false);

        SDL_Rect headRc = l.bookDetailHead;
        headRc.y -= sY;
        panel(r, pal, headRc);
        drawCover(r, pal, font, {headRc.x + 16, headRc.y + 14, 76, 112}, bTotal.title, bTotal.author);
        text(r, font, bTotal.title.empty() ? label(en, "Kitap Detayı", "Book Details") : bTotal.title,
             headRc.x + 104, headRc.y + 16, pal.fg, headRc.w - 120, true);
        text(r, font, bTotal.author, headRc.x + 104, headRc.y + 48, pal.muted, headRc.w - 120);

        font.drawProgressBar(r, headRc.x + 104, headRc.y + 80, headRc.w - 130, 8, static_cast<float>(bTotal.progress),
                             blendColor(pal.card, pal.cardBorder, 60), pal.accent);
        char metaBuf[128];
        std::snprintf(metaBuf, sizeof metaBuf, "%%%d %s  ·  %s  ·  %zu %s",
                      int(bTotal.progress * 100.0), label(en, "tamamlandı", "completed").c_str(),
                      duration(bTotal.activeMilliseconds, en).c_str(),
                      bTotal.sessions, label(en, "oturum", "sessions").c_str());
        textSmall(r, font, metaBuf, headRc.x + 104, headRc.y + 98, pal.muted, headRc.w - 120);

        text(r, font, label(en, "Bu Kitabın Oturum Geçmişi", "Session History for this Book"),
             l.contentArea.x, l.contentArea.y + 224 - sY, pal.fg, l.contentArea.w, true);

        if (bookSessions.empty()) {
            panel(r, pal, {l.contentArea.x, l.contentArea.y + 264 - sY, l.contentArea.w, 90});
            text(r, font, label(en, "Kayıtlı oturum bulunamadı.", "No recorded sessions found."),
                 l.contentArea.x + 20, l.contentArea.y + 300 - sY, pal.muted, l.contentArea.w - 40);
        } else {
            for (std::size_t i = 0; i < bookSessions.size(); ++i) {
                int rowY = l.contentArea.y + 264 + int(i) * 76 - sY;
                panel(r, pal, {l.contentArea.x, rowY, l.contentArea.w, 68});
                SDL_Rect dotRc{l.contentArea.x + 18, rowY + 28, 8, 8};
                ui::surface(r, dotRc, pal.accent, 4);
                text(r, font, localDate(bookSessions[i].endedUtc) + " · " + localTime(bookSessions[i].startedUtc) + "–" + localTime(bookSessions[i].endedUtc),
                     l.contentArea.x + 36, rowY + 14, pal.fg, l.contentArea.w - 240);
                char pBuf[64];
                std::snprintf(pBuf, sizeof pBuf, "%%%d -> %%%d",
                              int(bookSessions[i].startProgress * 100.0), int(bookSessions[i].endProgress * 100.0));
                textSmall(r, font, pBuf, l.contentArea.x + 36, rowY + 40, pal.muted, l.contentArea.w - 240);
                text(r, font, duration(bookSessions[i].activeMilliseconds, en),
                     l.contentArea.x + l.contentArea.w - 180, rowY + 20, pal.accent, 160);
            }
        }
    } else if (journal.totals().sessions == 0 && allBooks.empty()) {
        // Empty State
        SDL_Rect eRc{l.contentArea.x, l.contentArea.y - sY, l.contentArea.w, std::min(l.contentArea.h, 240)};
        panel(r, pal, eRc);
        text(r, font, label(en, "Henüz okuma kaydı yok.", "No reading records yet."),
             eRc.x + 24, eRc.y + 24, pal.fg, eRc.w - 48, true);
        text(r, font, label(en, "Bir kitap açıp okuduğunda aktif sürelerin burada görünecektir.",
                            "Your reading activity will appear here once you open and read a book."),
             eRc.x + 24, eRc.y + 74, pal.muted, eRc.w - 48);
        text(r, font, label(en, "Kitaplığa dön: sol üst düğme (veya B)", "Return to library: upper-left button (or B)"),
             eRc.x + 24, eRc.y + 124, pal.accent, eRc.w - 48);
    } else if (tab_ == 0) {
        // Tab 0: Overview
        auto stats = journal.calculatePeriodStats(period_, 0, en, resolver);
        text(r, font, label(en, "OKUMA ÖZETİ", "READING SUMMARY"),
             l.contentArea.x, l.contentArea.y - 4 - sY, pal.accent);
        text(r, font, stats.title, l.contentArea.x, l.contentArea.y + 27 - sY, pal.fg, 260, true);

        const char *pTr[] = {"Hafta", "Ay", "Yıl"};
        const char *pEn[] = {"Week", "Month", "Year"};
        for (int p = 0; p < 3; ++p) {
            SDL_Rect pRc = l.periodBtns[p];
            pRc.y -= sY;
            ui::button(r, font, pRc, en ? pEn[p] : pTr[p], pal, p == period_, false, false);
        }

        // Active Reading Chart Card
        SDL_Rect cRc = l.chartCard;
        cRc.y -= sY;
        panel(r, pal, cRc);
        textSmall(r, font, label(en, "Toplam aktif okuma süresi", "Total active reading time"),
                  cRc.x + 20, cRc.y + 12, pal.muted, cRc.w - 40);
        text(r, font, duration(stats.activeMilliseconds, en),
             cRc.x + 20, cRc.y + 36, pal.fg, cRc.w - 40, true);

        // Trend Pill
        SDL_Rect trendPill{cRc.x + 20, cRc.y + 72, std::min(cRc.w - 40, 260), 26};
        ui::surface(r, trendPill, blendColor(pal.card, pal.accent, 22), 13);
        textSmall(r, font, stats.trendText, trendPill.x + 10, trendPill.y + 4, pal.accent, trendPill.w - 20);

        // Bar Chart
        if (!stats.barValues.empty()) {
            std::uint64_t maxVal = 1;
            for (auto v : stats.barValues) maxVal = std::max(maxVal, v);
            int barAreaY = cRc.y + 104;
            int barCount = static_cast<int>(stats.barValues.size());
            int barStep = (cRc.w - 48) / barCount;
            int barW = std::clamp(barStep - 10, 8, 36);

            for (int i = 0; i < barCount; ++i) {
                int bX = cRc.x + 24 + i * barStep + (barStep - barW) / 2;
                int bH = static_cast<int>(50.0 * (double(stats.barValues[i]) / double(maxVal)));
                bH = std::max(4, bH);
                SDL_Rect bRc{bX, barAreaY + 50 - bH, barW, bH};
                SDL_Color bCol = (i == stats.highlightBar) ? pal.accent : blendColor(pal.card, pal.accent, 45);
                ui::surface(r, bRc, bCol, 5);
                textSmall(r, font, stats.barLabels[i], bX - 2, barAreaY + 54, pal.muted, barStep);
            }
        }
        line(r, pal.cardBorder, cRc.x + 20, cRc.y + cRc.h - 28, cRc.w - 40);
        textSmall(r, font, stats.range, cRc.x + 20, cRc.y + cRc.h - 24, pal.muted, cRc.w - 40);

        // Daily Goal Card
        SDL_Rect gRc = l.goalCard;
        gRc.y -= sY;
        panel(r, pal, gRc);
        textSmall(r, font, label(en, "Günlük okuma hedefim", "Daily reading goal"),
                  gRc.x + 18, gRc.y + 12, pal.muted, gRc.w - 36);
        std::string goalBadge = journal.dailyGoalMinutes()
            ? (std::to_string(journal.dailyGoalMinutes()) + (en ? " min/day" : " dk/gün"))
            : label(en, "Kapalı", "Disabled");
        textSmall(r, font, goalBadge, gRc.x + gRc.w - 120, gRc.y + 36, pal.accent, 110);

        // Ring with centered text inside
        int cxR = gRc.x + gRc.w / 2, cyR = gRc.y + 92, radius = 46, thickness = 8;
        float gFrac = stats.totalPeriodDays ? float(stats.goalDaysMet) / float(stats.totalPeriodDays) : 0.f;
        drawRing(r, pal, cxR, cyR, radius, thickness, gFrac);

        char gNum[32];
        std::snprintf(gNum, sizeof gNum, "%u/%u", stats.goalDaysMet, stats.totalPeriodDays);
        int nw = 0, nh = 0;
        font.measureText(font.uiFont(), gNum, &nw, &nh, 70);
        text(r, font, gNum, cxR - nw / 2, cyR - 16, pal.fg, 70, false);

        std::string dayUnit = label(en, "gün", "days");
        int dw = 0, dh = 0;
        font.measureText(font.smallFont(), dayUnit, &dw, &dh, 70);
        textSmall(r, font, dayUnit, cxR - dw / 2, cyR + 8, pal.muted, 70);

        std::string copy = journal.dailyGoalMinutes()
            ? (stats.goalDaysMet >= stats.totalPeriodDays / 2 ? label(en, "Harika gidiyorsun!", "Great progress!") : label(en, "Küçük adımlarla alışkanlık", "Building habits step by step"))
            : label(en, "Hedef belirlemek için dokun", "Tap Goals to set a daily target");
        textSmall(r, font, copy, gRc.x + 14, gRc.y + 148, pal.fg, gRc.w - 28);

        // 7 dots for period progress
        int dotCount = std::min(7, int(stats.totalPeriodDays));
        int dotStep = 18;
        int dotStartX = cxR - (dotCount * dotStep) / 2;
        for (int k = 0; k < dotCount; ++k) {
            SDL_Rect dRc{dotStartX + k * dotStep, gRc.y + 176, 10, 8};
            SDL_Color dCol = (k < int(stats.goalDaysMet)) ? pal.accent : blendColor(pal.card, pal.cardBorder, 70);
            ui::surface(r, dRc, dCol, 4);
        }

        // Mid Row: 4 Stat Tiles
        const std::string sLabels[] = {
            label(en, "Okunan gün", "Days read"),
            label(en, "Ortalama oturum", "Avg session"),
            label(en, "Bitirilen kitap", "Books finished"),
            label(en, "Okuma serisi", "Reading streak")
        };
        const std::string sValues[] = {
            std::to_string(stats.daysRead) + (en ? (stats.daysRead == 1 ? " day" : " days") : " gün"),
            std::to_string(stats.averageSessionMinutes) + (en ? " min" : " dk"),
            std::to_string(stats.finishedBooks),
            std::to_string(stats.streakDays) + (en ? (stats.streakDays == 1 ? " day" : " days") : " gün")
        };
        for (int i = 0; i < 4; ++i) {
            SDL_Rect tRc = l.metricTiles[i];
            tRc.y -= sY;
            panel(r, pal, tRc);
            textSmall(r, font, sLabels[i], tRc.x + 14, tRc.y + 10, pal.muted, tRc.w - 20);
            text(r, font, sValues[i], tRc.x + 14, tRc.y + 40, pal.fg, tRc.w - 20, true);
        }

        // Bottom Row: Currently Reading
        SDL_Rect curRc = l.currentCard;
        curRc.y -= sY;
        panel(r, pal, curRc);
        text(r, font, label(en, "Şu anda okuduğun", "Currently reading"),
             curRc.x + 16, curRc.y + 10, pal.fg, curRc.w - 170, true);
        SDL_Rect curBooksRc = l.currentBooksBtn;
        curBooksRc.y -= sY;
        ui::button(r, font, curBooksRc, label(en, "Kitaplar", "Books"), pal, false, false, false);

        auto cr = journal.currentlyReading(resolver);
        if (!cr.first.empty()) {
            auto bTot = journal.bookTotal(cr.first, resolver);
            drawCover(r, pal, font, {curRc.x + 16, curRc.y + 54, 52, 78}, bTot.title, bTot.author);
            text(r, font, bTot.title, curRc.x + 80, curRc.y + 54, pal.fg, curRc.w - 220);
            textSmall(r, font, bTot.author, curRc.x + 80, curRc.y + 80, pal.muted, curRc.w - 220);
            font.drawProgressBar(r, curRc.x + 80, curRc.y + 104, curRc.w - 220, 6,
                                 static_cast<float>(cr.second),
                                 blendColor(pal.card, pal.cardBorder, 60), pal.accent);
            char pStr[64];
            std::snprintf(pStr, sizeof pStr, "%%%d  ·  %s", int(cr.second * 100.0), duration(bTot.activeMilliseconds, en).c_str());
            textSmall(r, font, pStr, curRc.x + 80, curRc.y + 116, pal.muted, curRc.w - 220);

            SDL_Rect curDetRc = l.currentDetailBtn;
            curDetRc.y -= sY;
            ui::button(r, font, curDetRc, label(en, "Detay", "Details"), pal, true, false, false);
        } else {
            textSmall(r, font, label(en, "Şu anda aktif okunan kitap yok.", "No active book currently being read."),
                      curRc.x + 16, curRc.y + 68, pal.muted, curRc.w - 32);
        }

        // Bottom Row: Recent Activity
        SDL_Rect recRc = l.recentCard;
        recRc.y -= sY;
        panel(r, pal, recRc);
        text(r, font, label(en, "Son etkinlikler", "Recent activity"),
             recRc.x + 16, recRc.y + 10, pal.fg, recRc.w - 170, true);
        SDL_Rect recHistRc = l.recentHistoryBtn;
        recHistRc.y -= sY;
        ui::button(r, font, recHistRc, label(en, "Tümü", "See all"), pal, false, false, false);

        int sShown = 0;
        for (auto it = allSessions.rbegin(); it != allSessions.rend() && sShown < 2; ++it, ++sShown) {
            int itemY = recRc.y + 54 + sShown * 46;
            SDL_Rect dotRc{recRc.x + 16, itemY + 10, 8, 8};
            ui::surface(r, dotRc, pal.accent, 4);
            text(r, font, it->title, recRc.x + 36, itemY, pal.fg, recRc.w - 150);
            textSmall(r, font, localDate(it->endedUtc), recRc.x + 36, itemY + 22, pal.muted, recRc.w - 150);
            textSmall(r, font, duration(it->activeMilliseconds, en), recRc.x + recRc.w - 110, itemY + 8, pal.accent, 96);
        }
        if (allSessions.empty()) {
            textSmall(r, font, label(en, "Henüz kayıtlı okuma etkinliği yok.", "No recent reading activity."),
                      recRc.x + 16, recRc.y + 68, pal.muted, recRc.w - 32);
        }
    } else if (tab_ == 1) {
        // Tab 1: Calendar
        text(r, font, label(en, "OKUMA TAKVİMİM", "READING CALENDAR"),
             l.contentArea.x, l.contentArea.y - 4 - sY, pal.accent);
        textSmall(r, font, label(en, "Hangi günler okuduğunu ve sürelerini gör", "Track your active reading days"),
                  l.contentArea.x, l.contentArea.y + 25 - sY, pal.muted, l.contentArea.w - 200);

        SDL_Rect calRc = l.calCard;
        calRc.y -= sY;
        panel(r, pal, calRc);

        auto d = dateAt(monthDelta_);
        const char *trM[] = {"Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık"};
        const char *enM[] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
        char mHead[64];
        std::snprintf(mHead, sizeof mHead, "%s %d", en ? enM[d.tm_mon] : trM[d.tm_mon], d.tm_year + 1900);
        text(r, font, mHead, calRc.x + 20, calRc.y + 16, pal.fg, calRc.w - 180, true);

        SDL_Rect prevRc = l.prevMonthBtn; prevRc.y -= sY;
        SDL_Rect nextRc = l.nextMonthBtn; nextRc.y -= sY;
        ui::button(r, font, prevRc, "<", pal, false, false, false);
        ui::button(r, font, nextRc, ">", pal, false, false, false);

        const char *daysTr[] = {"Pzt", "Sal", "Çar", "Per", "Cum", "Cmt", "Paz"};
        const char *daysEn[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
        for (int i = 0; i < 7; ++i) {
            textSmall(r, font, en ? daysEn[i] : daysTr[i],
                      l.calGridX + i * l.calCellW + 10, calRc.y + 64, pal.muted, l.calCellW - 8);
        }

        const auto cal = journal.calendar();
        const auto now = nowDate();
        int first = (d.tm_wday + 6) % 7;
        int maxD = daysInMonth(d.tm_year + 1900, d.tm_mon);
        for (int k = 1; k <= maxD; ++k) {
            int z = first + k - 1, row = z / 7, col = z % 7;
            std::uint64_t ms = dayMs(cal, d.tm_year + 1900, d.tm_mon + 1, k);
            int intensity = (ms == 0) ? 0 : (ms < 1800000) ? 22 : (ms < 3600000) ? 45 : (ms < 5400000) ? 70 : 92;

            SDL_Rect box{l.calGridX + col * l.calCellW, l.calGridY + row * l.calCellH - sY, l.calCellW - 4, l.calCellH - 4};
            SDL_Color fill = (intensity == 0) ? blendColor(pal.card, pal.bg, 25) : blendColor(pal.card, pal.accent, intensity);
            ui::surface(r, box, fill, 8);
            SDL_SetRenderDrawColor(r, pal.cardBorder.r, pal.cardBorder.g, pal.cardBorder.b, intensity == 0 ? 90 : 180);
            SDL_RenderDrawRect(r, &box);

            if (k == selectedDay_ || (selectedDay_ == 0 && k == now.tm_mday && monthDelta_ == 0)) {
                SDL_SetRenderDrawColor(r, pal.accent.r, pal.accent.g, pal.accent.b, 255);
                for (int b = 0; b < 2; ++b) {
                    SDL_Rect sel{box.x + b, box.y + b, box.w - 2 * b, box.h - 2 * b};
                    if (sel.w > 0 && sel.h > 0) SDL_RenderDrawRect(r, &sel);
                }
            }
            SDL_Color tC = (intensity > 60) ? pal.bg : pal.fg;
            text(r, font, std::to_string(k), box.x + 8, box.y + 4, tC, box.w - 12);
            if (ms >= 60000) {
                textSmall(r, font, std::to_string(ms / 60000) + (en ? "m" : " dk"),
                          box.x + 6, box.y + 26, tC, box.w - 8);
            } else if (ms > 0) {
                textSmall(r, font, std::to_string(std::max<std::uint64_t>(1, ms / 1000)) + (en ? "s" : " sn"),
                          box.x + 6, box.y + 26, tC, box.w - 8);
            }
        }

        // Legend below calendar
        int legY = calRc.y + calRc.h - 36;
        textSmall(r, font, label(en, "Az", "Less"), calRc.x + 16, legY, pal.muted, 30);
        for (int lvl = 0; lvl < 5; ++lvl) {
            int intVal = lvl == 0 ? 0 : lvl == 1 ? 22 : lvl == 2 ? 45 : lvl == 3 ? 70 : 92;
            SDL_Rect lBox{calRc.x + 46 + lvl * 20, legY + 2, 14, 14};
            ui::surface(r, lBox, blendColor(pal.card, pal.accent, intVal), 3);
        }
        textSmall(r, font, label(en, "Çok  0/30/60/90+ dk", "More  0/30/60/90+ min"),
                  calRc.x + 154, legY, pal.muted, 180);

        // Daily Detail Panel
        SDL_Rect detRc = l.dayDetailCard;
        detRc.y -= sY;
        panel(r, pal, detRc);

        int dDay = selectedDay_ ? selectedDay_ : (monthDelta_ == 0 ? now.tm_mday : 1);
        char dStr[64];
        std::snprintf(dStr, sizeof dStr, "%d %s %d", dDay, en ? enM[d.tm_mon] : trM[d.tm_mon], d.tm_year + 1900);
        textSmall(r, font, label(en, "GÜNLÜK AYRINTI", "DAILY DETAIL"),
                  detRc.x + 18, detRc.y + 14, pal.accent);
        text(r, font, dStr, detRc.x + 18, detRc.y + 36, pal.fg, detRc.w - 36, true);

        std::uint64_t selMs = dayMs(cal, d.tm_year + 1900, d.tm_mon + 1, dDay);
        textSmall(r, font, label(en, "Aktif okuma süresi:", "Active reading time:"),
                  detRc.x + 18, detRc.y + 76, pal.muted, detRc.w - 36);
        text(r, font, duration(selMs, en), detRc.x + 18, detRc.y + 98, pal.accent, detRc.w - 36, true);
        line(r, pal.cardBorder, detRc.x + 18, detRc.y + 140, detRc.w - 36);

        text(r, font, label(en, "O gün okunan kitaplar", "Books read on this day"),
             detRc.x + 18, detRc.y + 152, pal.fg, detRc.w - 36);
        char dayK[32];
        std::snprintf(dayK, sizeof dayK, "%04d-%02d-%02d", d.tm_year + 1900, d.tm_mon + 1, dDay);
        auto dSessions = journal.sessionsForDay(dayK);
        if (dSessions.empty()) {
            textSmall(r, font, label(en, "Bu gün için kayıtlı oturum yok.", "No recorded reading sessions."),
                      detRc.x + 18, detRc.y + 186, pal.muted, detRc.w - 36);
        } else {
            int nS = 0;
            for (const auto &s : dSessions) {
                if (nS++ >= 3) break;
                int sYrow = detRc.y + 186 + (nS - 1) * 60;
                panel(r, pal, {detRc.x + 18, sYrow, detRc.w - 36, 52});
                SDL_Rect dotRc{detRc.x + 28, sYrow + 22, 8, 8};
                ui::surface(r, dotRc, pal.accent, 4);
                text(r, font, s.title, detRc.x + 44, sYrow + 6, pal.fg, detRc.w - 180);
                textSmall(r, font, duration(s.activeMilliseconds, en),
                          detRc.x + detRc.w - 120, sYrow + 16, pal.accent, 96);
            }
        }
    } else if (tab_ == 2) {
        // Tab 2: Books
        text(r, font, label(en, "KİTAPLARA GÖRE", "BOOKS READING TIME"),
             l.contentArea.x, l.contentArea.y - 4 - sY, pal.accent);
        textSmall(r, font, label(en, "İlerleme, aktif süre ve oturum geçmişi", "Progress, active time and sessions"),
                  l.contentArea.x, l.contentArea.y + 25 - sY, pal.muted, l.contentArea.w - 200);

        int n = 0;
        for (std::size_t i = 0; i < allBooks.size(); ++i) {
            int rowY = l.contentArea.y + 60 + (n++) * (l.bookRowH + l.bookRowGap) - sY;
            SDL_Rect bRc{l.contentArea.x, rowY, l.contentArea.w, l.bookRowH};
            panel(r, pal, bRc);
            drawCover(r, pal, font, {bRc.x + 16, bRc.y + 14, 54, 82}, allBooks[i].title, allBooks[i].author, int(i));
            text(r, font, allBooks[i].title, bRc.x + 84, bRc.y + 12, pal.fg, l.contentArea.w - 240, false);
            textSmall(r, font, allBooks[i].author, bRc.x + 84, bRc.y + 38, pal.muted, l.contentArea.w - 240);

            font.drawProgressBar(r, bRc.x + 84, bRc.y + 64, l.contentArea.w - 240, 6,
                                 static_cast<float>(allBooks[i].progress),
                                 blendColor(pal.card, pal.cardBorder, 60), pal.accent);

            char bInfo[128];
            std::snprintf(bInfo, sizeof bInfo, "%%%d  ·  %s  ·  %zu %s",
                          int(allBooks[i].progress * 100.0), duration(allBooks[i].activeMilliseconds, en).c_str(),
                          allBooks[i].sessions, label(en, "oturum", "sessions").c_str());
            textSmall(r, font, bInfo, bRc.x + 84, bRc.y + 76, pal.muted, l.contentArea.w - 240);

            SDL_Rect btnRc{bRc.x + bRc.w - 140, bRc.y + 26, 124, 56};
            ui::button(r, font, btnRc, label(en, "Detay", "Details"), pal, false, false, false);
        }
    } else {
        // Tab 3: History
        text(r, font, label(en, "OKUMA GEÇMİŞİM", "SESSION HISTORY"),
             l.contentArea.x, l.contentArea.y - 4 - sY, pal.accent);
        textSmall(r, font, label(en, "Kronolojik okuma oturumları ve kayıt silme", "Chronological reading sessions log"),
                  l.contentArea.x, l.contentArea.y + 25 - sY, pal.muted, l.contentArea.w - 360);

        int n = 0;
        for (auto it = allSessions.rbegin(); it != allSessions.rend(); ++it) {
            int rowY = l.contentArea.y + 60 + (n++) * (l.historyRowH + l.historyRowGap) - sY;
            SDL_Rect sRc{l.contentArea.x, rowY, l.contentArea.w, l.historyRowH};
            panel(r, pal, sRc);
            SDL_Rect dotRc{sRc.x + 18, rowY + 34, 8, 8};
            ui::surface(r, dotRc, pal.accent, 4);

            text(r, font, it->title, sRc.x + 36, rowY + 12, pal.fg, l.contentArea.w - 240, false);
            char subStr[128];
            std::snprintf(subStr, sizeof subStr, "%s · %s–%s · %%%d -> %%%d",
                          localDate(it->endedUtc).c_str(),
                          localTime(it->startedUtc).c_str(), localTime(it->endedUtc).c_str(),
                          int(it->startProgress * 100.0), int(it->endProgress * 100.0));
            textSmall(r, font, subStr, sRc.x + 36, rowY + 44, pal.muted, l.contentArea.w - 240);

            textSmall(r, font, duration(it->activeMilliseconds, en),
                      sRc.x + l.contentArea.w - 176, rowY + 28, pal.accent, 80);
            SDL_Rect delBtn{sRc.x + l.contentArea.w - 90, rowY + 14, 76, 56};
            ui::button(r, font, delBtn, label(en, "Sil", "Del"), pal,
                       false, false, false, false, false, false, true);
        }
    }

    SDL_RenderSetClipRect(r, nullptr);

    // 4. Modals (Overlays rendered above content)
    if (goals_) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        SDL_Rect dim{0, 0, w, h}; SDL_RenderFillRect(r, &dim);

        const int pw = std::min(540, w - 24), ph = 340, pxM = (w - pw) / 2, pyM = (h - ph) / 2;
        panel(r, pal, {pxM, pyM, pw, ph});
        textSmall(r, font, label(en, "OKUMA HEDEFLERİ", "READING GOALS"), pxM + 24, pyM + 18, pal.accent);
        text(r, font, label(en, "Okuma hedeflerim", "My Reading Goals"), pxM + 24, pyM + 42, pal.fg, pw - 48, true);
        textSmall(r, font, label(en, "Hedefler isteğe bağlıdır. Günlük okuma hedefini buradan ayarla.",
                                     "Goals are optional. Adjust your daily reading target below."),
                  pxM + 24, pyM + 76, pal.muted, pw - 48);

        // Steppers row with large readout
        ui::button(r, font, {pxM + 24, pyM + 114, 80, 56}, "-", pal, false, false, false);
        std::string curG = journal.dailyGoalMinutes()
            ? (std::to_string(journal.dailyGoalMinutes()) + (en ? " min / day" : " dakika / gün"))
            : label(en, "Kapalı (Hedef yok)", "Disabled (No goal)");
        int gw = 0, gh = 0; font.measureText(font.uiFont(), curG, &gw, &gh, pw - 220);
        text(r, font, curG, pxM + 24 + (pw - 48 - gw) / 2, pyM + 130, pal.fg, pw - 220, false);
        ui::button(r, font, {pxM + pw - 104, pyM + 114, 80, 56}, "+", pal, false, false, false);

        // Presets row: 15, 30, 45, Off
        int preW = (pw - 48 - 3 * 10) / 4;
        ui::button(r, font, {pxM + 24, pyM + 182, preW, 52}, label(en, "15 dk", "15 min"), pal, journal.dailyGoalMinutes() == 15, false, false);
        ui::button(r, font, {pxM + 24 + 1 * (preW + 10), pyM + 182, preW, 52}, label(en, "30 dk", "30 min"), pal, journal.dailyGoalMinutes() == 30, false, false);
        ui::button(r, font, {pxM + 24 + 2 * (preW + 10), pyM + 182, preW, 52}, label(en, "45 dk", "45 min"), pal, journal.dailyGoalMinutes() == 45, false, false);
        ui::button(r, font, {pxM + 24 + 3 * (preW + 10), pyM + 182, preW, 52}, label(en, "Kapalı", "Off"), pal, journal.dailyGoalMinutes() == 0, false, false);

        ui::button(r, font, {pxM + 24, pyM + 250, pw - 48, 56}, label(en, "Tamam", "Done"), pal, true, false, false);
    }

    if (exportModal_) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        SDL_Rect dim{0, 0, w, h}; SDL_RenderFillRect(r, &dim);

        const int pw = std::min(540, w - 24), ph = 330, pxM = (w - pw) / 2, pyM = (h - ph) / 2;
        panel(r, pal, {pxM, pyM, pw, ph});
        textSmall(r, font, label(en, "VERİ DIŞA AKTARMA", "DATA EXPORT"), pxM + 24, pyM + 18, pal.accent);
        text(r, font, label(en, "Okuma verilerini dışa aktar", "Export Reading Data"), pxM + 24, pyM + 42, pal.fg, pw - 48, true);
        textSmall(r, font, label(en, "Oturum geçmişini JSON ve CSV formatında microSD karta kaydeder.",
                                     "Saves reading session history in JSON and CSV format to microSD."),
                  pxM + 24, pyM + 76, pal.muted, pw - 48);

        std::string exportDirStr = journal.exportDirectory().string();
        textSmall(r, font, label(en, "Konum: ", "Path: ") + exportDirStr, pxM + 24, pyM + 104, pal.muted, pw - 48);

        if (!exportMessage_.empty()) {
            textSmall(r, font, ui::label(exportMessage_), pxM + 24, pyM + 132, pal.accent, pw - 48);
        }

        int btnW = (pw - 60) / 2;
        ui::button(r, font, {pxM + 24, pyM + 170, btnW, 56}, label(en, "JSON Olarak Aktar", "Export JSON"), pal, false, false, false);
        ui::button(r, font, {pxM + 36 + btnW, pyM + 170, btnW, 56}, label(en, "CSV Olarak Aktar", "Export CSV"), pal, false, false, false);
        ui::button(r, font, {pxM + 24, pyM + 244, pw - 48, 56}, label(en, "Kapat", "Close"), pal, true, false, false);
    }

    if (deleteModal_) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
        SDL_Rect dim{0, 0, w, h}; SDL_RenderFillRect(r, &dim);

        const int pw = std::min(520, w - 24), ph = 270, pxM = (w - pw) / 2, pyM = (h - ph) / 2;
        panel(r, pal, {pxM, pyM, pw, ph});
        textSmall(r, font, label(en, "OTURUMU SİL", "DELETE SESSION"), pxM + 24, pyM + 18, pal.accent);
        text(r, font, label(en, "Bu oturumu silmek istiyor musun?", "Delete this reading session?"),
             pxM + 24, pyM + 44, pal.fg, pw - 48, false);
        font.renderWrapped(r, font.smallFont(),
                           label(en, "Bu işlem kalıcıdır. Gün ve kitap toplamları yeniden hesaplanacaktır.",
                                     "This action is permanent. All totals and calendar stats will update."),
                           {pxM + 24, pyM + 86, pw - 48, 64}, pal.muted);

        int btnW = (pw - 60) / 2;
        ui::button(r, font, {pxM + 24, pyM + 190, btnW, 56}, label(en, "Sil (Kalıcı)", "Delete"),
                   pal, false, false, false, false, false, false, true);
        ui::button(r, font, {pxM + 36 + btnW, pyM + 190, btnW, 56}, label(en, "Vazgeç", "Cancel"),
                   pal, false, false, false);
    }
}

void ReadingJournalUI::renderPreview(SDL_Renderer *r, Typography &font, const ThemePalette &pal,
                                     const ReadingJournal &j, const std::string &bookId,
                                     int w, int h, bool en, const ProgressStore *progress) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
    SDL_Rect cover{0, 0, w, h}; SDL_RenderFillRect(r, &cover);

    const int pw = std::min(620, w - 24), px = (w - pw) / 2, py = (h - 320) / 2;
    panel(r, pal, {px, py, pw, 320});
    text(r, font, label(en, "Bu kitabın istatistikleri", "This book's statistics"),
         px + 24, py + 22, pal.fg, pw - 48, true);

    const auto today = nowDate();
    const auto days = j.calendar();
    text(r, font, label(en, "Bugün", "Today"), px + 24, py + 90, pal.muted, pw / 2);
    text(r, font, duration(dayMs(days, today.tm_year + 1900, today.tm_mon + 1, today.tm_mday), en),
         px + 24, py + 120, pal.fg, pw / 2);

    text(r, font, label(en, "Bu kitap", "This book"), px + pw / 2, py + 90, pal.muted, pw / 2 - 24);
    text(r, font, duration(j.bookMilliseconds(bookId), en), px + pw / 2, py + 120, pal.fg, pw / 2 - 24);

    if (progress) {
        float frac = static_cast<float>(progress->get(bookId).fraction);
        font.drawProgressBar(r, px + 24, py + 180, pw - 48, 8, frac,
                             blendColor(pal.card, pal.cardBorder, 60), pal.accent);
        char pBuf[64];
        std::snprintf(pBuf, sizeof pBuf, "%%%d %s", int(frac * 100.0), label(en, "tamamlandı", "completed").c_str());
        textSmall(r, font, pBuf, px + 24, py + 200, pal.muted, pw - 48);
    }

    int btnW = (pw - 60) / 2;
    ui::button(r, font, {px + 24, py + 244, btnW, 56},
               label(en, "Günlüğe git", "Open journal"), pal, true, false, false);
    ui::button(r, font, {px + 36 + btnW, py + 244, btnW, 56},
               label(en, "Kapat", "Close"), pal, false, false, false);
}

} // namespace papernx
