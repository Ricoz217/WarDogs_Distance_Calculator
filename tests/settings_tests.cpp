#include "wardogs/settings.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <string>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

}  // namespace

int main() {
    namespace fs = std::filesystem;
    using wardogs::AppSettings;
    using wardogs::CaptureRegion;

    const fs::path directory = fs::temp_directory_path() /
                               L"wardogs-distance-calculator-settings-test";
    const fs::path path = directory / L"settings.ini";
    fs::remove_all(directory);

    AppSettings saved;
    check(saved.impact_hotkey == L"F12",
          "the impact OCR shortcut defaults to F12");
    check(saved.ghost_arc_hotkey == L"F4",
          "the ghost trajectory shortcut defaults to the unused F4 key");
    check(saved.ghost_toggle_hotkey.empty(),
          "the ghost reticle toggle hotkey is disabled by default");
    check(saved.hotkey_profile == L"classic",
          "the classic hotkey profile is selected by default");
    check(saved.custom_hotkey_profiles.empty(),
          "no custom hotkey profiles exist by default");
    const auto& builtins = wardogs::builtin_hotkey_profiles();
    check(builtins.size() == 2 && builtins[0].id == L"classic" &&
              builtins[1].id == L"advanced" &&
              builtins[1].name == L"默认配置2",
          "two built-in hotkey profiles are available");
    check(builtins[0].hotkeys[0] == L"F8" &&
              builtins[0].triggers[0] == wardogs::HotkeyTrigger::single &&
              builtins[1].hotkeys[0] == L"F8" &&
              builtins[1].triggers[0] ==
                  wardogs::HotkeyTrigger::long_press &&
              builtins[1].hotkeys[2] == L"F9" &&
              builtins[1].triggers[3] ==
                  wardogs::HotkeyTrigger::double_press &&
              builtins[1].hotkeys[6] == L"F10" &&
              builtins[1].triggers[6] ==
                  wardogs::HotkeyTrigger::long_press,
          "the two default profiles use their intended mappings");
    check(saved.ghost_reticle.bearing_compensation_deg == 0.0,
          "ghost bearing compensation defaults to zero degrees");
    saved.region_hotkey = L"Ctrl+F8";
    saved.impact_hotkey = L"Ctrl+Shift+F12";
    saved.ghost_arc_hotkey = L"Ctrl+Shift+G";
    saved.ghost_toggle_hotkey = L"Ctrl+Alt+G";
    saved.target_trigger = wardogs::HotkeyTrigger::double_press;
    saved.ghost_arc_trigger = wardogs::HotkeyTrigger::long_press;
    saved.ghost_reticle.opacity_percent = 72;
    saved.ghost_reticle.width = 1120;
    saved.ghost_reticle.bearing_compensation_deg = -0.35;
    saved.ghost_reticle.preferred_arc = wardogs::Arc::high;
    saved.ghost_reticle.color_hue = 187;
    saved.ghost_reticle.color_saturation = 88;
    saved.ghost_reticle.color_lightness = 46;
    saved.ghost_reticle.line_width_tenths = 35;
    saved.ghost_reticle.outline_width_tenths = 20;
    saved.ghost_reticle.outline_enabled = true;
    saved.pinned_card.locked = true;
    saved.pinned_card.opacity_percent = 63;
    saved.pinned_card.unlock_hotkey = L"Ctrl+Shift+U";
    saved.capture_region = CaptureRegion{L"\\\\.\\DISPLAY2", {13, 27, 413, 81}};
    wardogs::save_settings_to(path, saved);

    const AppSettings loaded = wardogs::load_settings_from(path);
    check(loaded.region_hotkey == saved.region_hotkey,
          "ordinary settings survive the explicit-path round trip");
    check(loaded.impact_hotkey == saved.impact_hotkey,
          "the impact OCR shortcut survives the explicit-path round trip");
    check(loaded.ghost_arc_hotkey == saved.ghost_arc_hotkey,
          "the ghost trajectory shortcut survives the explicit-path round trip");
    check(loaded.ghost_toggle_hotkey == L"Ctrl+Alt+G",
          "the ghost reticle toggle hotkey survives the explicit-path round trip");
    check(loaded.target_trigger == wardogs::HotkeyTrigger::double_press &&
              loaded.ghost_arc_trigger == wardogs::HotkeyTrigger::long_press &&
              loaded.base_trigger == wardogs::HotkeyTrigger::single,
          "hotkey trigger modes survive the explicit-path round trip");
    check(loaded.hotkey_profile.rfind(L"custom-", 0) == 0,
          "changing the active hotkeys selects a custom profile");
    check(loaded.custom_hotkey_profiles.size() == 1 &&
              loaded.custom_hotkey_profiles.front().name == L"我的配置" &&
              loaded.custom_hotkey_profiles.front().hotkeys[0] == L"Ctrl+F8" &&
              loaded.custom_hotkey_profiles.front().triggers[2] ==
                  wardogs::HotkeyTrigger::double_press,
          "currently configured hotkeys migrate into a preserved custom profile");
    check(loaded.ghost_reticle.opacity_percent == 72 &&
              loaded.ghost_reticle.width == 1120 &&
              std::abs(loaded.ghost_reticle.bearing_compensation_deg + 0.35) <
                  1e-9 &&
              loaded.ghost_reticle.preferred_arc == wardogs::Arc::high,
          "ghost reticle size, opacity, bearing offset, and trajectory persist");
    check(loaded.ghost_reticle.color_hue == 187 &&
              loaded.ghost_reticle.color_saturation == 88 &&
              loaded.ghost_reticle.color_lightness == 46 &&
              loaded.ghost_reticle.line_width_tenths == 35 &&
              loaded.ghost_reticle.outline_width_tenths == 20 &&
              loaded.ghost_reticle.outline_enabled,
          "ghost HSL color, line width, and outline survive the round trip");
    check(loaded.pinned_card.locked,
          "the pinned card lock state survives the explicit-path round trip");
    check(loaded.pinned_card.opacity_percent == 63,
          "the pinned card opacity survives the explicit-path round trip");
    check(loaded.pinned_card.unlock_hotkey == L"Ctrl+Shift+U",
          "the pinned-card unlock hotkey survives the explicit-path round trip");
    check(loaded.capture_region.has_value(),
          "a configured OCR capture region is restored");
    if (loaded.capture_region) {
        check(loaded.capture_region->monitor_device == saved.capture_region->monitor_device,
              "the persisted region keeps its monitor device");
        check(loaded.capture_region->relative.left == 13 &&
                  loaded.capture_region->relative.top == 27 &&
                  loaded.capture_region->relative.right == 413 &&
                  loaded.capture_region->relative.bottom == 81,
              "the persisted region keeps its monitor-relative rectangle");
    }
    try {
        check(wardogs::parse_ocr_coordinate(L"x99.67, y11.06",
                                             loaded.coordinate_pattern) ==
                  wardogs::Point{99.67, 11.06},
              "a persisted default regex parses an ordinary coordinate pair");
    } catch (const std::invalid_argument&) {
        check(false, "a persisted default regex remains usable after INI round trip");
    }

    {
        std::wofstream invalid(path, std::ios::trunc);
        invalid << L"[settings]\n"
                   L"capture_monitor=\\\\.\\DISPLAY2\n"
                   L"capture_left=50\n"
                   L"capture_top=40\n"
                   L"capture_right=20\n"
                   L"capture_bottom=10\n";
    }
    check(!wardogs::load_settings_from(path).capture_region,
          "an empty or inverted persisted rectangle is ignored");

    {
        std::wofstream invalid_opacity(path, std::ios::trunc);
        invalid_opacity << L"[settings]\n"
                           L"pinned_card_opacity_percent=2\n";
    }
    check(wardogs::load_settings_from(path).pinned_card.opacity_percent == 35,
          "persisted card opacity is clamped to the visible minimum");

    {
        std::wofstream invalid_ghost(path, std::ios::trunc);
        invalid_ghost << L"[settings]\n"
                         L"ghost_reticle_opacity_percent=1\n"
                         L"ghost_reticle_width=12\n"
                         L"ghost_reticle_line_width_tenths=1\n"
                         L"ghost_reticle_outline_width_tenths=99\n"
                         L"ghost_reticle_color_hue=999\n"
                         L"ghost_reticle_color_saturation=-5\n"
                         L"ghost_reticle_color_lightness=120\n";
    }
    const auto clamped_ghost = wardogs::load_settings_from(path).ghost_reticle;
    check(clamped_ghost.opacity_percent == 20 && clamped_ghost.width == 480 &&
              clamped_ghost.line_width_tenths == 10 &&
              clamped_ghost.outline_width_tenths == 40 &&
              clamped_ghost.color_hue == 359 &&
              clamped_ghost.color_saturation == 0 &&
              clamped_ghost.color_lightness == 100,
          "invalid ghost style values fall back to usable defaults");

    saved.capture_region.reset();
    saved.base_hotkey.clear();
    wardogs::save_settings_to(path, saved);
    check(!wardogs::load_settings_from(path).capture_region,
          "saving an empty region removes an earlier persisted region");
    const AppSettings blanked = wardogs::load_settings_from(path);
    check(blanked.base_hotkey.empty(),
          "a cleared hotkey persists as disabled instead of recovering its default");

    {
        std::wofstream fresh(path, std::ios::trunc);
        fresh << L"[settings]\n";
    }
    const AppSettings defaults = wardogs::load_settings_from(path);
    check(defaults.base_hotkey == L"F9",
          "a missing hotkey key still falls back to its built-in default");
    check(defaults.ghost_toggle_hotkey.empty(),
          "the ghost reticle toggle stays opt-in for existing installations");
    check(defaults.target_trigger == wardogs::HotkeyTrigger::single,
          "missing trigger entries default to single press");
    check(defaults.hotkey_profile == L"classic" &&
              defaults.custom_hotkey_profiles.empty(),
          "a fresh settings file uses the classic profile without custom copies");

    AppSettings profile_saved;
    profile_saved.region_hotkey = L"F8";
    profile_saved.region_trigger = wardogs::HotkeyTrigger::long_press;
    profile_saved.base_hotkey = L"F8";
    profile_saved.base_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.target_hotkey = L"F9";
    profile_saved.target_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.quick_target_hotkey = L"F9";
    profile_saved.quick_target_trigger = wardogs::HotkeyTrigger::double_press;
    profile_saved.impact_hotkey = L"F10";
    profile_saved.impact_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.ghost_arc_hotkey = L"F10";
    profile_saved.ghost_arc_trigger = wardogs::HotkeyTrigger::double_press;
    profile_saved.ghost_toggle_hotkey = L"F10";
    profile_saved.ghost_toggle_trigger = wardogs::HotkeyTrigger::long_press;
    profile_saved.hotkey_profile = L"advanced";
    wardogs::save_settings_to(path, profile_saved);
    const AppSettings advanced_loaded = wardogs::load_settings_from(path);
    check(advanced_loaded.hotkey_profile == L"advanced" &&
              advanced_loaded.custom_hotkey_profiles.empty(),
          "the five-key built-in profile survives an explicit-path round trip");
    check(advanced_loaded.quick_target_trigger ==
                  wardogs::HotkeyTrigger::double_press &&
              advanced_loaded.ghost_toggle_trigger ==
                  wardogs::HotkeyTrigger::long_press &&
              advanced_loaded.region_hotkey == L"F8",
          "the five-key profile keeps its trigger mapping");

    wardogs::HotkeyProfile custom;
    custom.id = L"custom-test";
    custom.name = L"测试配置";
    custom.description = L"自定义配置";
    custom.hotkeys = {L"F6", L"F7", L"F8", L"F9",
                      L"F10", L"F11", L"F12"};
    custom.triggers.fill(wardogs::HotkeyTrigger::single);
    profile_saved.custom_hotkey_profiles = {custom};
    profile_saved.hotkey_profile = L"custom-test";
    profile_saved.region_hotkey = custom.hotkeys[0];
    profile_saved.base_hotkey = custom.hotkeys[1];
    profile_saved.target_hotkey = custom.hotkeys[2];
    profile_saved.quick_target_hotkey = custom.hotkeys[3];
    profile_saved.impact_hotkey = custom.hotkeys[4];
    profile_saved.ghost_arc_hotkey = custom.hotkeys[5];
    profile_saved.ghost_toggle_hotkey = custom.hotkeys[6];
    profile_saved.region_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.base_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.target_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.quick_target_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.impact_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.ghost_arc_trigger = wardogs::HotkeyTrigger::single;
    profile_saved.ghost_toggle_trigger = wardogs::HotkeyTrigger::single;
    wardogs::save_settings_to(path, profile_saved);
    const AppSettings custom_loaded = wardogs::load_settings_from(path);
    check(custom_loaded.hotkey_profile == L"custom-test" &&
              custom_loaded.custom_hotkey_profiles.size() == 1 &&
              custom_loaded.custom_hotkey_profiles.front().name == L"测试配置" &&
              custom_loaded.custom_hotkey_profiles.front().hotkeys[0] ==
                  L"F6",
          "custom hotkey profiles survive the explicit-path round trip");
    check(custom_loaded.region_hotkey == L"F6",
          "the selected custom profile supplies the active hotkeys");

    profile_saved.custom_hotkey_profiles.clear();
    for (int index = 1; index <= 105; ++index) {
        wardogs::HotkeyProfile extra;
        extra.id = L"custom-" + std::to_wstring(index);
        extra.name = L"配置" + std::to_wstring(index);
        extra.description = L"自定义配置";
        extra.hotkeys.fill(L"F6");
        extra.triggers.fill(wardogs::HotkeyTrigger::single);
        profile_saved.custom_hotkey_profiles.push_back(std::move(extra));
    }
    profile_saved.hotkey_profile = L"custom-1";
    profile_saved.region_hotkey = L"F6";
    profile_saved.base_hotkey = L"F6";
    profile_saved.target_hotkey = L"F6";
    profile_saved.quick_target_hotkey = L"F6";
    profile_saved.impact_hotkey = L"F6";
    profile_saved.ghost_arc_hotkey = L"F6";
    profile_saved.ghost_toggle_hotkey = L"F6";
    wardogs::save_settings_to(path, profile_saved);
    const AppSettings capped_loaded = wardogs::load_settings_from(path);
    check(capped_loaded.custom_hotkey_profiles.size() ==
              wardogs::max_custom_hotkey_profiles,
          "loading stops at the hundred-profile cap");
    check(capped_loaded.custom_hotkey_profiles.back().id == L"custom-100" &&
              capped_loaded.hotkey_profile == L"custom-1",
          "the cap keeps the earliest profiles and the selected profile");

    fs::remove_all(directory);
    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All settings tests passed\n";
    return 0;
}
