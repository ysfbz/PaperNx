#include "papernx/page_curl.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <vector>

int main() {
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    auto surface = SDL_CreateRGBSurfaceWithFormat(0, 360, 240, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(surface);
    auto renderer = SDL_CreateSoftwareRenderer(surface);
    assert(renderer);
    auto texture = [&](Uint8 r, Uint8 g, Uint8 b) {
        auto image = SDL_CreateRGBSurfaceWithFormat(0, 640, 400, 32, SDL_PIXELFORMAT_ARGB8888);
        assert(image);
        SDL_FillRect(image, nullptr, SDL_MapRGB(image->format, r, g, b));
        auto result = SDL_CreateTextureFromSurface(renderer, image);
        SDL_FreeSurface(image);
        assert(result);
        return result;
    };
    auto previous = texture(240, 30, 30), next = texture(20, 210, 30);
    const SDL_Rect page{20, 20, 320, 200}, clip{10, 10, 340, 220};
    auto pixel = [&](int x, int y) {
        Uint8 r, g, b;
        auto *row = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(surface->pixels) + y * surface->pitch);
        SDL_GetRGB(row[x], surface->format, &r, &g, &b);
        return SDL_Color{r, g, b, 255};
    };
    std::filesystem::create_directories("verification/page-curl");
    for (int direction : {1, -1}) {
        int earlyRed = 0, lateRed = 0;
        for (float progress : {0.f, .0005f, .25f, .5f, .75f, .99f, 1.f}) {
            SDL_RenderSetClipRect(renderer, nullptr);
            SDL_SetRenderDrawColor(renderer, 4, 5, 180, 255);
            SDL_RenderClear(renderer);
            SDL_RenderSetClipRect(renderer, &clip);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
            SDL_SetRenderDrawColor(renderer, 7, 8, 9, 10);
            papernx::PageCurlRenderer::render(renderer, previous, next, page, progress, direction);
            SDL_Rect restored{}; SDL_RenderGetClipRect(renderer, &restored);
            assert(restored.x == clip.x && restored.y == clip.y && restored.w == clip.w && restored.h == clip.h);
            SDL_BlendMode mode{}; SDL_GetRenderDrawBlendMode(renderer, &mode);
            assert(mode == SDL_BLENDMODE_ADD);
            Uint8 r, g, b, a; SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
            assert(r == 7 && g == 8 && b == 9 && a == 10);
            SDL_RenderPresent(renderer);
            int red = 0, green = 0;
            for (int y = 0; y < surface->h; ++y) {
                for (int x = 0; x < surface->w; ++x) {
                    auto color = pixel(x, y);
                    if (x < page.x || x >= page.x + page.w || y < page.y || y >= page.y + page.h)
                        assert(color.r == 4 && color.g == 5 && color.b == 180);
                    else {
                        red += color.r > color.g * 2;
                        green += color.g > color.r * 2;
                    }
                }
            }
            if (progress == 0.f) assert(red == page.w * page.h);
            if (progress == .0005f) assert(red > page.w * page.h * .98f);
            if (progress == 1.f) assert(green == page.w * page.h);
            if (progress == .25f) { earlyRed = red; assert(red > 0 && green > 0); }
            if (progress == .99f) { lateRed = red; assert(red < page.w * page.h / 50); }
            if (progress == .5f) {
                auto left = pixel(page.x + 10, 120), right = pixel(page.x + page.w - 10, 120);
                assert(direction > 0 ? (left.r > left.g && right.g > right.r) :
                                       (left.g > left.r && right.r > right.g));
            }
#ifndef PAPERNX_CURL_FORCE_FALLBACK
            auto name = std::string("verification/page-curl/") + (direction > 0 ? "forward-" : "backward-") +
                        std::to_string(int(progress * 100)) + ".bmp";
            assert(SDL_SaveBMP(surface, name.c_str()) == 0);
#endif
        }
        assert(lateRed < earlyRed / 10);
    }
    // Caller clipping is respected even when only part of the page is visible.
    SDL_Rect narrow{100, 80, 40, 40};
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 4, 5, 180, 255); SDL_RenderClear(renderer);
    SDL_RenderSetClipRect(renderer, &narrow);
    papernx::PageCurlRenderer::render(renderer, previous, next, page, .5f, 1);
    SDL_RenderPresent(renderer);
    assert(pixel(99, 90).b == 180 && pixel(141, 90).b == 180);
    // Zoomed sheets have their own complete paper geometry. The next sheet may
    // have a different aspect ratio/position; the caller clips the reader window.
    const SDL_Rect enlarged{-80,-100,240,480},nextSheet{170,60,100,300},reader{20,40,320,160};
    for(float progress:{0.f,.4f,1.f}) {
        SDL_RenderSetClipRect(renderer,nullptr);
        SDL_SetRenderDrawColor(renderer,4,5,180,255);SDL_RenderClear(renderer);
        SDL_RenderSetClipRect(renderer,&reader);
        papernx::PageCurlRenderer::render(renderer,previous,next,enlarged,progress,1,.4f,0,nullptr,&nextSheet);
        SDL_RenderPresent(renderer);
        assert(pixel(19,90).b==180 && pixel(40,39).b==180 && pixel(280,90).b==180);
        if(progress>0)assert(pixel(200,90).g==210); // next page uses its own bounds
        if(progress==0)assert(pixel(40,90).r==240);
        if(progress==1)assert(pixel(40,90).b==180); // no stretched viewport-sized next page
    }
#ifndef PAPERNX_CURL_FORCE_FALLBACK
    // Different grip heights and vertical travel must change the rendered fold.
    auto frameHash=[&](float grip,float travel) {
        SDL_RenderSetClipRect(renderer,nullptr);
        SDL_SetRenderDrawColor(renderer,4,5,180,255);SDL_RenderClear(renderer);
        papernx::PageCurlRenderer::render(renderer,previous,next,page,.32f,1,grip,travel);
        SDL_RenderPresent(renderer);
        std::uint64_t hash=1469598103934665603ull;
        for(int y=page.y;y<page.y+page.h;++y)for(int x=page.x;x<page.x+page.w;++x) {
            const auto c=pixel(x,y);hash=(hash^c.r)*1099511628211ull;hash=(hash^c.g)*1099511628211ull;
        }
        return hash;
    };
    assert(frameHash(.15f,0)!=frameHash(.85f,0));
    assert(frameHash(.5f,-.2f)!=frameHash(.5f,.2f));
#endif
    SDL_DestroyTexture(previous); SDL_DestroyTexture(next);
    SDL_DestroyRenderer(renderer); SDL_FreeSurface(surface); SDL_Quit();
    std::cout << "[PASS] page curl pixels, directions, continuous endpoint, scaled fallback and render state\n";
}
