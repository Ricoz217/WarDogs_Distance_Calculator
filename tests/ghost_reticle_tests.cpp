#include "wardogs/ghost_reticle.hpp"

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

    std::cout << "All ghost reticle tests passed\n";
    return 0;
}
