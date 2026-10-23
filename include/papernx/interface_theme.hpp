#pragma once
#include "papernx/theme.hpp"
#include <algorithm>

namespace papernx {
// Companion to the NX-torrent-player colour family. Reading paper remains an
// independent preference; Follow reading theme keeps the earlier behaviour.
inline ThemePalette modernInterfacePalette(const ThemePalette &reading,int variant,int accent) {
    const SDL_Color accents[]={{96,60,243,255},{36,104,216,255},{8,127,139,255},{23,125,66,255},
        {184,81,13,255},{202,51,71,255},{185,46,119,255}};
    auto p=reading;
    if(variant==0) {
        p.bg={10,11,18,255};p.fg={241,242,248,255};p.muted={156,161,184,255};
        p.card={22,24,36,255};p.header={30,33,48,255};p.cardBorder={48,52,72,255};
    } else if(variant==1) {
        p.bg={245,246,251,255};p.fg={29,34,53,255};p.muted={102,111,138,255};
        p.card={255,255,255,255};p.header={237,240,249,255};p.cardBorder={218,223,238,255};
    }
    p.accent=accents[std::clamp(accent,0,6)];return p;
}
} // namespace papernx
