#pragma once

#include "wardogs/capture.hpp"
#include "wardogs/core.hpp"
#include "wardogs/pinned_preferences.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace wardogs {

enum class OcrBackend { rapid, windows };

struct AppSettings {
    std::wstring region_hotkey{L"F8"};
    std::wstring base_hotkey{L"F9"};
    std::wstring target_hotkey{L"F10"};
    std::wstring quick_target_hotkey{L"F11"};
    OcrBackend backend{OcrBackend::rapid};
    std::wstring coordinate_pattern{default_ocr_coordinate_pattern};
    std::optional<CaptureRegion> capture_region;
    PinnedCardPreferences pinned_card;
};

std::filesystem::path settings_path();
AppSettings load_settings_from(const std::filesystem::path& path);
void save_settings_to(const std::filesystem::path& path,
                      const AppSettings& settings);
AppSettings load_settings();
void save_settings(const AppSettings& settings);

}  // namespace wardogs
