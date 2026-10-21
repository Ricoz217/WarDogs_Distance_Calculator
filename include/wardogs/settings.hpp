#pragma once

#include "wardogs/capture.hpp"
#include "wardogs/core.hpp"
#include "wardogs/ghost_reticle.hpp"
#include "wardogs/hotkeys.hpp"
#include "wardogs/pinned_preferences.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace wardogs {

inline constexpr std::size_t max_custom_hotkey_profiles = 100;

enum class OcrBackend { rapid, windows };

struct HotkeyProfile {
    std::wstring id;
    std::wstring name;
    std::wstring description;
    bool builtin{};
    std::array<std::wstring, 7> hotkeys;
    std::array<HotkeyTrigger, 7> triggers{};
};

struct AppSettings {
    std::wstring region_hotkey{L"F8"};
    std::wstring base_hotkey{L"F9"};
    std::wstring target_hotkey{L"F10"};
    std::wstring quick_target_hotkey{L"F11"};
    std::wstring impact_hotkey{L"F12"};
    std::wstring ghost_arc_hotkey{L"F4"};
    // Disabled by default: users opt in explicitly.
    std::wstring ghost_toggle_hotkey{};
    HotkeyTrigger region_trigger{HotkeyTrigger::single};
    HotkeyTrigger base_trigger{HotkeyTrigger::single};
    HotkeyTrigger target_trigger{HotkeyTrigger::single};
    HotkeyTrigger quick_target_trigger{HotkeyTrigger::single};
    HotkeyTrigger impact_trigger{HotkeyTrigger::single};
    HotkeyTrigger ghost_arc_trigger{HotkeyTrigger::single};
    HotkeyTrigger ghost_toggle_trigger{HotkeyTrigger::single};
    std::wstring hotkey_profile{L"classic"};
    std::vector<HotkeyProfile> custom_hotkey_profiles;
    OcrBackend backend{OcrBackend::rapid};
    std::wstring coordinate_pattern{default_ocr_coordinate_pattern};
    std::optional<CaptureRegion> capture_region;
    PinnedCardPreferences pinned_card;
    GhostReticlePreferences ghost_reticle;
};

[[nodiscard]] const std::vector<HotkeyProfile>& builtin_hotkey_profiles();

std::filesystem::path settings_path();
AppSettings load_settings_from(const std::filesystem::path& path);
void save_settings_to(const std::filesystem::path& path,
                      const AppSettings& settings);
AppSettings load_settings();
void save_settings(const AppSettings& settings);

}  // namespace wardogs
