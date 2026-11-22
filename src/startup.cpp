#include "startup.hpp"

#include <shlobj.h>
#include <wrl/client.h>
#include <string>

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;

HRESULT getStartupShortcut(fs::path& path, bool& enabled) {
    PWSTR folder = nullptr;
    const auto hr = SHGetKnownFolderPath(FOLDERID_Startup, 0, nullptr, &folder);
    if (FAILED(hr)) return hr;
    path = fs::path(folder) / L"WallShift.lnk";
    CoTaskMemFree(folder);

    const auto attributes = GetFileAttributesW(path.c_str());
    enabled = false;
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        const auto error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            S_OK : HRESULT_FROM_WIN32(error);
    }
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) return HRESULT_FROM_WIN32(ERROR_DIRECTORY);
    enabled = true;
    return S_OK;
}

HRESULT setStartupShortcut(const fs::path& path, bool enabled) {
    if (!enabled) {
        if (DeleteFileW(path.c_str())) return S_OK;
        const auto error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ?
            S_OK : HRESULT_FROM_WIN32(error);
    }

    std::wstring module(32768, L'\0');
    const auto length = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    if (length == 0) return HRESULT_FROM_WIN32(GetLastError());
    if (length == module.size()) return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
    module.resize(length);
    const fs::path executable(module);
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;
    auto hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (SUCCEEDED(hr)) hr = link->SetPath(executable.c_str());
    if (SUCCEEDED(hr)) hr = link->SetWorkingDirectory(executable.parent_path().c_str());
    if (SUCCEEDED(hr)) hr = link->SetDescription(L"WallShift");
    if (SUCCEEDED(hr)) hr = link.As(&file);
    if (SUCCEEDED(hr)) hr = file->Save(path.c_str(), TRUE);
    return hr;
}

HRESULT migrateStartupShortcut(const fs::path& path, bool enabled) {
    const auto legacyPath = path.parent_path() / L"WallChanger.lnk";
    const auto attributes = GetFileAttributesW(legacyPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return S_FALSE;
    }
    ComPtr<IShellLinkW> link;
    ComPtr<IPersistFile> file;
    auto hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (SUCCEEDED(hr)) hr = link.As(&file);
    if (SUCCEEDED(hr)) hr = file->Load(legacyPath.c_str(), STGM_READ);
    wchar_t target[32768]{};
    if (SUCCEEDED(hr)) hr = link->GetPath(target, ARRAYSIZE(target), nullptr, SLGP_RAWPATH);
    if (FAILED(hr)) return hr;
    if (_wcsicmp(fs::path(target).filename().c_str(), L"WallChanger.exe") != 0) return S_FALSE;
    if (!enabled) hr = setStartupShortcut(path, true);
    if (SUCCEEDED(hr)) hr = setStartupShortcut(legacyPath, false);
    return hr;
}
