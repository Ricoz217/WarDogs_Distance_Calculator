#include "wardogs/settings.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <array>
#include <stdexcept>

namespace wardogs {
namespace {

std::wstring read_value(const std::filesystem::path& path, const wchar_t* key,
                        std::wstring_view fallback) {
    std::array<wchar_t, 8192> buffer{};
    GetPrivateProfileStringW(L"settings", key, std::wstring{fallback}.c_str(),
                             buffer.data(), static_cast<DWORD>(buffer.size()),
                             path.c_str());
    return buffer.data();
}

void write_value(const std::filesystem::path& path, const wchar_t* key,
                 const std::wstring& value) {
    if (!WritePrivateProfileStringW(L"settings", key, value.c_str(), path.c_str())) {
        throw std::runtime_error("cannot save settings");
    }
}

}  // namespace

std::filesystem::path settings_path() {
    PWSTR raw = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &raw))) {
        throw std::runtime_error("cannot locate LocalAppData");
    }
    const std::filesystem::path path =
        std::filesystem::path{raw} / L"WarDogsDistanceCalculatorCpp" / L"settings.ini";
    CoTaskMemFree(raw);
    return path;
}

AppSettings load_settings() {
    AppSettings settings;
    const auto path = settings_path();
    settings.region_hotkey = read_value(path, L"region_hotkey", settings.region_hotkey);
    settings.base_hotkey = read_value(path, L"base_hotkey", settings.base_hotkey);
    settings.target_hotkey = read_value(path, L"target_hotkey", settings.target_hotkey);
    settings.quick_target_hotkey =
        read_value(path, L"quick_target_hotkey", settings.quick_target_hotkey);
    settings.coordinate_pattern =
        read_value(path, L"coordinate_pattern", settings.coordinate_pattern);
    settings.backend = read_value(path, L"ocr_backend", L"rapid") == L"windows"
                           ? OcrBackend::windows
                           : OcrBackend::rapid;
    return settings;
}

void save_settings(const AppSettings& settings) {
    const auto path = settings_path();
    std::filesystem::create_directories(path.parent_path());
    write_value(path, L"region_hotkey", settings.region_hotkey);
    write_value(path, L"base_hotkey", settings.base_hotkey);
    write_value(path, L"target_hotkey", settings.target_hotkey);
    write_value(path, L"quick_target_hotkey", settings.quick_target_hotkey);
    write_value(path, L"coordinate_pattern", settings.coordinate_pattern);
    write_value(path, L"ocr_backend",
                settings.backend == OcrBackend::rapid ? L"rapid" : L"windows");
}

}  // namespace wardogs
