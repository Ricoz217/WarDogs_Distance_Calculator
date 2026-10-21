#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <array>
#include <cstdint>
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

// Measured layout of the procedural scales inside the 960 x 720 base frame.
inline constexpr double ghost_bearing_baseline_y = 88.0;
inline constexpr double ghost_mil_rail_x = 790.0;

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
    static constexpr int minimum_line_width_tenths = 10;
    static constexpr int maximum_line_width_tenths = 60;
    static constexpr int minimum_outline_width_tenths = 5;
    static constexpr int maximum_outline_width_tenths = 40;
    static constexpr int minimum_hue = 0;
    static constexpr int maximum_hue = 359;
    static constexpr int minimum_color_percent = 0;
    static constexpr int maximum_color_percent = 100;

    int opacity_percent{80};
    int width{960};
    int preset_screen_width{2560};
    int preset_screen_height{1440};
    double bearing_compensation_deg{0.0};
    Arc preferred_arc{Arc::low};
    int line_width_tenths{27};
    int outline_width_tenths{16};
    bool outline_enabled{false};
    int color_hue{3};
    int color_saturation{100};
    int color_lightness{59};
};

// Every pixel decision of the ghost reticle renderer lives here so the
// arithmetic stays testable without Qt.
struct GhostStyle {
    std::uint32_t foreground{};
    std::uint32_t outline{};
    double line_width{};
    double outline_width{};
    double frame_width{};
    double label_pixel_size{};
    double hint_pixel_size{};
    bool outline_enabled{};
};

[[nodiscard]] GhostStyle ghost_style(int hue, int saturation, int lightness,
                                     int opacity_percent,
                                     int line_width_tenths,
                                     int outline_width_tenths,
                                     bool outline_enabled, double scale);
[[nodiscard]] std::array<int, 3> ghost_hsl_rgb(int hue, int saturation,
                                               int lightness);
[[nodiscard]] std::array<std::array<double, 2>, 8> ghost_text_halo_offsets(
    double distance);

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
