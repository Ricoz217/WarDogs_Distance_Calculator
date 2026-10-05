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

double trajectory_elevation(double distance_m, double height_delta_m,
                            wardogs::Arc arc) {
    const double maximum = wardogs::sph2_maximum_range_m;
    const double root = std::sqrt(maximum * maximum - distance_m * distance_m -
                                  2.0 * maximum * height_delta_m);
    return std::atan((maximum + (arc == wardogs::Arc::low ? -root : root)) /
                     distance_m);
}

double flat_trajectory_elevation(double distance_m, wardogs::Arc arc) {
    return trajectory_elevation(distance_m, 0.0, arc);
}

wardogs::Vector3 direction_from_bearing_and_elevation(double bearing_deg,
                                                       double elevation_rad) {
    const double bearing = bearing_deg * std::numbers::pi / 180.0;
    const double horizontal = std::cos(elevation_rad);
    return {horizontal * std::sin(bearing),
            horizontal * std::cos(bearing), std::sin(elevation_rad)};
}

wardogs::Point impact_for_rotation(wardogs::Point base, wardogs::Point aim,
                                   wardogs::Arc arc,
                                   const wardogs::Matrix3& rotation,
                                   double aim_height_delta_m = 0.0,
                                   double impact_height_delta_m = 0.0) {
    const double dx = aim.x - base.x;
    const double dy = aim.y - base.y;
    const double distance = std::hypot(dx, dy) * 100.0;
    double bearing = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (bearing < 0) bearing += 360.0;
    const auto direction = direction_from_bearing_and_elevation(
        bearing, trajectory_elevation(distance, aim_height_delta_m, arc));
    wardogs::Vector3 world{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            world[row] += rotation[row][column] * direction[column];
    double actual_bearing =
        std::atan2(world[0], world[1]) * 180.0 / std::numbers::pi;
    if (actual_bearing < 0) actual_bearing += 360.0;
    const double actual_elevation =
        std::atan2(world[2], std::hypot(world[0], world[1]));
    const double cosine = std::cos(actual_elevation);
    const double tangent = std::tan(actual_elevation);
    const double center = wardogs::sph2_maximum_range_m * cosine * cosine *
                          tangent;
    const double root = std::sqrt(
        center * center - 2.0 * wardogs::sph2_maximum_range_m * cosine *
                              cosine * impact_height_delta_m);
    const double actual_distance = center + root;
    return {base.x + std::sin(actual_bearing * std::numbers::pi / 180.0) *
                         actual_distance / 100.0,
            base.y + std::cos(actual_bearing * std::numbers::pi / 180.0) *
                         actual_distance / 100.0};
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

    const Point base{50, 50};
    const auto rotation = rotation_x(5);
    const Point first_aim{50, 68};
    const Point second_aim{68, 50};
    const auto calibration = wardogs::calibrate_platform(
        base,
        CalibrationShot{first_aim,
                        impact_for_rotation(base, first_aim, Arc::low, rotation),
                        Arc::low},
        CalibrationShot{
            second_aim,
            impact_for_rotation(base, second_aim, Arc::high, rotation),
            Arc::high});
    const auto probe = wardogs::direction_from_bearing_and_mil(42, 500);
    const auto recovered = calibration.local_to_world(probe);
    for (std::size_t row = 0; row < 3; ++row) {
        double expected = 0;
        for (std::size_t column = 0; column < 3; ++column)
            expected += rotation[row][column] * probe[column];
        close(recovered[row], expected, "two shots recover platform rotation",
              1e-9);
    }
    close(calibration.pair_angle_residual_deg, 0,
          "synthetic calibration residual", 1e-9);

    const Point raised_first_aim{50, 68};
    const Point lowered_second_aim{70, 50};
    constexpr double first_aim_height = 80.0;
    constexpr double first_impact_height = 25.0;
    constexpr double second_aim_height = -35.0;
    constexpr double second_impact_height = -10.0;
    const Point raised_first_impact = impact_for_rotation(
        base, raised_first_aim, Arc::low, rotation, first_aim_height,
        first_impact_height);
    const Point lowered_second_impact = impact_for_rotation(
        base, lowered_second_aim, Arc::high, rotation, second_aim_height,
        second_impact_height);
    const auto varied_height = [&](Point point) -> std::optional<double> {
        if (point == base) return 0.0;
        if (point == raised_first_aim) return first_aim_height;
        if (point == raised_first_impact) return first_impact_height;
        if (point == lowered_second_aim) return second_aim_height;
        if (point == lowered_second_impact) return second_impact_height;
        return std::nullopt;
    };
    const auto height_calibration = wardogs::calibrate_platform(
        base,
        {raised_first_aim, raised_first_impact, Arc::low},
        {lowered_second_aim, lowered_second_impact, Arc::high},
        varied_height);
    close(height_calibration.pair_angle_residual_deg, 0,
          "height-aware synthetic calibration closes in physical angle space",
          1e-8);
    const auto height_recovered = height_calibration.local_to_world(probe);
    for (std::size_t row = 0; row < 3; ++row) {
        double expected = 0;
        for (std::size_t column = 0; column < 3; ++column)
            expected += rotation[row][column] * probe[column];
        close(height_recovered[row], expected,
              "height-aware calibration recovers platform rotation", 1e-8);
    }

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
    const double desired_world_elevation =
        flat_trajectory_elevation(1800.0, Arc::low);
    const double required_local_elevation =
        desired_world_elevation - 5.0 * std::numbers::pi / 180.0;
    const double required_equivalent_range = wardogs::sph2_maximum_range_m *
        std::sin(2.0 * required_local_elevation);
    close(tilted_solution.mil,
          wardogs::sph2_mil_for_distance(required_equivalent_range, Arc::low),
          "platform tilt rotates physical launch elevation before sight lookup",
          1e-8);

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
