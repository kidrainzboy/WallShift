# WallShift

[Tiếng Việt](README.vi.md) · [Download for Windows](https://github.com/kidrainzboy/WallShift/releases/latest)

WallShift is a super lightweight Windows 11 utility for event crews who need to change the desktop wallpaper quickly. Drag event artwork onto a small desktop widget before the show. When the event ends, double-click the widget to restore your personal or company wallpaper.

## Features

- Drop a PNG or JPEG image, multiple images for a slideshow, or one H.264 MP4. Videos loop without sound. Slideshows support 5, 10, 15, and 30 seconds per image.
- Choose one display or all displays for the next wallpaper change. Pick Fill, Fit, Stretch, Tile, Center, or Span. Windows applies the image and slideshow fit setting system-wide.
- Undo the last change in the current session or save the current video frame as a still wallpaper.
- Drag the widget to move it. Drag its bottom-right grip to resize it between 48 and 128 device-independent pixels. You can also use the tray menu.
- Switch between English and Vietnamese and optionally start WallShift when you sign in to Windows.

## Quick start

1. Download the x64 executable and `SHA256SUMS.txt` from the [latest release](https://github.com/kidrainzboy/WallShift/releases/latest).
2. Run the executable from a folder you plan to keep. WallShift opens a desktop widget and adds an icon to the notification area.
3. Right-click either icon and select **Choose default wallpaper…**.
4. Drop your wallpaper files onto the widget. Double-click it when you want your default wallpaper back.

To check the v1.0.0 executable, run the following in PowerShell and compare the result with the corresponding line in `SHA256SUMS.txt`.

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath .\WallShift-1.0.0-windows-x64.exe
```

WallShift stores settings, the default image, and captured video frames in `%LOCALAPPDATA%\WallShift`. Keep event files in place while using them. If you move the executable after enabling **Start with Windows**, open it at its new location and toggle the option off and on to refresh the shortcut.

## Build from source

Use Visual Studio with the **Desktop development with C++** workload, a Windows SDK, and CMake 3.20 or newer. Run these commands from the repository root.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

The executable is `build/Release/WallShift.exe`. The app uses C++20, Win32, Media Foundation, and Direct3D 11. Live video wallpaper depends on the Explorer desktop layer and a compatible H.264 decoder.

## Project links

- [Changelog](CHANGELOG.md)
- [Contributing](CONTRIBUTING.md) and [Code of conduct](CODE_OF_CONDUCT.md)
- [Report an issue](https://github.com/kidrainzboy/WallShift/issues/new/choose) or [report a vulnerability privately](SECURITY.md)
- [Windows build checks](https://github.com/kidrainzboy/WallShift/actions/workflows/windows-build.yml)

WallShift is open source under the [MIT License](LICENSE).
