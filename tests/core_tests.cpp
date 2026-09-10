#include "wardogs/core.hpp"
#include "wardogs/hotkeys.hpp"
#include "wardogs/settings.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void close(double actual, double expected, const char* message) {
    check(std::abs(actual - expected) < 1e-8, message);
}

void rejects(const std::function<void()>& action, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument&) {
    }
}

}  // namespace

int main() {
    using wardogs::Point;

    const std::vector<std::pair<Point, double>> bearings{
        {{0, 10}, 0}, {{10, 0}, 90}, {{0, -10}, 180}, {{-10, 0}, 270},
    };
    for (const auto& [target, expected] : bearings) {
        close(wardogs::calculate_shot({0, 0}, target).angle, expected,
              "bearing is clockwise from positive Y");
    }
    const auto shot = wardogs::calculate_shot({0, 0}, {3, 4});
    close(shot.distance, 5, "distance uses Euclidean length");
    close(shot.angle, 36.86989764584402, "north-zero diagonal bearing");

    const std::vector<std::pair<std::wstring, Point>> ocr_cases{
        {L"x12.34, y56.78", {12.34, 56.78}},
        {L"旧 x1.00, y2.00\n新 x30.00, y40.00", {30, 40}},
        {L"x1 1.32, y54.1 3", {11.32, 54.13}},
        {L"x114.51, yl 91.81", {114.51, 191.81}},
        {L"xI14.51, yO91.81", {114.51, 91.81}},
        {L"xli4.51& YI 91.81", {114.51, 191.81}},
        {L"x13.11, y14.21|", {13.11, 14.21}},
        {L"x13.11, y14.2|", {13.11, 14.21}},
        {L"x101.33, y112.554", {101.33, 112.55}},
        {L"x101.3399, y112.5588", {101.33, 112.55}},
        {L"x13.11, y14.21|3", {13.11, 14.21}},
    };
    for (const auto& [text, expected] : ocr_cases) {
        check(wardogs::parse_ocr_coordinate(text) == expected,
              "OCR parser repairs spaces and glyph confusions");
    }
    rejects([] { wardogs::parse_ocr_coordinate(L"x121 51, y131.81"); },
            "OCR rejects a missing decimal point");
    rejects([] { wardogs::parse_ocr_coordinate(L"x12.3, y123.22"); },
            "OCR still requires two fractional digits");

    check(wardogs::parse_manual_coordinate(L"12.34 56.78") == Point{12.34, 56.78},
          "manual parser accepts a whitespace pair");
    check(wardogs::parse_manual_coordinate(L"x1, y2") == Point{1, 2},
          "manual parser accepts labelled integers");
    check(wardogs::parse_manual_coordinate(L"x1.239, y2.999") == Point{1.239, 2.999},
          "manual input keeps its full precision");
    rejects([] { wardogs::parse_manual_coordinate(L"12 34 56"); },
            "manual parser rejects three values");

    check(wardogs::format_point({12, 34.5}) == L"x12, y34.5",
          "point formatting trims trailing zeroes");
    check(wardogs::format_distance_meters(5) == L"500 m",
          "game units convert to metres");
    check(wardogs::format_bearing(36.86989765) == L"36.9° NE",
          "bearing includes an eight-way compass direction");
    check(wardogs::format_bearing(359.96) == L"0.0° N",
          "bearing display wraps rounded north");
    check(wardogs::format_raw_distance(5) ==
              L"原始距离 5.0000 单位 · 1 单位 = 100 m",
          "raw distance remains available as secondary text");

    const auto f8 = wardogs::parse_hotkey(L"f8");
    check(f8.virtual_key == VK_F8 && f8.modifiers == MOD_NOREPEAT &&
              f8.display == L"F8",
          "function-key hotkeys are normalized");
    const auto chord = wardogs::parse_hotkey(L"Ctrl + Alt + q");
    check(chord.virtual_key == 'Q' &&
              chord.modifiers == (MOD_CONTROL | MOD_ALT | MOD_NOREPEAT) &&
              chord.display == L"Ctrl+Alt+Q",
          "modifier chords are normalized");
    rejects([] { wardogs::parse_hotkey(L"Ctrl+Alt"); },
            "a hotkey requires a non-modifier key");
    rejects([] { wardogs::parse_hotkey(L"F25"); },
            "unsupported function keys are rejected");

    const wardogs::AppSettings default_settings;
    check(default_settings.quick_target_hotkey == L"F11",
          "quick target capture has an independent default hotkey");
    const std::array unique_hotkeys{
        wardogs::parse_hotkey(L"F8"), wardogs::parse_hotkey(L"F9"),
        wardogs::parse_hotkey(L"F10"), wardogs::parse_hotkey(L"F11")};
    try {
        wardogs::validate_unique_hotkeys(unique_hotkeys);
    } catch (const std::invalid_argument&) {
        check(false, "four distinct hotkeys are accepted");
    }
    const std::array duplicate_hotkeys{
        wardogs::parse_hotkey(L"F8"), wardogs::parse_hotkey(L"F9"),
        wardogs::parse_hotkey(L"F10"), wardogs::parse_hotkey(L"F8")};
    rejects([&] { wardogs::validate_unique_hotkeys(duplicate_hotkeys); },
            "a duplicate quick target hotkey is rejected");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All core tests passed\n";
    return 0;
}
