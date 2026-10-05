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

double flat_trajectory_elevation(double distance_m, wardogs::Arc arc) {
    const double principal =
        std::asin(distance_m / wardogs::sph2_maximum_range_m);
    return arc == wardogs::Arc::low ? principal / 2.0
                                     : (std::numbers::pi - principal) / 2.0;
}

wardogs::Vector3 direction_from_bearing_and_elevation(double bearing_deg,
                                                       double elevation_rad) {
    const double bearing = bearing_deg * std::numbers::pi / 180.0;
    const double horizontal = std::cos(elevation_rad);
    return {horizontal * std::sin(bearing),
            horizontal * std::cos(bearing), std::sin(elevation_rad)};
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
    const double local_flat_range = wardogs::sph2_distance_for_mil(
        firing.mil - mil_offset, arc);
    const auto world = calibration.local_to_world(
        direction_from_bearing_and_elevation(
            firing.bearing_deg - bearing_offset_deg,
            flat_trajectory_elevation(local_flat_range, arc)));
    const double bearing = std::atan2(world[0], world[1]);
    const double world_elevation =
        std::atan2(world[2], std::hypot(world[0], world[1]));
    const double range = wardogs::sph2_maximum_range_m *
                         std::sin(2.0 * world_elevation);
    return {base.x + std::sin(bearing) * range / 100.0,
            base.y + std::cos(bearing) * range / 100.0};
}

wardogs::Point impact_from_firing(
    wardogs::Point base, const wardogs::FiringSnapshot& firing,
    const wardogs::PlatformCalibration& actual_platform) {
    const double flat_range = wardogs::sph2_distance_for_mil(
        firing.mil, firing.arc);
    const auto local = direction_from_bearing_and_elevation(
        firing.bearing_deg,
        flat_trajectory_elevation(flat_range, firing.arc));
    const auto world = actual_platform.local_to_world(local);
    const double bearing = std::atan2(world[0], world[1]);
    const double elevation =
        std::atan2(world[2], std::hypot(world[0], world[1]));
    const double range = wardogs::sph2_maximum_range_m *
                         std::sin(2.0 * elevation);
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
    const auto direct_first = model.firing_snapshot(north, Arc::low);
    check(direct_first.target == north && direct_first.arc == Arc::low &&
              std::abs(direct_first.bearing_deg - initial.bearing_deg) < 1e-9 &&
              std::abs(direct_first.mil - initial.mil) < 1e-9,
          "one-step impact entry captures the current target and firing solution");
    check(std::abs(initial.bearing_deg - plain.bearing_deg) < 1e-9 &&
              std::abs(initial.mil - plain.mil) < 1e-9,
          "empty online model preserves the two-shot baseline");

    // A plausible first shot should remove most of the miss now that the
    // reticle overlay makes precise entry practical. Outliers remain gated by
    // their confidence score below.
    const auto first = model.add_landing(
        snapshot(base, north, Arc::low),
        impact_with_required_offset(base, north, Arc::low, 1.0, 10.0));
    check(first.confidence > 0.35 && first.confidence <= 1.0,
          "a first shot is provisional, not automatically unreliable");
    const auto after_first = model.solution(north, Arc::low);
    check(after_first.bearing_deg > plain.bearing_deg + 0.6 &&
              after_first.bearing_deg < plain.bearing_deg + 1.1 &&
              after_first.mil > plain.mil + 6.0 &&
              after_first.mil < plain.mil + 12.0,
          "one plausible shot applies most of the local correction immediately");

    model.add_landing(snapshot(base, north, Arc::low),
                      impact_with_required_offset(base, north, Arc::low, 1.05, 9.0));
    model.add_landing(snapshot(base, north, Arc::low),
                      impact_with_required_offset(base, north, Arc::low, 0.95, 11.0));
    const auto learned = model.solution(north, Arc::low);
    const auto direct_next = model.firing_snapshot(north, Arc::low, 12.0);
    const auto raised = model.solution(north, Arc::low, 12.0);
    check(direct_next.target == north && direct_next.arc == Arc::low &&
              direct_next.target_height_delta_m == 12.0 &&
              std::abs(direct_next.bearing_deg - raised.bearing_deg) < 1e-9 &&
              std::abs(direct_next.mil - raised.mil) < 1e-9,
          "later impact entry uses the latest compensation and target height");
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
    check(std::abs(high_prediction.bearing_deg - high_plain.bearing_deg) > 1e-5 ||
              std::abs(high_prediction.mil - high_plain.mil) > 1e-5,
          "low-arc observations transfer global platform information to high arc");
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
                   low_before_high.bearing_deg) > 1e-5,
          "high-arc observations can refine the shared global platform model");

    model.clear();
    check(model.sample_count() == 0,
          "clear removes active online observations");
    const auto restored = model.solution(north, Arc::low);
    check(std::abs(restored.bearing_deg - plain.bearing_deg) < 1e-9 &&
              std::abs(restored.mil - plain.mil) < 1e-9,
          "clear restores the untouched two-shot model");

    ContinuousCalibration global_model(base, baseline);
    const double global_tilt = 4.0 * std::numbers::pi / 180.0;
    const PlatformCalibration actual_platform{{{{1, 0, 0},
                                                 {0, std::cos(global_tilt),
                                                  -std::sin(global_tilt)},
                                                 {0, std::sin(global_tilt),
                                                  std::cos(global_tilt)}}},
                                               0.0};
    for (const Point training_target :
         {Point{50, 68}, Point{68, 50}, Point{38, 62}}) {
        for (int shot = 0; shot < 3; ++shot) {
            const auto firing = global_model.firing_snapshot(
                training_target, Arc::low);
            global_model.add_landing(
                firing, impact_from_firing(base, firing, actual_platform));
        }
    }
    const auto& learned_platform = global_model.global_calibration();
    const auto learned_probe = learned_platform.local_to_world(
        direction_from_bearing_and_elevation(32.0, 0.4));
    const auto actual_probe = actual_platform.local_to_world(
        direction_from_bearing_and_elevation(32.0, 0.4));
    const auto baseline_probe = baseline.local_to_world(
        direction_from_bearing_and_elevation(32.0, 0.4));
    double learned_error = 0.0;
    double baseline_error = 0.0;
    for (std::size_t index = 0; index < 3; ++index) {
        learned_error += std::abs(learned_probe[index] - actual_probe[index]);
        baseline_error += std::abs(baseline_probe[index] - actual_probe[index]);
    }
    check(learned_error < baseline_error * 0.55,
          "consistent continuous shots refine the global platform model");
    check(global_model.global_rotation_adjustment_deg() > 1.0,
          "global refinement exposes its accumulated rotation adjustment");
    global_model.clear();
    const auto reset_probe = global_model.global_calibration().local_to_world(
        direction_from_bearing_and_elevation(32.0, 0.4));
    check(std::abs(reset_probe[0] - baseline_probe[0]) < 1e-9 &&
              std::abs(reset_probe[1] - baseline_probe[1]) < 1e-9 &&
              std::abs(reset_probe[2] - baseline_probe[2]) < 1e-9,
          "clearing continuous compensation restores the original global model");
    check(global_model.global_rotation_adjustment_deg() < 1e-9,
          "clearing continuous compensation resets the global adjustment");

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
