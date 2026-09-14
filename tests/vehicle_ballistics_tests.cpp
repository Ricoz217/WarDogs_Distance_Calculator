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

wardogs::Point impact_for_rotation(wardogs::Point base, wardogs::Point aim,
                                   wardogs::Arc arc,
                                   const wardogs::Matrix3& rotation) {
    const double dx = aim.x - base.x;
    const double dy = aim.y - base.y;
    const double distance = std::hypot(dx, dy) * 100.0;
    double bearing = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (bearing < 0) bearing += 360.0;
    const auto direction = wardogs::direction_from_bearing_and_mil(
        bearing, wardogs::sph2_mil_for_distance(distance, arc));
    wardogs::Vector3 world{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            world[row] += rotation[row][column] * direction[column];
    double actual_bearing =
        std::atan2(world[0], world[1]) * 180.0 / std::numbers::pi;
    if (actual_bearing < 0) actual_bearing += 360.0;
    const double actual_mil =
        std::atan2(world[2], std::hypot(world[0], world[1])) * 1000.0;
    const double actual_distance =
        wardogs::sph2_distance_for_mil(actual_mil, arc);
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

    const auto identity = wardogs::PlatformCalibration{
        wardogs::identity_rotation(), 0};
    const auto flat = wardogs::corrected_solution(
        {20, 20}, {20, 38}, identity, Arc::low);
    const auto uphill = wardogs::corrected_solution(
        {20, 20}, {20, 38}, identity, Arc::low, 100);
    close(flat.reticle_distance_m, 1800, "flat table remains unchanged");
    check(uphill.reticle_distance_m > flat.reticle_distance_m,
          "uphill low solution increases sight distance");

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
