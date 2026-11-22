#pragma once

#include <windows.h>

inline constexpr int widgetMinSize = 48;
inline constexpr int widgetDefaultSize = 64;
inline constexpr int widgetMaxSize = 128;

struct WidgetDisplay {
    RECT workArea;
    UINT dpi;
};

WidgetDisplay widgetDisplay(HMONITOR monitor);
RECT widgetBounds(const WidgetDisplay& display, POINT origin, int size);
bool widgetResizeGrip(RECT bounds, POINT pointer);

struct WidgetVisual {
    bool focused{};
    bool acceptingDrop{};
    bool warning{};
    bool highContrast{};
    bool operator==(const WidgetVisual&) const = default;
};

class WidgetSurface {
public:
    ~WidgetSurface();
    bool present(HWND window, const RECT& bounds, WidgetVisual visual);
    void invalidate() { side_ = 0; }

private:
    ULONG_PTR graphics_{};
    int side_{};
    WidgetVisual visual_{};
};
