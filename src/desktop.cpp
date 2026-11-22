#include "desktop.hpp"

#include <objbase.h>
#include <cwchar>

namespace {
DesktopHost findIcons() {
    const HWND progman = FindWindowW(L"Progman", nullptr);
    if (HWND icons = FindWindowExW(progman, nullptr, L"SHELLDLL_DefView", nullptr);
        progman && icons) {
        const bool layered = (GetWindowLongPtrW(progman, GWL_EXSTYLE) &
                              WS_EX_NOREDIRECTIONBITMAP) != 0;
        return {progman, icons, layered};
    }

    DesktopHost host;
    EnumWindows([](HWND parent, LPARAM data) -> BOOL {
        HWND icons = FindWindowExW(parent, nullptr, L"SHELLDLL_DefView", nullptr);
        if (!icons) return TRUE;
        *reinterpret_cast<DesktopHost*>(data) = {parent, icons, false};
        return FALSE;
    }, reinterpret_cast<LPARAM>(&host));
    return host;
}
}

DesktopHost findDesktopHost(bool forVideo) {
    if (!forVideo) return findIcons();
    const HWND progman = FindWindowW(L"Progman", nullptr);
    if (!progman) return {};

    // Explorer's desktop composition is private. Keep this out of the image path.
    constexpr UINT raiseDesktop = 0x052C;
    SendMessageTimeoutW(progman, raiseDesktop, 0xD, 0, SMTO_ABORTIFHUNG, 500, nullptr);
    SendMessageTimeoutW(progman, raiseDesktop, 0xD, 1, SMTO_ABORTIFHUNG, 500, nullptr);
    const auto host = findIcons();
    if (host.layered) return host;
    if (!host.parent) return {};

    HWND worker = FindWindowExW(nullptr, host.parent, L"WorkerW", nullptr);
    return worker ? DesktopHost{worker, nullptr, false} : DesktopHost{};
}

std::vector<Monitor> getMonitors(IDesktopWallpaper* desktop) {
    std::vector<Monitor> monitors;
    UINT count = 0;
    if (FAILED(desktop->GetMonitorDevicePathCount(&count))) return monitors;
    for (UINT i = 0; i < count; ++i) {
        PWSTR id = nullptr;
        if (FAILED(desktop->GetMonitorDevicePathAt(i, &id)) || !id) continue;
        Monitor monitor;
        monitor.id = id;
        CoTaskMemFree(id);
        if (desktop->GetMonitorRECT(monitor.id.c_str(), &monitor.bounds) == S_OK) {
            monitors.push_back(std::move(monitor));
        }
    }
    return monitors;
}

HRESULT captureDesktop(IDesktopWallpaper* desktop, DesktopState& state) {
    state = {};
    HRESULT hr = desktop->GetPosition(&state.position);
    if (FAILED(hr)) return hr;
    hr = desktop->GetBackgroundColor(&state.color);
    if (FAILED(hr)) return hr;
    DESKTOP_SLIDESHOW_STATE status{};
    hr = desktop->GetStatus(&status);
    if (FAILED(hr)) return hr;
    state.enabled = (status & DSS_ENABLED) != 0;
    if ((status & DSS_SLIDESHOW) != 0) {
        hr = desktop->GetSlideshow(&state.slideshow);
        if (FAILED(hr)) return hr;
        if (state.slideshow) {
            hr = desktop->GetSlideshowOptions(&state.slideshowOptions, &state.slideshowInterval);
            if (FAILED(hr)) return hr;
        }
    }
    state.monitors = getMonitors(desktop);
    if (state.monitors.empty()) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    for (auto& monitor : state.monitors) {
        PWSTR image = nullptr;
        hr = desktop->GetWallpaper(monitor.id.c_str(), &image);
        if (SUCCEEDED(hr) && image) monitor.image = image;
        CoTaskMemFree(image);
        if (FAILED(hr)) return hr;
    }
    return S_OK;
}

HRESULT restoreDesktop(IDesktopWallpaper* desktop, const DesktopState& state) {
    HRESULT result = S_OK;
    const auto rememberError = [&result](HRESULT hr) {
        if (FAILED(hr) && SUCCEEDED(result)) result = hr;
    };
    rememberError(desktop->SetBackgroundColor(state.color));
    rememberError(desktop->SetPosition(state.position));
    if (state.slideshow) {
        const HRESULT hr = desktop->SetSlideshow(state.slideshow.Get());
        rememberError(hr);
        if (SUCCEEDED(hr)) {
            rememberError(desktop->SetSlideshowOptions(state.slideshowOptions,
                                                       state.slideshowInterval));
        }
    } else if (state.position == DWPOS_SPAN && !state.monitors.empty() &&
               !state.monitors.front().image.empty()) {
        rememberError(desktop->SetWallpaper(nullptr, state.monitors.front().image.c_str()));
    } else {
        const auto connected = getMonitors(desktop);
        for (const auto& monitor : state.monitors) {
            for (const auto& current : connected) {
                if (monitor.id != current.id) continue;
                const auto hr = desktop->SetWallpaper(monitor.id.c_str(), monitor.image.c_str());
                // Some Windows configurations reject an empty path. A disabled background
                // is still restored below even when it has no saved wallpaper.
                if (state.enabled || !monitor.image.empty()) rememberError(hr);
                break;
            }
        }
    }
    rememberError(desktop->Enable(state.enabled));
    return result;
}

bool isImage(const fs::path& path) {
    const auto extension = path.extension().wstring();
    return _wcsicmp(extension.c_str(), L".png") == 0 ||
           _wcsicmp(extension.c_str(), L".jpg") == 0 ||
           _wcsicmp(extension.c_str(), L".jpeg") == 0;
}

bool isVideo(const fs::path& path) {
    return _wcsicmp(path.extension().c_str(), L".mp4") == 0;
}

HRESULT openImage(IWICImagingFactory* wic, const fs::path& path,
                  ComPtr<IWICBitmapFrameDecode>& frame) {
    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                               WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return hr;
    GUID container{};
    hr = decoder->GetContainerFormat(&container);
    if (FAILED(hr)) return hr;
    if (container != GUID_ContainerFormatPng && container != GUID_ContainerFormatJpeg) {
        return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    }
    return decoder->GetFrame(0, &frame);
}

HRESULT savePng(IWICImagingFactory* wic, IWICBitmapSource* source, const fs::path& path) {
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapEncoder> encoder;
    ComPtr<IWICBitmapFrameEncode> frame;
    UINT width{}, height{};
    HRESULT hr = source->GetSize(&width, &height);
    if (SUCCEEDED(hr)) hr = wic->CreateStream(&stream);
    if (SUCCEEDED(hr)) hr = stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE);
    if (SUCCEEDED(hr)) hr = wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
    if (SUCCEEDED(hr)) hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) hr = encoder->CreateNewFrame(&frame, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Initialize(nullptr);
    if (SUCCEEDED(hr)) hr = frame->SetSize(width, height);
    if (SUCCEEDED(hr)) hr = frame->WriteSource(source, nullptr);
    if (SUCCEEDED(hr)) hr = frame->Commit();
    if (SUCCEEDED(hr)) hr = encoder->Commit();
    return hr;
}

fs::path temporaryPng(const fs::path& directory) {
    GUID token{};
    wchar_t name[40]{};
    if (FAILED(CoCreateGuid(&token)) || !StringFromGUID2(token, name, ARRAYSIZE(name))) return {};
    return directory / (std::wstring(name) + L".png");
}
