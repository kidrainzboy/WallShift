# WallShift

WallShift is a super lightweight Windows 11 utility for event crews who need to change the desktop wallpaper quickly. Drag the event artwork onto a small desktop widget before the show. When the event ends, double-click the widget to restore your personal or company wallpaper.

- **Drop an image, a slideshow, or a video.** Use PNG or JPEG images, or loop a muted H.264 MP4. Slideshows support 5, 10, 15, and 30 seconds per image.
- **Choose where it goes.** Apply the next wallpaper to one display or all displays, with six Windows wallpaper fit modes.
- **Recover from a misdrop.** Undo the last change, or save the current video frame as a still wallpaper.
- **Keep it within reach.** Move and resize the widget, use the system tray menu, or enable Start with Windows.

Built in C++ with Win32, Media Foundation, and Direct3D 11. Available in English and Vietnamese.

## Sử dụng

- Chuột phải vào widget hoặc biểu tượng khay hệ thống. Giao diện mặc định là tiếng Anh. Chọn **Language → Tiếng Việt** để đổi ngay.
- Chọn **Choose default wallpaper…** để lưu ảnh nền cá nhân hoặc công ty. Nhấp đúp widget để áp dụng ảnh này lên tất cả màn hình.
- Thả một ảnh **PNG, JPG hoặc JPEG** để đặt nền tĩnh. Thả nhiều ảnh để chạy album theo tên file. Widget hiện biểu tượng **Điểm thả** khi giữ tệp hợp lệ trên widget. **Slideshow interval** có các mốc **5, 10, 15 và 30 giây/ảnh**.
- Thả một video **MP4 (H.264)** để phát lặp và tắt tiếng. **Use current video frame** chuyển khung hình đang phát thành ảnh nền tĩnh.
- **Wallpaper fit** có sáu chế độ của Windows. **Display for the next change** chọn màn hình cho lần thả tiếp theo. **Undo last change** hoàn tác lần đổi gần nhất trong phiên đang chạy.
- Kéo thân widget để di chuyển. Kéo góc dưới bên phải để đổi kích thước trong khoảng **48–128 DIP**. **Widget size and position** có các cỡ đặt sẵn và lệnh đưa về góc trên bên phải.
- Bật **Start with Windows** để mở WallShift khi đăng nhập tài khoản Windows hiện tại. Tùy chọn mặc định tắt. Nếu chuyển file thực thi sang vị trí mới, mở file ở vị trí mới rồi tắt và bật lại tùy chọn để cập nhật đường dẫn.

Ảnh mặc định, ảnh chụp từ video và cài đặt nằm trong `%LOCALAPPDATA%\WallShift`. Ảnh và video thả vào được đọc từ đường dẫn gốc.

## Build

Dùng Windows 11, Visual Studio với workload **Desktop development with C++**, Windows SDK và CMake.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

File build nằm tại `build/Release/WallShift.exe`. Bản để sử dụng được đặt ở `release/WallShift.exe` để giữ một đường dẫn ổn định khi bật tự khởi động.

## License

WallShift is released under the [MIT License](LICENSE).
