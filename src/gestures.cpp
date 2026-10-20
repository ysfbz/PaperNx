#include "papernx/gestures.hpp"
#include <cmath>

#ifndef SDL_CONTROLLER_AXIS_LEFTX
#define SDL_CONTROLLER_AXIS_LEFTX 0
#endif
#ifndef SDL_CONTROLLER_AXIS_LEFTY
#define SDL_CONTROLLER_AXIS_LEFTY 1
#endif
#ifndef SDL_CONTROLLER_AXIS_RIGHTX
#define SDL_CONTROLLER_AXIS_RIGHTX 2
#endif
#ifndef SDL_CONTROLLER_AXIS_RIGHTY
#define SDL_CONTROLLER_AXIS_RIGHTY 3
#endif

namespace papernx {

TouchGestureTracker::TouchGestureTracker() {
    reset();
}

void TouchGestureTracker::reset() {
    active_ = false;
    trackingFinger_ = -1;
    startX_ = 0.0f;
    startY_ = 0.0f;
    currentX_ = 0.0f;
    currentY_ = 0.0f;
}

void TouchGestureTracker::onFingerDown(float x, float y, std::int64_t fingerId) {
    if (!active_ || trackingFinger_ == fingerId) {
        active_ = true;
        trackingFinger_ = fingerId;
        startX_ = x;
        startY_ = y;
        currentX_ = x;
        currentY_ = y;
        startTime_ = std::chrono::steady_clock::now();
    }
}

void TouchGestureTracker::onFingerMotion(float x, float y, std::int64_t fingerId) {
    if (active_ && trackingFinger_ == fingerId) {
        currentX_ = x;
        currentY_ = y;
    }
}

GestureResult TouchGestureTracker::onFingerUp(float x, float y, std::int64_t fingerId) {
    GestureResult result;
    if (!active_ || trackingFinger_ != fingerId) {
        return result;
    }

    currentX_ = x;
    currentY_ = y;


    float dx = currentX_ - startX_;
    float dy = currentY_ - startY_;
    float dist = std::hypotf(dx, dy);

    result.tapX = x;
    result.tapY = y;
    result.deltaX = dx;
    result.deltaY = dy;

    // A swipe must have sufficient distance, horizontal dominance, and no speed requirement
    if (dist > 0.06f) {
        if (std::abs(dx) > std::abs(dy) * 1.3f) {
            result.type = (dx < 0.0f) ? GestureType::SwipeLeft : GestureType::SwipeRight;
        } else if (std::abs(dy) > std::abs(dx) * 1.3f) {
            result.type = (dy < 0.0f) ? GestureType::SwipeUp : GestureType::SwipeDown;
        }
    } else if (dist < 0.05f) {
        // Tap gesture: classify by reading touch zones
        if (x < 0.30f) {
            result.type = GestureType::TapLeft;
        } else if (x > 0.70f) {
            result.type = GestureType::TapRight;
        } else {
            result.type = GestureType::TapCenter;
        }
    }

    reset();
    return result;
}

ControllerNavigator::ControllerNavigator() {
    auto now = std::chrono::steady_clock::now();
    lastTickX_ = now - std::chrono::milliseconds(220);
    lastTickY_ = now - std::chrono::milliseconds(220);
    lastTickRightX_ = lastTickX_;
    lastTickRightY_ = lastTickY_;
}

bool ControllerNavigator::processAxis(int axis, int value, int &outDeltaX, int &outDeltaY) {
    outDeltaX = 0;
    outDeltaY = 0;
    constexpr int deadzone = 12000;
    constexpr int repeatMs = 220;
    std::chrono::steady_clock::time_point *last = nullptr;
    bool horizontal = false;
    switch(axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: last=&lastTickX_; horizontal=true; break;
        case SDL_CONTROLLER_AXIS_LEFTY: last=&lastTickY_; break;
        case SDL_CONTROLLER_AXIS_RIGHTX: last=&lastTickRightX_; horizontal=true; break;
        case SDL_CONTROLLER_AXIS_RIGHTY: last=&lastTickRightY_; break;
        default: return false; // Ignore triggers and unsupported axes.
    }
    const auto now = std::chrono::steady_clock::now();
    if(value >= -deadzone && value <= deadzone) {
        *last = now - std::chrono::milliseconds(repeatMs);
        return false;
    }
    if (now - *last < std::chrono::milliseconds(repeatMs)) return false;
    (horizontal ? outDeltaX : outDeltaY) = value > 0 ? 1 : -1;
    *last = now;
    return true;
}

} // namespace papernx
