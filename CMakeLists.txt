cmake_minimum_required(VERSION 3.20)
project(WallShift VERSION 1.0.0 LANGUAGES CXX RC)

configure_file(src/WallShift.manifest.in WallShift.manifest @ONLY)
configure_file(src/version.h.in version.h @ONLY)

add_executable(WallShift WIN32
    src/main.cpp src/branding.cpp src/desktop.cpp src/startup.cpp src/video.cpp src/widget.cpp
    src/WallShift.rc)
target_include_directories(WallShift PRIVATE design "${CMAKE_CURRENT_BINARY_DIR}")
set_property(SOURCE src/WallShift.rc APPEND PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/design/wallshift-icon.ico"
    "${CMAKE_CURRENT_SOURCE_DIR}/design/wallshift-logo.png"
    "${CMAKE_CURRENT_BINARY_DIR}/WallShift.manifest"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h")
target_compile_features(WallShift PRIVATE cxx_std_20)
target_compile_definitions(WallShift PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(WallShift PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/W4;/utf-8;/permissive->)
set_property(TARGET WallShift PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
target_link_options(WallShift PRIVATE /MANIFEST:NO)
target_link_libraries(WallShift PRIVATE user32 gdi32 gdiplus msimg32 shell32 shcore shlwapi comctl32 ole32 oleaut32
    d3d11 dxgi d3dcompiler mfplat mfuuid windowscodecs uuid)
