#pragma once

#include "wardogs/core.hpp"

#include <array>
#include <functional>
#include <optional>

namespace wardogs {

enum class Arc { low, high };

using Vector3 = std::array<double, 3>;
using Matrix3 = std::array<Vector3, 3>;
using HeightLookup = std::function<std::optional<double>(Point)>;

struct CalibrationShot {
    Point aim_point;
    Point impact_point;
    Arc arc{Arc::low};
};

struct CorrectedSolution {
    Arc arc{Arc::low};
    double bearing_deg{};
    double reticle_distance_m{};
    double mil{};
};

struct PlatformCalibration {
    Matrix3 rotation{};
    double pair_angle_residual_deg{};

    [[nodiscard]] Vector3 local_to_world(Vector3 direction) const;
    [[nodiscard]] Vector3 world_to_local(Vector3 direction) const;
};

inline constexpr double sph2_maximum_range_m = 2629.0;
inline constexpr double minimum_calibration_separation_deg = 30.0;
inline constexpr double maximum_calibration_separation_deg = 150.0;

[[nodiscard]] Matrix3 identity_rotation();
[[nodiscard]] Vector3 direction_from_bearing_and_mil(double bearing_deg,
                                                     double mil);
[[nodiscard]] double sph2_mil_for_distance(double distance_m, Arc arc);
[[nodiscard]] double sph2_world_mil_for_distance(double distance_m, Arc arc);
[[nodiscard]] double sph2_distance_for_mil(double mil, Arc arc);
[[nodiscard]] double sph2_mil_for_trajectory(double horizontal_distance_m,
                                             double height_delta_m, Arc arc,
                                             bool extend_to_physical_endpoint = false);
[[nodiscard]] PlatformCalibration calibrate_platform(
    Point base, const CalibrationShot& first, const CalibrationShot& second,
    const HeightLookup& height_lookup = {});
[[nodiscard]] CorrectedSolution corrected_solution(
    Point base, Point target, const PlatformCalibration& calibration, Arc arc,
    double height_delta_m = 0.0);

}  // namespace wardogs
