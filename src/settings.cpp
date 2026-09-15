#include "wardogs/settings.hpp"

#include <Windows.h>
#include <ShlObj.h>
#include <wrl/client.h>

#include <array>
#include <algorithm>
#include <optional>
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

std::optional<long> read_integer(const std::filesystem::path& path,
                                 const wchar_t* key) {
    const std::wstring text = read_value(path, key, L"");
    if (text.empty()) return std::nullopt;
    try {
        std::size_t consumed = 0;
        const long value = std::stol(text, &consumed);
        if (consumed != text.size()) return std::nullopt;
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void remove_value(const std::filesystem::path& path, const wchar_t* key) {
    if (!WritePrivateProfileStringW(L"settings", key, nullptr, path.c_str())) {
        throw std::runtime_error("cannot remove settings value");
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

AppSettings load_settings_from(const std::filesystem::path& path) {
    AppSettings settings;
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
    settings.pinned_card.locked =
        read_value(path, L"pinned_card_locked", L"0") == L"1";
    settings.pinned_card.unlock_hotkey = read_value(
        path, L"pinned_card_unlock_hotkey",
        settings.pinned_card.unlock_hotkey);
    if (const auto opacity = read_integer(path, L"pinned_card_opacity_percent")) {
        settings.pinned_card.opacity_percent = static_cast<int>(std::clamp(
            *opacity,
            static_cast<long>(PinnedCardPreferences::minimum_opacity_percent),
            static_cast<long>(PinnedCardPreferences::maximum_opacity_percent)));
    }
    const std::wstring monitor = read_value(path, L"capture_monitor", L"");
    const auto left = read_integer(path, L"capture_left");
    const auto top = read_integer(path, L"capture_top");
    const auto right = read_integer(path, L"capture_right");
    const auto bottom = read_integer(path, L"capture_bottom");
    if (!monitor.empty() && left && top && right && bottom && *left >= 0 &&
        *top >= 0 && *right > *left && *bottom > *top) {
        settings.capture_region = CaptureRegion{
            monitor, {static_cast<LONG>(*left), static_cast<LONG>(*top),
                      static_cast<LONG>(*right), static_cast<LONG>(*bottom)}};
    }
    return settings;
}

void save_settings_to(const std::filesystem::path& path,
                      const AppSettings& settings) {
    std::filesystem::create_directories(path.parent_path());
    write_value(path, L"region_hotkey", settings.region_hotkey);
    write_value(path, L"base_hotkey", settings.base_hotkey);
    write_value(path, L"target_hotkey", settings.target_hotkey);
    write_value(path, L"quick_target_hotkey", settings.quick_target_hotkey);
    write_value(path, L"coordinate_pattern", settings.coordinate_pattern);
    write_value(path, L"ocr_backend",
                settings.backend == OcrBackend::rapid ? L"rapid" : L"windows");
    write_value(path, L"pinned_card_locked",
                settings.pinned_card.locked ? L"1" : L"0");
    write_value(path, L"pinned_card_unlock_hotkey",
                settings.pinned_card.unlock_hotkey);
    write_value(path, L"pinned_card_opacity_percent",
                std::to_wstring(std::clamp(
                    settings.pinned_card.opacity_percent,
                    PinnedCardPreferences::minimum_opacity_percent,
                    PinnedCardPreferences::maximum_opacity_percent)));
    if (settings.capture_region) {
        const auto& region = *settings.capture_region;
        write_value(path, L"capture_monitor", region.monitor_device);
        write_value(path, L"capture_left", std::to_wstring(region.relative.left));
        write_value(path, L"capture_top", std::to_wstring(region.relative.top));
        write_value(path, L"capture_right", std::to_wstring(region.relative.right));
        write_value(path, L"capture_bottom", std::to_wstring(region.relative.bottom));
    } else {
        for (const wchar_t* key : {L"capture_monitor", L"capture_left", L"capture_top",
                                   L"capture_right", L"capture_bottom"}) {
            remove_value(path, key);
        }
    }
}

AppSettings load_settings() { return load_settings_from(settings_path()); }

void save_settings(const AppSettings& settings) {
    save_settings_to(settings_path(), settings);
}

}  // namespace wardogs
