#pragma once

#include "wardogs/core.hpp"

#include <filesystem>
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
};

std::filesystem::path settings_path();
AppSettings load_settings();
void save_settings(const AppSettings& settings);

}  // namespace wardogs
