#include "wardogs/settings.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

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
    saved.region_hotkey = L"Ctrl+F8";
    saved.capture_region = CaptureRegion{L"\\\\.\\DISPLAY2", {13, 27, 413, 81}};
    wardogs::save_settings_to(path, saved);

    const AppSettings loaded = wardogs::load_settings_from(path);
    check(loaded.region_hotkey == saved.region_hotkey,
          "ordinary settings survive the explicit-path round trip");
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

    saved.capture_region.reset();
    wardogs::save_settings_to(path, saved);
    check(!wardogs::load_settings_from(path).capture_region,
          "saving an empty region removes an earlier persisted region");

    fs::remove_all(directory);
    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All settings tests passed\n";
    return 0;
}
