#include "wardogs/vehicle_ballistics.hpp"

#include <array>
#include <cmath>
#include <functional>
#include <iostream>
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

void close(double actual, double expected, const char* message,
           double tolerance = 1e-8) {
    check(std::abs(actual - expected) <= tolerance, message);
}

void rejects(const std::function<void()>& action, const char* message) {
    try {
        action();
        check(false, message);
    } catch (const std::invalid_argument&) {
    }
}

wardogs::Matrix3 rotation_x(double degrees) {
    const double angle = degrees * std::numbers::pi / 180.0;
    return {{{1, 0, 0},
             {0, std::cos(angle), -std::sin(angle)},
             {0, std::sin(angle), std::cos(angle)}}};
}

wardogs::Vector3 direction_from_bearing_and_elevation(double bearing_deg,
                                                       double elevation_rad) {
    const double bearing = bearing_deg * std::numbers::pi / 180.0;
    const double horizontal = std::cos(elevation_rad);
    return {horizontal * std::sin(bearing),
            horizontal * std::cos(bearing), std::sin(elevation_rad)};
}

}  // namespace

int main() {
    using wardogs::Arc;
    using wardogs::CalibrationShot;
    using wardogs::Point;

    close(wardogs::sph2_mil_for_distance(1181, Arc::low), 20,
          "low table endpoint");
    close(wardogs::sph2_mil_for_distance(1206.5, Arc::low), 25,
          "low table interpolation");
    close(wardogs::sph2_distance_for_mil(900, Arc::high), 2360,
          "high inverse table");
    close(wardogs::sph2_mil_for_distance(2629, Arc::high), 610,
          "duplicate maximum range keeps the first high-arc sight value");
    check(wardogs::sph2_world_mil_for_distance(500, Arc::low) > 0,
          "observed low impact extends below sight range");
    check(wardogs::sph2_world_mil_for_distance(553.399, Arc::high) > 1400,
          "observed high impact extends toward vertical");
    rejects([] { (void)wardogs::sph2_mil_for_distance(700, Arc::low); },
            "unsupported low range is rejected");

    close(wardogs::sph2_mil_for_trajectory(
              1751.097084687197, 330.35, Arc::high),
          1124.19921875,
          "drag ensemble reproduces upstream high-arc height estimate",
          0.03);
    close(wardogs::sph2_mil_for_trajectory(1800.0, 100.0, Arc::low),
          210.654296875,
          "drag ensemble reproduces upstream low-arc height estimate",
          0.03);
    close(wardogs::sph2_mil_for_trajectory(
              1728.7180221192812, -41.125, Arc::high),
          1147.8292253057916,
          "drag ensemble reproduces upstream downhill estimate",
          0.03);

    const Point base{50, 50};
    const auto rotation = rotation_x(5);
    const Point field_base{97.88, 109.54};
    const Point first_aim{87.62, 95.51};
    const Point first_impact{89.06, 97.59};
    const Point second_aim{84.43, 116.13};
    const Point second_impact{85.89, 119.41};
    const auto measured_height = [&](Point point) -> std::optional<double> {
        if (point == field_base) return 0.0;
        if (point == first_aim) return -41.2;
        if (point == first_impact) return -10.9;
        if (point == second_aim) return 155.875;
        if (point == second_impact) return 248.875;
        return std::nullopt;
    };
    const CalibrationShot measured_first{first_aim, first_impact, Arc::high};
    const CalibrationShot measured_second{second_aim, second_impact, Arc::high};
    const auto drag_calibration = wardogs::calibrate_platform(
        field_base, measured_first, measured_second, measured_height);
    const auto flat_calibration = wardogs::calibrate_platform(
        field_base, measured_first, measured_second);
    check(std::isfinite(drag_calibration.pair_angle_residual_deg),
          "field calibration produces a finite fitted-drag residual");
    check(drag_calibration.pair_angle_residual_deg < 1.6,
          "fitted-drag model reduces the former vacuum-model field residual");
    check(std::isfinite(flat_calibration.pair_angle_residual_deg),
          "flat field calibration remains available as a diagnostic baseline");
    const auto replayed_first_solution = wardogs::corrected_solution(
        field_base, {80.58, 106.83}, drag_calibration, Arc::high, 330.35);
    close(replayed_first_solution.mil, 1072.741301,
          "published field log replays near its later converged command", 0.05);

    const auto probe = wardogs::direction_from_bearing_and_mil(42, 500);

    const auto identity = wardogs::PlatformCalibration{
        wardogs::identity_rotation(), 0};
    const auto flat = wardogs::corrected_solution(
        {20, 20}, {20, 38}, identity, Arc::low);
    const auto uphill = wardogs::corrected_solution(
        {20, 20}, {20, 38}, identity, Arc::low, 100);
    close(flat.reticle_distance_m, 1800, "flat table remains unchanged");
    check(uphill.reticle_distance_m > flat.reticle_distance_m,
          "uphill low solution increases sight distance");

    const auto tilted_solution = wardogs::corrected_solution(
        {20, 20}, {20, 38}, {rotation_x(5), 0.0}, Arc::low);
    close(tilted_solution.mil,
          flat.mil - 5.0 * std::numbers::pi / 180.0 * 1000.0,
          "platform tilt converts local elevation back to sight milliradians",
          0.03);

    std::vector<wardogs::DirectionObservation> observations;
    for (double bearing : {0.0, 45.0, 90.0, 180.0, 270.0}) {
        const auto local = direction_from_bearing_and_elevation(
            bearing, 35.0 * std::numbers::pi / 180.0);
        wardogs::Vector3 world{};
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                world[row] += rotation[row][column] * local[column];
        observations.push_back({local, world, 1.0});
    }
    observations.push_back({
        direction_from_bearing_and_elevation(225.0, 0.6),
        direction_from_bearing_and_elevation(20.0, 1.2), 0.05});
    const wardogs::PlatformCalibration refined =
        wardogs::refine_platform_calibration(
            {wardogs::identity_rotation(), 0.0}, observations, 1.5);
    const auto refined_probe = refined.local_to_world(probe);
    double refined_error = 0.0;
    double prior_error = 0.0;
    for (std::size_t row = 0; row < 3; ++row) {
        double expected = 0.0;
        for (std::size_t column = 0; column < 3; ++column)
            expected += rotation[row][column] * probe[column];
        refined_error += std::abs(refined_probe[row] - expected);
        prior_error += std::abs(probe[row] - expected);
    }
    check(refined_error < prior_error * 0.65,
          "weighted direction observations refine the global platform rotation");

    const Point near_base{96.08, 108.81};
    try {
        const auto near = wardogs::calibrate_platform(
            near_base,
            {{80.87, 108.82}, {82.01, 119.01}, Arc::high},
            {{95.06, 93.16}, {96.29, 103.28}, Arc::high});
        check(std::isfinite(near.pair_angle_residual_deg),
              "near-vehicle high impacts calibrate");
    } catch (...) {
        check(false, "near-vehicle high impacts calibrate");
    }

    rejects(
        [] {
            const Point origin{0, 0};
            (void)wardogs::calibrate_platform(
                origin, {{0, 18}, {0, 18}, Arc::low},
                {{std::sin(29.9 * std::numbers::pi / 180.0) * 18,
                  std::cos(29.9 * std::numbers::pi / 180.0) * 18},
                 {18, 0}, Arc::low});
        },
        "calibration requires 30 degree separation");

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All vehicle ballistics tests passed\n";
    return 0;
}
