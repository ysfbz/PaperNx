#pragma once
#include "papernx/ui.hpp"
#include "papernx/cover_repository.hpp"
#include <algorithm>
#include <cstdint>
#include <string>

namespace papernx::ui {

struct CoverInk {
    int paper, band, ink, accent;
};

// 14 curated editorial palettes with restrained ink/accent contrast across Paper, Sepia, Dark & E-Ink themes.
inline constexpr CoverInk coverInks[] = {
    {0xeee7d7, 0xdcd0b8, 0x26211b, 0x8b4c2d}, // Warm Parchment & Terracotta
    {0xe3eae4, 0xcbd8ce, 0x1b2e26, 0x3b6e57}, // Sage Cloth & Forest
    {0xe1e6ee, 0xc8d2e2, 0x1e2736, 0x3c5a80}, // Slate Linen & Cobalt
    {0xf0e5e4, 0xdfc8c6, 0x341f1f, 0x8d3d43}, // Rose Clay & Burgundy
    {0xece6d5, 0xd8ceba, 0x2c2920, 0x7b6a38}, // Ochre Buckram & Bronze
    {0xdfe8e8, 0xc4d6d6, 0x1a2c2e, 0x2d6970}, // Mineral Teal & Deep Sea
    {0xe8e4ee, 0xd1cadc, 0x272133, 0x5c4b7d}, // Plum Paper & Indigo
    {0xf2eae0, 0xe2d3c0, 0x302419, 0x9c5a25}, // Amber Vellum & Copper
    {0xe4e9df, 0xccd7c3, 0x222c1d, 0x4e6b3a}, // Olive Folio & Moss
    {0xebe7e2, 0xd6cfc6, 0x252320, 0x695a4e}, // Stone Archive & Umber
    {0xe2e8f0, 0xc9d4e3, 0x1c2533, 0x2f5b88}, // Nordic Blue & Navy
    {0xefe7eb, 0xdecbd4, 0x321f28, 0x854361}, // Mulberry Press & Wine
    {0xe8ebe0, 0xd2d8c5, 0x25291e, 0x5d6d37}, // Lichen Press & Sage
    {0xede8dc, 0xd9d0be, 0x29241c, 0x78593c}  // Classic Folio & Walnut
};

inline std::uint32_t coverHash(const std::string &text) {
    std::uint32_t h = 2166136261u;
    for (unsigned char c : text) h = (h ^ c) * 16777619u;
    return h;
}

inline std::size_t coverVariant(const std::string &id) {
    return coverHash(id) % (sizeof(coverInks) / sizeof(coverInks[0]));
}

inline const char *coverEditionLabel(Format format) {
    return format == Format::Pdf ? "PDF EDITION"
         : format == Format::Cbz ? "CBZ COMIC"
         : format == Format::Cbr ? "CBR COMIC"
                                 : "EPUB EDITION";
}

inline const char *coverBadgeLabel(Format format) {
    return format == Format::Pdf ? "PDF"
         : format == Format::Cbz ? "CBZ"
         : format == Format::Cbr ? "CBR"
                                 : "EPUB";
}

inline SDL_Rect portraitCoverRect(SDL_Rect bounds) {
    int h = std::min(bounds.h, int(bounds.w * 1.4f)), w = std::min(bounds.w, int(h / 1.4f));
    return {bounds.x + (bounds.w - w) / 2, bounds.y + (bounds.h - h) / 2, w, h};
}

// Offline editorial procedural cover: adapts cleanly to grid/list sizes and all themes.
inline void fallbackCover(SDL_Renderer *r, Typography &type, SDL_Rect bounds, const Book &book,
                          CoverState state, const ThemePalette &palette) {
    surface(r, bounds, palette.header, 8);
    const auto box = portraitCoverRect(bounds);
    if (box.w < 12 || box.h < 12) return;

    const std::uint32_t hash = coverHash(book.id + ":" + book.title);
    const auto &colors = coverInks[hash % (sizeof(coverInks) / sizeof(coverInks[0]))];
    const int styleVariant = int((hash >> 8) % 4);
    const bool comicBook = isComic(book.format);

    const SDL_Color paper = readingColor(colors.paper, palette.bg);
    const SDL_Color band = readingColor(colors.band, palette.header);
    const SDL_Color ink = readingColor(colors.ink, palette.fg);
    const SDL_Color accent = readingColor(colors.accent, palette.accent);
    const SDL_Color subtle = blendColor(ink, paper, 42);

    surface(r, box, paper, 4);

    // Compact list thumbnail layout
    if (box.h <= 140) {
        const int spineW = std::max(3, box.w / 14);
        surface(r, {box.x, box.y, spineW, box.h}, accent, 2);
        const int topBandH = std::max(14, box.h / 4);
        surface(r, {box.x + spineW, box.y, box.w - spineW, topBandH}, band, 0);
        const int pad = std::max(5, box.w / 10);
        const int tw = std::max(8, box.w - spineW - pad * 2);
        type.renderText(r, type.smallFont(), coverBadgeLabel(book.format),
                        box.x + spineW + pad, box.y + std::max(1, (topBandH - 14) / 2), accent, tw);
        if (!book.title.empty() && box.h >= 64) {
            SDL_Rect titleRc{box.x + spineW + pad, box.y + topBandH + 4, tw, std::max(16, box.h - topBandH - 8)};
            type.renderWrapped(r, type.smallFont(), book.title, titleRc, ink);
        }
        return;
    }

    // Full grid editorial book / comic cover layout
    const int inset = std::max(12, box.w / 10);
    const int innerW = std::max(24, box.w - inset * 2);

    if (comicBook) {
        // Minimalist comic panel framing with dynamic corner issue badge
        const int spineW = std::max(4, box.w / 22);
        surface(r, {box.x, box.y, spineW, box.h}, accent, 2);
        surface(r, {box.x + spineW + 6, box.y + 8, box.w - spineW - 14, box.h - 16}, band, 2);
        SDL_SetRenderDrawColor(r, accent.r, accent.g, accent.b, 220);
        SDL_Rect panelFrame{box.x + spineW + 6, box.y + 8, box.w - spineW - 14, box.h - 16};
        SDL_RenderDrawRect(r, &panelFrame);

        SDL_Rect badgeRc{box.x + spineW + 12, box.y + 14, std::min(68, innerW), 22};
        surface(r, badgeRc, accent, 4);
        type.renderText(r, type.smallFont(), coverBadgeLabel(book.format),
                        badgeRc.x + 8, badgeRc.y + 2, paper, badgeRc.w - 12);

        const int titleY = box.y + 46;
        const int titleH = std::max(44, box.h - 96);
        type.renderWrapped(r, book.title.size() > 38 ? type.buttonFont() : type.sectionFont(),
                           book.title, {box.x + spineW + 14, titleY, std::max(20, box.w - spineW - 28), titleH}, ink);

        // Minimalist geometric halftone/panel bars at bottom
        for (int b = 0; b < 3; ++b) {
            surface(r, {box.x + spineW + 14 + b * 22, box.y + box.h - 32, 16, 4},
                    b == 0 ? accent : subtle, 2);
        }
    } else {
        // 4 editorial book templates sharing a single cohesive typographic system
        const int spineW = std::max(3, box.w / 24);
        surface(r, {box.x, box.y, spineW, box.h}, accent, 2);
        SDL_SetRenderDrawColor(r, band.r, band.g, band.b, 255);
        SDL_RenderDrawLine(r, box.x + spineW + 3, box.y + 4, box.x + spineW + 3, box.y + box.h - 5);

        if (styleVariant == 0) {
            // Classic framed edition with subtle top accent rule
            SDL_SetRenderDrawColor(r, accent.r, accent.g, accent.b, 150);
            SDL_Rect innerFrame{box.x + spineW + 8, box.y + 10, box.w - spineW - 16, box.h - 20};
            SDL_RenderDrawRect(r, &innerFrame);
        } else if (styleVariant == 1) {
            // Two-tone lower folio band
            const int bandH = box.h / 3;
            surface(r, {box.x + spineW, box.y + box.h - bandH, box.w - spineW, bandH}, band, 0);
        } else if (styleVariant == 2) {
            // Upper editorial header block
            const int headerH = 44;
            surface(r, {box.x + spineW, box.y, box.w - spineW, headerH}, band, 0);
        } else {
            // Minimalist geometric motif stripe
            surface(r, {box.x + inset, box.y + box.h - 46, innerW / 3, 3}, accent, 1);
            surface(r, {box.x + inset + innerW / 3 + 6, box.y + box.h - 46, 10, 3}, subtle, 1);
        }

        // Subtle format pill at top
        const int badgeX = box.x + inset + (styleVariant == 0 ? 4 : 0);
        type.renderText(r, type.smallFont(), coverBadgeLabel(book.format), badgeX, box.y + 15, accent, innerW - 12);
        SDL_SetRenderDrawColor(r, band.r, band.g, band.b, 255);
        SDL_RenderDrawLine(r, box.x + inset, box.y + 40, box.x + box.w - inset, box.y + 40);

        // Dynamic title typography with clean wrap and fallback scale for long titles
        const int titleTop = box.y + 50;
        const int authorReserve = book.author.empty() ? 32 : 56;
        const int titleMaxH = std::max(40, box.h - 54 - authorReserve);
        TTF_Font *titleFont = book.title.size() > 42 ? type.smallFont()
                             : book.title.size() > 24 ? type.buttonFont()
                                                      : type.sectionFont();
        type.renderWrapped(r, titleFont, book.title,
                           {box.x + inset, titleTop, innerW, titleMaxH}, ink);

        if (!book.author.empty()) {
            type.renderText(r, type.smallFont(), book.author,
                            box.x + inset, box.y + box.h - 42, subtle, innerW);
        }
    }

    // Subtle status indicator in bottom-right corner without obstructing cover typography
    if (state == CoverState::Loading) {
        surface(r, {box.x + box.w - inset - 14, box.y + box.h - 18, 14, 4}, accent, 2);
    } else if (state == CoverState::Failed) {
        type.renderText(r, type.smallFont(), "!", box.x + box.w - inset - 8, box.y + box.h - 24, accent, 12);
    }
}

} // namespace papernx::ui
