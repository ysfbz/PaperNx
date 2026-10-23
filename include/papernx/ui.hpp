#pragma once
#include "papernx/typography.hpp"
#include "papernx/theme.hpp"
#include "papernx/localization.hpp"
#include <algorithm>
namespace papernx::ui {
// Native touch design tokens, in output pixels. Logical sizes scale with layout.
inline constexpr int touch = 56, gap = 16, radius = 12, row = 64;
struct Tokens {
    static constexpr int title=30,section=24,label=22,metadata=18,description=17;
    static constexpr int spaceSmall=8,space=16,spaceLarge=24,border=1,focusBorder=3;
    static constexpr int headerHeight=80,footerHeight=64,dialogMaxWidth=920,coverWidth=154,coverHeight=218;
};
inline int pointerX=0,pointerY=0;
inline bool pointerDown=false;
inline int language=1;
inline std::string label(const std::string &text) { return localizeUiText(text,language); }
inline void pointer(int x,int y,bool down) { pointerX=x; pointerY=y; pointerDown=down; }
inline bool contains(SDL_Rect r, int x, int y) { return x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h; }
inline void surface(SDL_Renderer *r, SDL_Rect rect, SDL_Color color, int corner=radius) {
    if(rect.w<=0 || rect.h<=0) return;
    corner=std::min({corner,rect.w/2,rect.h/2});
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,color.a);
    SDL_Rect center{rect.x,rect.y+corner,rect.w,rect.h-corner*2}; SDL_RenderFillRect(r,&center);
    for(int y=0;y<corner;++y) {
        float d=corner-y-.5f; int inset=corner-int(std::sqrt(float(corner*corner)-d*d));
        SDL_RenderDrawLine(r,rect.x+inset,rect.y+y,rect.x+rect.w-1-inset,rect.y+y);
        SDL_RenderDrawLine(r,rect.x+inset,rect.y+rect.h-1-y,rect.x+rect.w-1-inset,rect.y+rect.h-1-y);
    }
}
inline void elevated(SDL_Renderer *r,SDL_Rect rect,const ThemePalette &pal) {
    SDL_Rect shadow{rect.x+3,rect.y+4,rect.w,rect.h};surface(r,shadow,{0,0,0,42});
    surface(r,rect,pal.card);
    SDL_SetRenderDrawColor(r,pal.cardBorder.r,pal.cardBorder.g,pal.cardBorder.b,255);
    SDL_RenderDrawRect(r,&rect);
}
inline void focusRing(SDL_Renderer *r,SDL_Rect rect,const ThemePalette &pal,bool editing=false) {
    // Two contrasting outlines stay visible on selected accent fills and on
    // surrounding surfaces. Shape communicates focus independently of hue.
    SDL_SetRenderDrawColor(r,pal.bg.r,pal.bg.g,pal.bg.b,255);
    SDL_Rect outer{rect.x-1,rect.y-1,rect.w+2,rect.h+2}; SDL_RenderDrawRect(r,&outer);
    auto color=editing?pal.accent:pal.fg;
    SDL_SetRenderDrawColor(r,color.r,color.g,color.b,255);
    for(int i=0;i<Tokens::focusBorder;++i) {
        SDL_Rect edge{rect.x+i,rect.y+i,rect.w-2*i,rect.h-2*i}; SDL_RenderDrawRect(r,&edge);
    }
    if(editing) { SDL_Rect underline{rect.x+8,rect.y+rect.h-7,rect.w-16,3}; SDL_RenderFillRect(r,&underline); }
}
inline SDL_Color destructiveColor(const ThemePalette &pal) {
    return int(pal.bg.r)+pal.bg.g+pal.bg.b<384?SDL_Color{255,160,150,255}:SDL_Color{155,36,32,255};
}
inline void button(SDL_Renderer *r, Typography &type, SDL_Rect rect, const std::string &text,
                   const ThemePalette &pal, bool selected=false, bool pressed=false,bool translate=true,
                   bool focused=false,bool editing=false,bool disabled=false,bool destructive=false) {
    auto label=translate?ui::label(text):text;
    pressed=!disabled && (pressed || (pointerDown && contains(rect,pointerX,pointerY)));
    auto fill=selected?(pressed?blendColor(pal.accent,pal.bg,12):pal.accent):pressed?pal.cardBorder:focused?pal.card:pal.header;
    surface(r,rect,fill);
    // Consistent tactile button boundary across paper, sepia and dark palettes.
    SDL_SetRenderDrawColor(r,pal.cardBorder.r,pal.cardBorder.g,pal.cardBorder.b,255);
    SDL_Rect border{rect.x,rect.y,rect.w-1,rect.h-1};
    if(border.w>0 && border.h>0) SDL_RenderDrawRect(r,&border);
    if(focused || editing) focusRing(r,rect,pal,editing);
    int tw=0,th=0; type.measureText(type.buttonFont(),label,&tw,&th,rect.w-24);
    type.renderText(r,type.buttonFont(),label,rect.x+std::max(12,(rect.w-tw)/2),rect.y+(rect.h-th)/2,
                    disabled?pal.muted:selected?pal.bg:destructive?destructiveColor(pal):pal.fg,rect.w-24);
}
// Professional e-reader list action: left-aligned label, separate focus/selection,
// restrained divider, and consistent typography across modal and quick-action lists.
inline void menuRow(SDL_Renderer *r,Typography &type,SDL_Rect rect,const std::string &rawLabel,
                    const ThemePalette &pal,bool selected=false,bool focused=false,bool destructive=false) {
    if(rect.w<=0||rect.h<=0)return;
    const std::string label = ui::label(rawLabel);
    const auto fill=selected?pal.accent:(pointerDown&&contains(rect,pointerX,pointerY)?pal.cardBorder:pal.card);
    surface(r,rect,fill,8);
    SDL_SetRenderDrawColor(r,pal.cardBorder.r,pal.cardBorder.g,pal.cardBorder.b,255);
    SDL_RenderDrawLine(r,rect.x+14,rect.y+rect.h-1,rect.x+rect.w-14,rect.y+rect.h-1);
    int textHeight=0;
    type.measureText(type.buttonFont(),label,nullptr,&textHeight,rect.w-76);
    const auto color=selected?pal.bg:destructive?destructiveColor(pal):pal.fg;
    type.renderText(r,type.buttonFont(),label,rect.x+20,rect.y+std::max(0,(rect.h-textHeight)/2),color,rect.w-76);
    if(focused)focusRing(r,rect,pal);
}
inline void preferenceRow(SDL_Renderer *r,Typography &type,SDL_Rect rect,const std::string &label,
                          const ThemePalette &pal,bool selected=false) {
    menuRow(r,type,rect,label,pal,selected);
}
struct StepperLayout {
    SDL_Rect label,minus,value,plus;
    explicit StepperLayout(SDL_Rect r) {
        int size=std::min({touch,std::max(0,r.h),std::max(0,r.w)/3});
        int valueWidth=std::min(80,std::max(0,r.w-2*size));
        plus={r.x+r.w-size,r.y,size,r.h}; value={plus.x-valueWidth,r.y,valueWidth,r.h};
        minus={value.x-size,r.y,size,r.h}; label={r.x+12,r.y,std::max(0,minus.x-r.x-24),r.h};
    }
    int hit(int x,int y) const { return contains(minus,x,y)?-1:contains(plus,x,y)?1:0; }
};
inline void settingsRow(SDL_Renderer *r,Typography &type,SDL_Rect rect,const std::string &label,
                        const std::string &value,const ThemePalette &pal,bool focused=false,bool editing=false) {
    surface(r,rect,focused?pal.card:pal.header);
    StepperLayout layout(rect); int height=0; type.measureText(type.uiFont(),ui::label(label),nullptr,&height);
    if(layout.label.w>0) type.renderText(r,type.uiFont(),ui::label(label),layout.label.x,rect.y+(rect.h-height)/2,pal.fg,layout.label.w);
    button(r,type,layout.minus,"-",pal,false,false,false);
    button(r,type,layout.plus,"+",pal,false,false,false);
    int width=0; type.measureText(type.buttonFont(),value,&width,&height);
    type.renderText(r,type.buttonFont(),value,layout.value.x+std::max(0,(layout.value.w-width)/2),rect.y+(rect.h-height)/2,pal.fg,layout.value.w);
    if(focused || editing) focusRing(r,rect,pal,editing);
}
inline SDL_Rect settingsSliderTrack(SDL_Rect rect) {
    const int valueWidth=54,labelWidth=std::max(112,rect.w*45/100);
    return {rect.x+labelWidth,rect.y+rect.h/2-3,std::max(24,rect.w-labelWidth-valueWidth-18),6};
}
inline int settingsSliderValue(SDL_Rect track,int x,int minimum,int maximum) {
    const float ratio=std::clamp(float(x-track.x)/std::max(1,track.w-1),0.f,1.f);
    return minimum+int(std::lround(ratio*(maximum-minimum)));
}
inline void settingsSliderRow(SDL_Renderer *r,Typography &type,SDL_Rect rect,const std::string &label,
                              int value,int minimum,int maximum,const ThemePalette &pal,bool focused=false) {
    surface(r,rect,focused?pal.card:pal.header);
    const auto track=settingsSliderTrack(rect);int height=0;
    type.measureText(type.uiFont(),ui::label(label),nullptr,&height);
    type.renderText(r,type.uiFont(),ui::label(label),rect.x+12,rect.y+(rect.h-height)/2,pal.fg,track.x-rect.x-20);
    surface(r,track,pal.cardBorder,3);
    const int thumbX=track.x+int(float(track.w-1)*(std::clamp(value,minimum,maximum)-minimum)/std::max(1,maximum-minimum));
    surface(r,{track.x,track.y,thumbX-track.x+1,track.h},pal.accent,3);
    surface(r,{thumbX-8,rect.y+rect.h/2-12,16,24},pal.accent,8);
    const auto text=std::to_string(value)+"%";int width=0;
    type.measureText(type.buttonFont(),text,&width,&height);
    type.renderText(r,type.buttonFont(),text,rect.x+rect.w-width-12,rect.y+(rect.h-height)/2,pal.fg,54);
    if(focused)focusRing(r,rect,pal);
}
inline void favorite(SDL_Renderer *r,SDL_Rect rect,const ThemePalette &pal,bool selected) {
    surface(r,rect,selected?pal.accent:(pointerDown&&contains(rect,pointerX,pointerY)?pal.cardBorder:pal.header));
    auto color=selected?pal.bg:pal.accent; SDL_SetRenderDrawColor(r,color.r,color.g,color.b,255);
    SDL_Point points[11];
    for(int i=0;i<10;++i) {
        float angle=float(i)*3.14159265f/5-3.14159265f/2, radius=i%2?8.f:18.f;
        points[i]={rect.x+rect.w/2+int(std::round(std::cos(angle)*radius)),rect.y+rect.h/2+int(std::round(std::sin(angle)*radius))};
    }
    points[10]=points[0]; SDL_RenderDrawLines(r,points,11);
}
struct SettingsLayout {
    SDL_Rect sheet, close, scope, content, preview, footer;
    int sidebar;
    SettingsLayout(int w,int h) {
        int inset=w<800?8:24;
        sheet={inset,16,w-2*inset,h-32}; sidebar=w<800?142:190;
        close={sheet.x+sheet.w-208,sheet.y+8,112,56};
        int x=sheet.x+sidebar+24;
        scope={x,sheet.y+72,sheet.w-sidebar-40,56};
        int previewW=w>=1100?280:0;
        content={x,sheet.y+148,sheet.w-sidebar-40-previewW-(previewW?24:0),sheet.h-230};
        preview={x+content.w+24,content.y,previewW,content.h};
        footer={x,sheet.y+sheet.h-72,scope.w,56};
    }
    SDL_Rect tab(int i) const { return {sheet.x+12,sheet.y+80+i*56,sidebar-12,56}; }
    SDL_Rect control(int i) const { return {content.x,content.y+i*64,content.w,56}; }
};
struct LibraryLayout {
    int columns,cardWidth,cardHeight=330,top=174,stepX,stepY=350,left=32;
    LibraryLayout(int w) { top=w<900?238:174; columns=std::max(1,(w-48)/224); stepX=(w-64)/columns; cardWidth=stepX-20; }
};
} // namespace papernx::ui
