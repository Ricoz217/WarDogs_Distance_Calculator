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

void rejects_with_message(const std::function<void()>& action,
                          std::string_view expected, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument& error) {
        check(error.what() == expected, message);
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
    rejects_with_message([] { wardogs::parse_hotkey(L"Ctrl+Alt"); },
                         "热键必须包含一个非修饰键",
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
    rejects_with_message(
        [&] { wardogs::validate_unique_hotkeys(duplicate_hotkeys); },
        "四个热键不能重复", "a duplicate quick target hotkey is rejected");

    wardogs::HotkeyMatcher matcher{unique_hotkeys};
    check(matcher.handle_key_event(VK_F8, true, 0) == 0,
          "a matching key-down selects the first hotkey");
    check(!matcher.handle_key_event(VK_F8, true, 0),
          "holding a hotkey does not trigger repeatedly");
    check(!matcher.handle_key_event(VK_F8, false, 0),
          "key-up only rearms the hotkey");
    check(matcher.handle_key_event(VK_F8, true, 0) == 0,
          "a hotkey triggers again after it is released");
    check(!matcher.handle_key_event(VK_F9, true, MOD_CONTROL),
          "extra modifiers do not trigger an unmodified hotkey");
    matcher.handle_key_event(VK_F9, false, MOD_CONTROL);

    const std::array modifier_hotkeys{wardogs::parse_hotkey(L"F8"),
                                      wardogs::parse_hotkey(L"Ctrl+F8")};
    wardogs::HotkeyMatcher modifier_matcher{modifier_hotkeys};
    check(modifier_matcher.handle_key_event(VK_F8, true, MOD_CONTROL) == 1,
          "the same key can coexist with a distinct modifier combination");
    modifier_matcher.handle_key_event(VK_F8, false, MOD_CONTROL);
    check(modifier_matcher.handle_key_event(VK_F8, true, 0) == 0,
          "the unmodified form remains independently available");

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All core tests passed\n";
    return 0;
}
