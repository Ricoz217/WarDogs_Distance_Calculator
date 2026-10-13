#include "wardogs/ghost_reticle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool near(double left, double right, double tolerance = 1e-9) {
    return std::abs(left - right) <= tolerance;
}

}  // namespace

int main() {
    using wardogs::Arc;

    const auto bearing = wardogs::ghost_bearing_ticks(188.4);
    check(!bearing.empty(), "bearing scale produces visible ticks");
    bool found_195 = false;
    for (const auto& tick : bearing) {
        check(tick.position >= wardogs::ghost_bearing_left &&
                  tick.position <= wardogs::ghost_bearing_right,
              "bearing ticks stay inside the measured 662 px span");
        if (tick.value == 195) {
            found_195 = true;
            check(tick.major, "15 degree labels are major ticks");
            check(near(tick.position,
                       wardogs::ghost_center_x + (195.0 - 188.4) * 7.4),
                  "fractional bearing is represented by sub-tick translation");
        }
    }
    check(found_195, "bearing ruler includes the next standard label");

    const auto wrapped = wardogs::ghost_bearing_ticks(358.5);
    bool found_zero = false;
    for (const auto& tick : wrapped) {
        if (tick.value == 0) {
            found_zero = true;
            check(near(tick.position,
                       wardogs::ghost_center_x + 1.5 * 7.4),
                  "bearing ruler wraps through north without a jump");
        }
    }
    check(found_zero, "wrapped bearing ruler labels north as zero");
    check(near(wardogs::ghost_game_bearing(236.0, 0.0), 236.0),
          "ghost bearing compensation defaults to no offset");
    check(near(wardogs::ghost_game_bearing(236.0, -0.35), 235.65),
          "user bearing compensation retains sub-degree precision");
    check(near(wardogs::ghost_game_bearing(359.5, 1.0), 0.5),
          "user bearing compensation wraps cleanly through north");

    const auto mil = wardogs::ghost_mil_ticks(403.25);
    check(!mil.empty(), "MIL scale produces visible marks");
    const wardogs::GhostTick* mark_400 = nullptr;
    const wardogs::GhostTick* mark_410 = nullptr;
    for (const auto& tick : mil) {
        check(tick.position >= wardogs::ghost_mil_top &&
                  tick.position <= wardogs::ghost_mil_bottom,
              "MIL marks are clipped 93 px from both reticle borders");
        if (tick.value == 400) mark_400 = &tick;
        if (tick.value == 410) mark_410 = &tick;
    }
    check(mark_400 && mark_410, "MIL scale includes neighbouring labels");
    check(near(mark_410->position - mark_400->position, 176.0),
          "10 mil labels retain the measured 176 px spacing");
    check(near(mark_400->position,
               wardogs::ghost_center_y + (400.0 - 403.25) * 17.6),
          "fractional MIL is represented by continuous ruler translation");

    const auto mortar_mil = wardogs::ghost_mortar_mil_ticks(725.0);
    const wardogs::GhostTick* mortar_700 = nullptr;
    const wardogs::GhostTick* mortar_750 = nullptr;
    for (const auto& tick : mortar_mil) {
        check(tick.position >= wardogs::ghost_mil_top &&
                  tick.position <= wardogs::ghost_mil_bottom,
              "mortar MIL marks use the measured 93 px vertical margins");
        if (tick.value == 700) mortar_700 = &tick;
        if (tick.value == 750) mortar_750 = &tick;
    }
    check(mortar_700 && mortar_750,
          "mortar ruler includes adjacent 50 MIL labels");
    check(near(mortar_750->position - mortar_700->position, 136.0),
          "mortar 50 MIL labels retain the measured 136 px spacing");
    check(near(mortar_700->position,
               wardogs::ghost_center_y + (700.0 - 725.0) * 2.72),
          "mortar ruler scrolls continuously to fractional MIL solutions");

    check(wardogs::effective_ghost_arc(true, true, Arc::high) == Arc::high,
          "both valid solutions use the persisted preference");
    check(wardogs::effective_ghost_arc(true, false, Arc::high) == Arc::low,
          "a lone low solution temporarily overrides a high preference");
    check(wardogs::effective_ghost_arc(false, true, Arc::low) == Arc::high,
          "a lone high solution temporarily overrides a low preference");
    check(!wardogs::effective_ghost_arc(false, false, Arc::low),
          "no valid solution produces no ghost reticle result");
    check(wardogs::ghost_preset_width(1280, 720) == 480,
          "the 720p preset keeps the measured reticle-to-screen ratio");
    check(wardogs::ghost_preset_width(1600, 900) == 600,
          "the 900p preset keeps the measured reticle-to-screen ratio");
    check(wardogs::ghost_preset_width(1920, 1080) == 720,
          "the 1080p preset keeps the measured reticle-to-screen ratio");
    check(wardogs::ghost_preset_width(2560, 1440) == 960,
          "the 1440p preset restores the measured 960 px width");
    check(!wardogs::ghost_preset_width(1920, 1200),
          "non-16:9 resolutions are not offered without measurements");

    using wardogs::GhostColorPreset;
    using wardogs::GhostStyle;

    const auto check_rgb = [](GhostColorPreset preset, int r, int g, int b,
                              const char* message) {
        const auto rgb = wardogs::ghost_color_rgb(preset);
        check(rgb[0] == r && rgb[1] == g && rgb[2] == b, message);
    };
    check_rgb(GhostColorPreset::red, 255, 59, 48,
              "red keeps the factory identity color");
    check_rgb(GhostColorPreset::green, 0, 200, 83,
              "green uses a high-visibility competitive-FPS green");
    check_rgb(GhostColorPreset::cyan, 0, 188, 212,
              "cyan uses a distinct high-visibility cyan");
    check_rgb(GhostColorPreset::yellow, 255, 214, 0,
              "yellow uses a high-visibility yellow");
    check_rgb(GhostColorPreset::white, 255, 255, 255,
              "white remains a neutral high-contrast option");

    const auto halo_alpha = [](const GhostStyle& style) {
        return static_cast<int>(style.halo >> 24) & 0xFF;
    };
    const auto foreground_alpha = [](const GhostStyle& style) {
        return static_cast<int>(style.foreground >> 24) & 0xFF;
    };
    const auto scrim_alpha = [](const GhostStyle& style) {
        return static_cast<int>(style.scrim >> 24) & 0xFF;
    };

    const GhostStyle factory =
        wardogs::ghost_style(GhostColorPreset::red, 80, 100, 0, false, 1.0);
    check(factory.foreground == 0xCCFF3B30u,
          "the default style reproduces #ff3b30 at 80 percent opacity");
    check(near(factory.bearing_width, 2.6) && near(factory.rail_width, 2.7),
          "the default strokes keep the measured 2.6 / 2.7 px widths");
    check(near(factory.label_pixel_size, 17.0) &&
              near(factory.hint_pixel_size, 14.0),
          "the default fonts keep the measured 17 / 14 px sizes");
    check(!factory.halo_enabled && !factory.marker_enabled &&
              near(factory.halo_width_delta, 0.0) && !factory.scrim_enabled,
          "the readability assist and scrim are strictly opt-in");

    check(halo_alpha(wardogs::ghost_style(GhostColorPreset::red, 20, 100, 0,
                                          true, 1.0)) == 140 &&
              halo_alpha(wardogs::ghost_style(GhostColorPreset::red, 50, 100, 0,
                                              true, 1.0)) == 179 &&
              halo_alpha(wardogs::ghost_style(GhostColorPreset::red, 80, 100, 0,
                                              true, 1.0)) == 217 &&
              halo_alpha(wardogs::ghost_style(GhostColorPreset::red, 100, 100,
                                              0, true, 1.0)) == 242,
          "the halo alpha ladder follows the compressed opacity slope");

    for (int opacity = 20; opacity <= 50; opacity += 5) {
        const GhostStyle style = wardogs::ghost_style(
            GhostColorPreset::red, opacity, 100, 0, true, 1.0);
        check(static_cast<double>(halo_alpha(style)) >=
                  1.35 * foreground_alpha(style),
              "the outline carries the glyphs while the foreground stays faint");
    }
    for (const int opacity : {0, 19, 20, 100, 101, 500}) {
        const GhostStyle style = wardogs::ghost_style(
            GhostColorPreset::red, opacity, 100, 0, true, 1.0);
        check(halo_alpha(style) >= 115 && halo_alpha(style) <= 245,
              "the halo stays inside its usable alpha band");
        check(static_cast<double>(halo_alpha(style)) >=
                  0.9 * foreground_alpha(style),
              "the halo never becomes a black sticker over the foreground");
    }

    const double bearing_pitch = wardogs::ghost_bearing_tick_pitch_base();
    const double rail_pitch = wardogs::ghost_mil_tick_pitch_base();
    check(near(bearing_pitch, 22.2, 1e-9),
          "3 degree bearing ticks keep the measured 7.4 px base spacing");
    check(near(rail_pitch, 136.0, 1e-9),
          "the tightest rail pitch is the mortar 50 mil / 136 px spacing");
    for (const double scale : {0.5, 0.8333, 1.0, 2.0, 4.0}) {
        for (const int stroke : {75, 100, 225}) {
            const GhostStyle style = wardogs::ghost_style(
                GhostColorPreset::red, 80, stroke, 0, true, scale);
            check(style.bearing_width + 2.0 * style.halo_width_delta <
                      0.55 * bearing_pitch * scale,
                  "bearing stroke plus halo stays inside the anti-fuzz budget");
            check(style.rail_width + 2.0 * style.halo_width_delta <
                      0.55 * rail_pitch * scale,
                  "rail stroke plus halo stays inside the anti-fuzz budget");
        }
    }
    check(wardogs::ghost_style(GhostColorPreset::red, 80, 75, 0, true, 0.5)
                  .bearing_width >= 1.5,
          "even the thinnest preset keeps a usable stroke in the smallest window");
    for (const double scale : {0.5, 0.75, 1.0, 2.0, 4.0}) {
        double previous_width = 0.0;
        double previous_halo = 0.0;
        for (int stroke = wardogs::GhostReticlePreferences::
                          minimum_stroke_scale_percent;
             stroke <= wardogs::GhostReticlePreferences::
                           maximum_stroke_scale_percent;
             stroke +=
             wardogs::GhostReticlePreferences::stroke_scale_step_percent) {
            const GhostStyle style = wardogs::ghost_style(
                GhostColorPreset::red, 80, stroke, 0, true, scale);
            check(style.bearing_width > previous_width,
                  "every stroke preset is a distinct line width at every window size");
            previous_width = style.bearing_width;
            if (stroke <= 125) {
                check(style.halo_width_delta > previous_halo,
                      "the halo follows the stroke multiplier at the low end");
                previous_halo = style.halo_width_delta;
            }
        }
    }

    const double width75 = wardogs::ghost_style(GhostColorPreset::red, 80, 75, 0,
                                                true, 1.0)
                               .bearing_width;
    const double width100 = wardogs::ghost_style(GhostColorPreset::red, 80, 100,
                                                 0, true, 1.0)
                                .bearing_width;
    const double width225 = wardogs::ghost_style(GhostColorPreset::red, 80, 225,
                                                 0, true, 1.0)
                                .bearing_width;
    check(width75 < width100 && width100 < width225,
          "stroke presets increase the line width monotonically");
    check(foreground_alpha(wardogs::ghost_style(GhostColorPreset::red, 20, 100,
                                                0, true, 1.0)) <
              foreground_alpha(wardogs::ghost_style(GhostColorPreset::red, 100,
                                                    100, 0, true, 1.0)),
          "opacity raises the foreground alpha monotonically");

    for (const double scale : {0.5, 0.8333, 1.0, 2.0, 4.0}) {
        const GhostStyle style = wardogs::ghost_style(
            GhostColorPreset::red, 80, 100, 0, true, scale);
        check(style.halo_width_delta > 0.0,
              "the readability assist always adds a visible halo");
        check(style.text_halo_offset >= 1.0 &&
                  style.text_halo_offset < 0.15 * style.label_pixel_size,
              "the text halo never blurs more than its glyph budget");
    }
    for (const double scale : {0.9, 1.0, 2.0, 4.0}) {
        check(wardogs::ghost_style(GhostColorPreset::red, 80, 100, 0, true,
                                   scale)
                      .halo_width_delta >= 1.5,
              "the halo stays at least 1.5 px on every real scale");
    }

    const GhostStyle scrim_off =
        wardogs::ghost_style(GhostColorPreset::red, 80, 100, 0, false, 1.0);
    const GhostStyle scrim_light =
        wardogs::ghost_style(GhostColorPreset::red, 80, 100, 1, false, 1.0);
    const GhostStyle scrim_strong =
        wardogs::ghost_style(GhostColorPreset::red, 80, 100, 2, false, 1.0);
    const GhostStyle scrim_clamped =
        wardogs::ghost_style(GhostColorPreset::red, 80, 100, 9, false, 1.0);
    check(!scrim_off.scrim_enabled && !scrim_off.scrim_bands &&
              scrim_alpha(scrim_off) == 0,
          "the default scrim adds no pixels at all");
    check(scrim_light.scrim_enabled && !scrim_light.scrim_bands &&
              scrim_alpha(scrim_light) >= 20 && scrim_alpha(scrim_light) <= 40,
          "the light scrim only plates the labels");
    check(scrim_strong.scrim_enabled && scrim_strong.scrim_bands &&
              scrim_alpha(scrim_strong) >= 55 && scrim_alpha(scrim_strong) <= 75,
          "the strong scrim adds the narrow rail band");
    check(scrim_clamped.scrim_bands &&
              scrim_alpha(scrim_clamped) == scrim_alpha(scrim_strong),
          "out of range scrim levels clamp to the strongest preset");
    check(scrim_alpha(scrim_light) < halo_alpha(scrim_off) &&
              scrim_alpha(scrim_strong) < halo_alpha(scrim_off),
          "the scrim plate never overpowers the outline");

    for (const auto preset :
         {GhostColorPreset::red, GhostColorPreset::cyan,
          GhostColorPreset::green, GhostColorPreset::yellow,
          GhostColorPreset::white}) {
        const auto parsed = wardogs::ghost_color_preset_from_name(
            wardogs::ghost_color_preset_name(preset));
        check(parsed && *parsed == preset,
              "every color preset survives its persisted name round trip");
    }
    check(!wardogs::ghost_color_preset_from_name(L"") &&
              !wardogs::ghost_color_preset_from_name(L"magenta"),
          "an unknown persisted preset falls back instead of picking the first");
    check(wardogs::ghost_color_preset_from_name(L"violet") ==
                  GhostColorPreset::green &&
              wardogs::ghost_color_preset_from_name(L"amber") ==
                  GhostColorPreset::yellow,
          "old local pastel preset names migrate to the new vivid presets");

    const auto bearing_point = wardogs::ghost_bearing_read_point();
    check(near(bearing_point[0], 480.0) && near(bearing_point[1], 88.0),
          "the bearing read point sits on the measured center baseline");
    const auto mil_point = wardogs::ghost_mil_read_point();
    check(near(mil_point[0], 790.0) && near(mil_point[1], 360.0),
          "the MIL read point sits on the measured rail center");
    check(mil_point[1] >= wardogs::ghost_mil_top &&
              mil_point[1] <= wardogs::ghost_mil_bottom,
          "the MIL read point can never be clipped away");

    double max_halo_offset = 0.0;
    for (const auto& offset : wardogs::ghost_text_halo_offsets(2.0)) {
        max_halo_offset = std::max(
            max_halo_offset,
            std::max(std::abs(offset[0]), std::abs(offset[1])));
    }
    check(near(max_halo_offset, 2.0, 1e-9),
          "eight-direction text halos stay inside the requested distance");

    std::cout << "All ghost reticle tests passed\n";
    return 0;
}
