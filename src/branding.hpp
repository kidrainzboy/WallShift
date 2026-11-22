#pragma once

#include <windows.h>
#include <wincodec.h>

class MenuBranding {
public:
    MenuBranding() = default;
    ~MenuBranding();
    MenuBranding(const MenuBranding&) = delete;
    MenuBranding& operator=(const MenuBranding&) = delete;

    bool append(HMENU menu, UINT itemId, IWICImagingFactory* factory, HINSTANCE instance, UINT dpi);
    void measure(MEASUREITEMSTRUCT& item) const;
    void draw(const DRAWITEMSTRUCT& item) const;

private:
    UINT width_{};
    UINT height_{};
    UINT padding_{};
    HBITMAP bitmap_{};
};
