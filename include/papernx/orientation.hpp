#pragma once
#include <utility>
namespace papernx {
// Pixel-edge coordinates: x in [0,width], y in [0,height]. The renderer and
// pointer input use this same transform so rotated hit regions remain exact.
struct DisplayTransform {
    int physicalWidth{1280}, physicalHeight{720}, orientation{0};
    int logicalWidth() const { return orientation ? physicalHeight : physicalWidth; }
    int logicalHeight() const { return orientation ? physicalWidth : physicalHeight; }
    // Apply the orientation once to a controller direction (translation has no effect).
    std::pair<int,int> vectorToLogical(int x,int y) const {
        if(orientation==1) return {y,-x};
        if(orientation==2) return {-y,x};
        return {x,y};
    }
    std::pair<float,float> toLogical(float x,float y) const {
        if(orientation==1) return {y,float(physicalWidth)-x};
        if(orientation==2) return {float(physicalHeight)-y,x};
        return {x,y};
    }
    std::pair<float,float> toPhysical(float x,float y) const {
        if(orientation==1) return {float(physicalWidth)-y,x};
        if(orientation==2) return {y,float(physicalHeight)-x};
        return {x,y};
    }
};
}
