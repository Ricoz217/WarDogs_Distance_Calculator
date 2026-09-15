#pragma once

#include <string>

namespace wardogs {

struct PinnedCardPreferences {
    static constexpr int minimum_opacity_percent = 35;
    static constexpr int maximum_opacity_percent = 100;

    bool locked{};
    int opacity_percent{maximum_opacity_percent};
    std::wstring unlock_hotkey{L"Ctrl+Alt+Q"};
};

}  // namespace wardogs
