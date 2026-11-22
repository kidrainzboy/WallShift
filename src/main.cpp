#include "branding.hpp"
#include "desktop.hpp"
#include "resource.h"
#include "startup.hpp"
#include "strings.hpp"
#include "video.hpp"
#include "widget.hpp"

#include <commctrl.h>
#include <mfapi.h>
#include <ole2.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>

namespace {
constexpr wchar_t appName[] = L"WallShift";
constexpr wchar_t legacyAppName[] = L"WallChanger";
constexpr wchar_t widgetClass[] = L"WallShift.Widget";
constexpr wchar_t controlClass[] = L"WallShift.Control";
constexpr UINT trayMessage = WM_APP + 1;
constexpr UINT dropMessage = WM_APP + 2;
constexpr UINT widgetLayoutMessage = WM_APP + 3;
constexpr UINT widgetStackMessage = WM_APP + 4;
constexpr UINT_PTR albumTimer = 1;
constexpr UINT_PTR videoTimer = 2;
constexpr UINT_PTR desktopTimer = 3;
constexpr UINT_PTR flashTimer = 4;
constexpr std::array intervals{5u, 10u, 15u, 30u};
constexpr std::array fitOrder{DWPOS_FILL, DWPOS_FIT, DWPOS_STRETCH,
                              DWPOS_TILE, DWPOS_CENTER, DWPOS_SPAN};
constexpr std::array widgetSizes{widgetMinSize, widgetDefaultSize, 96, widgetMaxSize};
constexpr int unsetCoordinate = std::numeric_limits<int>::min();
thread_local HWND widgetEventReceiver{};

void CALLBACK widgetForegroundChanged(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD) {
    if (widgetEventReceiver) PostMessageW(widgetEventReceiver, widgetStackMessage, 0, 0);
}

enum Command : UINT {
    chooseDefault = 1, resetDefault, undoChange, chooseMedia, freezeFrame, quit, resetWidgetPosition,
    toggleStartup, selectEnglish, selectVietnamese,
    menuBrand,
    intervalBase = 20, fitBase = 30, widgetSizeBase = 40, monitorBase = 100
};

enum class Mode { system, image, album, video };
enum class WidgetGesture { none, pressed, pressedGrip, moving, resizing };

struct Scene {
    Mode mode{Mode::system};
    std::vector<fs::path> files;
    std::wstring monitor;
    DESKTOP_WALLPAPER_POSITION fit{DWPOS_FILL};
    size_t index{};
    UINT interval{10};
    double videoTime{};
};

struct History {
    Scene scene;
    DesktopState desktop;
};

struct PendingVideo {
    std::unique_ptr<VideoWallpaper> player;
    Scene scene;
    ULONGLONG deadline;
    bool undo;
};

class App;

class DropTarget final : public IDropTarget {
public:
    explicit DropTarget(App& app) : app_(app) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* object, DWORD, POINTL, DWORD* effect) override;
    HRESULT STDMETHODCALLTYPE DragOver(DWORD, POINTL, DWORD* effect) override;
    HRESULT STDMETHODCALLTYPE DragLeave() override;
    HRESULT STDMETHODCALLTYPE Drop(IDataObject* object, DWORD, POINTL, DWORD* effect) override;

private:
    App& app_;
    std::atomic<ULONG> refs_{1};
    bool valid_{};
};

class App {
public:
    explicit App(HINSTANCE instance) : instance_(instance) {}
    ~App();
    HRESULT initialize();
    Language language() const { return language_; }
    LRESULT onControl(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    LRESULT onWidget(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    void queueDrop(std::vector<fs::path> paths);
    void highlightDrop(bool highlight);

private:
    static LRESULT CALLBACK controlProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT CALLBACK widgetProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static App* fromWindow(HWND hwnd, UINT message, LPARAM lp);
    HRESULT initializeWindows();
    void ensureWidget();
    void restackWidget(bool reveal);
    bool layoutWidget();
    bool placeWidget(const RECT& bounds);
    void destroyWidget();
    void refreshWidget();
    void updateWidgetContrast();
    void beginWidgetGesture(POINT pointer);
    void moveWidgetGesture(POINT pointer);
    void endWidgetGesture(bool save);
    void updateTooltip(const wchar_t* message);
    void updateWallpaperTooltip(Mode mode);
    void setupTray();
    void report(const wchar_t* message, HRESULT hr);
    void report(LocalizedText message, HRESULT hr) { report(translate(message), hr); }
    const wchar_t* translate(LocalizedText message) const { return message.in(language_); }
    void menu(POINT point);
    void command(UINT id, const std::vector<Monitor>& monitors);

    HRESULT loadSettings();
    bool saveSetting(const wchar_t* key, const std::wstring& value);
    void setDefaultFile();
    void restoreDefault();
    void useFiles(std::vector<fs::path> paths);
    void setFit(DESKTOP_WALLPAPER_POSITION position);
    void setInterval(UINT seconds);
    void undo();
    void freeze();
    void switchScene(Scene scene, bool undo);
    HRESULT commit(Scene scene, bool undo, std::unique_ptr<VideoWallpaper> player = {});
    HRESULT applyImage(const Scene& scene);
    HRESULT captureHistory(History& history);
    HRESULT openVideo(Scene scene, bool undo);
    std::vector<Monitor> videoMonitors(const Scene& scene);
    void updateVideo();
    void stopFailedVideo(HRESULT hr);
    void syncTimers();
    void desktopChanged();

    HINSTANCE instance_;
    HWND control_{};
    HWND widget_{};
    HWND tooltip_{};
    HWINEVENTHOOK foregroundHook_{};
    HICON trayIcon_{};
    UINT taskbarCreated_{};
    ULONGLONG widgetRetryUntil_{};
    bool trayAdded_{};
    bool dropRegistered_{};
    bool mediaStarted_{};
    bool videoPaused_{};
    bool dragOver_{};
    bool flashError_{};
    bool widgetHighContrast_{};
    bool widgetDoubleClick_{};
    WidgetGesture widgetGesture_{WidgetGesture::none};
    POINT widgetPress_{};
    RECT widgetBeforeDrag_{};
    WidgetDisplay widgetDragDisplay_{};
    int widgetOriginalSize_{widgetDefaultSize};
    int widgetSize_{widgetDefaultSize};
    std::optional<POINT> widgetPosition_;
    WidgetSurface widgetSurface_;
    COLORREF videoColor_{};
    ULONGLONG visibilityCheck_{};
    wchar_t tip_[128]{};
    ComPtr<IDesktopWallpaper> desktop_;
    ComPtr<IWICImagingFactory> wic_;
    fs::path directory_;
    fs::path settings_;
    fs::path defaultImage_;
    Language language_{Language::english};
    UINT preferredInterval_{10};
    DESKTOP_WALLPAPER_POSITION preferredFit_{DWPOS_FILL};
    std::wstring selectedMonitor_;
    Scene current_;
    std::optional<History> history_;
    std::optional<PendingVideo> pending_;
    std::unique_ptr<VideoWallpaper> video_;
    std::optional<std::vector<fs::path>> queuedDrop_;
};

bool validSelection(const std::vector<fs::path>& paths) {
    return !paths.empty() && ((paths.size() == 1 && isVideo(paths.front())) ||
        std::all_of(paths.begin(), paths.end(), isImage));
}

std::vector<fs::path> droppedFiles(IDataObject* object) {
    FORMATETC format{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    STGMEDIUM medium{};
    std::vector<fs::path> paths;
    if (FAILED(object->GetData(&format, &medium))) return paths;
    const auto drop = static_cast<HDROP>(medium.hGlobal);
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        const UINT size = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring path(size + 1, L'\0');
        if (size && DragQueryFileW(drop, i, path.data(), size + 1) == size) {
            path.resize(size);
            paths.emplace_back(std::move(path));
        }
    }
    ReleaseStgMedium(&medium);
    return paths;
}

HRESULT DropTarget::QueryInterface(REFIID iid, void** object) {
    if (!object) return E_POINTER;
    *object = nullptr;
    if (iid != IID_IUnknown && iid != IID_IDropTarget) return E_NOINTERFACE;
    *object = static_cast<IDropTarget*>(this);
    AddRef();
    return S_OK;
}

ULONG DropTarget::Release() {
    const ULONG left = --refs_;
    if (left == 0) delete this;
    return left;
}

HRESULT DropTarget::DragEnter(IDataObject* object, DWORD, POINTL, DWORD* effect) {
    valid_ = validSelection(droppedFiles(object));
    return DragOver(0, {}, effect);
}

HRESULT DropTarget::DragOver(DWORD, POINTL, DWORD* effect) {
    *effect = valid_ ? (*effect & DROPEFFECT_COPY) : DROPEFFECT_NONE;
    app_.highlightDrop(*effect != DROPEFFECT_NONE);
    return S_OK;
}

HRESULT DropTarget::DragLeave() {
    valid_ = false;
    app_.highlightDrop(false);
    return S_OK;
}

HRESULT DropTarget::Drop(IDataObject* object, DWORD, POINTL, DWORD* effect) {
    auto paths = droppedFiles(object);
    const bool valid = validSelection(paths);
    *effect = valid ? (*effect & DROPEFFECT_COPY) : DROPEFFECT_NONE;
    if (*effect == DROPEFFECT_COPY || (!valid && !paths.empty())) app_.queueDrop(std::move(paths));
    return DragLeave();
}

std::vector<fs::path> pickFiles(HWND owner, bool imagesOnly, Language language) {
    ComPtr<IFileOpenDialog> dialog;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) return {};
    FILEOPENDIALOGOPTIONS options{};
    dialog->GetOptions(&options);
    options |= FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
    if (!imagesOnly) options |= FOS_ALLOWMULTISELECT;
    dialog->SetOptions(options);
    const COMDLG_FILTERSPEC filter = imagesOnly
        ? COMDLG_FILTERSPEC{ui::pictureFilter.in(language), L"*.png;*.jpg;*.jpeg"}
        : COMDLG_FILTERSPEC{ui::mediaFilter.in(language), L"*.png;*.jpg;*.jpeg;*.mp4"};
    dialog->SetFileTypes(1, &filter);
    dialog->SetTitle((imagesOnly ? ui::chooseDefaultTitle : ui::chooseMediaTitle).in(language));
    if (FAILED(dialog->Show(owner))) return {};
    ComPtr<IShellItemArray> items;
    if (FAILED(dialog->GetResults(&items))) return {};
    DWORD count{};
    items->GetCount(&count);
    std::vector<fs::path> paths;
    for (DWORD i = 0; i < count; ++i) {
        ComPtr<IShellItem> item;
        PWSTR path = nullptr;
        if (SUCCEEDED(items->GetItemAt(i, &item)) &&
            SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
            paths.emplace_back(path);
        }
        CoTaskMemFree(path);
    }
    return paths;
}

App::~App() {
    if (foregroundHook_) UnhookWinEvent(foregroundHook_);
    if (widgetEventReceiver == control_) widgetEventReceiver = nullptr;
    pending_.reset();
    video_.reset();
    if (mediaStarted_) MFShutdown();
    destroyWidget();
    if (trayAdded_) {
        NOTIFYICONDATAW icon{sizeof(icon)};
        icon.hWnd = control_;
        icon.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &icon);
    }
    if (trayIcon_) DestroyIcon(trayIcon_);
    if (IsWindow(control_)) DestroyWindow(control_);
}

HRESULT App::initialize() {
    HRESULT hr = CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&desktop_));
    if (SUCCEEDED(hr)) hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                           CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic_));
    if (SUCCEEDED(hr)) hr = loadSettings();
    if (FAILED(hr)) return hr;
    return initializeWindows();
}

HRESULT App::initializeWindows() {
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    WNDCLASSW wc{};
    wc.hInstance = instance_;
    wc.lpszClassName = controlClass;
    wc.lpfnWndProc = controlProc;
    if (!RegisterClassW(&wc)) return HRESULT_FROM_WIN32(GetLastError());
    wc.lpszClassName = widgetClass;
    wc.lpfnWndProc = widgetProc;
    wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
    wc.style = CS_DBLCLKS;
    if (!RegisterClassW(&wc)) return HRESULT_FROM_WIN32(GetLastError());
    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    control_ = CreateWindowExW(WS_EX_TOOLWINDOW, controlClass, appName, WS_POPUP,
                               0, 0, 0, 0, nullptr, nullptr, instance_, this);
    if (!control_) return HRESULT_FROM_WIN32(GetLastError());
    widgetEventReceiver = control_;
    foregroundHook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
        widgetForegroundChanged, 0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    setupTray();
    fs::path startupShortcut;
    bool startupEnabled{};
    if (SUCCEEDED(getStartupShortcut(startupShortcut, startupEnabled))) {
        const auto hr = migrateStartupShortcut(startupShortcut, startupEnabled);
        if (FAILED(hr)) report(ui::startupFailed, hr);
    }
    updateWidgetContrast();
    widgetRetryUntil_ = GetTickCount64() + 8000;
    ensureWidget();
    if (!widget_) SetTimer(control_, desktopTimer, 400, nullptr);
    return S_OK;
}

HRESULT prepareSettings(const fs::path& settings, const fs::path& legacyDirectory) {
    if (GetFileAttributesW(settings.c_str()) != INVALID_FILE_ATTRIBUTES) return S_OK;
    const auto pending = settings.parent_path() / L"settings.migrating";
    const HANDLE config = CreateFileW(pending.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (config == INVALID_HANDLE_VALUE) return HRESULT_FROM_WIN32(GetLastError());
    constexpr wchar_t bom = 0xFEFF;
    DWORD written{};
    HRESULT hr = WriteFile(config, &bom, sizeof(bom), &written, nullptr) ?
        S_OK : HRESULT_FROM_WIN32(GetLastError());
    CloseHandle(config);

    const auto legacySettings = legacyDirectory / L"settings.ini";
    if (SUCCEEDED(hr) && GetFileAttributesW(legacySettings.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::vector<wchar_t> values(32768);
        const auto count = GetPrivateProfileSectionW(legacyAppName, values.data(),
            static_cast<DWORD>(values.size()), legacySettings.c_str());
        if (count == values.size() - 2) {
            hr = HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
        } else if (count && !WritePrivateProfileSectionW(appName, values.data(), pending.c_str())) {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
        wchar_t name[512]{};
        GetPrivateProfileStringW(legacyAppName, L"Default", L"", name, ARRAYSIZE(name), legacySettings.c_str());
        const fs::path image(name);
        if (SUCCEEDED(hr) && !image.empty() && image == image.filename() && isImage(image)) {
            const auto source = legacyDirectory / image;
            const auto destination = settings.parent_path() / image;
            if (GetFileAttributesW(source.c_str()) != INVALID_FILE_ATTRIBUTES &&
                !CopyFileW(source.c_str(), destination.c_str(), TRUE)) {
                const auto error = GetLastError();
                if (error != ERROR_FILE_EXISTS) hr = HRESULT_FROM_WIN32(error);
            }
        }
    }
    if (SUCCEEDED(hr) && !MoveFileExW(pending.c_str(), settings.c_str(), MOVEFILE_WRITE_THROUGH)) {
        hr = HRESULT_FROM_WIN32(GetLastError());
    }
    if (FAILED(hr)) DeleteFileW(pending.c_str());
    return hr;
}

HRESULT App::loadSettings() {
    PWSTR local = nullptr;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local);
    if (FAILED(hr)) return hr;
    const fs::path userData(local);
    directory_ = userData / appName;
    CoTaskMemFree(local);
    std::error_code error;
    fs::create_directories(directory_, error);
    if (error) return HRESULT_FROM_WIN32(error.value());
    settings_ = directory_ / L"settings.ini";
    hr = prepareSettings(settings_, userData / legacyAppName);
    if (FAILED(hr)) return hr;
    desktop_->GetPosition(&preferredFit_);
    const auto readInt = [this](const wchar_t* key, int fallback) {
        return GetPrivateProfileIntW(appName, key, fallback, settings_.c_str());
    };
    const auto interval = readInt(L"Interval", 10);
    if (std::find(intervals.begin(), intervals.end(), interval) != intervals.end()) {
        preferredInterval_ = interval;
    }
    const auto fit = readInt(L"Fit", preferredFit_);
    if (fit <= DWPOS_SPAN) preferredFit_ = static_cast<DESKTOP_WALLPAPER_POSITION>(fit);
    wchar_t setting[512]{};
    GetPrivateProfileStringW(appName, L"Language", L"en", setting, ARRAYSIZE(setting), settings_.c_str());
    language_ = wcscmp(setting, L"vi") == 0 ? Language::vietnamese : Language::english;
    GetPrivateProfileStringW(appName, L"Default", L"", setting, ARRAYSIZE(setting), settings_.c_str());
    const fs::path name(setting);
    if (!name.empty() && name == name.filename() && isImage(name)) defaultImage_ = directory_ / name;
    GetPrivateProfileStringW(appName, L"Monitor", L"", setting, ARRAYSIZE(setting), settings_.c_str());
    selectedMonitor_ = setting;
    GetPrivateProfileStringW(appName, L"WidgetPosition", L"", setting, ARRAYSIZE(setting), settings_.c_str());
    POINT position{};
    if (swscanf_s(setting, L"%ld,%ld", &position.x, &position.y) == 2) {
        widgetPosition_ = position;
    } else if (*setting == L'\0') {
        const auto x = static_cast<LONG>(readInt(L"WidgetX", unsetCoordinate));
        const auto y = static_cast<LONG>(readInt(L"WidgetY", unsetCoordinate));
        if (x != unsetCoordinate && y != unsetCoordinate) {
            const auto primary = widgetDisplay(MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY));
            widgetPosition_ = POINT{primary.workArea.left + x, primary.workArea.top + y};
        }
    }
    widgetSize_ = std::clamp(static_cast<int>(readInt(L"WidgetSize", widgetDefaultSize)),
                             widgetMinSize, widgetMaxSize);
    return S_OK;
}

bool App::saveSetting(const wchar_t* key, const std::wstring& value) {
    if (WritePrivateProfileStringW(appName, key, value.c_str(), settings_.c_str())) return true;
    report(ui::settingsFailed, HRESULT_FROM_WIN32(GetLastError()));
    return false;
}

void App::ensureWidget() {
    const auto host = findDesktopHost(false);
    if (host.parent && widget_ && GetWindow(widget_, GW_OWNER) == host.parent) {
        restackWidget(true);
        return;
    }
    destroyWidget();
    if (!host.parent) return;
    // Explorer may not composite per-pixel alpha on a cross-process child HWND.
    // An owned popup can use the alpha channel while staying above the desktop.
    widget_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        widgetClass, appName, WS_POPUP, 0, 0, 60, 60, host.parent, nullptr, instance_, this);
    if (!widget_) return;
    if (!layoutWidget()) {
        destroyWidget();
        return;
    }
    ComPtr<IDropTarget> target;
    target.Attach(new DropTarget(*this));
    dropRegistered_ = SUCCEEDED(RegisterDragDrop(widget_, target.Get()));
    tooltip_ = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               control_, nullptr, instance_, nullptr);
    if (tooltip_) {
        TOOLINFOW tool{sizeof(tool)};
        tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        tool.hwnd = control_;
        tool.uId = reinterpret_cast<UINT_PTR>(widget_);
        tool.lpszText = tip_;
        SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
        SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 340);
    }
    updateWallpaperTooltip(current_.mode);
    restackWidget(true);
}

void App::restackWidget(bool reveal) {
    const auto desktop = widget_ ? GetWindow(widget_, GW_OWNER) : nullptr;
    if (!desktop || !IsWindowVisible(desktop)) return;
    const bool showing = IsWindowVisible(widget_) != FALSE;
    if (!showing && !reveal) return;
    const auto aboveDesktop = GetWindow(desktop, GW_HWNDPREV);
    if (showing && aboveDesktop == widget_) return;

    UINT flags = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER;
    if (!showing) flags |= SWP_SHOWWINDOW;
    if (aboveDesktop == widget_) flags |= SWP_NOZORDER;
    const HWND insertAfter = aboveDesktop &&
        !(GetWindowLongPtrW(aboveDesktop, GWL_EXSTYLE) & WS_EX_TOPMOST) ?
        aboveDesktop : HWND_NOTOPMOST;
    SetWindowPos(widget_, insertAfter, 0, 0, 0, 0, flags);
}

void App::destroyWidget() {
    endWidgetGesture(false);
    if (tooltip_) DestroyWindow(tooltip_);
    tooltip_ = nullptr;
    if (widget_ && IsWindow(widget_)) {
        if (dropRegistered_) RevokeDragDrop(widget_);
        DestroyWindow(widget_);
    }
    widget_ = nullptr;
    widgetSurface_.invalidate();
    dropRegistered_ = false;
}

bool App::layoutWidget() {
    if (!widget_) return false;
    const auto monitor = widgetPosition_ ? MonitorFromPoint(*widgetPosition_, MONITOR_DEFAULTTONEAREST) :
                                          MonitorFromPoint(POINT{}, MONITOR_DEFAULTTOPRIMARY);
    const auto display = widgetDisplay(monitor);
    const auto margin = MulDiv(20, display.dpi, USER_DEFAULT_SCREEN_DPI);
    const auto side = MulDiv(widgetSize_, display.dpi, USER_DEFAULT_SCREEN_DPI);
    const POINT origin = widgetPosition_.value_or(POINT{
        display.workArea.right - side - margin, display.workArea.top + margin
    });
    return placeWidget(widgetBounds(display, origin, widgetSize_));
}

bool App::placeWidget(const RECT& bounds) {
    return widgetSurface_.present(widget_, bounds,
        {GetFocus() == widget_, dragOver_, flashError_, widgetHighContrast_});
}

void App::refreshWidget() {
    RECT bounds{};
    if (widget_ && GetWindowRect(widget_, &bounds)) placeWidget(bounds);
}

void App::updateWidgetContrast() {
    HIGHCONTRASTW contrast{sizeof(contrast)};
    SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
    widgetHighContrast_ = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
    widgetSurface_.invalidate();
    refreshWidget();
}

void App::beginWidgetGesture(POINT pointer) {
    if (!GetWindowRect(widget_, &widgetBeforeDrag_)) return;
    widgetPress_ = pointer;
    widgetOriginalSize_ = widgetSize_;
    widgetDragDisplay_ = widgetDisplay(MonitorFromWindow(widget_, MONITOR_DEFAULTTONEAREST));
    widgetGesture_ = widgetResizeGrip(widgetBeforeDrag_, pointer) ?
        WidgetGesture::pressedGrip : WidgetGesture::pressed;
    widgetDoubleClick_ = false;
    SetFocus(widget_);
    SetCapture(widget_);
}

void App::moveWidgetGesture(POINT pointer) {
    if (widgetGesture_ == WidgetGesture::none) return;
    const auto dx = pointer.x - widgetPress_.x;
    const auto dy = pointer.y - widgetPress_.y;
    if (widgetGesture_ == WidgetGesture::pressed || widgetGesture_ == WidgetGesture::pressedGrip) {
        if (std::abs(dx) < GetSystemMetricsForDpi(SM_CXDRAG, widgetDragDisplay_.dpi) &&
            std::abs(dy) < GetSystemMetricsForDpi(SM_CYDRAG, widgetDragDisplay_.dpi)) return;
        widgetGesture_ = widgetGesture_ == WidgetGesture::pressedGrip ?
            WidgetGesture::resizing : WidgetGesture::moving;
    }
    if (widgetGesture_ == WidgetGesture::resizing) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENWSE));
        const auto delta = std::abs(dx) >= std::abs(dy) ? dx : dy;
        widgetSize_ = std::clamp(widgetOriginalSize_ + MulDiv(delta, USER_DEFAULT_SCREEN_DPI,
                                  widgetDragDisplay_.dpi), widgetMinSize, widgetMaxSize);
        placeWidget(widgetBounds(widgetDragDisplay_,
                                 {widgetBeforeDrag_.left, widgetBeforeDrag_.top}, widgetSize_));
        return;
    }
    SetCursor(LoadCursorW(nullptr, IDC_SIZEALL));
    const auto display = widgetDisplay(MonitorFromPoint(pointer, MONITOR_DEFAULTTONEAREST));
    const POINT origin{
        pointer.x - MulDiv(widgetPress_.x - widgetBeforeDrag_.left, display.dpi, widgetDragDisplay_.dpi),
        pointer.y - MulDiv(widgetPress_.y - widgetBeforeDrag_.top, display.dpi, widgetDragDisplay_.dpi)
    };
    placeWidget(widgetBounds(display, origin, widgetSize_));
}

void App::endWidgetGesture(bool save) {
    const auto gesture = widgetGesture_;
    if (gesture == WidgetGesture::none) return;
    widgetGesture_ = WidgetGesture::none;
    widgetDoubleClick_ = save && gesture == WidgetGesture::pressed;
    if (widget_ && GetCapture() == widget_) ReleaseCapture();
    if (gesture != WidgetGesture::moving && gesture != WidgetGesture::resizing) return;
    if (!save) {
        widgetSize_ = widgetOriginalSize_;
        layoutWidget();
        return;
    }
    RECT bounds{};
    if (GetWindowRect(widget_, &bounds)) {
        widgetPosition_ = POINT{bounds.left, bounds.top};
        saveSetting(L"WidgetPosition", std::to_wstring(bounds.left) + L"," + std::to_wstring(bounds.top));
    }
    if (gesture == WidgetGesture::resizing) saveSetting(L"WidgetSize", std::to_wstring(widgetSize_));
}

void App::updateTooltip(const wchar_t* message) {
    wcsncpy_s(tip_, message, _TRUNCATE);
    if (tooltip_ && widget_) {
        TOOLINFOW tool{sizeof(tool)};
        tool.uFlags = TTF_IDISHWND;
        tool.hwnd = control_;
        tool.uId = reinterpret_cast<UINT_PTR>(widget_);
        tool.lpszText = tip_;
        SendMessageW(tooltip_, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tool));
    }
}

void App::updateWallpaperTooltip(Mode mode) {
    const auto status = mode == Mode::video ? ui::videoPlaying :
                        mode == Mode::album ? ui::albumPlaying : ui::widgetHelp;
    updateTooltip(translate(status));
}

void App::setupTray() {
    NOTIFYICONDATAW icon{sizeof(icon)};
    icon.hWnd = control_;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    icon.uCallbackMessage = trayMessage;
    if (!trayIcon_) LoadIconMetric(instance_, MAKEINTRESOURCEW(IDI_WALLSHIFT), LIM_SMALL, &trayIcon_);
    icon.hIcon = trayIcon_ ? trayIcon_ : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(icon.szTip, appName);
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
    if (trayAdded_) {
        icon.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &icon);
    }
}

void App::report(const wchar_t* message, HRESULT hr) {
    updateTooltip(message);
    OutputDebugStringW((std::wstring(L"WallShift: ") + message +
        L" [" + std::to_wstring(static_cast<unsigned long>(hr)) + L"]\n").c_str());
    if (!trayAdded_) return;
    NOTIFYICONDATAW icon{sizeof(icon)};
    icon.hWnd = control_;
    icon.uID = 1;
    icon.uFlags = NIF_INFO;
    icon.dwInfoFlags = FAILED(hr) ? NIIF_WARNING : NIIF_INFO;
    wcscpy_s(icon.szInfoTitle, appName);
    wcsncpy_s(icon.szInfo, message, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &icon);
    if (FAILED(hr) && widget_) {
        flashError_ = true;
        refreshWidget();
        SetTimer(control_, flashTimer, 2200, nullptr);
    }
}

void App::highlightDrop(bool highlight) {
    if (dragOver_ == highlight) return;
    dragOver_ = highlight;
    refreshWidget();
}

void App::queueDrop(std::vector<fs::path> paths) {
    queuedDrop_ = std::move(paths);
    PostMessageW(control_, dropMessage, 0, 0);
}

void App::menu(POINT point) {
    const HMENU popup = CreatePopupMenu();
    if (!popup) return;
    MenuBranding brand;
    if (!widgetHighContrast_) {
        const auto display = widgetDisplay(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST));
        if (brand.append(popup, menuBrand, wic_.Get(), instance_, display.dpi)) {
            AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
        }
    }
    const auto monitors = getMonitors(desktop_.Get());
    const auto enabled = [](bool ready) { return ready ? MF_ENABLED : MF_GRAYED; };
    AppendMenuW(popup, MF_STRING, chooseMedia, translate(ui::chooseMedia));
    AppendMenuW(popup, MF_STRING, chooseDefault, translate(ui::chooseDefault));
    AppendMenuW(popup, MF_STRING, resetDefault, translate(ui::restoreDefault));
    AppendMenuW(popup, MF_STRING | enabled(history_.has_value()), undoChange, translate(ui::undo));
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);

    const HMENU fits = CreatePopupMenu();
    const auto currentFit = current_.mode == Mode::system ? preferredFit_ : current_.fit;
    for (const auto fit : fitOrder) {
        const bool allowed = fit != DWPOS_SPAN || (selectedMonitor_.empty() && current_.monitor.empty());
        AppendMenuW(fits, MF_STRING | enabled(allowed) | (fit == currentFit ? MF_CHECKED : 0),
                    fitBase + static_cast<UINT>(fit), translate(ui::wallpaperFits[fit]));
    }
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(fits), translate(ui::fit));

    const HMENU timing = CreatePopupMenu();
    for (size_t i = 0; i < intervals.size(); ++i) {
        const auto label = std::to_wstring(intervals[i]) + translate(ui::secondsPerImage);
        AppendMenuW(timing, MF_STRING | (intervals[i] == preferredInterval_ ? MF_CHECKED : 0),
                    intervalBase + i, label.c_str());
    }
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(timing), translate(ui::albumInterval));
    const HMENU displays = CreatePopupMenu();
    AppendMenuW(displays, MF_STRING | (selectedMonitor_.empty() ? MF_CHECKED : 0),
                monitorBase, translate(ui::allDisplays));
    for (size_t i = 0; i < monitors.size(); ++i) {
        const auto bounds = monitors[i].bounds;
        auto label = translate(ui::display) + std::to_wstring(i + 1) + L" · " +
            std::to_wstring(bounds.right - bounds.left) + L" × " +
            std::to_wstring(bounds.bottom - bounds.top) + L" (" +
            std::to_wstring(bounds.left) + L", " + std::to_wstring(bounds.top) + L")";
        AppendMenuW(displays, MF_STRING | enabled(preferredFit_ != DWPOS_SPAN) |
                    (selectedMonitor_ == monitors[i].id ? MF_CHECKED : 0),
                    monitorBase + i + 1, label.c_str());
    }
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(displays), translate(ui::nextDisplay));
    AppendMenuW(popup, MF_STRING | enabled(video_ != nullptr), freezeFrame, translate(ui::freezeFrame));
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    const HMENU widgetMenu = CreatePopupMenu();
    for (size_t i = 0; i < widgetSizes.size(); ++i) {
        auto label = std::to_wstring(widgetSizes[i]);
        if (widgetSizes[i] == widgetDefaultSize) label += translate(ui::defaultSize);
        AppendMenuW(widgetMenu, MF_STRING | (widgetSizes[i] == widgetSize_ ? MF_CHECKED : 0),
                    widgetSizeBase + i, label.c_str());
    }
    AppendMenuW(widgetMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(widgetMenu, MF_STRING, resetWidgetPosition, translate(ui::resetWidgetPosition));
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(widgetMenu), translate(ui::widgetLayout));

    fs::path shortcut;
    bool startupEnabled{};
    const auto startupStatus = getStartupShortcut(shortcut, startupEnabled);
    AppendMenuW(popup, MF_STRING | enabled(SUCCEEDED(startupStatus)) | (startupEnabled ? MF_CHECKED : 0),
                toggleStartup, translate(ui::startWithWindows));
    const HMENU languages = CreatePopupMenu();
    AppendMenuW(languages, MF_STRING, selectEnglish, L"English");
    AppendMenuW(languages, MF_STRING, selectVietnamese, L"Tiếng Việt");
    CheckMenuRadioItem(languages, selectEnglish, selectVietnamese,
                       language_ == Language::english ? selectEnglish : selectVietnamese, MF_BYCOMMAND);
    AppendMenuW(popup, MF_POPUP, reinterpret_cast<UINT_PTR>(languages), translate(ui::language));
    AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(popup, MF_STRING, quit, translate(ui::exit));
    SetMenuDefaultItem(popup, resetDefault, FALSE);
    SetForegroundWindow(control_);
    const auto id = static_cast<UINT>(TrackPopupMenuEx(popup,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, point.x, point.y, control_, nullptr));
    PostMessageW(control_, WM_NULL, 0, 0);
    DestroyMenu(popup);
    if (id) command(id, monitors);
}

void App::command(UINT id, const std::vector<Monitor>& monitors) {
    switch (id) {
    case chooseDefault: setDefaultFile(); break;
    case resetDefault: restoreDefault(); break;
    case undoChange: undo(); break;
    case chooseMedia: {
        auto paths = pickFiles(control_, false, language_);
        if (!paths.empty()) useFiles(std::move(paths));
        break;
    }
    case freezeFrame: freeze(); break;
    case toggleStartup: {
        fs::path shortcut;
        bool enabled{};
        auto hr = getStartupShortcut(shortcut, enabled);
        if (SUCCEEDED(hr)) hr = setStartupShortcut(shortcut, !enabled);
        if (FAILED(hr)) report(ui::startupFailed, hr);
        break;
    }
    case selectEnglish:
    case selectVietnamese: {
        const auto selected = id == selectVietnamese ? Language::vietnamese : Language::english;
        if (selected != language_ && saveSetting(L"Language", selected == Language::vietnamese ? L"vi" : L"en")) {
            language_ = selected;
            if (pending_) updateTooltip(translate(ui::openingVideo));
            else updateWallpaperTooltip(current_.mode);
        }
        break;
    }
    case resetWidgetPosition:
        if (saveSetting(L"WidgetPosition", L"default")) {
            widgetPosition_.reset();
            layoutWidget();
        }
        break;
    case quit: PostQuitMessage(0); break;
    default:
        if (id >= fitBase && id <= fitBase + static_cast<UINT>(DWPOS_SPAN)) {
            setFit(static_cast<DESKTOP_WALLPAPER_POSITION>(id - fitBase));
        } else if (id >= intervalBase && id < intervalBase + intervals.size()) {
            setInterval(intervals[id - intervalBase]);
        } else if (id >= widgetSizeBase && id < widgetSizeBase + widgetSizes.size()) {
            const auto size = widgetSizes[id - widgetSizeBase];
            if (saveSetting(L"WidgetSize", std::to_wstring(size))) {
                widgetSize_ = size;
                layoutWidget();
            }
        } else if (id >= monitorBase && id <= monitorBase + monitors.size()) {
            const auto monitor = id == monitorBase ? std::wstring{} : monitors[id - monitorBase - 1].id;
            if (saveSetting(L"Monitor", monitor)) selectedMonitor_ = monitor;
        }
        break;
    }
}

void App::setDefaultFile() {
    const auto paths = pickFiles(control_, true, language_);
    if (paths.empty()) return;
    ComPtr<IWICBitmapFrameDecode> frame;
    auto hr = isImage(paths.front()) ? openImage(wic_.Get(), paths.front(), frame)
                                     : WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
    auto path = temporaryPng(directory_);
    if (SUCCEEDED(hr)) {
        if (path.empty()) {
            hr = E_FAIL;
        } else {
            path.replace_extension(paths.front().extension());
            if (!CopyFileW(paths.front().c_str(), path.c_str(), TRUE)) hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    if (FAILED(hr)) {
        if (!path.empty()) DeleteFileW(path.c_str());
        report(ui::defaultFailed, hr);
    } else if (saveSetting(L"Default", path.filename().wstring())) {
        defaultImage_ = path;
        report(ui::defaultSaved, S_OK);
    } else {
        DeleteFileW(path.c_str());
    }
}

void App::restoreDefault() {
    auto apply = [this] {
        Scene scene;
        scene.mode = Mode::image;
        scene.files = {defaultImage_};
        scene.fit = preferredFit_;
        switchScene(std::move(scene), false);
    };
    if (!defaultImage_.empty() && GetFileAttributesW(defaultImage_.c_str()) != INVALID_FILE_ATTRIBUTES) {
        apply();
        return;
    }
    setDefaultFile();
    if (!defaultImage_.empty() && GetFileAttributesW(defaultImage_.c_str()) != INVALID_FILE_ATTRIBUTES) {
        apply();
    }
}

void App::useFiles(std::vector<fs::path> paths) {
    if (!validSelection(paths)) {
        report(ui::invalidSelection, E_INVALIDARG);
        return;
    }
    Scene scene;
    if (preferredFit_ != DWPOS_SPAN) scene.monitor = selectedMonitor_;
    scene.fit = preferredFit_;
    scene.interval = preferredInterval_;
    if (isVideo(paths.front())) {
        scene.mode = Mode::video;
    } else {
        scene.mode = paths.size() == 1 ? Mode::image : Mode::album;
        for (const auto& path : paths) {
            ComPtr<IWICBitmapFrameDecode> frame;
            const auto hr = openImage(wic_.Get(), path, frame);
            if (FAILED(hr)) {
                report((translate(ui::imageFailed) + path.filename().wstring()).c_str(), hr);
                return;
            }
        }
        std::stable_sort(paths.begin(), paths.end(), [](const auto& a, const auto& b) {
            return StrCmpLogicalW(a.filename().c_str(), b.filename().c_str()) < 0;
        });
    }
    scene.files = std::move(paths);
    switchScene(std::move(scene), false);
}

HRESULT App::captureHistory(History& history) {
    history.scene = current_;
    if (video_) history.scene.videoTime = video_->time();
    return captureDesktop(desktop_.Get(), history.desktop);
}

HRESULT App::applyImage(const Scene& scene) {
    if (!scene.monitor.empty()) {
        RECT bounds{};
        if (desktop_->GetMonitorRECT(scene.monitor.c_str(), &bounds) != S_OK) {
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }
    }
    const auto& path = scene.files.at(scene.index);
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    DESKTOP_WALLPAPER_POSITION currentFit{};
    auto hr = desktop_->GetPosition(&currentFit);
    if (SUCCEEDED(hr) && currentFit != scene.fit) hr = desktop_->SetPosition(scene.fit);
    if (SUCCEEDED(hr)) hr = desktop_->SetWallpaper(
        scene.monitor.empty() ? nullptr : scene.monitor.c_str(), path.c_str());
    return hr;
}

HRESULT App::commit(Scene scene, bool undo, std::unique_ptr<VideoWallpaper> player) {
    History before;
    HRESULT hr = captureHistory(before);
    if (FAILED(hr)) return hr;
    if (!undo && scene.mode == Mode::image &&
        (current_.mode == Mode::system || current_.mode == Mode::image) &&
        before.desktop.enabled && !before.desktop.slideshow && before.desktop.position == scene.fit) {
        bool matchedMonitor = false;
        bool sameImage = true;
        for (const auto& monitor : before.desktop.monitors) {
            if (!scene.monitor.empty() && scene.monitor != monitor.id) continue;
            matchedMonitor = true;
            if (_wcsicmp(monitor.image.c_str(), scene.files.front().c_str()) != 0) sameImage = false;
        }
        if (matchedMonitor && sameImage) return S_FALSE;
    }
    if (undo) {
        hr = restoreDesktop(desktop_.Get(), history_->desktop);
    } else if (scene.mode == Mode::image || scene.mode == Mode::album) {
        hr = applyImage(scene);
    }
    if (FAILED(hr)) {
        restoreDesktop(desktop_.Get(), before.desktop);
        return hr;
    }
    if (undo) {
        preferredFit_ = scene.mode == Mode::system ? history_->desktop.position : scene.fit;
        saveSetting(L"Fit", std::to_wstring(preferredFit_));
        if (scene.mode == Mode::album) {
            preferredInterval_ = scene.interval;
            saveSetting(L"Interval", std::to_wstring(preferredInterval_));
        }
        history_.reset();
    } else {
        history_ = std::move(before);
    }
    if (player) {
        desktop_->GetBackgroundColor(&videoColor_);
        player->show();
    }
    updateWallpaperTooltip(scene.mode);
    video_ = std::move(player);
    videoPaused_ = false;
    visibilityCheck_ = 0;
    current_ = std::move(scene);
    syncTimers();
    if (current_.mode == Mode::album) SetTimer(control_, albumTimer, current_.interval * 1000, nullptr);
    return S_OK;
}

void App::switchScene(Scene scene, bool undo) {
    pending_.reset();
    const bool video = scene.mode == Mode::video;
    const auto hr = video ? openVideo(std::move(scene), undo) : commit(std::move(scene), undo);
    syncTimers();
    if (FAILED(hr)) {
        report(video ? ui::videoOpenFailed : ui::wallpaperFailed, hr);
    }
}

void App::undo() {
    if (!history_) return;
    if (video_ && history_->scene.mode == Mode::video &&
        current_.files == history_->scene.files && current_.monitor == history_->scene.monitor &&
        current_.fit != history_->scene.fit) {
        const auto& before = *history_;
        const auto hr = video_->draw(before.scene.fit, before.desktop.color);
        if (FAILED(hr)) {
            report(ui::videoUndoFailed, hr);
            return;
        }
        pending_.reset();
        videoColor_ = before.desktop.color;
        current_.fit = preferredFit_ = before.scene.fit;
        history_.reset();
        saveSetting(L"Fit", std::to_wstring(preferredFit_));
        syncTimers();
        return;
    }
    switchScene(history_->scene, true);
}

void App::freeze() {
    if (!video_) return;
    const auto path = temporaryPng(directory_);
    auto hr = path.empty() ? E_FAIL : video_->capture(wic_.Get(), path);
    if (SUCCEEDED(hr)) {
        Scene still = current_;
        still.mode = Mode::image;
        still.files = {path};
        still.index = 0;
        pending_.reset();
        hr = commit(std::move(still), false);
        syncTimers();
    }
    if (FAILED(hr)) {
        if (!path.empty()) DeleteFileW(path.c_str());
        report(ui::freezeFailed, hr);
    }
}

void App::setFit(DESKTOP_WALLPAPER_POSITION position) {
    if (position == DWPOS_SPAN && (!selectedMonitor_.empty() || !current_.monitor.empty())) return;
    if (position == preferredFit_ && (current_.mode != Mode::system || !pending_)) {
        DESKTOP_WALLPAPER_POSITION systemFit{};
        if (video_ || (SUCCEEDED(desktop_->GetPosition(&systemFit)) && systemFit == position)) return;
    }
    pending_.reset();
    syncTimers();
    History before;
    HRESULT hr = captureHistory(before);
    if (SUCCEEDED(hr)) {
        hr = video_ ? video_->draw(position, videoColor_) : desktop_->SetPosition(position);
    }
    if (FAILED(hr)) {
        report(ui::fitFailed, hr);
        return;
    }
    current_.fit = preferredFit_ = position;
    history_ = std::move(before);
    saveSetting(L"Fit", std::to_wstring(position));
}

void App::setInterval(UINT seconds) {
    if (!saveSetting(L"Interval", std::to_wstring(seconds))) return;
    preferredInterval_ = seconds;
    if (current_.mode != Mode::album) return;
    current_.interval = seconds;
    SetTimer(control_, albumTimer, seconds * 1000, nullptr);
}

std::vector<Monitor> App::videoMonitors(const Scene& scene) {
    auto monitors = getMonitors(desktop_.Get());
    if (!scene.monitor.empty()) {
        std::erase_if(monitors, [&scene](const Monitor& monitor) { return monitor.id != scene.monitor; });
    }
    return monitors;
}

HRESULT App::openVideo(Scene scene, bool undo) {
    if (!mediaStarted_) {
        const HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
        if (FAILED(hr)) return hr;
        mediaStarted_ = true;
    }
    auto player = std::make_unique<VideoWallpaper>();
    HRESULT hr = player->open(scene.files.front(), scene.videoTime);
    if (FAILED(hr)) return hr;
    pending_ = PendingVideo{std::move(player), std::move(scene), GetTickCount64() + 12000, undo};
    updateTooltip(translate(ui::openingVideo));
    return S_OK;
}

void App::updateVideo() {
    if (video_) {
        if (!video_->attached()) {
            desktopChanged();
        }
        const auto now = GetTickCount64();
        if (video_ && now >= visibilityCheck_) {
            visibilityCheck_ = now + 250;
            const bool paused = video_->coveredByForeground();
            if (paused != videoPaused_ && SUCCEEDED(video_->setPaused(paused))) {
                videoPaused_ = paused;
                syncTimers();
            }
        }
        if (video_ && !videoPaused_) {
            auto hr = video_->tick();
            if (hr == S_OK) hr = video_->draw(current_.fit, videoColor_);
            if (FAILED(hr)) stopFailedVideo(hr);
        }
    }
    if (!pending_) return;
    auto& player = pending_->player;
    auto hr = player->tick();
    if (hr == S_FALSE && GetTickCount64() < pending_->deadline) return;
    if (hr == S_FALSE) hr = HRESULT_FROM_WIN32(ERROR_TIMEOUT);
    if (SUCCEEDED(hr)) hr = player->attach(videoMonitors(pending_->scene));
    if (SUCCEEDED(hr)) {
        ensureWidget();
        COLORREF background{};
        desktop_->GetBackgroundColor(&background);
        if (pending_->undo) background = history_->desktop.color;
        hr = player->draw(pending_->scene.fit, background);
    }
    if (SUCCEEDED(hr)) hr = commit(std::move(pending_->scene), pending_->undo, std::move(player));
    pending_.reset();
    syncTimers();
    if (FAILED(hr)) report(ui::videoPlaybackFailed, hr);
}

void App::stopFailedVideo(HRESULT hr) {
    const auto path = temporaryPng(directory_);
    if (!path.empty() && SUCCEEDED(video_->capture(wic_.Get(), path))) {
        Scene still = current_;
        still.mode = Mode::image;
        still.files = {path};
        still.index = 0;
        if (SUCCEEDED(applyImage(still))) current_ = std::move(still);
        else DeleteFileW(path.c_str());
    } else if (!path.empty()) {
        DeleteFileW(path.c_str());
    }
    if (current_.mode == Mode::video) current_ = {};
    video_.reset();
    syncTimers();
    report(ui::videoStopped, hr);
}

void App::syncTimers() {
    if (current_.mode != Mode::album) KillTimer(control_, albumTimer);
    if (!video_ && !pending_) {
        KillTimer(control_, videoTimer);
        if (mediaStarted_) {
            MFShutdown();
            mediaStarted_ = false;
        }
    } else {
        SetTimer(control_, videoTimer, videoPaused_ && !pending_ ? 250 : 15, nullptr);
    }
}

void App::desktopChanged() {
    endWidgetGesture(false);
    ensureWidget();
    layoutWidget();
    if (!video_) return;
    auto hr = video_->attach(videoMonitors(current_));
    if (SUCCEEDED(hr)) hr = video_->draw(current_.fit, videoColor_);
    if (SUCCEEDED(hr)) video_->show();
    else stopFailedVideo(hr);
    ensureWidget();
}

App* App::fromWindow(HWND hwnd, UINT message, LPARAM lp) {
    if (message == WM_NCCREATE) {
        auto* app = static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    return reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK App::controlProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (auto* app = fromWindow(hwnd, message, lp)) return app->onControl(hwnd, message, wp, lp);
    return DefWindowProcW(hwnd, message, wp, lp);
}

LRESULT CALLBACK App::widgetProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (auto* app = fromWindow(hwnd, message, lp)) return app->onWidget(hwnd, message, wp, lp);
    return DefWindowProcW(hwnd, message, wp, lp);
}

LRESULT App::onControl(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    if (taskbarCreated_ && message == taskbarCreated_) {
        setupTray();
        widgetRetryUntil_ = GetTickCount64() + 8000;
        SetTimer(hwnd, desktopTimer, 400, nullptr);
        return 0;
    }
    switch (message) {
    case WM_MEASUREITEM: {
        auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lp);
        if (!item || item->CtlType != ODT_MENU || item->itemID != menuBrand || !item->itemData) break;
        reinterpret_cast<const MenuBranding*>(item->itemData)->measure(*item);
        return TRUE;
    }
    case WM_DRAWITEM: {
        const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
        if (!item || item->CtlType != ODT_MENU || item->itemID != menuBrand || !item->itemData) break;
        reinterpret_cast<const MenuBranding*>(item->itemData)->draw(*item);
        return TRUE;
    }
    case trayMessage: {
        const auto input = LOWORD(lp);
        if (input == WM_CONTEXTMENU || input == NIN_KEYSELECT) {
            POINT point{};
            GetCursorPos(&point);
            if (input == NIN_KEYSELECT) {
                NOTIFYICONIDENTIFIER icon{sizeof(icon)};
                icon.hWnd = control_;
                icon.uID = 1;
                RECT bounds{};
                if (SUCCEEDED(Shell_NotifyIconGetRect(&icon, &bounds))) point = {bounds.left, bounds.top};
            }
            menu(point);
        } else if (input == WM_LBUTTONDBLCLK) {
            restoreDefault();
        }
        return 0;
    }
    case dropMessage:
        if (queuedDrop_) {
            auto paths = std::move(*queuedDrop_);
            queuedDrop_.reset();
            useFiles(std::move(paths));
        }
        return 0;
    case widgetLayoutMessage:
        if (widgetGesture_ == WidgetGesture::none) layoutWidget();
        return 0;
    case widgetStackMessage:
        restackWidget(widget_ && GetForegroundWindow() == GetWindow(widget_, GW_OWNER));
        return 0;
    case WM_DISPLAYCHANGE:
    case WM_THEMECHANGED:
        widgetRetryUntil_ = GetTickCount64() + 8000;
        SetTimer(hwnd, desktopTimer, 400, nullptr);
        return 0;
    case WM_SETTINGCHANGE:
        updateWidgetContrast();
        if (widgetGesture_ == WidgetGesture::none) layoutWidget();
        return 0;
    case WM_TIMER:
        if (wp == desktopTimer) {
            KillTimer(hwnd, desktopTimer);
            desktopChanged();
            if (!widget_ && GetTickCount64() < widgetRetryUntil_) {
                SetTimer(hwnd, desktopTimer, 400, nullptr);
            }
        } else if (wp == videoTimer) {
            updateVideo();
        } else if (wp == albumTimer && current_.mode == Mode::album) {
            const auto original = current_.index;
            HRESULT hr = E_FAIL;
            do {
                current_.index = (current_.index + 1) % current_.files.size();
                hr = applyImage(current_);
            } while (FAILED(hr) && current_.index != original);
            if (FAILED(hr)) {
                KillTimer(hwnd, albumTimer);
                report(ui::albumFailed, hr);
            }
        } else if (wp == flashTimer) {
            KillTimer(hwnd, flashTimer);
            flashError_ = false;
            refreshWidget();
        }
        return 0;
    case WM_CLOSE:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

LRESULT App::onWidget(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) {
    switch (message) {
    case WM_PAINT:
        ValidateRect(hwnd, nullptr);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_ACTIVATE:
        PostMessageW(control_, widgetStackMessage, 0, 0);
        break;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        refreshWidget();
        return 0;
    case WM_SYSCOLORCHANGE:
        updateWidgetContrast();
        return 0;
    case WM_DPICHANGED:
        PostMessageW(control_, widgetLayoutMessage, 0, 0);
        return 0;
    case WM_SETCURSOR: {
        if (LOWORD(lp) != HTCLIENT) break;
        RECT bounds{};
        POINT point{};
        GetWindowRect(hwnd, &bounds);
        GetCursorPos(&point);
        const bool resize = widgetGesture_ == WidgetGesture::resizing || widgetResizeGrip(bounds, point);
        SetCursor(LoadCursorW(nullptr, resize ? IDC_SIZENWSE :
            widgetGesture_ == WidgetGesture::moving ? IDC_SIZEALL : IDC_HAND));
        return TRUE;
    }
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd, &point);
        RECT bounds{};
        GetWindowRect(hwnd, &bounds);
        if (message == WM_LBUTTONDBLCLK && widgetDoubleClick_ && !widgetResizeGrip(bounds, point)) {
            widgetDoubleClick_ = false;
            restoreDefault();
        } else {
            beginWidgetGesture(point);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
    case WM_LBUTTONUP:
        if (widgetGesture_ != WidgetGesture::none) {
            POINT point{};
            if (GetCursorPos(&point)) moveWidgetGesture(point);
            if (message == WM_LBUTTONUP) endWidgetGesture(true);
        }
        return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        endWidgetGesture(false);
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_RETURN && widgetGesture_ == WidgetGesture::none && !(lp & (1LL << 30))) {
            restoreDefault();
        }
        else if (wp == VK_ESCAPE) endWidgetGesture(false);
        return 0;
    case WM_CONTEXTMENU: {
        POINT point{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        if (point.x == -1 && point.y == -1) {
            RECT bounds{};
            GetWindowRect(hwnd, &bounds);
            point = {bounds.left, bounds.bottom};
        }
        menu(point);
        return 0;
    }
    case WM_DESTROY:
        widgetGesture_ = WidgetGesture::none;
        widgetDoubleClick_ = false;
        dragOver_ = false;
        if (dropRegistered_) {
            RevokeDragDrop(hwnd);
            dropRegistered_ = false;
        }
        return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    const HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\WallShift.Desktop.Widget");
    if (!singleton) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(singleton);
        return 0;
    }
    int exitCode = 1;
    if (SUCCEEDED(OleInitialize(nullptr))) {
        {
            App app(instance);
            const auto hr = app.initialize();
            if (SUCCEEDED(hr)) {
                MSG message{};
                BOOL result{};
                while ((result = GetMessageW(&message, nullptr, 0, 0)) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                exitCode = result == 0 ? static_cast<int>(message.wParam) : 1;
            } else {
                wchar_t message[128]{};
                swprintf_s(message, ui::launchFailed.in(app.language()),
                           static_cast<unsigned long>(hr));
                MessageBoxW(nullptr, message, appName, MB_OK | MB_ICONERROR);
            }
        }
        OleUninitialize();
    }
    CloseHandle(singleton);
    return exitCode;
}
