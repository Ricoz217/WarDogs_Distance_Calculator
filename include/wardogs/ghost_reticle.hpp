#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <optional>
#include <string>
#include <vector>

namespace wardogs {

inline constexpr double ghost_base_width = 960.0;
inline constexpr double ghost_base_height = 720.0;
inline constexpr double ghost_center_x = 480.0;
inline constexpr double ghost_center_y = 360.0;
inline constexpr double ghost_bearing_left = 149.0;
inline constexpr double ghost_bearing_right = 811.0;
inline constexpr double ghost_mil_top = 93.0;
inline constexpr double ghost_mil_bottom = 627.0;

struct GhostTick {
    double position{};
    int value{};
    bool major{};
};

struct GhostReticlePreferences {
    static constexpr int minimum_opacity_percent = 20;
    static constexpr int maximum_opacity_percent = 100;
    static constexpr int minimum_width = 480;
    static constexpr int maximum_width = 3840;
    static constexpr double minimum_bearing_compensation_deg = -30.0;
    static constexpr double maximum_bearing_compensation_deg = 30.0;
    static constexpr double bearing_compensation_step_deg = 0.05;

    int opacity_percent{80};
    int width{960};
    int preset_screen_width{2560};
    int preset_screen_height{1440};
    double bearing_compensation_deg{0.0};
    Arc preferred_arc{Arc::low};
};

[[nodiscard]] std::vector<GhostTick> ghost_bearing_ticks(
    double target_bearing_degrees);
[[nodiscard]] double ghost_game_bearing(double calculated_bearing_degrees,
                                        double compensation_degrees);
[[nodiscard]] std::vector<GhostTick> ghost_mil_ticks(double target_mil);
[[nodiscard]] std::vector<GhostTick> ghost_mortar_mil_ticks(double target_mil);
[[nodiscard]] std::optional<Arc> effective_ghost_arc(
    bool low_available, bool high_available, Arc preferred);
[[nodiscard]] std::optional<int> ghost_preset_width(
    int screen_width, int screen_height);

}  // namespace wardogs
