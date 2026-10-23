#include "papernx/ui.hpp"
#include "papernx/cover_design.hpp"
#include "papernx/focus.hpp"
#include "papernx/paper_texture.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace {
double luminance(SDL_Color c) {
    auto channel=[](int n) { double v=n/255.;return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4); };
    return .2126*channel(c.r)+.7152*channel(c.g)+.0722*channel(c.b);
}
double contrast(SDL_Color a,SDL_Color b) {
    double x=luminance(a),y=luminance(b);return (std::max(x,y)+.05)/(std::min(x,y)+.05);
}
SDL_Color rgb(int v) { return {Uint8(v>>16),Uint8(v>>8),Uint8(v),255}; }
}
int main() {
    using namespace papernx;
    assert(SDL_Init(SDL_INIT_VIDEO)==0);assert(TTF_Init()==0);
    auto *surface=SDL_CreateRGBSurfaceWithFormat(0,360,140,32,SDL_PIXELFORMAT_ARGB8888); assert(surface);
    auto *renderer=SDL_CreateSoftwareRenderer(surface);assert(renderer);
    {
        Typography type(".");assert(type.init());
        // Instrumentation measures actual work and observes both cache paths.
        type.resetMetrics();
        int metricWidth=0,metricHeight=0;
        type.measureText(type.uiFont(),"unique metric probe",&metricWidth,&metricHeight);
        auto firstMeasure=type.metrics().textMeasure.count;
        assert(firstMeasure==1 && metricWidth>0 && metricHeight>0);
        type.measureText(type.uiFont(),"unique metric probe",&metricWidth,&metricHeight);
        assert(type.metrics().textMeasure.count==firstMeasure && type.metrics().measureCacheHits==1);
        type.renderText(renderer,type.uiFont(),"unique raster probe",0,0,{0,0,0,255});
        auto firstRaster=type.metrics().textRaster.count,firstTexture=type.metrics().textureCreate.count;
        assert(firstRaster==1 && firstTexture==1);
        type.renderText(renderer,type.uiFont(),"unique raster probe",0,0,{0,0,0,255});
        assert(type.metrics().textRaster.count==firstRaster && type.metrics().textureCreate.count==firstTexture);
        assert(type.metrics().textCacheHits==1);
        type.renderWrapped(renderer,type.uiFont(),"unique wrapped probe",{0,0,100,100},{0,0,0,255});
        auto wrappedRaster=type.metrics().textRaster.count;
        type.renderWrapped(renderer,type.uiFont(),"unique wrapped probe",{0,0,100,100},{0,0,0,255});
        assert(type.metrics().textRaster.count==wrappedRaster && type.metrics().textCacheHits==2);
        type.renderParagraph(renderer,type.uiFont(),"layout timing probe",{0,0,300,100},{0,0,0,255},24,true,0);
        assert(type.metrics().paragraphLayout.count==1);
        for(auto timing:{type.metrics().textMeasure,type.metrics().textRaster,type.metrics().textureCreate,type.metrics().paragraphLayout})
            assert(timing.count>0 && timing.totalNs>=timing.worstNs && timing.worstNs>0);
        type.resetMetrics();assert(type.metrics().textMeasure.count==0 && type.metrics().textRaster.count==0);
        auto draw=[&](const ThemePalette &pal,bool selected,bool pressed,bool focused,bool editing=false,bool disabled=false) {
            SDL_SetRenderDrawColor(renderer,pal.bg.r,pal.bg.g,pal.bg.b,255);SDL_RenderClear(renderer);
            ui::button(renderer,type,{20,20,260,64},"Okuma ayarları",pal,selected,pressed,false,focused,editing,disabled);
            SDL_RenderPresent(renderer);
            std::vector<Uint32> pixels(360*140);assert(SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),360*4)==0);return pixels;
        };
        for(const auto &pal:g_palettes) {
            assert(contrast(pal.bg,pal.fg)>=4.5);
            assert(contrast(pal.accent,pal.bg)>=4.5);
            auto normal=draw(pal,false,false,false),selected=draw(pal,true,false,false);
            auto focused=draw(pal,false,false,true),pressed=draw(pal,false,true,false);
            assert(normal!=focused && normal!=pressed && normal!=selected);
            assert(selected!=draw(pal,true,false,true));
            assert(selected!=draw(pal,true,true,false));
            assert(focused!=draw(pal,false,false,true,true));
            assert(normal!=draw(pal,false,false,false,false,true));
        }
        const SDL_Color accentColors[]={{96,60,243,255},{36,104,216,255},{8,127,139,255},{23,125,66,255},
            {184,81,13,255},{202,51,71,255},{185,46,119,255}};
        for(int variant=0;variant<3;++variant)for(int accent=0;accent<7;++accent) {
            const auto themed=resolveInterfacePalette(g_palettes[0],variant,accent);
            assert(themed.accent.r==accentColors[accent].r && themed.accent.g==accentColors[accent].g && themed.accent.b==accentColors[accent].b);
            if(variant==2)assert(themed.bg.r==g_palettes[0].bg.r);
        }
        ReaderPreferences cssStyle;
        assert(generateEpubCss(cssStyle,true).find("@page")!=std::string::npos);
        assert(generateEpubCss(cssStyle,false).find("@page")==std::string::npos);
        for(const auto &preset:g_paperPresets) {
            assert(contrast(rgb(preset.background),rgb(preset.text))>=4.5);
            ReaderPreferences style;style.theme=preset.theme;style.backgroundColor=preset.background;style.textColor=preset.text;
            auto chrome=readerChromePalette(style,false,0,0);
            assert(contrast(chrome.header,chrome.fg)>=4.5);
            assert(contrast(chrome.accent,chrome.bg)>=4.5);
        }
        // PDF and EPUB share exactly one resolved ReaderPreferences appearance.
        // Legacy pdfColorMode values never alter canvas, ink or UI chrome.
        {
            ReaderPreferences style;style.theme=2;
            style.backgroundColor=0xe8d6b3;style.textColor=0x413224;
            auto epub=readerChromePalette(style,false,0,0);
            for(int legacy=0;legacy<=3;++legacy){
                auto pdf=readerChromePalette(style,true,legacy,0);
                assert(pdf.bg.r==epub.bg.r && pdf.bg.g==epub.bg.g && pdf.bg.b==epub.bg.b);
                assert(pdf.fg.r==epub.fg.r && pdf.fg.g==epub.fg.g && pdf.fg.b==epub.fg.b);
            }
        }
        for(int width:{64,120,320,560}) {
            ui::StepperLayout l({10,20,width,64});
            for(auto r:{l.minus,l.value,l.plus})assert(r.x>=10 && r.x+r.w<=10+width && r.w>=0);
            assert(l.hit(l.minus.x+l.minus.w/2,40)==-1);
            assert(l.hit(l.plus.x+l.plus.w/2,40)==1);
        }
        for(int minimum:{10,50})for(int width:{200,320,560}) {
            const SDL_Rect row{20,30,width,56};const auto track=ui::settingsSliderTrack(row);
            assert(track.x>row.x && track.x+track.w<row.x+row.w);
            assert(ui::settingsSliderValue(track,track.x,minimum,100)==minimum);
            assert(ui::settingsSliderValue(track,track.x+track.w-1,minimum,100)==100);
            assert(ui::settingsSliderValue(track,track.x-20,minimum,100)==minimum);
            assert(ui::settingsSliderValue(track,track.x+track.w+20,minimum,100)==100);
        }
        ui::elevated(renderer,{20,20,260,64},g_palettes[0]);
        ui::preferenceRow(renderer,type,{20,20,260,64},"Brightness",g_palettes[0]);
        unsigned char texture[]={247,243,232,40,35,30,0,0,0,247,243,232};
        std::vector<unsigned char> original(texture,texture+12);applyPaperTexture(texture,4,1,12,0xf7f3e8,0);assert(std::equal(original.begin(),original.end(),texture));
        applyPaperTexture(texture,4,1,12,0xf7f3e8,2);
        for(int i=3;i<9;++i)assert(texture[i]==original[i]);
        for(int i=0;i<12;++i)assert(std::abs(int(texture[i])-int(original[i]))<=2);
        FocusManager focus;
        focus.begin("library");focus.add("card",{0,60,100,100});focus.add("menu",{120,0,56,56});focus.finish();assert(focus.focused("card"));
        assert(focus.move(1,0) && focus.focused("menu"));focus.begin("dialog");focus.add("cancel",{0,0,56,56});focus.add("delete",{80,0,56,56});focus.add("disabled",{160,0,56,56},false);focus.finish();
        assert(focus.focused("cancel"));assert(focus.move(1,0) && focus.focused("delete"));assert(!focus.move(1,0));
        focus.begin("library");focus.add("card",{0,60,100,100});focus.add("menu",{120,0,56,56});focus.finish();assert(focus.focused("menu"));assert(focus.touch(10,80) && focus.focused("card"));
        for(auto ink:ui::coverInks)assert(contrast(rgb(ink.paper),rgb(ink.ink))>=4.5);
        for(int w:{64,180,360}){auto r=ui::portraitCoverRect({0,0,w,206});assert(r.w<=w && r.h<=206 && std::abs(r.h-r.w*1.4f)<2);}
        Book book;book.id="stable-identity";book.title="A thoughtfully designed local book";book.author="PaperNX";book.format=Format::Epub;
        const auto index=ui::coverVariant(book.id);assert(index==ui::coverVariant(book.id) && index<6);
        for(auto state:{CoverState::Loading,CoverState::NoEmbeddedCover,CoverState::Failed})ui::fallbackCover(renderer,type,{20,0,180,140},book,state,g_palettes[0]);
        // Ellipsis is clipped to the actual control width, including on cache hits.
        for(int repeat=0;repeat<2;++repeat) {
            SDL_SetRenderDrawColor(renderer,255,255,255,255);SDL_RenderClear(renderer);
            type.renderText(renderer,type.uiFont(),"Türkçe küçük alan",20,20,{0,0,0,255},4);
            SDL_RenderPresent(renderer);
            std::vector<Uint32> pixels(360*140);SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_ARGB8888,pixels.data(),360*4);
            for(int y=0;y<140;++y)for(int x=24;x<360;++x)assert(pixels[y*360+x]==0xffffffffu);
        }
    }
    SDL_DestroyRenderer(renderer);SDL_FreeSurface(surface);TTF_Quit();SDL_Quit();
    std::cout<<"UI states, nine preset contrast, narrow steppers and UTF-8 clipping PASS\n";
}
