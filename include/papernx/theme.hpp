#pragma once
#include <SDL.h>
#include "papernx/core.hpp"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <string>

namespace papernx {

struct ThemePalette {
    const char *name;
    SDL_Color bg;
    SDL_Color fg;
    SDL_Color muted;
    SDL_Color accent;
    SDL_Color card;
    SDL_Color cardBorder;
    SDL_Color header;
};

// Curated e-reader color palettes tailored for Switch LCD & OLED
inline const ThemePalette g_palettes[] = {
    // 0: Paper (Clean, warm off-white)
    {"Paper", {248, 246, 240, 255}, {34, 34, 34, 255}, {125, 120, 110, 255}, {139, 90, 43, 255},
     {255, 255, 255, 255}, {225, 220, 210, 255}, {240, 237, 230, 255}},

    // 1: Ivory (Soft cream, gentle on eyes)
    {"Ivory", {254, 252, 243, 255}, {45, 40, 35, 255}, {135, 125, 115, 255}, {145, 95, 45, 255},
     {255, 255, 250, 255}, {235, 228, 215, 255}, {247, 243, 232, 255}},

    // 2: Sepia (Classic vintage parchment)
    {"Sepia", {236, 224, 198, 255}, {65, 50, 36, 255}, {130, 105, 82, 255}, {140, 85, 45, 255},
     {245, 235, 212, 255}, {215, 200, 175, 255}, {228, 214, 186, 255}},

    // 3: Slate (Modern cool gray)
    {"Slate", {218, 224, 228, 255}, {38, 48, 56, 255}, {108, 120, 130, 255}, {48, 92, 130, 255},
     {228, 234, 238, 255}, {198, 206, 212, 255}, {208, 215, 220, 255}},

    // 4: Dark (Warm graphite night mode)
    {"Dark", {28, 30, 32, 255}, {230, 228, 222, 255}, {150, 148, 142, 255}, {195, 160, 110, 255},
     {38, 40, 44, 255}, {55, 58, 62, 255}, {22, 24, 26, 255}},

    // 5: OLED Black (True black #000000 for maximum battery & contrast)
    {"OLED Black", {0, 0, 0, 255}, {235, 235, 235, 255}, {140, 140, 140, 255}, {210, 180, 130, 255},
     {16, 16, 16, 255}, {36, 36, 36, 255}, {0, 0, 0, 255}}
};

inline const int g_paletteCount = sizeof(g_palettes) / sizeof(g_palettes[0]);

struct PaperPreset { const char *name; int background,text,theme; };
inline constexpr PaperPreset g_paperPresets[] = {
    {"Pure Paper",0xffffff,0x242424,0},{"Soft Ivory",0xf7f3e8,0x35312b,1},
    {"Warm Cream",0xf4e8d0,0x41362a,1},{"Natural Paper",0xeee4cf,0x383229,2},
    {"Vintage Sepia",0xe8d6b3,0x4b3828,2},{"Linen",0xe5e1d6,0x343632,0},
    {"Cool Paper",0xe9ebe8,0x303638,3},{"Charcoal Night",0x252522,0xe8e3d9,4},
    {"OLED Black",0x000000,0xeae7df,5},
    // Neutral, low-saturation Kindle-inspired paper. Legacy presets unchanged.
    {"Soft E-Ink",0xf2f1ec,0x383833,0}
};
inline SDL_Color blendColor(SDL_Color a,SDL_Color b,int percent) {
    auto mix=[percent](int x,int y){return Uint8((x*(100-percent)+y*percent)/100);};
    return {mix(a.r,b.r),mix(a.g,b.g),mix(a.b,b.b),255};
}
inline ThemePalette resolveInterfacePalette(const ThemePalette &reading,int variant,int accent) {
    static constexpr SDL_Color accents[]={{96,60,243,255},{36,104,216,255},{8,127,139,255},{23,125,66,255},
        {184,81,13,255},{202,51,71,255},{185,46,119,255}};
    auto palette=reading;
    if(variant==0) {
        palette.bg={10,11,18,255};palette.fg={241,242,248,255};palette.muted={156,161,184,255};
        palette.card={22,24,36,255};palette.header={30,33,48,255};palette.cardBorder={48,52,72,255};
    } else if(variant==1) {
        palette.bg={245,246,251,255};palette.fg={29,34,53,255};palette.muted={102,111,138,255};
        palette.card={255,255,255,255};palette.header={237,240,249,255};palette.cardBorder={218,223,238,255};
    }
    // Accent 7 means use the selected paper's own accent. Explicit accent
    // choices remain available with Dark, Light and Follow reading theme.
    if (accent >= 0 && accent < 7) palette.accent=accents[accent];
    return palette;
}
inline double readingContrast(SDL_Color a,SDL_Color b);
// One appearance resolver for both native EPUB and PDF reader chrome.
// Legacy PDF filter bytes are ignored: PDF derives paper and text colors from
// ReaderPreferences.theme/backgroundColor/textColor like EPUB.
inline ThemePalette readerChromePalette(const ReaderPreferences &style,bool pdf,int pdfFilter,
                                        int appTheme,int policy=0,int customTheme=0) {
    (void)pdf; (void)pdfFilter; (void)appTheme; (void)policy; (void)customTheme;
    auto p=g_palettes[std::clamp(style.theme,0,5)];
    auto rgb=[](int c){return SDL_Color{Uint8(c>>16),Uint8(c>>8),Uint8(c),255};};
    if(style.backgroundColor>=0) p.bg=rgb(style.backgroundColor);
    if(style.textColor>=0) p.fg=rgb(style.textColor);
    if(style.backgroundColor>=0 && readingContrast(p.accent,p.bg)<4.5 && readingContrast(p.fg,p.bg)>=4.5) {
        const auto accent=p.accent;
        for(int percent=1;percent<=100 && readingContrast(p.accent,p.bg)<4.5;++percent)
            p.accent=blendColor(accent,p.fg,percent);
    }
    p.card=blendColor(p.bg,p.fg,5); p.header=blendColor(p.bg,p.fg,8);
    p.cardBorder=blendColor(p.bg,p.fg,24); p.muted=blendColor(p.bg,p.fg,68);
    return p;
}

inline std::string generateEpubCss(int themeIdx, int fontFamily, int fontSize, int lineHeightPercent, int marginPx, int textAlign) {
    if (themeIdx < 0 || themeIdx >= g_paletteCount) themeIdx = 0;
    const auto &p = g_palettes[themeIdx];

    const char *fontFamilyCss = "serif";
    if (fontFamily == 1) fontFamilyCss = "sans-serif";
    else if (fontFamily == 2) fontFamilyCss = "inherit"; // publisher default

    const char *alignCss = (textAlign == 1) ? "left" : "justify";
    float lineHeight = (lineHeightPercent > 0) ? (float)lineHeightPercent / 100.0f : 1.4f;

    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer),
        "html, body { "
        "background-color: #%02x%02x%02x !important; "
        "color: #%02x%02x%02x !important; "
        "font-family: %s !important; "
        "font-size: %dpx !important; "
        "line-height: %.2f !important; "
        "text-align: %s !important; "
        "margin: %dpx !important; "
        "padding: 0 !important; "
        "} "
        "p { margin-top: 0; margin-bottom: 0.8em; text-indent: 1.2em; } "
        "h1, h2, h3, h4, h5, h6 { color: #%02x%02x%02x !important; text-align: center; } "
        "a { color: #%02x%02x%02x !important; text-decoration: none; } "
        "img { max-width: 100%%; height: auto; display: block; margin: 0 auto; }",
        p.bg.r, p.bg.g, p.bg.b,
        p.fg.r, p.fg.g, p.fg.b,
        fontFamilyCss,
        fontSize,
        lineHeight,
        alignCss,
        marginPx,
        p.fg.r, p.fg.g, p.fg.b,
        p.accent.r, p.accent.g, p.accent.b
    );
    return std::string(buffer);
}

inline SDL_Color readingColor(int rgb, SDL_Color fallback) {
    return rgb < 0 ? fallback : SDL_Color{Uint8(rgb >> 16), Uint8(rgb >> 8), Uint8(rgb), 255};
}
inline double readingContrast(SDL_Color a, SDL_Color b) {
    auto luminance = [](SDL_Color c) {
        auto linear = [](int x) { double v = x / 255.0; return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
        return .2126 * linear(c.r) + .7152 * linear(c.g) + .0722 * linear(c.b);
    };
    double x = luminance(a), y = luminance(b);
    return (std::max(x, y) + .05) / (std::min(x, y) + .05);
}
inline std::string generateEpubCss(const ReaderPreferences &style,bool expandedViewport=true) {
    const auto &pal = g_palettes[std::clamp(style.theme, 0, 5)];
    auto bg = readingColor(style.backgroundColor, pal.bg), fg = readingColor(style.textColor, pal.fg);
    char css[2048];
    // Publisher mode keeps document typography; selected page colors remain independent.
    std::snprintf(css, sizeof css,
        "html, body { background-color: #%02x%02x%02x !important; color: #%02x%02x%02x !important; } ",
        bg.r, bg.g, bg.b, fg.r, fg.g, fg.b);
    std::string result = expandedViewport?"@page { margin: 0 !important; padding: 0 !important; } ":"";
    result += css;
    // Selected paper colors override embedded white publisher backgrounds.
    // MuPDF would otherwise repaint the interior after the reader chrome changes.
    const bool explicitPaper=style.backgroundColor>=0 || style.textColor>=0 || style.theme!=0;
    if(style.publisherStyles && !explicitPaper) return result;
    if(style.publisherStyles && explicitPaper) {
        result += " /*papernx-reader-controlled*/ ";
        return result;
    }
    // MuPDF 1.23 gives inline styles higher specificity than user !important.
    // Tell the bridge to use semantic HTML and reader CSS for controlled typography.
    result += "/*papernx-reader-controlled*/ ";
    std::snprintf(css, sizeof css,
        "body { font-size: %dpx !important; line-height: %.2f !important; margin: 0 !important; padding: 0; } "
        "p, li, blockquote, div, span, em, strong, td, th, a { color: #%02x%02x%02x !important; line-height: %.2f !important; text-align: %s !important; } "
        "div,span,li,blockquote,td,th,a,em,strong { font-size: 1em !important; } "
        "p { font-size: 1em !important; margin-top: 0; margin-bottom: %.2fem !important; text-indent: %.2fem !important; } "
        "h1,h2,h3,h4,h5,h6 { color: #%02x%02x%02x !important; } "
        "img,svg { max-width: 100%%; height: auto; } ",
        style.fontSize, style.lineHeight / 100.0, fg.r, fg.g, fg.b, style.lineHeight / 100.0,
        style.textAlign ? "left" : "justify", style.paragraphSpacing / 100.0, style.indentation / 100.0,
        fg.r, fg.g, fg.b);
    result += css;
    if (style.fontFamily != 2) result += std::string("body, p, li, blockquote, span, em, strong, td, th { font-family: ") +
        (style.fontFamily == 1 ? "sans-serif" : "serif") + " !important; }";
    return result;
}

} // namespace papernx
