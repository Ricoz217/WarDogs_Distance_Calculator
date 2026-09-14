#pragma once

namespace wardogs {

struct PinnedCardPreferences {
    static constexpr int minimum_opacity_percent = 35;
    static constexpr int maximum_opacity_percent = 100;

    bool locked{};
    int opacity_percent{maximum_opacity_percent};
};

}  // namespace wardogs
