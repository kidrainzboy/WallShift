#pragma once

#include "desktop.hpp"

#include <d3d11.h>
#include <dxgi.h>
#include <mfmediaengine.h>

class VideoWallpaper {
public:
    ~VideoWallpaper();

    HRESULT open(const fs::path& path, double resumeAt);
    HRESULT tick();
    HRESULT attach(const std::vector<Monitor>& monitors);
    void show();
    HRESULT draw(DESKTOP_WALLPAPER_POSITION position, COLORREF color);
    HRESULT capture(IWICImagingFactory* wic, const fs::path& path);
    HRESULT setPaused(bool paused);
    bool coveredByForeground() const;
    double time() const;
    bool attached() const;

private:
    struct Surface {
        HWND window{};
        RECT bounds{};
        ComPtr<IDXGISwapChain> swapChain;
        ComPtr<ID3D11RenderTargetView> target;
    };

    void detach();
    HRESULT initializeGraphics();
    HRESULT createFrame(DWORD width, DWORD height);
    HRESULT createSurface(const Monitor& monitor);

    DesktopHost host_;
    std::vector<Surface> surfaces_;
    RECT span_{};
    double resumeAt_{};
    bool mediaStarted_{};
    bool hasFrame_{};
    ComPtr<IMFMediaEngine> engine_;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIFactory> factory_;
    ComPtr<ID3D11Texture2D> frame_;
    ComPtr<ID3D11ShaderResourceView> frameView_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11Buffer> constants_;
    ComPtr<ID3D11SamplerState> clampSampler_;
    ComPtr<ID3D11SamplerState> wrapSampler_;
};
