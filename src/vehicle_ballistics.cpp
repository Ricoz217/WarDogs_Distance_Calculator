#include "wardogs/vehicle_ballistics.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <stdexcept>
#include <utility>

namespace wardogs {
namespace {

using TableEntry = std::pair<double, double>;

// SPH-2 flat-ground observations from https://wardogs.t0ki.cn/js/data.js,
// retrieved 2026-09-11. Values are treated as sight milliradians.
constexpr std::array<TableEntry, 59> low_table{{
    TableEntry{1181, 20}, {1232, 30}, {1283, 40}, {1334, 50}, {1384, 60},
    {1433, 70}, {1482, 80}, {1529, 90}, {1576, 100}, {1622, 110},
    {1666, 120}, {1709, 130}, {1751, 140}, {1792, 150}, {1832, 160},
    {1870, 170}, {1907, 180}, {1944, 190}, {1979, 200}, {2014, 210},
    {2046, 220}, {2079, 230}, {2110, 240}, {2139, 250}, {2168, 260},
    {2196, 270}, {2223, 280}, {2249, 290}, {2273, 300}, {2296, 310},
    {2319, 320}, {2341, 330}, {2362, 340}, {2383, 350}, {2403, 360},
    {2422, 370}, {2439, 380}, {2456, 390}, {2471, 400}, {2485, 410},
    {2499, 420}, {2513, 430}, {2526, 440}, {2538, 450}, {2550, 460},
    {2561, 470}, {2570, 480}, {2579, 490}, {2586, 500}, {2593, 510},
    {2599, 520}, {2605, 530}, {2610, 540}, {2615, 550}, {2620, 560},
    {2623, 570}, {2626, 580}, {2628, 590}, {2629, 600},
}};

constexpr std::array<TableEntry, 80> high_table{{
    TableEntry{2629, 610}, {2629, 620}, {2628, 630}, {2626, 640},
    {2624, 650}, {2621, 660}, {2617, 670}, {2613, 680}, {2609, 690},
    {2604, 700}, {2599, 710}, {2592, 720}, {2584, 730}, {2576, 740},
    {2567, 750}, {2557, 760}, {2546, 770}, {2536, 780}, {2524, 790},
    {2513, 800}, {2501, 810}, {2488, 820}, {2474, 830}, {2460, 840},
    {2444, 850}, {2429, 860}, {2412, 870}, {2395, 880}, {2378, 890},
    {2360, 900}, {2342, 910}, {2323, 920}, {2303, 930}, {2282, 940},
    {2261, 950}, {2239, 960}, {2217, 970}, {2194, 980}, {2171, 990},
    {2147, 1000}, {2123, 1010}, {2098, 1020}, {2072, 1030},
    {2046, 1040}, {2019, 1050}, {1991, 1060}, {1963, 1070},
    {1934, 1080}, {1905, 1090}, {1875, 1100}, {1844, 1110},
    {1813, 1120}, {1782, 1130}, {1750, 1140}, {1717, 1150},
    {1684, 1160}, {1650, 1170}, {1616, 1180}, {1582, 1190},
    {1547, 1200}, {1512, 1210}, {1475, 1220}, {1438, 1230},
    {1401, 1240}, {1363, 1250}, {1324, 1260}, {1285, 1270},
    {1245, 1280}, {1205, 1290}, {1165, 1300}, {1124, 1310},
    {1083, 1320}, {1041, 1330}, {999, 1340}, {956, 1350}, {913, 1360},
    {869, 1370}, {825, 1380}, {780, 1390}, {735, 1400},
}};

std::span<const TableEntry> table_for(Arc arc) {
    return arc == Arc::low ? std::span<const TableEntry>{low_table}
                           : std::span<const TableEntry>{high_table};
}

double interpolate(double value, std::span<const TableEntry> pairs,
                   bool input_is_distance, const char* label) {
    const auto input = [input_is_distance](const TableEntry& pair) {
        return input_is_distance ? pair.first : pair.second;
    };
    const auto output = [input_is_distance](const TableEntry& pair) {
        return input_is_distance ? pair.second : pair.first;
    };
    if (value < input(pairs.front()) || value > input(pairs.back())) {
        throw std::invalid_argument(label);
    }
    for (std::size_t index = 1; index < pairs.size(); ++index) {
        const auto& left = pairs[index - 1];
        const auto& right = pairs[index];
        const double x1 = input(left);
        const double x2 = input(right);
        if (x1 <= value && value <= x2) {
            if (x1 == x2) return output(left);
            const double fraction = (value - x1) / (x2 - x1);
            return output(left) + fraction * (output(right) - output(left));
        }
    }
    return output(pairs.back());
}

double dot(Vector3 first, Vector3 second) {
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

Vector3 cross(Vector3 first, Vector3 second) {
    return {first[1] * second[2] - first[2] * second[1],
            first[2] * second[0] - first[0] * second[2],
            first[0] * second[1] - first[1] * second[0]};
}

Vector3 normalize(Vector3 vector) {
    const double length = std::sqrt(dot(vector, vector));
    if (length < 1e-12) {
        throw std::invalid_argument("两发方向无法构成稳定的校准基准");
    }
    for (double& value : vector) value /= length;
    return vector;
}

Matrix3 transpose(const Matrix3& matrix) {
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            result[row][column] = matrix[column][row];
    return result;
}

Vector3 matrix_vector(const Matrix3& matrix, Vector3 vector) {
    return {dot(matrix[0], vector), dot(matrix[1], vector),
            dot(matrix[2], vector)};
}

Matrix3 matrix_multiply(const Matrix3& first, const Matrix3& second) {
    const auto columns = transpose(second);
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
            result[row][column] = dot(first[row], columns[column]);
    return result;
}

Matrix3 pair_basis(Vector3 first, Vector3 second) {
    Vector3 sum{}, difference{};
    for (std::size_t index = 0; index < 3; ++index) {
        sum[index] = first[index] + second[index];
        difference[index] = first[index] - second[index];
    }
    const auto bisector = normalize(sum);
    difference = normalize(difference);
    const auto normal = normalize(cross(bisector, difference));
    return transpose(Matrix3{bisector, difference, normal});
}

std::pair<double, double> shot_geometry(Point base, Point point,
                                        const char* label) {
    const double dx = point.x - base.x;
    const double dy = point.y - base.y;
    const double distance_m = std::hypot(dx, dy) * 100.0;
    if (distance_m <= 0.0) throw std::invalid_argument(label);
    double bearing = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (bearing < 0.0) bearing += 360.0;
    return {distance_m, bearing};
}

double bearing_separation(double first, double second) {
    double value = std::fmod(second - first + 540.0, 360.0) - 180.0;
    return std::abs(value);
}

double height_delta(Point base, Point point, const HeightLookup& lookup) {
    if (!lookup) return 0.0;
    const auto base_height = lookup(base);
    const auto point_height = lookup(point);
    if (!base_height) throw std::invalid_argument("炮位坐标不在高度数据范围内");
    if (!point_height) throw std::invalid_argument("瞄准点或落点不在高度数据范围内");
    return *point_height - *base_height;
}

double equivalent_flat_range(double horizontal_distance_m, double height_delta_m,
                             Arc arc) {
    if (horizontal_distance_m <= 0.0)
        throw std::invalid_argument("水平射程必须大于 0 m");
    const double discriminant = sph2_maximum_range_m * sph2_maximum_range_m -
        horizontal_distance_m * horizontal_distance_m -
        2.0 * sph2_maximum_range_m * height_delta_m;
    if (discriminant < -1e-7)
        throw std::invalid_argument("目标超出弹道包线");
    const double root = std::sqrt(std::max(0.0, discriminant));
    const double numerator = arc == Arc::low ? sph2_maximum_range_m - root
                                             : sph2_maximum_range_m + root;
    const double tangent = numerator / horizontal_distance_m;
    if (tangent <= 0.0 || !std::isfinite(tangent))
        throw std::invalid_argument("所选弹道无法命中目标高差");
    const double elevation = std::atan(tangent);
    return std::clamp(sph2_maximum_range_m * std::sin(2.0 * elevation), 0.0,
                      sph2_maximum_range_m);
}

}  // namespace

Matrix3 identity_rotation() {
    return {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
}

Vector3 PlatformCalibration::local_to_world(Vector3 direction) const {
    return matrix_vector(rotation, direction);
}

Vector3 PlatformCalibration::world_to_local(Vector3 direction) const {
    return matrix_vector(transpose(rotation), direction);
}

Vector3 direction_from_bearing_and_mil(double bearing_deg, double mil) {
    const double bearing = bearing_deg * std::numbers::pi / 180.0;
    const double elevation = mil / 1000.0;
    const double horizontal = std::cos(elevation);
    return {horizontal * std::sin(bearing), horizontal * std::cos(bearing),
            std::sin(elevation)};
}

double sph2_mil_for_distance(double distance_m, Arc arc) {
    auto pairs = table_for(arc);
    if (arc == Arc::high) {
        std::array<TableEntry, high_table.size()> ordered{};
        std::copy(pairs.begin(), pairs.end(), ordered.begin());
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const auto& left, const auto& right) {
                             return left.first < right.first;
                         });
        return interpolate(distance_m, ordered, true, "射程超出支持范围");
    }
    return interpolate(distance_m, pairs, true, "射程超出支持范围");
}

double sph2_world_mil_for_distance(double distance_m, Arc arc) {
    if (distance_m < 0.0 || distance_m > sph2_maximum_range_m)
        throw std::invalid_argument("射程超出支持范围");
    if (arc == Arc::low) {
        std::array<TableEntry, low_table.size() + 1> extended{};
        extended[0] = {0.0, 0.0};
        std::copy(low_table.begin(), low_table.end(), extended.begin() + 1);
        return interpolate(distance_m, extended, true, "射程超出支持范围");
    }
    std::array<TableEntry, high_table.size() + 1> extended{};
    extended[0] = {0.0, std::numbers::pi * 500.0};
    std::copy(high_table.begin(), high_table.end(), extended.begin() + 1);
    std::stable_sort(extended.begin(), extended.end(),
                     [](const auto& left, const auto& right) {
                         return left.first < right.first;
                     });
    return interpolate(distance_m, extended, true, "射程超出支持范围");
}

double sph2_distance_for_mil(double mil, Arc arc) {
    auto pairs = table_for(arc);
    return interpolate(mil, pairs, false, "分划超出支持范围");
}

double sph2_mil_for_trajectory(double horizontal_distance_m,
                               double height_delta_m, Arc arc,
                               bool extend_to_physical_endpoint) {
    if (std::abs(height_delta_m) < 1e-9) {
        return extend_to_physical_endpoint
            ? sph2_world_mil_for_distance(horizontal_distance_m, arc)
            : sph2_mil_for_distance(horizontal_distance_m, arc);
    }
    const double equivalent =
        equivalent_flat_range(horizontal_distance_m, height_delta_m, arc);
    return extend_to_physical_endpoint
        ? sph2_world_mil_for_distance(equivalent, arc)
        : sph2_mil_for_distance(equivalent, arc);
}

PlatformCalibration calibrate_platform(Point base, const CalibrationShot& first,
                                       const CalibrationShot& second,
                                       const HeightLookup& height_lookup) {
    const auto first_aim = shot_geometry(base, first.aim_point, "第一发瞄准点不能与炮位重合");
    const auto second_aim = shot_geometry(base, second.aim_point, "第二发瞄准点不能与炮位重合");
    const double separation = bearing_separation(first_aim.second, second_aim.second);
    if (separation < minimum_calibration_separation_deg ||
        separation > maximum_calibration_separation_deg)
        throw std::invalid_argument("两发计划方位角差必须在 30°～150°之间");
    const auto first_impact =
        shot_geometry(base, first.impact_point, "第一发实际落点不能与炮位重合");
    const auto second_impact =
        shot_geometry(base, second.impact_point, "第二发实际落点不能与炮位重合");
    const std::array nominal{
        direction_from_bearing_and_mil(
            first_aim.second,
            sph2_mil_for_trajectory(first_aim.first,
                                    height_delta(base, first.aim_point, height_lookup),
                                    first.arc)),
        direction_from_bearing_and_mil(
            second_aim.second,
            sph2_mil_for_trajectory(second_aim.first,
                                    height_delta(base, second.aim_point, height_lookup),
                                    second.arc))};
    const std::array observed{
        direction_from_bearing_and_mil(
            first_impact.second,
            sph2_mil_for_trajectory(
                first_impact.first,
                height_delta(base, first.impact_point, height_lookup), first.arc,
                true)),
        direction_from_bearing_and_mil(
            second_impact.second,
            sph2_mil_for_trajectory(
                second_impact.first,
                height_delta(base, second.impact_point, height_lookup), second.arc,
                true))};
    const double nominal_angle =
        std::acos(std::clamp(dot(nominal[0], nominal[1]), -1.0, 1.0));
    const double observed_angle =
        std::acos(std::clamp(dot(observed[0], observed[1]), -1.0, 1.0));
    return {matrix_multiply(pair_basis(observed[0], observed[1]),
                            transpose(pair_basis(nominal[0], nominal[1]))),
            std::abs(observed_angle - nominal_angle) * 180.0 /
                std::numbers::pi};
}

CorrectedSolution corrected_solution(Point base, Point target,
                                     const PlatformCalibration& calibration,
                                     Arc arc, double height_delta_m) {
    const auto geometry = shot_geometry(base, target, "目标点不能与炮位重合");
    const double desired_mil = sph2_mil_for_trajectory(
        geometry.first, height_delta_m, arc, true);
    const auto desired_world =
        direction_from_bearing_and_mil(geometry.second, desired_mil);
    const auto corrected = calibration.world_to_local(desired_world);
    double bearing = std::atan2(corrected[0], corrected[1]) * 180.0 /
                     std::numbers::pi;
    if (bearing < 0.0) bearing += 360.0;
    const double mil =
        std::atan2(corrected[2], std::hypot(corrected[0], corrected[1])) * 1000.0;
    return {arc, bearing, sph2_distance_for_mil(mil, arc), mil};
}

}  // namespace wardogs
