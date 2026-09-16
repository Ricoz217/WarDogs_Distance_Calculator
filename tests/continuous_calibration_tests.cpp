#include "wardogs/continuous_calibration.hpp"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

void rejects(const std::function<void()>& action, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument&) {
    }
}

wardogs::Point impact_with_required_offset(
    wardogs::Point base, wardogs::Point target, wardogs::Arc arc,
    double bearing_offset_deg, double mil_offset,
    double fired_bearing_adjustment_deg = 0.0) {
    const auto base_solution = wardogs::corrected_solution(
        base, target, {wardogs::identity_rotation(), 0.0}, arc);
    const double bearing = (base_solution.bearing_deg +
                            fired_bearing_adjustment_deg - bearing_offset_deg) *
                           std::numbers::pi / 180.0;
    const double range = wardogs::sph2_distance_for_mil(
        base_solution.mil - mil_offset, arc);
    return {base.x + std::sin(bearing) * range / 100.0,
            base.y + std::cos(bearing) * range / 100.0};
}

wardogs::FiringSnapshot snapshot(wardogs::Point base, wardogs::Point target,
                                 wardogs::Arc arc,
                                 double bearing_adjustment_deg = 0.0) {
    const auto solution = wardogs::corrected_solution(
        base, target, {wardogs::identity_rotation(), 0.0}, arc);
    return {target, arc, solution.bearing_deg + bearing_adjustment_deg,
            solution.mil};
}

wardogs::Point impact_with_rotated_baseline(
    wardogs::Point base, wardogs::Point target, wardogs::Arc arc,
    const wardogs::PlatformCalibration& calibration,
    double bearing_offset_deg, double mil_offset) {
    const auto firing = wardogs::corrected_solution(
        base, target, calibration, arc);
    const auto world = calibration.local_to_world(
        wardogs::direction_from_bearing_and_mil(
            firing.bearing_deg - bearing_offset_deg,
            firing.mil - mil_offset));
    const double bearing = std::atan2(world[0], world[1]);
    const double world_mil =
        std::atan2(world[2], std::hypot(world[0], world[1])) * 1000.0;
    const double range = wardogs::sph2_distance_for_mil(world_mil, arc);
    return {base.x + std::sin(bearing) * range / 100.0,
            base.y + std::cos(bearing) * range / 100.0};
}

}  // namespace

int main() {
    using namespace wardogs;
    const Point base{50, 50};
    const PlatformCalibration baseline{identity_rotation(), 0.0};
    ContinuousCalibration model(base, baseline);
    const Point north{50, 68};
    const auto initial = model.solution(north, Arc::low);
    const auto plain = corrected_solution(base, north, baseline, Arc::low);
    check(std::abs(initial.bearing_deg - plain.bearing_deg) < 1e-9 &&
              std::abs(initial.mil - plain.mil) < 1e-9,
          "empty online model preserves the two-shot baseline");

    // A single shot has limited influence, even when its correction is large.
    const auto first = model.add_landing(
        snapshot(base, north, Arc::low),
        impact_with_required_offset(base, north, Arc::low, 1.0, 10.0));
    check(first.confidence > 0.35 && first.confidence <= 1.0,
          "a first shot is provisional, not automatically unreliable");
    check(model.solution(north, Arc::low).bearing_deg > plain.bearing_deg &&
              model.solution(north, Arc::low).bearing_deg < plain.bearing_deg + 1.0,
          "one shot nudges but cannot fully determine the correction");

    model.add_landing(snapshot(base, north, Arc::low),
                      impact_with_required_offset(base, north, Arc::low, 1.05, 9.0));
    model.add_landing(snapshot(base, north, Arc::low),
                      impact_with_required_offset(base, north, Arc::low, 0.95, 11.0));
    const auto learned = model.solution(north, Arc::low);
    check(learned.bearing_deg > plain.bearing_deg + 0.65 &&
              learned.bearing_deg < plain.bearing_deg + 1.2,
          "consistent shots learn the bearing correction");
    check(learned.mil > plain.mil + 6.0 && learned.mil < plain.mil + 14.0,
          "consistent shots learn the reticle correction");

    const auto bad = model.add_landing(
        snapshot(base, north, Arc::low),
        impact_with_required_offset(base, north, Arc::low, -2.0, -25.0));
    const auto after_bad = model.solution(north, Arc::low);
    check(bad.confidence < 0.3,
          "a lone incompatible impact has low confidence");
    check(std::abs(after_bad.bearing_deg - learned.bearing_deg) < 0.35 &&
              std::abs(after_bad.mil - learned.mil) < 4.0,
          "one bad impact cannot overturn consistent history");

    const Point east{68, 50};
    const auto before_switch = model.solution(east, Arc::low);
    const auto east_plain = corrected_solution(base, east, baseline, Arc::low);
    check(before_switch.bearing_deg > east_plain.bearing_deg,
          "switching targets retains a bounded shared correction");
    const auto east_first = model.add_landing(
        snapshot(base, east, Arc::low),
        impact_with_required_offset(base, east, Arc::low, 1.0, 10.0));
    check(east_first.confidence > 0.35,
          "a new target does not automatically lower shot confidence");
    model.add_landing(snapshot(base, east, Arc::low),
                      impact_with_required_offset(base, east, Arc::low, 1.0, 10.0));
    check(model.solution(east, Arc::low).bearing_deg >
              before_switch.bearing_deg,
          "new-target impacts continue updating the same model");

    ContinuousCalibration isolated(base, baseline);
    const Point south{50, 32};
    const auto extreme_first = isolated.add_landing(
        snapshot(base, south, Arc::low),
        impact_with_required_offset(base, south, Arc::low, -10.0, -100.0));
    const auto south_plain = corrected_solution(base, south, baseline, Arc::low);
    const auto provisional = isolated.solution(south, Arc::low);
    check(extreme_first.confidence < 0.2,
          "a single extreme unexplained impact starts at low confidence");
    check(std::abs(provisional.bearing_deg - south_plain.bearing_deg) < 1.0 &&
              std::abs(provisional.mil - south_plain.mil) < 15.0,
          "one extreme shot has a strict immediate influence limit");
    isolated.add_landing(snapshot(base, south, Arc::low),
                         impact_with_required_offset(base, south, Arc::low,
                                                     -10.0, -100.0));
    check(isolated.solution(south, Arc::low).bearing_deg <
              south_plain.bearing_deg - 1.0,
          "repeated extreme but coherent evidence can regain influence");

    const Point northeast{63, 63};
    const auto transfer_before = model.solution(northeast, Arc::low);
    const auto south_first = model.add_landing(
        snapshot(base, south, Arc::low),
        impact_with_required_offset(base, south, Arc::low, -10.0, -100.0));
    check(south_first.confidence < 0.2,
          "an unexplained extreme impact is provisional for its size, not its target");
    model.add_landing(snapshot(base, south, Arc::low),
                      impact_with_required_offset(base, south, Arc::low,
                                                  -10.0, -100.0));
    const auto transfer_after = model.solution(northeast, Arc::low);
    check(std::abs(transfer_after.bearing_deg - transfer_before.bearing_deg) < 0.25 &&
              std::abs(transfer_after.mil - transfer_before.mil) < 5.0,
          "a conflicting target group does not rewrite shared correction");
    const auto south_adjusted = model.solution(south, Arc::low);
    check(south_adjusted.bearing_deg < south_plain.bearing_deg - 1.0,
          "a coherent target-specific shift remains useful near that target");

    const auto high_plain = corrected_solution(base, north, baseline, Arc::high);
    const auto high_prediction = model.solution(north, Arc::high);
    check(std::abs(high_prediction.bearing_deg - high_plain.bearing_deg) < 1e-9 &&
              std::abs(high_prediction.mil - high_plain.mil) < 1e-9,
          "low-arc data does not contaminate the high arc");
    const auto low_before_high = model.solution(north, Arc::low);
    model.add_landing(snapshot(base, north, Arc::high),
                      impact_with_required_offset(base, north, Arc::high,
                                                  0.7, 12.0));
    model.add_landing(snapshot(base, north, Arc::high),
                      impact_with_required_offset(base, north, Arc::high,
                                                  0.7, 12.0));
    check(model.solution(north, Arc::high).bearing_deg >
              high_plain.bearing_deg + 0.35,
          "high-arc observations update the high-arc solution");
    check(std::abs(model.solution(north, Arc::low).bearing_deg -
                   low_before_high.bearing_deg) < 1e-9,
          "high-arc observations leave the low-arc solution untouched");

    model.clear();
    check(model.sample_count() == 0,
          "clear removes active online observations");
    const auto restored = model.solution(north, Arc::low);
    check(std::abs(restored.bearing_deg - plain.bearing_deg) < 1e-9 &&
              std::abs(restored.mil - plain.mil) < 1e-9,
          "clear restores the untouched two-shot model");

    const auto edited_shot = snapshot(base, north, Arc::low, 0.4);
    model.add_landing(
        edited_shot,
        impact_with_required_offset(base, north, Arc::low, 1.0, 10.0, 0.4));
    check(model.solution(north, Arc::low).bearing_deg > plain.bearing_deg,
          "recorded firing settings account for manual bearing adjustment");

    const double tilt = 2.0 * std::numbers::pi / 180.0;
    const PlatformCalibration tilted{{{{1, 0, 0},
                                       {0, std::cos(tilt), -std::sin(tilt)},
                                       {0, std::sin(tilt), std::cos(tilt)}}}, 0.0};
    ContinuousCalibration rotated(base, tilted);
    const auto tilted_plain = corrected_solution(base, north, tilted, Arc::low);
    for (int shot = 0; shot < 3; ++shot) {
        rotated.add_landing(
            {north, Arc::low, tilted_plain.bearing_deg, tilted_plain.mil},
            impact_with_rotated_baseline(base, north, Arc::low, tilted,
                                         0.8, 8.0));
    }
    const auto tilted_online = rotated.solution(north, Arc::low);
    check(tilted_online.bearing_deg > tilted_plain.bearing_deg + 0.5 &&
              tilted_online.mil > tilted_plain.mil + 5.0,
          "online correction composes with a non-identity two-shot rotation");

    ContinuousCalibration edge(base, baseline);
    try {
        edge.add_landing(snapshot(base, {50, 62}, Arc::low),
                         {50, 61.5});
        check(edge.sample_count() == 1,
              "a physically reachable impact below the sight-table minimum is kept");
    } catch (...) {
        check(false,
              "an undershoot just below the sight-table minimum is a valid observation");
    }

    rejects([&] {
        auto invalid = snapshot(base, north, Arc::low);
        invalid.mil = std::numeric_limits<double>::quiet_NaN();
        model.add_landing(invalid, north);
    }, "non-finite firing settings are rejected");
    rejects([&] {
        model.add_landing(snapshot(base, north, Arc::low), base);
    }, "an impact at the gun position is rejected");

    if (failures) return 1;
    std::cout << "All continuous calibration tests passed\n";
}
