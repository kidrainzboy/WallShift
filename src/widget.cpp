#include "widget.hpp"

#include <shellscalingapi.h>
#include <gdiplus.h>
#include <algorithm>

namespace {
constexpr float designSide = 64.0f;
using namespace Gdiplus;

void widgetOutline(GraphicsPath& path) {
    path.AddBezier(3, 32, 3, 16, 16, 3, 32, 3);
    path.AddBezier(32, 3, 48, 3, 61, 16, 61, 32);
    path.AddLine(61, 32, 61, 51);
    path.AddBezier(61, 51, 61, 57, 57, 61, 51, 61);
    path.AddLine(51, 61, 32, 61);
    path.AddBezier(32, 61, 16, 61, 3, 48, 3, 32);
    path.CloseFigure();
}

Color systemColor(int index) {
    const auto rgb = GetSysColor(index);
    return Color(255, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
}

BYTE widgetOpacity(const WidgetVisual& visual) {
    if (visual.highContrast) return 255;
    return visual.acceptingDrop || visual.focused ? 232 : 206;
}

void drawDropGlyph(Graphics& context, Color ink, Color accent) {
    GraphicsPath tray;
    tray.AddLine(18.5f, 34.5f, 18.5f, 41.3f);
    tray.AddBezier(18.5f, 41.3f, 18.5f, 43.8f, 20.2f, 45.5f, 22.7f, 45.5f);
    tray.AddLine(22.7f, 45.5f, 41.3f, 45.5f);
    tray.AddBezier(41.3f, 45.5f, 43.8f, 45.5f, 45.5f, 43.8f, 45.5f, 41.3f);
    tray.AddLine(45.5f, 41.3f, 45.5f, 34.5f);
    Pen trayStroke(ink, 2.6f);
    trayStroke.SetStartCap(LineCapRound);
    trayStroke.SetEndCap(LineCapRound);
    context.DrawPath(&trayStroke, &tray);

    GraphicsPath tab;
    tab.AddBezier(27.0f, 18.5f, 25.6f, 18.5f, 24.8f, 19.3f, 24.8f, 20.7f);
    tab.AddLine(24.8f, 20.7f, 24.8f, 30.4f);
    tab.AddLine(24.8f, 30.4f, 32.0f, 37.2f);
    tab.AddLine(32.0f, 37.2f, 39.2f, 30.4f);
    tab.AddLine(39.2f, 30.4f, 39.2f, 20.7f);
    tab.AddBezier(39.2f, 20.7f, 39.2f, 19.3f, 38.4f, 18.5f, 37.0f, 18.5f);
    tab.CloseFigure();
    SolidBrush fill(accent);
    context.FillPath(&fill, &tab);
}

void drawWallpaperGlyph(Graphics& context, Color ink, Color accent) {
    Pen arrowStroke(accent, 1.65f);
    arrowStroke.SetStartCap(LineCapRound);
    context.DrawArc(&arrowStroke, RectF(12.8f, 13.4f, 37.4f, 37.4f), 220.0f, 180.0f);
    const PointF arrowHead[]{{43.4f, 46.1f}, {44.6f, 40.6f}, {48.5f, 44.8f}};
    SolidBrush arrowFill(accent);
    context.FillPolygon(&arrowFill, arrowHead, ARRAYSIZE(arrowHead));

    GraphicsPath landscape;
    landscape.AddBezier(16.5f, 37.3f, 20.4f, 34.3f, 23.7f, 30.2f, 26.1f, 30.2f);
    landscape.AddBezier(26.1f, 30.2f, 28.9f, 30.2f, 30.6f, 34.6f, 32.7f, 34.6f);
    landscape.AddBezier(32.7f, 34.6f, 34.7f, 34.6f, 35.8f, 31.6f, 37.6f, 31.6f);
    landscape.AddBezier(37.6f, 31.6f, 40.2f, 31.6f, 42.3f, 35.0f, 45.0f, 37.3f);
    Pen contour(ink, 1.4f);
    contour.SetStartCap(LineCapRound);
    contour.SetEndCap(LineCapRound);
    contour.SetLineJoin(LineJoinRound);
    context.DrawPath(&contour, &landscape);
    SolidBrush sun(ink);
    context.FillEllipse(&sun, 35.2f, 23.9f, 4.25f, 4.25f);
}

void renderWidget(Graphics& context, int side, WidgetVisual visual) {
    context.Clear(Color(0, 0, 0, 0));
    context.SetSmoothingMode(SmoothingModeAntiAlias);
    context.SetPixelOffsetMode(PixelOffsetModeHalf);
    const auto scale = static_cast<float>(side) / designSide;
    context.ScaleTransform(scale, scale);

    const Color ink = visual.highContrast ? systemColor(COLOR_WINDOWTEXT) : Color(48, 53, 60);
    const Color accent = visual.highContrast ? systemColor(COLOR_HIGHLIGHT) : Color(108, 149, 204);
    const Color rim = visual.warning && !visual.highContrast ? Color(148, 81, 32) :
        visual.focused || visual.acceptingDrop ? accent :
        visual.highContrast ? ink : Color(211, 247, 251, 255);
    const RectF faceBounds(3, 3, 58, 58);
    GraphicsPath face;
    widgetOutline(face);
    if (visual.highContrast) {
        SolidBrush fill(systemColor(COLOR_WINDOW));
        context.FillPath(&fill, &face);
    } else {
        const Color upper = Color(251, 253, 255);
        const Color lower = visual.acceptingDrop ? Color(188, 214, 246) : Color(200, 213, 231);
        LinearGradientBrush glass(faceBounds, upper, lower, LinearGradientModeForwardDiagonal);
        context.FillPath(&glass, &face);

        GraphicsPath glint;
        glint.AddEllipse(RectF(5, 4, 37, 26));
        PathGradientBrush gloss(&glint);
        gloss.SetCenterPoint(PointF(17, 10));
        gloss.SetCenterColor(Color(225, 255, 255, 255));
        const Color outerGlint(0, 255, 255, 255);
        INT colorCount = 1;
        gloss.SetSurroundColors(&outerGlint, &colorCount);
        const auto clipped = context.Save();
        context.SetClip(&face);
        context.FillPath(&gloss, &glint);
        context.Restore(clipped);

        GraphicsPath reflection;
        widgetOutline(reflection);
        Matrix inset(0.96f, 0, 0, 0.96f, 1.28f, 1.28f);
        reflection.Transform(&inset);
        LinearGradientBrush highlight(faceBounds, Color(217, 255, 255, 255), Color(86, 249, 253, 255),
                                      LinearGradientModeVertical);
        Pen sheen(&highlight, 0.95f);
        context.DrawPath(&sheen, &reflection);
    }
    Pen faceBorder(rim, visual.focused || visual.acceptingDrop || visual.warning ? 1.9f : 1.0f);
    context.DrawPath(&faceBorder, &face);

    if (visual.acceptingDrop) {
        drawDropGlyph(context, ink, visual.highContrast ? ink : Color(75, 121, 181));
    } else {
        drawWallpaperGlyph(context, ink, accent);
    }

    Pen grip(ink, 1.5f);
    grip.SetStartCap(LineCapRound);
    grip.SetEndCap(LineCapRound);
    context.DrawLine(&grip, 47.0f, 55.5f, 55.5f, 47.0f);
    context.DrawLine(&grip, 52.0f, 55.5f, 55.5f, 52.0f);
}

void renderWidgetSurface(Bitmap& image, int side, WidgetVisual visual) {
    // Supersample against transparent black so antialiasing covers the outside
    // edge as well as the strokes inside the widget.
    Bitmap highResolution(side * 2, side * 2, PixelFormat32bppPARGB);
    {
        Graphics drawing(&highResolution);
        renderWidget(drawing, side * 2, visual);
    }
    Graphics output(&image);
    output.SetCompositingMode(CompositingModeSourceCopy);
    output.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    output.SetPixelOffsetMode(PixelOffsetModeHalf);
    output.DrawImage(&highResolution, 0, 0, side, side);
}
}

WidgetDisplay widgetDisplay(HMONITOR monitor) {
    WidgetDisplay result{{}, USER_DEFAULT_SCREEN_DPI};
    MONITORINFO info{sizeof(info)};
    if (GetMonitorInfoW(monitor, &info)) {
        result.workArea = info.rcWork;
        UINT dpiX{}, dpiY{};
        if (SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && dpiX) {
            result.dpi = dpiX;
        }
    } else {
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &result.workArea, 0);
    }
    return result;
}

RECT widgetBounds(const WidgetDisplay& display, POINT origin, int logicalSize) {
    const auto& work = display.workArea;
    const auto requested = static_cast<LONG>(MulDiv(
        std::clamp(logicalSize, widgetMinSize, widgetMaxSize), display.dpi, USER_DEFAULT_SCREEN_DPI));
    const LONG side = std::max(1L, std::min({requested, work.right - work.left, work.bottom - work.top}));
    const LONG x = std::clamp(origin.x, work.left, std::max(work.left, work.right - side));
    const LONG y = std::clamp(origin.y, work.top, std::max(work.top, work.bottom - side));
    return {x, y, x + side, y + side};
}

bool widgetResizeGrip(RECT bounds, POINT pointer) {
    const LONG grip = (bounds.right - bounds.left) * 18 / 64;
    return PtInRect(&bounds, pointer) && pointer.x >= bounds.right - grip &&
           pointer.y >= bounds.bottom - grip;
}

WidgetSurface::~WidgetSurface() {
    if (graphics_) GdiplusShutdown(graphics_);
}

bool WidgetSurface::present(HWND window, const RECT& bounds, WidgetVisual visual) {
    const auto side = bounds.right - bounds.left;
    if (side <= 0) return false;
    if (side_ == side && visual_ == visual) {
        RECT current{};
        if (GetWindowRect(window, &current) && EqualRect(&bounds, &current)) return true;
        return SetWindowPos(window, nullptr, bounds.left, bounds.top, 0, 0,
            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
    }
    if (!graphics_) {
        const GdiplusStartupInput startup;
        if (GdiplusStartup(&graphics_, &startup, nullptr) != Ok) return false;
    }
    const HDC buffer = CreateCompatibleDC(nullptr);
    if (!buffer) return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = side;
    info.bmiHeader.biHeight = -side;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels{};
    const HBITMAP bitmap = CreateDIBSection(buffer, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap) {
        DeleteDC(buffer);
        return false;
    }
    {
        Bitmap image(side, side, side * 4, PixelFormat32bppPARGB, static_cast<BYTE*>(pixels));
        renderWidgetSurface(image, side, visual);
    }
    const auto previous = SelectObject(buffer, bitmap);
    POINT destination{bounds.left, bounds.top};
    POINT source{};
    SIZE size{side, side};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, widgetOpacity(visual), AC_SRC_ALPHA};
    const auto updated = UpdateLayeredWindow(window, nullptr, &destination, &size, buffer,
                                              &source, 0, &blend, ULW_ALPHA);
    SelectObject(buffer, previous);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    if (updated) {
        side_ = side;
        visual_ = visual;
    }
    return updated != FALSE;
}
