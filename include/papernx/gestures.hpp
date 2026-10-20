#pragma once
#include <chrono>
#include <cstdint>

namespace papernx {

enum class GestureType {
    None,
    TapLeft,
    TapCenter,
    TapRight,
    SwipeLeft,
    SwipeRight,
    SwipeUp,
    SwipeDown,
    PinchZoom
};

struct GestureResult {
    GestureType type{GestureType::None};
    float tapX{0.0f};
    float tapY{0.0f};
    float deltaX{0.0f};
    float deltaY{0.0f};
    float pinchScale{1.0f};
};

class TouchGestureTracker {
public:
    TouchGestureTracker();
    void reset();

    void onFingerDown(float x, float y, std::int64_t fingerId);
    void onFingerMotion(float x, float y, std::int64_t fingerId);
    GestureResult onFingerUp(float x, float y, std::int64_t fingerId);

private:
    float startX_{0.0f};
    float startY_{0.0f};
    float currentX_{0.0f};
    float currentY_{0.0f};
    std::chrono::steady_clock::time_point startTime_;
    std::int64_t trackingFinger_{-1};
    bool active_{false};
};

class ControllerNavigator {
public:
    ControllerNavigator();
    bool processAxis(int axis, int value, int &outDeltaX, int &outDeltaY);

private:
    std::chrono::steady_clock::time_point lastTickX_;
    std::chrono::steady_clock::time_point lastTickY_;
    std::chrono::steady_clock::time_point lastTickRightX_;
    std::chrono::steady_clock::time_point lastTickRightY_;
};

} // namespace papernx
