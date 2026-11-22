# WallShift

[English](README.md) · [Tải bản Windows](https://github.com/kidrainzboy/WallShift/releases/latest)

WallShift là tiện ích siêu nhẹ cho Windows 11, dành cho kỹ thuật viên sự kiện cần đổi hình nền desktop nhanh. Trước chương trình, kéo hình nền sự kiện vào một widget nhỏ trên desktop. Khi kết thúc, nhấp đúp vào widget để về hình nền cá nhân hoặc công ty.

## Tính năng

- Thả một ảnh PNG hoặc JPEG, nhiều ảnh để chạy trình chiếu, hoặc một video MP4 mã hóa H.264. Video phát lặp và tắt tiếng. Trình chiếu hỗ trợ 5, 10, 15 và 30 giây mỗi ảnh.
- Chọn một hoặc tất cả màn hình cho lần đổi tiếp theo. Có sáu chế độ hiển thị gồm Fill, Fit, Stretch, Tile, Center và Span. Windows áp dụng kiểu hiển thị ảnh và trình chiếu trên toàn hệ thống.
- Hoàn tác lần đổi gần nhất trong phiên đang chạy hoặc lưu khung hình video hiện tại làm ảnh nền tĩnh.
- Kéo thân widget để di chuyển. Kéo góc dưới bên phải để đổi kích thước trong khoảng 48–128 DIP. Có thể dùng menu ở khay hệ thống.
- Chuyển giữa tiếng Anh và tiếng Việt, đồng thời tùy chọn mở WallShift khi đăng nhập Windows.

## Bắt đầu

1. Tải file thực thi x64 và `SHA256SUMS.txt` từ [bản phát hành mới nhất](https://github.com/kidrainzboy/WallShift/releases/latest).
2. Mở file thực thi từ một thư mục cố định. WallShift mở widget trên desktop và thêm biểu tượng vào khay hệ thống.
3. Nhấp chuột phải vào một trong hai biểu tượng và chọn **Choose default wallpaper…** hoặc **Chọn hình nền mặc định…**.
4. Thả tệp hình nền lên widget. Nhấp đúp để quay về nền mặc định.

Để kiểm tra file thực thi v1.0.0, chạy lệnh sau trong PowerShell rồi đối chiếu với dòng tương ứng trong `SHA256SUMS.txt`.

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath .\WallShift-1.0.0-windows-x64.exe
```

WallShift lưu cài đặt, ảnh mặc định và khung hình đã chụp trong `%LOCALAPPDATA%\WallShift`. Giữ nguyên vị trí tệp sự kiện khi đang sử dụng. Nếu di chuyển file thực thi sau khi bật **Khởi động cùng Windows**, mở file ở vị trí mới rồi tắt và bật lại tùy chọn để cập nhật lối tắt.

## Build từ mã nguồn

Cần Visual Studio với workload **Desktop development with C++**, Windows SDK và CMake 3.20 trở lên. Chạy các lệnh sau từ thư mục gốc của repo.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

File thực thi nằm ở `build/Release/WallShift.exe`. Ứng dụng dùng C++20, Win32, Media Foundation và Direct3D 11. Hình nền video cần lớp desktop của Explorer và bộ giải mã H.264 tương thích.

## Thông tin dự án

- [Nhật ký thay đổi](CHANGELOG.md)
- [Hướng dẫn đóng góp](CONTRIBUTING.md) và [quy tắc ứng xử](CODE_OF_CONDUCT.md)
- [Báo lỗi](https://github.com/kidrainzboy/WallShift/issues/new/choose) hoặc [báo cáo lỗ hổng riêng tư](SECURITY.md)
- [Kiểm tra build Windows](https://github.com/kidrainzboy/WallShift/actions/workflows/windows-build.yml)

WallShift là phần mềm mã nguồn mở theo [giấy phép MIT](LICENSE).
