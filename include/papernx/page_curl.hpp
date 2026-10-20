#pragma once

#include <SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace papernx {

// A developable sheet wrapped around an inclined, tapered cylinder. Source
// coordinates stay on the paper; the camera projects its actual height, rather
// than skewing rows of text. The crease follows the grip and vertical travel.
class PageCurlRenderer {
    static constexpr float pi = 3.14159265358979323846f;
    struct Point { float x, y; };
#if SDL_VERSION_ATLEAST(2,0,18)
    struct SurfacePoint { SDL_Vertex vertex; float z, theta; };
    struct Face { std::array<SDL_Vertex, 3> vertices; float z; bool back; std::array<Uint8, 3> shine; };
#endif
    struct DrawState {
        SDL_Renderer *renderer;
        SDL_Rect clip{};
        SDL_bool clipped;
        SDL_BlendMode blend{};
        Uint8 r{}, g{}, b{}, a{};
        explicit DrawState(SDL_Renderer *value) : renderer(value), clipped(SDL_RenderIsClipEnabled(value)) {
            SDL_RenderGetClipRect(renderer, &clip);
            SDL_GetRenderDrawBlendMode(renderer, &blend);
            SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
        }
        ~DrawState() {
            SDL_RenderSetClipRect(renderer, clipped ? &clip : nullptr);
            SDL_SetRenderDrawBlendMode(renderer, blend);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
        }
    };

    struct Fold {
        float w, h, progress, anchor, travel;
        Point n{}, tangent{}, middle{};
        float radius, tMin, tMax, dMax, taper;
        int direction;

        Fold(float width, float height, float p, int dir, float grip, float dy)
            : w(width), h(height), progress(p), anchor(grip), travel(dy), direction(dir) {
            // A grip near a corner inclines the crease toward the other corner.
            // Vertical finger movement adds directly to this inclination.
            const float horizontal = std::max(.0001f, 2.f * w * p);
            const float lift=std::sin(pi*p);
            // Inclination also vanishes at rest: an infinitesimal drag must not
            // suddenly fold a large corner of a portrait page.
            const float vertical=h*(dy+(.5f-grip)*.72f)*lift*lift;
            const float length = std::hypot(horizontal, vertical);
            n = {horizontal / length, -vertical / length};
            // Keep the spine stable even for an almost vertical drag.
            n.x = std::max(.52f, n.x);
            const float normalLength = std::hypot(n.x, n.y);
            n.x /= normalLength; n.y /= normalLength;
            tangent = {-n.y, n.x};
            middle = {w * (1.f - p), h * grip + vertical * .5f};
            radius = w * .075f * std::sin(pi * p);
            taper = (.5f - grip) * .75f;
            tMin = 1e9f; tMax = -1e9f; dMax = -1e9f;
            for (Point q : {Point{0,0}, Point{w,0}, Point{w,h}, Point{0,h}}) {
                const float t = along(q), d = distance(q);
                tMin = std::min(tMin, t); tMax = std::max(tMax, t); dMax = std::max(dMax, d);
            }
        }
        float distance(Point p) const { return (p.x-middle.x)*n.x + (p.y-middle.y)*n.y; }
        float along(Point p) const { return (p.x-middle.x)*tangent.x + (p.y-middle.y)*tangent.y; }
        float localRadius(float t) const {
            return radius * (1.f + taper * (t - (tMin+tMax)*.5f) / std::max(1.f,tMax-tMin));
        }
        float contact(Point p) const { return distance(p) + pi*.5f*localRadius(along(p)); }
        Point source(float t, float d) const {
            return {middle.x + tangent.x*t + n.x*d, middle.y + tangent.y*t + n.y*d};
        }
#if SDL_VERSION_ATLEAST(2,0,18)
        SurfacePoint project(Point source, const SDL_Rect &page, bool back) const {
            const float d = distance(source), r = std::max(.00001f, localRadius(along(source)));
            const float theta = std::clamp((d+pi*.5f*r)/r, 0.f, pi);
            float projected = d, z = 0;
            if (d > -pi*.5f*r) {
                if (theta < pi) { projected = -pi*.5f*r + r*std::sin(theta); z = r*(1.f-std::cos(theta)); }
                else { projected = -d; z = 2.f*r; }
            }
            float x = source.x + (projected-d)*n.x;
            float y = source.y + (projected-d)*n.y;
            // Perspective from a stationary camera above the book.
            const float camera = std::max(w,h) * 3.8f;
            const float scale = camera / (camera-z);
            x = w*.5f + (x-w*.5f)*scale;
            y = h*.5f + (y-h*.5f)*scale;
            const float sign = back ? -1.f : 1.f;
            const float nx = n.x*std::sin(theta)*sign;
            const float ny = n.y*std::sin(theta)*sign;
            const float nz = std::cos(theta)*sign;
            const float diffuse = std::max(0.f, -.32f*nx-.28f*ny+.90f*nz);
            const float light = back ? .78f+.20f*diffuse : .70f+.30f*diffuse;
            const Uint8 shade = Uint8(std::clamp(light,0.f,1.f)*255);
            SDL_Vertex v{};
            v.position = {page.x+(direction>0?x:w-x), page.y+y};
            v.tex_coord = {std::clamp((direction>0?source.x:w-source.x)/w,0.f,1.f), std::clamp(source.y/h,0.f,1.f)};
            v.color = {shade,shade,shade,255};
            return {v,z,theta};
        }
#endif
    };

    // Clip in source space before bending, so every triangle stays on the sheet.
    template<class Distance>
    static int clipPolygon(const Point *input, int count, Point *output, Distance distance) {
        int out = 0;
        if (!count) return 0;
        Point a = input[count-1]; float da = distance(a);
        for (int i=0;i<count;++i) {
            const Point b=input[i]; const float db=distance(b);
            if ((da<=0)!=(db<=0)) {
                const float k=da/(da-db);
                output[out++]={a.x+(b.x-a.x)*k,a.y+(b.y-a.y)*k};
            }
            if (db<=0) output[out++]=b;
            a=b; da=db;
        }
        return out;
    }

#if SDL_VERSION_ATLEAST(2,0,18) && !defined(PAPERNX_CURL_FORCE_FALLBACK)
    static bool geometry(SDL_Renderer *renderer, SDL_Texture *front, SDL_Texture *back,
                         const SDL_Rect &page, const Fold &fold) {
        // Buffers retain their capacity across frames; SDL copies queued vertices.
        static thread_local std::vector<Face> faces;
        static thread_local std::vector<SDL_Vertex> batch, highlights, shadows;
        faces.clear(); batch.clear(); highlights.clear(); shadows.clear();
        constexpr int rows=32, curveColumns=32, tailColumns=8;
        faces.reserve(rows*(curveColumns+tailColumns)*3);
        auto addFace = [&](Point a, Point b, Point c, bool underside) {
            const auto v0=fold.project(a,page,underside), v1=fold.project(b,page,underside), v2=fold.project(c,page,underside);
            Face face{{v0.vertex,v1.vertex,v2.vertex},(v0.z+v1.z+v2.z)/3.f,underside,{}};
            const std::array<SurfacePoint,3> points{v0,v1,v2};
            for(int i=0;i<3;++i) {
                const float rim=std::exp(-std::pow((points[i].theta-pi*.52f)*5.f,2.f));
                face.shine[i]=Uint8(34.f*rim*std::sin(pi*fold.progress));
            }
            faces.push_back(face);
        };
        for(int row=0;row<rows;++row) {
            const float ta=fold.tMin+(fold.tMax-fold.tMin)*float(row)/rows;
            const float tb=fold.tMin+(fold.tMax-fold.tMin)*float(row+1)/rows;
            auto distanceAt = [&](float t,int col) {
                const float r=fold.localRadius(t);
                if(col<=curveColumns) return -pi*.5f*r + pi*r*float(col)/curveColumns;
                return pi*.5f*r + std::max(0.f,fold.dMax-pi*.5f*r)*float(col-curveColumns)/tailColumns;
            };
            for(int col=0;col<curveColumns+tailColumns;++col) {
                Point a[12]{fold.source(ta,distanceAt(ta,col)),fold.source(tb,distanceAt(tb,col)),
                            fold.source(tb,distanceAt(tb,col+1)),fold.source(ta,distanceAt(ta,col+1))};
                Point b[12]{};
                int count=clipPolygon(a,4,b,[](Point p){return -p.x;});
                count=clipPolygon(b,count,a,[&](Point p){return p.x-fold.w;});
                count=clipPolygon(a,count,b,[](Point p){return -p.y;});
                count=clipPolygon(b,count,a,[&](Point p){return p.y-fold.h;});
                for(int i=1;i+1<count;++i) addFace(a[0],a[i],a[i+1],col>=curveColumns/2);
            }
        }
        std::stable_sort(faces.begin(),faces.end(),[](const Face &a,const Face &b){return a.z<b.z;});
        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
        // Four translucent footprints produce contact shadow and a soft penumbra.
        for(int pass=3;pass>=0;--pass) {
            shadows.clear();
            for(const auto &face:faces) {
                for(auto v:face.vertices) {
                    const float side=fold.direction>0?1.f:-1.f;
                    v.position.x+=side*(face.z*.30f+float(pass)*2.5f*std::sin(pi*fold.progress));
                    v.position.y+=face.z*.12f;
                    v.color={4,3,2,Uint8(pass==0?17:5)};
                    shadows.push_back(v);
                }
            }
            if(!shadows.empty() && SDL_RenderGeometry(renderer,nullptr,shadows.data(),int(shadows.size()),nullptr,0)!=0) return false;
        }
        // One untessellated flat polygon preserves perfectly sharp text.
        Point original[12]{{0,0},{fold.w,0},{fold.w,fold.h},{0,fold.h}}, flat[12]{};
        const int flatCount=clipPolygon(original,4,flat,[&](Point p){return fold.contact(p);});
        for(int i=1;i+1<flatCount;++i) {
            for(Point p:{flat[0],flat[i],flat[i+1]}) {
                auto v=fold.project(p,page,false).vertex;
                v.color={255,255,255,255}; batch.push_back(v);
            }
        }
        if(!batch.empty() && SDL_RenderGeometry(renderer,front,batch.data(),int(batch.size()),nullptr,0)!=0) return false;
        batch.clear();
        bool side=false;
        auto flush = [&]() {
            if(batch.empty()) return true;
            if(SDL_RenderGeometry(renderer,side && back?back:front,batch.data(),int(batch.size()),nullptr,0)!=0) return false;
            if(SDL_RenderGeometry(renderer,nullptr,highlights.data(),int(highlights.size()),nullptr,0)!=0) return false;
            batch.clear(); highlights.clear(); return true;
        };
        for(const auto &face:faces) {
            if(face.back!=side) {if(!flush()) return false;side=face.back;}
            for(int i=0;i<3;++i) {
                batch.push_back(face.vertices[i]);
                auto light=face.vertices[i]; light.color={255,250,235,face.shine[i]}; highlights.push_back(light);
            }
        }
        return flush();
    }
#endif

public:
    // progress is geometric (not time-eased): holding a finger holds the sheet.
    // anchorY and dragY use page-height units and affect crease inclination.
    static void render(SDL_Renderer *renderer, SDL_Texture *previous, SDL_Texture *next,
                       const SDL_Rect &page, float progress, int direction,
                       float anchorY=.78f, float dragY=0.f, [[maybe_unused]] SDL_Texture *back=nullptr,
                       const SDL_Rect *nextPage=nullptr) {
        if(!renderer || !next || page.w<=0 || page.h<=0) return;
        progress=std::clamp(progress,0.f,1.f);
        const SDL_Rect &nextRect=nextPage?*nextPage:page;
        if(!previous || progress>=.99999f) {SDL_RenderCopy(renderer,next,nullptr,&nextRect);return;}
        if(progress<=.00001f) {SDL_RenderCopy(renderer,previous,nullptr,&page);return;}
        DrawState state(renderer);
        if(!nextPage) {
            SDL_Rect clip=page;
            if(state.clipped) SDL_IntersectRect(&page,&state.clip,&clip);
            SDL_RenderSetClipRect(renderer,&clip);
        }
        SDL_RenderCopy(renderer,next,nullptr,&nextRect);
        Fold fold(float(page.w),float(page.h),progress,direction,std::clamp(anchorY,0.f,1.f),
                  std::clamp(dragY,-.5f,.5f));
#if SDL_VERSION_ATLEAST(2,0,18) && !defined(PAPERNX_CURL_FORCE_FALLBACK)
        if(geometry(renderer,previous,back,page,fold)) return;
#endif
        // Old/backends without geometry retain a proportional, correctly scaled reveal.
        SDL_RenderCopy(renderer,next,nullptr,&nextRect);
        int sw=0,sh=0;
        if(SDL_QueryTexture(previous,nullptr,nullptr,&sw,&sh)!=0) return;
        const float fraction=1.f-progress;
        const int visible=std::max(1,int(page.w*fraction)), sourceWidth=std::max(1,int(sw*fraction));
        SDL_Rect source{direction>=0?0:sw-sourceWidth,0,sourceWidth,sh};
        SDL_Rect destination{page.x+(direction>=0?0:page.w-visible),page.y,visible,page.h};
        SDL_RenderCopy(renderer,previous,&source,&destination);
    }
};

} // namespace papernx
