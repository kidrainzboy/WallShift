#include "video.hpp"

#include <d3d11_4.h>
#include <d3dcompiler.h>
#include <mfapi.h>
#include <mferror.h>
#include <oleauto.h>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace {
class MediaEvents final : public IMFMediaEngineNotify {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (iid != IID_IUnknown && iid != __uuidof(IMFMediaEngineNotify)) return E_NOINTERFACE;
        *object = static_cast<IMFMediaEngineNotify*>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG left = --refs_;
        if (left == 0) delete this;
        return left;
    }
    HRESULT STDMETHODCALLTYPE EventNotify(DWORD event, DWORD_PTR param, DWORD) override {
        if (event == MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE && param) {
            SetEvent(reinterpret_cast<HANDLE>(param));
        }
        return S_OK;
    }
private:
    std::atomic<ULONG> refs_{1};
};

constexpr char shaderSource[] = R"(
cbuffer Layout : register(b0) {
    float2 scale;
    float2 offset;
    float3 background;
    float tiled;
};
Texture2D image : register(t0);
SamplerState imageSampler : register(s0);

struct Vertex {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};
Vertex vertexMain(uint id : SV_VertexID) {
    Vertex vertex;
    vertex.uv = float2((id << 1) & 2, id & 2);
    vertex.position = float4(vertex.uv * float2(2, -2) + float2(-1, 1), 0, 1);
    return vertex;
}
float4 pixelMain(Vertex vertex) : SV_TARGET {
    float2 uv = vertex.uv * scale + offset;
    if (!tiled && (any(uv < 0) || any(uv > 1))) return float4(background, 1);
    return float4(image.Sample(imageSampler, uv).rgb, 1);
}
)";

struct Layout {
    float scale[2];
    float offset[2];
    float background[3];
    float tiled;
};

Layout videoLayout(const RECT& monitor, const RECT& span, UINT sourceWidth, UINT sourceHeight,
                   DESKTOP_WALLPAPER_POSITION position, COLORREF color) {
    const RECT& workspace = position == DWPOS_SPAN || position == DWPOS_TILE ? span : monitor;
    const auto width = static_cast<float>(workspace.right - workspace.left);
    const auto height = static_cast<float>(workspace.bottom - workspace.top);
    const auto imageWidth = static_cast<float>(sourceWidth);
    const auto imageHeight = static_cast<float>(sourceHeight);
    float drawWidth = width;
    float drawHeight = height;
    if (position == DWPOS_FILL || position == DWPOS_FIT || position == DWPOS_SPAN) {
        const float x = width / imageWidth;
        const float y = height / imageHeight;
        const float factor = position == DWPOS_FIT ? std::min(x, y) : std::max(x, y);
        drawWidth = imageWidth * factor;
        drawHeight = imageHeight * factor;
    } else if (position == DWPOS_CENTER || position == DWPOS_TILE) {
        drawWidth = imageWidth;
        drawHeight = imageHeight;
    }
    const bool tile = position == DWPOS_TILE;
    const float left = tile ? 0.0f : (width - drawWidth) / 2.0f;
    const float top = tile ? 0.0f : (height - drawHeight) / 2.0f;
    return {
        {static_cast<float>(monitor.right - monitor.left) / drawWidth,
         static_cast<float>(monitor.bottom - monitor.top) / drawHeight},
        {(static_cast<float>(monitor.left - workspace.left) - left) / drawWidth,
         (static_cast<float>(monitor.top - workspace.top) - top) / drawHeight},
        {GetRValue(color) / 255.0f, GetGValue(color) / 255.0f, GetBValue(color) / 255.0f},
        tile ? 1.0f : 0.0f
    };
}

LRESULT CALLBACK surfaceProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hwnd, message, wp, lp);
}
}

VideoWallpaper::~VideoWallpaper() {
    if (engine_) engine_->Shutdown();
    detach();
}

HRESULT VideoWallpaper::initializeGraphics() {
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &device_, nullptr, &context_);
    if (FAILED(hr)) return hr;
    ComPtr<ID3D11Multithread> multithread;
    if (SUCCEEDED(context_.As(&multithread))) multithread->SetMultithreadProtected(TRUE);
    ComPtr<IDXGIDevice> dxgiDevice;
    ComPtr<IDXGIAdapter> adapter;
    hr = device_.As(&dxgiDevice);
    if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&factory_));

    ComPtr<ID3DBlob> vertex;
    ComPtr<ID3DBlob> pixel;
    if (SUCCEEDED(hr)) hr = D3DCompile(shaderSource, sizeof(shaderSource) - 1,
        nullptr, nullptr, nullptr, "vertexMain", "vs_4_0", 0, 0, &vertex, nullptr);
    if (SUCCEEDED(hr)) hr = D3DCompile(shaderSource, sizeof(shaderSource) - 1,
        nullptr, nullptr, nullptr, "pixelMain", "ps_4_0", 0, 0, &pixel, nullptr);
    if (SUCCEEDED(hr)) hr = device_->CreateVertexShader(
        vertex->GetBufferPointer(), vertex->GetBufferSize(), nullptr, &vertexShader_);
    if (SUCCEEDED(hr)) hr = device_->CreatePixelShader(
        pixel->GetBufferPointer(), pixel->GetBufferSize(), nullptr, &pixelShader_);
    const D3D11_BUFFER_DESC buffer{sizeof(Layout), D3D11_USAGE_DEFAULT,
                                  D3D11_BIND_CONSTANT_BUFFER, 0, 0, 0};
    if (SUCCEEDED(hr)) hr = device_->CreateBuffer(&buffer, nullptr, &constants_);
    D3D11_SAMPLER_DESC sampler{};
    sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampler.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr)) hr = device_->CreateSamplerState(&sampler, &clampSampler_);
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    if (SUCCEEDED(hr)) hr = device_->CreateSamplerState(&sampler, &wrapSampler_);
    return hr;
}

HRESULT VideoWallpaper::open(const fs::path& path, double resumeAt) {
    HRESULT hr = initializeGraphics();
    if (FAILED(hr)) return hr;
    UINT resetToken{};
    ComPtr<IMFDXGIDeviceManager> manager;
    ComPtr<IMFAttributes> attributes;
    ComPtr<IMFMediaEngineClassFactory> mediaFactory;
    ComPtr<IMFMediaEngineNotify> events;
    events.Attach(new MediaEvents());
    hr = MFCreateDXGIDeviceManager(&resetToken, &manager);
    if (SUCCEEDED(hr)) hr = manager->ResetDevice(device_.Get(), resetToken);
    if (SUCCEEDED(hr)) hr = MFCreateAttributes(&attributes, 3);
    if (SUCCEEDED(hr)) hr = attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, manager.Get());
    if (SUCCEEDED(hr)) hr = attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, events.Get());
    if (SUCCEEDED(hr)) hr = attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,
                                                 DXGI_FORMAT_B8G8R8A8_UNORM);
    if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mediaFactory));
    if (SUCCEEDED(hr)) hr = mediaFactory->CreateInstance(0, attributes.Get(), &engine_);
    if (SUCCEEDED(hr)) hr = engine_->SetMuted(TRUE);
    if (SUCCEEDED(hr)) hr = engine_->SetLoop(TRUE);
    if (FAILED(hr)) return hr;
    BSTR source = SysAllocString(path.c_str());
    if (!source) return E_OUTOFMEMORY;
    hr = engine_->SetSource(source);
    SysFreeString(source);
    resumeAt_ = resumeAt;
    return hr;
}

HRESULT VideoWallpaper::createFrame(DWORD width, DWORD height) {
    const D3D11_TEXTURE2D_DESC desc{width, height, 1, 1, DXGI_FORMAT_B8G8R8A8_UNORM,
        {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0};
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &frame_);
    if (SUCCEEDED(hr)) hr = device_->CreateShaderResourceView(frame_.Get(), nullptr, &frameView_);
    return hr;
}

HRESULT VideoWallpaper::tick() {
    ComPtr<IMFMediaError> error;
    if (SUCCEEDED(engine_->GetError(&error)) && error) {
        const HRESULT hr = error->GetExtendedErrorCode();
        return FAILED(hr) ? hr : MF_E_INVALID_FILE_FORMAT;
    }
    if (!mediaStarted_) {
        if (engine_->GetReadyState() < MF_MEDIA_ENGINE_READY_HAVE_CURRENT_DATA) return S_FALSE;
        if (!engine_->HasVideo()) return MF_E_INVALIDMEDIATYPE;
        DWORD width{}, height{};
        HRESULT hr = engine_->GetNativeVideoSize(&width, &height);
        if (FAILED(hr)) return hr;
        if (!width || !height) return MF_E_INVALIDMEDIATYPE;
        hr = createFrame(width, height);
        if (FAILED(hr)) return hr;
        if (resumeAt_ > 0 && resumeAt_ < engine_->GetDuration()) {
            hr = engine_->SetCurrentTime(resumeAt_);
            if (FAILED(hr)) return hr;
        }
        hr = engine_->Play();
        if (FAILED(hr)) return hr;
        mediaStarted_ = true;
    }

    LONGLONG timestamp{};
    const HRESULT tickResult = engine_->OnVideoStreamTick(&timestamp);
    if (tickResult != S_OK) return tickResult;
    D3D11_TEXTURE2D_DESC desc{};
    frame_->GetDesc(&desc);
    const RECT area{0, 0, static_cast<LONG>(desc.Width), static_cast<LONG>(desc.Height)};
    constexpr MFARGB black{0, 0, 0, 255};
    ID3D11ShaderResourceView* unbind = nullptr;
    context_->PSSetShaderResources(0, 1, &unbind);
    const HRESULT hr = engine_->TransferVideoFrame(frame_.Get(), nullptr, &area, &black);
    if (SUCCEEDED(hr)) hasFrame_ = true;
    return hr;
}

HRESULT VideoWallpaper::attach(const std::vector<Monitor>& monitors) {
    detach();
    if (monitors.empty()) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
    host_ = findDesktopHost(true);
    if (!host_.parent) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);

    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpfnWndProc = surfaceProc;
    windowClass.lpszClassName = L"WallShift.Video";
    if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    span_ = monitors.front().bounds;
    for (const auto& monitor : monitors) {
        UnionRect(&span_, &span_, &monitor.bounds);
        const HRESULT hr = createSurface(monitor);
        if (FAILED(hr)) {
            detach();
            return hr;
        }
    }
    return S_OK;
}

HRESULT VideoWallpaper::createSurface(const Monitor& monitor) {
    RECT local = monitor.bounds;
    MapWindowPoints(nullptr, host_.parent, reinterpret_cast<POINT*>(&local), 2);
    const DWORD layered = host_.layered ? WS_EX_LAYERED : 0;
    auto& surface = surfaces_.emplace_back();
    surface.bounds = monitor.bounds;
    surface.window = CreateWindowExW(layered | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        L"WallShift.Video", L"", WS_CHILD, local.left, local.top,
        local.right - local.left, local.bottom - local.top,
        host_.parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!surface.window) return HRESULT_FROM_WIN32(GetLastError());
    if (host_.layered && !SetLayeredWindowAttributes(surface.window, 0, 255, LWA_ALPHA)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!SetWindowPos(surface.window, host_.icons ? host_.icons : HWND_TOP,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DXGI_SWAP_CHAIN_DESC swap{};
    swap.BufferDesc.Width = static_cast<UINT>(monitor.bounds.right - monitor.bounds.left);
    swap.BufferDesc.Height = static_cast<UINT>(monitor.bounds.bottom - monitor.bounds.top);
    swap.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap.SampleDesc.Count = 1;
    swap.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap.BufferCount = 1;
    swap.OutputWindow = surface.window;
    swap.Windowed = TRUE;
    swap.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    HRESULT hr = factory_->CreateSwapChain(device_.Get(), &swap, &surface.swapChain);
    ComPtr<ID3D11Texture2D> target;
    if (SUCCEEDED(hr)) hr = surface.swapChain->GetBuffer(0, IID_PPV_ARGS(&target));
    if (SUCCEEDED(hr)) hr = device_->CreateRenderTargetView(target.Get(), nullptr, &surface.target);
    return hr;
}

void VideoWallpaper::show() {
    for (const auto& surface : surfaces_) ShowWindow(surface.window, SW_SHOWNOACTIVATE);
}

void VideoWallpaper::detach() {
    if (context_) context_->OMSetRenderTargets(0, nullptr, nullptr);
    for (auto& surface : surfaces_) {
        surface.target.Reset();
        surface.swapChain.Reset();
        if (IsWindow(surface.window)) DestroyWindow(surface.window);
    }
    surfaces_.clear();
}

bool VideoWallpaper::attached() const {
    return IsWindow(host_.parent) && !surfaces_.empty() &&
        std::all_of(surfaces_.begin(), surfaces_.end(),
                    [](const Surface& surface) { return IsWindow(surface.window) != FALSE; });
}

HRESULT VideoWallpaper::draw(DESKTOP_WALLPAPER_POSITION position, COLORREF color) {
    if (!hasFrame_ || !attached()) return HRESULT_FROM_WIN32(ERROR_NOT_READY);
    D3D11_TEXTURE2D_DESC desc{};
    frame_->GetDesc(&desc);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context_->PSSetShaderResources(0, 1, frameView_.GetAddressOf());
    context_->PSSetConstantBuffers(0, 1, constants_.GetAddressOf());
    auto* sampler = position == DWPOS_TILE ? wrapSampler_.Get() : clampSampler_.Get();
    context_->PSSetSamplers(0, 1, &sampler);
    for (const auto& surface : surfaces_) {
        const auto layout = videoLayout(surface.bounds, span_, desc.Width, desc.Height,
                                        position, color);
        context_->UpdateSubresource(constants_.Get(), 0, nullptr, &layout, 0, 0);
        const D3D11_VIEWPORT viewport{0, 0,
            static_cast<float>(surface.bounds.right - surface.bounds.left),
            static_cast<float>(surface.bounds.bottom - surface.bounds.top), 0, 1};
        context_->RSSetViewports(1, &viewport);
        context_->OMSetRenderTargets(1, surface.target.GetAddressOf(), nullptr);
        context_->Draw(3, 0);
        const HRESULT hr = surface.swapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
        if (FAILED(hr) && hr != DXGI_ERROR_WAS_STILL_DRAWING) return hr;
    }
    return S_OK;
}

HRESULT VideoWallpaper::capture(IWICImagingFactory* wic, const fs::path& path) {
    if (!hasFrame_) return HRESULT_FROM_WIN32(ERROR_NOT_READY);
    D3D11_TEXTURE2D_DESC desc{};
    frame_->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &readback);
    if (FAILED(hr)) return hr;
    context_->CopyResource(readback.Get(), frame_.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context_->Map(readback.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return hr;
    ComPtr<IWICBitmap> bitmap;
    hr = wic->CreateBitmapFromMemory(desc.Width, desc.Height, GUID_WICPixelFormat32bppBGR,
        mapped.RowPitch, mapped.RowPitch * desc.Height, static_cast<BYTE*>(mapped.pData), &bitmap);
    if (SUCCEEDED(hr)) hr = savePng(wic, bitmap.Get(), path);
    context_->Unmap(readback.Get(), 0);
    return hr;
}

double VideoWallpaper::time() const {
    return engine_ ? engine_->GetCurrentTime() : 0;
}

HRESULT VideoWallpaper::setPaused(bool paused) {
    return paused ? engine_->Pause() : engine_->Play();
}

bool VideoWallpaper::coveredByForeground() const {
    const HWND foreground = GetForegroundWindow();
    if (!foreground || foreground == host_.parent || IsIconic(foreground)) return false;
    wchar_t name[40]{};
    GetClassNameW(foreground, name, ARRAYSIZE(name));
    if (wcscmp(name, L"Progman") == 0 || wcscmp(name, L"WorkerW") == 0) return false;
    RECT area{};
    if (!GetWindowRect(foreground, &area)) return false;
    return !surfaces_.empty() && std::all_of(surfaces_.begin(), surfaces_.end(),
        [&area](const Surface& surface) {
            const auto& bounds = surface.bounds;
            return area.left <= bounds.left && area.top <= bounds.top &&
                   area.right >= bounds.right && area.bottom >= bounds.bottom;
        });
}
