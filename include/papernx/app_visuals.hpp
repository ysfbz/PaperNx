#pragma once
#include "papernx/ui.hpp"
namespace papernx::ui {
enum class Icon {Home,Books,Collection,Journal,Folder,Settings,Search,Plus,Back,Bookmark,Type,Rotate,Refresh,Arrow,Grid,Check,Download};
inline void icon(SDL_Renderer *r,SDL_Rect area,Icon glyph,SDL_Color c) {
    const int x=area.x+area.w/2,y=area.y+area.h/2;const float scale=std::min(area.w,area.h)/40.f;SDL_SetRenderDrawColor(r,c.r,c.g,c.b,c.a);
    auto line=[&](int a,int b,int d,int e){for(int k=0;k<2;++k)SDL_RenderDrawLine(r,x+int(a*scale),y+int(b*scale)+k,x+int(d*scale),y+int(e*scale)+k);};
    auto box=[&](int a,int b,int w,int h){SDL_Rect v{x+int(a*scale),y+int(b*scale),int(w*scale),int(h*scale)};SDL_RenderDrawRect(r,&v);++v.x;++v.y;v.w-=2;v.h-=2;SDL_RenderDrawRect(r,&v);};
    switch(glyph) {
    case Icon::Home:line(-12,0,0,-11);line(0,-11,12,0);line(-9,-2,-9,11);line(-9,11,9,11);line(9,11,9,-2);box(-3,3,6,8);break;
    case Icon::Books:case Icon::Type:box(-12,-11,10,23);box(2,-11,10,23);line(0,-10,0,13);break;
    case Icon::Collection:case Icon::Grid:for(int a:{-11,2})for(int b:{-11,2})box(a,b,9,9);break;
    case Icon::Journal:box(-11,-10,23,22);for(int i=0;i<3;++i)line(-5,6-i*5,6,6-i*5);line(-6,-14,-6,-7);line(6,-14,6,-7);break;
    case Icon::Folder:line(-13,-7,-3,-7);line(-3,-7,0,-3);line(0,-3,13,-3);line(13,-3,13,11);line(13,11,-13,11);line(-13,11,-13,-7);break;
    case Icon::Settings:for(int i=0;i<3;++i){line(-12,-8+i*8,12,-8+i*8);box((i==1?5:-7),-11+i*8,5,7);}break;
    case Icon::Search:{SDL_Point pts[33];for(int i=0;i<33;++i){float a=i*6.283185f/32;pts[i]={x+int((-3+std::cos(a)*9)*scale),y+int((-3+std::sin(a)*9)*scale)};}SDL_RenderDrawLines(r,pts,33);line(4,4,13,13);break;}
    case Icon::Plus:line(-10,0,10,0);line(0,-10,0,10);break;
    case Icon::Back:line(5,-10,-5,0);line(-5,0,5,10);break;
    case Icon::Arrow:line(-5,-10,5,0);line(5,0,-5,10);break;
    case Icon::Bookmark:line(-8,12,-8,-12);line(-8,-12,8,-12);line(8,-12,8,12);line(8,12,0,6);line(0,6,-8,12);break;
    case Icon::Refresh:case Icon::Rotate:line(-10,-6,8,-6);line(8,-6,4,-11);line(8,-6,4,-1);line(10,6,-8,6);line(-8,6,-4,11);line(-8,6,-4,1);break;
    case Icon::Check:line(-10,0,-3,8);line(-3,8,12,-9);break;
    case Icon::Download:line(0,-12,0,5);line(-6,-1,0,5);line(0,5,6,-1);line(-11,8,-11,12);line(-11,12,11,12);line(11,12,11,8);break;
    }
}
inline void iconButton(SDL_Renderer *r,Typography &font,SDL_Rect rect,Icon glyph,const ThemePalette &p,bool selected=false) {
    surface(r,rect,selected?blendColor(p.card,p.accent,24):p.header,18);
    if(glyph==Icon::Type){int tw=0,th=0;font.measureText(font.buttonFont(),"Aa",&tw,&th);font.renderText(r,font.buttonFont(),"Aa",rect.x+(rect.w-tw)/2,rect.y+(rect.h-th)/2,p.fg);}else icon(r,rect,glyph,selected?p.accent:p.fg);
}
inline void navItem(SDL_Renderer *r,Typography &font,SDL_Rect rect,Icon glyph,const std::string &text,const ThemePalette &p,bool selected,bool compact=false) {
    if(selected)surface(r,rect,blendColor(p.card,p.accent,18),16);
    auto c=selected?p.accent:p.muted;
    icon(r,{rect.x+8,rect.y+4,compact?rect.w-16:40,compact?34:rect.h-8},glyph,c);
    int tw=0,th=0;font.measureText(compact?font.smallFont():font.buttonFont(),label(text),&tw,&th,rect.w-16);
    font.renderText(r,compact?font.smallFont():font.buttonFont(),label(text),compact?rect.x+(rect.w-tw)/2:rect.x+56,compact?rect.y+42:rect.y+(rect.h-th)/2,c,compact?rect.w-12:rect.w-64);
}
inline void eyebrow(SDL_Renderer *r,Typography &f,const std::string &text,int x,int y,const ThemePalette &p,int max=0){f.renderText(r,f.smallFont(),label(text),x,y,p.accent,max);}
inline void divider(SDL_Renderer *r,SDL_Rect a,const ThemePalette &p){surface(r,a,p.cardBorder,0);}
} // namespace papernx::ui
