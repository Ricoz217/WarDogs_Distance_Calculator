#include "wardogs/settings.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>

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

    fs::remove_all(directory);
    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All settings tests passed\n";
    return 0;
}
