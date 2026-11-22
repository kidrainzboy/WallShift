#pragma once

enum class Language { english, vietnamese };

struct LocalizedText {
    const wchar_t* english;
    const wchar_t* vietnamese;

    constexpr const wchar_t* in(Language language) const {
        return language == Language::vietnamese ? vietnamese : english;
    }
};

namespace ui {
inline constexpr LocalizedText widgetHelp{
    L"WallShift · Drop wallpaper or double-click to restore the default. Drag the corner to resize.",
    L"WallShift · Thả hình nền hoặc nhấp đúp để về nền mặc định. Kéo góc để đổi cỡ."
};
inline constexpr LocalizedText pictureFilter{L"Pictures (PNG, JPG, JPEG)", L"Ảnh (PNG, JPG, JPEG)"};
inline constexpr LocalizedText mediaFilter{L"Pictures or video", L"Ảnh hoặc video"};
inline constexpr LocalizedText chooseMedia{L"Choose wallpaper…", L"Chọn hình nền…"};
inline constexpr LocalizedText chooseDefault{L"Choose default wallpaper…", L"Chọn hình nền mặc định…"};
inline constexpr LocalizedText chooseMediaTitle{L"Choose wallpaper", L"Chọn hình nền"};
inline constexpr LocalizedText chooseDefaultTitle{L"Choose default wallpaper", L"Chọn hình nền mặc định"};
inline constexpr LocalizedText restoreDefault{L"Restore default wallpaper", L"Về hình nền mặc định"};
inline constexpr LocalizedText undo{L"Undo last change", L"Hoàn tác lần đổi gần nhất"};
inline constexpr LocalizedText wallpaperFits[]{
    {L"Center", L"Căn giữa (Center)"}, {L"Tile", L"Lặp ô (Tile)"},
    {L"Stretch", L"Kéo giãn (Stretch)"}, {L"Fit", L"Vừa khung (Fit)"},
    {L"Fill", L"Lấp đầy (Fill)"}, {L"Span", L"Trải nhiều màn hình (Span)"}
};
inline constexpr LocalizedText fit{L"Wallpaper fit", L"Kiểu hiển thị hình nền"};
inline constexpr LocalizedText secondsPerImage{L" sec per image", L" giây/ảnh"};
inline constexpr LocalizedText albumInterval{L"Slideshow interval", L"Thời gian chuyển ảnh"};
inline constexpr LocalizedText allDisplays{L"All displays", L"Tất cả màn hình"};
inline constexpr LocalizedText display{L"Display ", L"Màn hình "};
inline constexpr LocalizedText nextDisplay{L"Display for the next change", L"Màn hình cho lần đổi sau"};
inline constexpr LocalizedText freezeFrame{L"Use current video frame", L"Dùng khung hình hiện tại"};
inline constexpr LocalizedText defaultSize{L" (default)", L" (mặc định)"};
inline constexpr LocalizedText resetWidgetPosition{L"Move to the top-right corner", L"Đưa về góc trên bên phải"};
inline constexpr LocalizedText widgetLayout{L"Widget size and position", L"Kích thước và vị trí widget"};
inline constexpr LocalizedText startWithWindows{L"Start with Windows", L"Khởi động cùng Windows"};
inline constexpr LocalizedText language{L"Language", L"Ngôn ngữ"};
inline constexpr LocalizedText exit{L"Exit WallShift", L"Thoát WallShift"};
inline constexpr LocalizedText settingsFailed{L"Couldn't save the settings.", L"Không lưu được cài đặt."};
inline constexpr LocalizedText startupFailed{
    L"Couldn't change the Windows startup setting.", L"Không đổi được tùy chọn khởi động cùng Windows."
};
inline constexpr LocalizedText defaultFailed{
    L"Couldn't save the default wallpaper. Choose a valid PNG or JPEG image.",
    L"Không thể lưu hình nền mặc định. Hãy chọn một ảnh PNG hoặc JPEG hợp lệ."
};
inline constexpr LocalizedText defaultSaved{L"Default wallpaper saved.", L"Đã lưu hình nền mặc định."};
inline constexpr LocalizedText invalidSelection{
    L"Choose PNG, JPG or JPEG images, or one MP4 video.",
    L"Hãy chọn ảnh PNG, JPG, JPEG hoặc một video MP4."
};
inline constexpr LocalizedText imageFailed{L"Couldn't open image ", L"Không mở được ảnh "};
inline constexpr LocalizedText videoPlaying{L"WallShift · Video wallpaper is playing", L"WallShift · Đang phát hình nền video"};
inline constexpr LocalizedText albumPlaying{L"WallShift · Wallpaper slideshow is running", L"WallShift · Đang chạy album hình nền"};
inline constexpr LocalizedText videoOpenFailed{
    L"Couldn't open the video as wallpaper.", L"Không mở được video làm hình nền."
};
inline constexpr LocalizedText wallpaperFailed{L"Couldn't change the wallpaper.", L"Không đổi được hình nền."};
inline constexpr LocalizedText videoUndoFailed{
    L"Couldn't undo the video fit change.", L"Không thể hoàn tác kiểu hiển thị của video."
};
inline constexpr LocalizedText freezeFailed{
    L"Couldn't use the current frame as wallpaper.", L"Không thể dùng khung hình hiện tại làm hình nền."
};
inline constexpr LocalizedText fitFailed{L"Couldn't change the wallpaper fit.", L"Không thể đổi kiểu hiển thị hình nền."};
inline constexpr LocalizedText openingVideo{L"WallShift · Opening video…", L"WallShift · Đang mở video…"};
inline constexpr LocalizedText videoPlaybackFailed{
    L"Couldn't play the video. Check the H.264 MP4 file and connected displays.",
    L"Không phát được video. Kiểm tra tệp MP4 H.264 và màn hình đang kết nối."
};
inline constexpr LocalizedText videoStopped{
    L"Video stopped. A still wallpaper is showing.", L"Video đã dừng. Hình nền tĩnh đang hiển thị."
};
inline constexpr LocalizedText albumFailed{
    L"Couldn't continue the slideshow. Check the images and target display.",
    L"Không thể tiếp tục album. Kiểm tra các ảnh và màn hình đích."
};
inline constexpr LocalizedText launchFailed{
    L"Couldn't start WallShift (0x%08lX).", L"Không thể khởi động WallShift (0x%08lX)."
};
}
