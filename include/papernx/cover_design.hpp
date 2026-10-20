#pragma once
#include "papernx/ui.hpp"
#include "papernx/cover_repository.hpp"
#include <cstdint>
namespace papernx::ui {
struct CoverInk {int paper,ink,accent;};
inline constexpr CoverInk coverInks[]={{0xe7dfcc,0x332f29,0x8a653b},{0xd7ded8,0x273c34,0x6c8271},{0xdce1e7,0x2d3748,0x6c7d93},{0xeadedb,0x49332f,0x9b746a},{0xe5dfd0,0x3e3c32,0x8c8663},{0xd9dedc,0x303a3a,0x6a7c7b}};
inline std::size_t coverVariant(const std::string &id) {
    std::uint32_t hash=2166136261u;for(unsigned char c:id)hash=(hash^c)*16777619u;return hash%6;
}
inline SDL_Rect portraitCoverRect(SDL_Rect bounds) {
    int h=std::min(bounds.h,int(bounds.w*1.4f)),w=std::min(bounds.w,int(h/1.4f));
    return {bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};
}
// Pure native cover: fixed geometry while loading; no runtime bitmap generation.
inline void fallbackCover(SDL_Renderer *r,Typography &type,SDL_Rect bounds,const Book &book,
                          CoverState state,const ThemePalette &palette) {
    surface(r,bounds,palette.header,8);auto box=portraitCoverRect(bounds);if(box.w<8 || box.h<8)return;
    auto colors=coverInks[coverVariant(book.id)];auto paper=readingColor(colors.paper,palette.bg),ink=readingColor(colors.ink,palette.fg),accent=readingColor(colors.accent,palette.accent);
    surface(r,box,paper,4);SDL_SetRenderDrawColor(r,accent.r,accent.g,accent.b,255);
    SDL_Rect spine{box.x,box.y,std::max(3,box.w/24),box.h};SDL_RenderFillRect(r,&spine);
    SDL_RenderDrawLine(r,box.x+spine.w+2,box.y+4,box.x+spine.w+2,box.y+box.h-5);
    const int inset=std::max(10,box.w/11),width=box.w-inset*2;
    if(box.h>140) {
        type.renderText(r,type.smallFont(),book.format==Format::Pdf?"PDF EDITION":book.format==Format::Cbz?"CBZ COMIC":"EPUB EDITION",box.x+inset,box.y+14,accent,width);
        SDL_RenderDrawLine(r,box.x+inset,box.y+42,box.x+box.w-inset,box.y+42);
        type.renderWrapped(r,type.buttonFont(),book.title,{box.x+inset,box.y+54,width,std::max(28,box.h-120)},ink);
        if(!book.author.empty())type.renderText(r,type.smallFont(),book.author,box.x+inset,box.y+box.h-54,ink,width);
        type.renderText(r,type.smallFont(),state==CoverState::Loading?"...":state==CoverState::Failed?"LOCAL EDITION":"PaperNX",box.x+inset,box.y+box.h-28,accent,width);
    } else {
        type.renderText(r,type.smallFont(),book.format==Format::Pdf?"PDF":book.format==Format::Cbz?"CBZ":"EPUB",box.x+inset,box.y+box.h/2-8,ink,width);
    }
}
}
