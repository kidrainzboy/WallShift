#include "branding.hpp"
#include "resource.h"

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

MenuBranding::~MenuBranding() {
    if (bitmap_) DeleteObject(bitmap_);
}

bool MenuBranding::append(HMENU menu, UINT itemId, IWICImagingFactory* factory,
                          HINSTANCE instance, UINT dpi) {
    const auto resource = FindResourceW(instance, MAKEINTRESOURCEW(IDR_MENU_LOGO), RT_RCDATA);
    if (!resource || !factory) return false;
    const auto bytes = static_cast<BYTE*>(LockResource(LoadResource(instance, resource)));
    const auto length = SizeofResource(instance, resource);
    if (!bytes || !length) return false;

    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICBitmapScaler> scaler;
    ComPtr<IWICFormatConverter> converter;
    HRESULT hr = factory->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromMemory(bytes, length);
    if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromStream(stream.Get(), nullptr,
                                                            WICDecodeMetadataCacheOnLoad, &decoder);
    if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
    UINT originalWidth{}, originalHeight{};
    if (SUCCEEDED(hr)) hr = frame->GetSize(&originalWidth, &originalHeight);
    if (FAILED(hr) || !originalWidth || !originalHeight) return false;

    width_ = MulDiv(142, dpi, USER_DEFAULT_SCREEN_DPI);
    height_ = MulDiv(originalHeight, width_, originalWidth);
    padding_ = MulDiv(8, dpi, USER_DEFAULT_SCREEN_DPI);
    hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr)) hr = scaler->Initialize(frame.Get(), width_, height_, WICBitmapInterpolationModeFant);
    if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) hr = converter->Initialize(scaler.Get(), GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = static_cast<LONG>(width_);
    info.bmiHeader.biHeight = -static_cast<LONG>(height_);
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels{};
    bitmap_ = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap_) return false;
    constexpr UINT bytesPerPixel = 4;
    const UINT stride = width_ * bytesPerPixel;
    hr = converter->CopyPixels(nullptr, stride, stride * height_, static_cast<BYTE*>(pixels));
    if (FAILED(hr)) return false;

    wchar_t title[] = L"WallShift";
    MENUITEMINFOW item{sizeof(item)};
    item.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_DATA | MIIM_STRING;
    item.fType = MFT_OWNERDRAW;
    item.fState = MFS_DISABLED;
    item.wID = itemId;
    item.dwItemData = reinterpret_cast<ULONG_PTR>(this);
    item.dwTypeData = title;
    return InsertMenuItemW(menu, 0, TRUE, &item) != FALSE;
}

void MenuBranding::measure(MEASUREITEMSTRUCT& item) const {
    item.itemWidth = width_ + 2 * padding_;
    item.itemHeight = height_ + 2 * padding_;
}

void MenuBranding::draw(const DRAWITEMSTRUCT& item) const {
    const HDC target = item.hDC;
    const auto& bounds = item.rcItem;
    FillRect(target, &bounds, GetSysColorBrush(COLOR_MENU));
    const auto source = CreateCompatibleDC(target);
    if (!source) return;
    const auto previous = SelectObject(source, bitmap_);
    const int x = bounds.left + (bounds.right - bounds.left - static_cast<int>(width_)) / 2;
    const int y = bounds.top + static_cast<int>(padding_);
    const BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    AlphaBlend(target, x, y, width_, height_, source, 0, 0, width_, height_, blend);
    SelectObject(source, previous);
    DeleteDC(source);
}
