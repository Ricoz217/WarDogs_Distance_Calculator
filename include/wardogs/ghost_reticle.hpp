#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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
inline constexpr double ghost_marker_arm = 9.0;
inline constexpr double ghost_marker_depth = 8.0;
inline constexpr double ghost_index_length = 28.0;
inline constexpr double ghost_scrim_padding = 4.0;
inline constexpr double ghost_scrim_band_half = 7.0;

enum class GhostColorPreset { red, green, cyan, yellow, white };

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
    static constexpr int minimum_stroke_scale_percent = 75;
    static constexpr int maximum_stroke_scale_percent = 225;
    static constexpr int stroke_scale_step_percent = 25;
    static constexpr int minimum_scrim_level = 0;
    static constexpr int maximum_scrim_level = 2;

    int opacity_percent{80};
    int width{960};
    int preset_screen_width{2560};
    int preset_screen_height{1440};
    double bearing_compensation_deg{0.0};
    Arc preferred_arc{Arc::low};
    GhostColorPreset color_preset{GhostColorPreset::red};
    int stroke_scale_percent{100};
    int scrim_level{0};
    bool readability_enabled{false};
};

// Every pixel decision of the ghost reticle renderer lives here so the
// arithmetic stays testable without Qt.
struct GhostStyle {
    std::uint32_t foreground{};
    std::uint32_t halo{};
    std::uint32_t scrim{};
    double bearing_width{};
    double rail_width{};
    double frame_width{};
    double halo_width_delta{};
    double text_halo_offset{};
    double label_pixel_size{};
    double hint_pixel_size{};
    bool halo_enabled{};
    bool marker_enabled{};
    bool scrim_enabled{};
    bool scrim_bands{};
};

[[nodiscard]] GhostStyle ghost_style(GhostColorPreset preset, int opacity_percent,
                                     int stroke_scale_percent, int scrim_level,
                                     bool readability_enabled, double scale);
[[nodiscard]] std::array<int, 3> ghost_color_rgb(GhostColorPreset preset);
[[nodiscard]] const wchar_t* ghost_color_preset_name(GhostColorPreset preset);
[[nodiscard]] std::optional<GhostColorPreset> ghost_color_preset_from_name(
    std::wstring_view name);
[[nodiscard]] std::array<std::array<double, 2>, 8> ghost_text_halo_offsets(
    double distance);
[[nodiscard]] std::array<double, 2> ghost_bearing_read_point();
[[nodiscard]] std::array<double, 2> ghost_mil_read_point();
[[nodiscard]] double ghost_bearing_tick_pitch_base();
[[nodiscard]] double ghost_mil_tick_pitch_base();

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