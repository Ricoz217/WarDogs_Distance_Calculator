#include "wardogs/ghost_reticle.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wardogs {
namespace {

constexpr double bearing_pixels_per_degree = 111.0 / 15.0;
constexpr double mil_pixels_per_mil = 176.0 / 10.0;
constexpr double mortar_pixels_per_mil = 136.0 / 50.0;

constexpr double bearing_stroke_pixels = 2.6;
constexpr double rail_stroke_pixels = 2.7;
constexpr double frame_stroke_pixels = 2.5;
constexpr double minimum_stroke_pixels = 2.0;
constexpr double label_font_pixels = 17.0;
constexpr double hint_font_pixels = 14.0;
constexpr double minimum_font_pixels = 12.0;
constexpr double halo_stroke_ratio = 1.6;
constexpr double halo_minimum_pixels = 1.5;
constexpr double text_halo_ratio = 1.3;
constexpr double text_halo_minimum = 1.0;
// The halo may consume at most half of the remaining tick gap, so the
// foreground stroke plus both halo shoulders always stay below 0.54 of the
// tightest tick pitch instead of smearing the ruler shut.
constexpr double tick_budget_ratio = 0.54;

// The darkest existing app token (#0b1018), reused by both the halo and the
// scrim plates. Halo and scrim alphas are applied at style time.
constexpr std::array<int, 3> outline_rgb{11, 16, 24};

std::uint32_t argb(int alpha, const std::array<int, 3>& rgb) {
    return (static_cast<std::uint32_t>(std::clamp(alpha, 0, 255)) << 24) |
           (static_cast<std::uint32_t>(rgb[0]) << 16) |
           (static_cast<std::uint32_t>(rgb[1]) << 8) |
           static_cast<std::uint32_t>(rgb[2]);
}

int normalized_bearing(int degrees) {
    const int value = degrees % 360;
    return value < 0 ? value + 360 : value;
}

}  // namespace

GhostStyle ghost_style(GhostColorPreset preset, int opacity_percent,
                       int stroke_scale_percent, int scrim_level,
                       bool readability_enabled, double scale) {
    const double opacity =
        std::clamp(opacity_percent, GhostReticlePreferences::minimum_opacity_percent,
                   GhostReticlePreferences::maximum_opacity_percent) /
        100.0;
    const double stroke =
        std::clamp(stroke_scale_percent,
                   GhostReticlePreferences::minimum_stroke_scale_percent,
                   GhostReticlePreferences::maximum_stroke_scale_percent) /
        100.0;
    const int level = std::clamp(scrim_level,
                                 GhostReticlePreferences::minimum_scrim_level,
                                 GhostReticlePreferences::maximum_scrim_level);
    const double safe_scale = std::max(scale, 0.0);
    const auto rgb = ghost_color_rgb(preset);

    GhostStyle style;
    style.foreground = argb(static_cast<int>(std::lround(255.0 * opacity)), rgb);
    style.halo = argb(
        std::clamp(static_cast<int>(std::lround(255.0 * (0.45 + 0.50 * opacity))),
                   115, 245),
        outline_rgb);
    style.scrim = argb(level >= 2 ? 65 : (level == 1 ? 30 : 0), outline_rgb);
    // The stroke multiplier scales the whole stroke including the pixel
    // floor. Applying it inside max(2.0, ...) collapses 75-150 percent into
    // the same 2 px line on small windows, so the dropdown stops meaning
    // anything exactly where the ruler is densest.
    const double base_bearing =
        std::max(minimum_stroke_pixels, bearing_stroke_pixels * safe_scale);
    const double base_rail =
        std::max(minimum_stroke_pixels, rail_stroke_pixels * safe_scale);
    style.bearing_width = base_bearing * stroke;
    style.rail_width = base_rail * stroke;
    style.frame_width =
        std::max(minimum_stroke_pixels, frame_stroke_pixels * safe_scale);
    style.label_pixel_size =
        std::max(minimum_font_pixels, std::round(label_font_pixels * safe_scale));
    style.hint_pixel_size =
        std::max(minimum_font_pixels, std::round(hint_font_pixels * safe_scale));
    style.halo_enabled = readability_enabled;
    style.marker_enabled = readability_enabled;
    style.scrim_enabled = level > 0;
    style.scrim_bands = level >= 2;
    if (readability_enabled) {
        const double desired = halo_stroke_ratio * safe_scale * stroke;
        const double budget = std::max(
            0.0, (tick_budget_ratio * ghost_bearing_tick_pitch_base() * safe_scale -
                  style.bearing_width) /
                     2.0);
        style.halo_width_delta = std::max(
            0.0, std::min(std::max(halo_minimum_pixels * stroke, desired),
                          budget));
    }
    // Text halos follow the stroke multiplier too, capped by the glyph budget
    // so a 225 percent outline never blurs the digits into each other.
    style.text_halo_offset = std::min(
        std::max(text_halo_minimum, text_halo_ratio * safe_scale * stroke),
        0.14 * style.label_pixel_size);
    return style;
}

std::array<int, 3> ghost_color_rgb(GhostColorPreset preset) {
    switch (preset) {
        case GhostColorPreset::green:
            return {0, 200, 83};
        case GhostColorPreset::cyan:
            return {0, 188, 212};
        case GhostColorPreset::yellow:
            return {255, 214, 0};
        case GhostColorPreset::white:
            return {255, 255, 255};
        case GhostColorPreset::red:
            break;
    }
    return {255, 59, 48};
}

const wchar_t* ghost_color_preset_name(GhostColorPreset preset) {
    switch (preset) {
        case GhostColorPreset::green:
            return L"green";
        case GhostColorPreset::cyan:
            return L"cyan";
        case GhostColorPreset::yellow:
            return L"yellow";
        case GhostColorPreset::white:
            return L"white";
        case GhostColorPreset::red:
            break;
    }
    return L"red";
}

std::optional<GhostColorPreset> ghost_color_preset_from_name(
    std::wstring_view name) {
    for (const auto preset : {GhostColorPreset::red, GhostColorPreset::green,
                              GhostColorPreset::cyan, GhostColorPreset::yellow,
                              GhostColorPreset::white}) {
        if (name == ghost_color_preset_name(preset)) return preset;
    }
    // Migrate the unreleased pastel names used by earlier local builds.
    if (name == L"violet") return GhostColorPreset::green;
    if (name == L"amber") return GhostColorPreset::yellow;
    return std::nullopt;
}

std::array<std::array<double, 2>, 8> ghost_text_halo_offsets(double distance) {
    const double diagonal = distance / std::numbers::sqrt2;
    return {{{0.0, -distance},
             {0.0, distance},
             {-distance, 0.0},
             {distance, 0.0},
             {-diagonal, -diagonal},
             {diagonal, -diagonal},
             {-diagonal, diagonal},
             {diagonal, diagonal}}};
}

std::array<double, 2> ghost_bearing_read_point() {
    return {ghost_center_x, ghost_bearing_baseline_y};
}

std::array<double, 2> ghost_mil_read_point() {
    return {ghost_mil_rail_x, ghost_center_y};
}

double ghost_bearing_tick_pitch_base() {
    return 3.0 * bearing_pixels_per_degree;
}

double ghost_mil_tick_pitch_base() {
    // The mortar ruler is the tightest rail: 50 mil every 2.72 px base.
    return 50.0 * mortar_pixels_per_mil;
}

std::vector<GhostTick> ghost_bearing_ticks(double target_bearing_degrees) {
    std::vector<GhostTick> ticks;
    const double visible_degrees =
        (ghost_bearing_right - ghost_bearing_left) / bearing_pixels_per_degree;
    const int first = static_cast<int>(
        std::ceil((target_bearing_degrees - visible_degrees / 2.0) / 3.0));
    const int last = static_cast<int>(
        std::floor((target_bearing_degrees + visible_degrees / 2.0) / 3.0));
    for (int step = first; step <= last; ++step) {
        const int raw_value = step * 3;
        const double position = ghost_center_x +
                                (raw_value - target_bearing_degrees) *
                                    bearing_pixels_per_degree;
        if (position < ghost_bearing_left - 1e-9 ||
            position > ghost_bearing_right + 1e-9)
            continue;
        const int value = normalized_bearing(raw_value);
        ticks.push_back({position, value, value % 15 == 0});
    }
    return ticks;
}

double ghost_game_bearing(double calculated_bearing_degrees,
                          double compensation_degrees) {
    double value = std::fmod(calculated_bearing_degrees + compensation_degrees,
                             360.0);
    if (value < 0.0) value += 360.0;
    return value;
}

std::vector<GhostTick> ghost_mil_ticks(double target_mil) {
    std::vector<GhostTick> ticks;
    const double visible_mil =
        (ghost_mil_bottom - ghost_mil_top) / mil_pixels_per_mil;
    const int first = static_cast<int>(
        std::ceil((target_mil - visible_mil / 2.0) / 10.0));
    const int last = static_cast<int>(
        std::floor((target_mil + visible_mil / 2.0) / 10.0));
    for (int step = first; step <= last; ++step) {
        const int value = step * 10;
        if (value < 20 || value > 1400) continue;
        const double position = ghost_center_y +
                                (value - target_mil) * mil_pixels_per_mil;
        if (position < ghost_mil_top - 1e-9 ||
            position > ghost_mil_bottom + 1e-9)
            continue;
        ticks.push_back({position, value, true});
    }
    return ticks;
}

std::vector<GhostTick> ghost_mortar_mil_ticks(double target_mil) {
    std::vector<GhostTick> ticks;
    const double visible_mil =
        (ghost_mil_bottom - ghost_mil_top) / mortar_pixels_per_mil;
    const int first = static_cast<int>(
        std::ceil((target_mil - visible_mil / 2.0) / 50.0));
    const int last = static_cast<int>(
        std::floor((target_mil + visible_mil / 2.0) / 50.0));
    for (int step = first; step <= last; ++step) {
        const int value = step * 50;
        if (value < 150 || value > 850) continue;
        const double position = ghost_center_y +
                                (value - target_mil) *
                                    mortar_pixels_per_mil;
        if (position < ghost_mil_top - 1e-9 ||
            position > ghost_mil_bottom + 1e-9)
            continue;
        ticks.push_back({position, value, true});
    }
    return ticks;
}

std::optional<Arc> effective_ghost_arc(bool low_available,
                                       bool high_available,
                                       Arc preferred) {
    if (low_available && high_available) return preferred;
    if (low_available) return Arc::low;
    if (high_available) return Arc::high;
    return std::nullopt;
}

std::optional<int> ghost_preset_width(int screen_width, int screen_height) {
    if (screen_width <= 0 || screen_height <= 0 ||
        static_cast<long long>(screen_width) * 9 !=
            static_cast<long long>(screen_height) * 16)
        return std::nullopt;
    const long long scaled_width =
        static_cast<long long>(screen_width) * 3;
    if (scaled_width % 8 != 0) return std::nullopt;
    const int reticle_width = static_cast<int>(scaled_width / 8);
    if (reticle_width < GhostReticlePreferences::minimum_width ||
        reticle_width > GhostReticlePreferences::maximum_width)
        return std::nullopt;
    return reticle_width;
}

}  // namespace wardogs