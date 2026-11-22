#pragma once

#include <windows.h>
#include <filesystem>

HRESULT getStartupShortcut(std::filesystem::path& path, bool& enabled);
HRESULT setStartupShortcut(const std::filesystem::path& path, bool enabled);
HRESULT migrateStartupShortcut(const std::filesystem::path& path, bool enabled);
