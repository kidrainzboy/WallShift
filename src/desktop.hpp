#pragma once

#include <windows.h>
#include <shobjidl.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

struct Monitor {
    std::wstring id;
    RECT bounds{};
    fs::path image;
};

struct DesktopState {
    std::vector<Monitor> monitors;
    ComPtr<IShellItemArray> slideshow;
    DESKTOP_SLIDESHOW_OPTIONS slideshowOptions{};
    UINT slideshowInterval{};
    DESKTOP_WALLPAPER_POSITION position{DWPOS_FILL};
    COLORREF color{};
    bool enabled{};
};

struct DesktopHost {
    HWND parent{};
    HWND icons{};
    bool layered{};
};

DesktopHost findDesktopHost(bool forVideo);
std::vector<Monitor> getMonitors(IDesktopWallpaper* desktop);
HRESULT captureDesktop(IDesktopWallpaper* desktop, DesktopState& state);
HRESULT restoreDesktop(IDesktopWallpaper* desktop, const DesktopState& state);
bool isImage(const fs::path& path);
bool isVideo(const fs::path& path);
HRESULT openImage(IWICImagingFactory* wic, const fs::path& path,
                  ComPtr<IWICBitmapFrameDecode>& frame);
HRESULT savePng(IWICImagingFactory* wic, IWICBitmapSource* source, const fs::path& path);
fs::path temporaryPng(const fs::path& directory);
