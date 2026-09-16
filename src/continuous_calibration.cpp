#include "wardogs/continuous_calibration.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace wardogs {
namespace {

constexpr double maximum_bearing_correction_deg = 3.0;
constexpr double maximum_mil_correction = 50.0;
constexpr double bearing_noise_deg = 0.45;
constexpr double mil_noise = 10.0;

double wrapped_difference(double first, double second) {
    return std::remainder(first - second, 360.0);
}

double clamp_correction(double value, double limit) {
    return std::clamp(value, -limit, limit);
}

void require_finite(double value, const char* label) {
    if (!std::isfinite(value)) throw std::invalid_argument(label);
}

std::pair<double, double> bearing_and_range(Point base, Point point) {
    const double dx = point.x - base.x;
    const double dy = point.y - base.y;
    const double range = std::hypot(dx, dy) * 100.0;
    if (!std::isfinite(range) || range <= 0.0)
        throw std::invalid_argument("射击目标不能与炮位重合");
    double bearing = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (bearing < 0.0) bearing += 360.0;
    return {bearing, range};
}

double weighted_median(std::vector<std::pair<double, double>> values) {
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    double total = 0.0;
    for (const auto& value : values) total += value.second;
    double accumulated = 0.0;
    for (const auto& value : values) {
        accumulated += value.second;
        if (accumulated >= total * 0.5) return value.first;
    }
    return values.back().first;
}

}  // namespace

ContinuousCalibration::ContinuousCalibration(Point base,
                                             PlatformCalibration baseline)
    : base_(base), baseline_(baseline) {}

std::size_t ContinuousCalibration::sample_count() const noexcept {
    return samples_.size();
}

void ContinuousCalibration::clear() {
    samples_.clear();
    confidence_scores_.clear();
}

double ContinuousCalibration::proximity(const Sample& sample,
                                        double bearing_deg, double range_m,
                                        double height_delta_m) const {
    const double angle = wrapped_difference(sample.target_bearing_deg,
                                            bearing_deg) / 22.0;
    const double range = (sample.target_range_m - range_m) / 400.0;
    const double height =
        (sample.target_height_delta_m - height_delta_m) / 120.0;
    return std::exp(-0.5 * (angle * angle + range * range + height * height));
}

double ContinuousCalibration::confidence(std::size_t index) const {
    const auto& current = samples_.at(index);
    std::vector<std::pair<double, double>> bearings;
    std::vector<std::pair<double, double>> mils;
    for (std::size_t other = 0; other < samples_.size(); ++other) {
        if (other == index || samples_[other].arc != current.arc) continue;
        const double weight = proximity(
            samples_[other], current.target_bearing_deg,
            current.target_range_m, current.target_height_delta_m);
        if (weight < 0.05) continue;
        bearings.emplace_back(samples_[other].bearing_offset_deg, weight);
        mils.emplace_back(samples_[other].mil_offset, weight);
    }

    // An isolated observation is useful, but a very large correction cannot
    // safely drive the model until another shot corroborates it.
    if (bearings.empty()) {
        const double magnitude = std::hypot(
            current.bearing_offset_deg / maximum_bearing_correction_deg,
            current.mil_offset / maximum_mil_correction);
        const double excess = std::max(0.0, magnitude - 1.0);
        return 0.6 / (1.0 + excess * excess * excess * excess);
    }
    const double bearing_center = weighted_median(std::move(bearings));
    const double mil_center = weighted_median(std::move(mils));
    const double normalized_error = std::hypot(
        (current.bearing_offset_deg - bearing_center) / bearing_noise_deg,
        (current.mil_offset - mil_center) / mil_noise);
    const double scaled = normalized_error / 2.0;
    return 0.08 + 0.92 / (1.0 + scaled * scaled * scaled * scaled);
}

ObservationAssessment ContinuousCalibration::add_landing(
    FiringSnapshot firing, Point impact, double impact_height_delta_m) {
    for (double coordinate : {base_.x, base_.y, firing.target.x,
                              firing.target.y, impact.x, impact.y})
        require_finite(coordinate, "坐标必须是有限数值");
    require_finite(firing.bearing_deg, "射击方位必须是有限数值");
    require_finite(firing.mil, "射击分划必须是有限数值");
    require_finite(firing.target_height_delta_m, "目标高差必须是有限数值");
    require_finite(impact_height_delta_m, "落点高差必须是有限数值");

    // Validate the recorded firing settings and both geometries before
    // changing state. The impact's inverse solution is the setting that the
    // frozen two-shot baseline would have required to land there.
    (void)sph2_distance_for_mil(firing.mil, firing.arc);
    (void)corrected_solution(base_, firing.target, baseline_, firing.arc,
                             firing.target_height_delta_m);
    const auto impact_solution = required_firing_angles(
        base_, impact, baseline_, firing.arc, impact_height_delta_m);
    const auto [bearing, range] = bearing_and_range(base_, firing.target);
    samples_.push_back({firing.target, firing.arc, bearing, range,
                        firing.target_height_delta_m,
                        wrapped_difference(firing.bearing_deg,
                                           impact_solution.bearing_deg),
                        firing.mil - impact_solution.mil});
    confidence_scores_.resize(samples_.size());
    for (std::size_t index = 0; index < samples_.size(); ++index)
        confidence_scores_[index] = confidence(index);
    return {confidence_scores_.back(), samples_.size()};
}

std::pair<double, double> ContinuousCalibration::correction(
    Point target, Arc arc, double height_delta_m) const {
    if (samples_.empty()) return {0.0, 0.0};
    const auto [bearing, range] = bearing_and_range(base_, target);

    struct Group {
        Point target;
        double bearing_sum{};
        double mil_sum{};
        double weight_sum{};
    };
    std::vector<Group> groups;
    double local_bearing_sum = 0.0;
    double local_mil_sum = 0.0;
    double local_weight_sum = 0.0;
    for (std::size_t index = 0; index < samples_.size(); ++index) {
        const auto& sample = samples_[index];
        if (sample.arc != arc) continue;
        const double quality = confidence_scores_[index];
        const double near = proximity(sample, bearing, range, height_delta_m);
        const double local_weight = quality * near;
        local_bearing_sum += local_weight * sample.bearing_offset_deg;
        local_mil_sum += local_weight * sample.mil_offset;
        local_weight_sum += local_weight;

        auto group = std::find_if(groups.begin(), groups.end(), [&](const Group& item) {
            return std::hypot(sample.target.x - item.target.x,
                              sample.target.y - item.target.y) * 100.0 < 50.0;
        });
        if (group == groups.end()) {
            groups.push_back({sample.target});
            group = std::prev(groups.end());
        }
        group->bearing_sum += quality * sample.bearing_offset_deg;
        group->mil_sum += quality * sample.mil_offset;
        group->weight_sum += quality;
    }
    if (groups.empty()) return {0.0, 0.0};

    // One heavily sampled target contributes one group, not one vote per shot.
    double shared_bearing_sum = 0.0;
    double shared_mil_sum = 0.0;
    double shared_weight_sum = 0.0;
    std::vector<std::pair<double, double>> group_bearings;
    std::vector<std::pair<double, double>> group_mils;
    for (const auto& group : groups) {
        const double group_weight = std::min(1.0, group.weight_sum / 2.0);
        group_bearings.emplace_back(group.bearing_sum / group.weight_sum,
                                    group_weight);
        group_mils.emplace_back(group.mil_sum / group.weight_sum,
                                group_weight);
    }
    const double median_bearing = weighted_median(group_bearings);
    const double median_mil = weighted_median(group_mils);
    for (std::size_t index = 0; index < groups.size(); ++index) {
        const double disagreement = std::hypot(
            (group_bearings[index].first - median_bearing) / 0.8,
            (group_mils[index].first - median_mil) / 20.0);
        const double scaled = disagreement / 2.0;
        const double robust_weight = 1.0 /
            (1.0 + scaled * scaled * scaled * scaled);
        const double weight = group_bearings[index].second * robust_weight;
        shared_bearing_sum += weight * group_bearings[index].first;
        shared_mil_sum += weight * group_mils[index].first;
        shared_weight_sum += weight;
    }
    const double shared_strength = std::min(0.55, 0.3 * shared_weight_sum);
    const double shared_bearing = shared_strength *
                                  shared_bearing_sum / shared_weight_sum;
    const double shared_mil = shared_strength * shared_mil_sum / shared_weight_sum;

    const double local_strength = std::min(1.0, local_weight_sum / 2.0);
    const double local_bearing = local_weight_sum > 0.0
        ? local_bearing_sum / local_weight_sum : shared_bearing;
    const double local_mil = local_weight_sum > 0.0
        ? local_mil_sum / local_weight_sum : shared_mil;
    return {
        clamp_correction(shared_bearing + local_strength *
                             (local_bearing - shared_bearing),
                         maximum_bearing_correction_deg),
        clamp_correction(shared_mil + local_strength *
                             (local_mil - shared_mil),
                         maximum_mil_correction)};
}

CorrectedSolution ContinuousCalibration::solution(
    Point target, Arc arc, double height_delta_m) const {
    auto result = corrected_solution(base_, target, baseline_, arc,
                                     height_delta_m);
    const auto [bearing, mil] = correction(target, arc, height_delta_m);
    result.bearing_deg = std::fmod(result.bearing_deg + bearing + 360.0, 360.0);
    result.mil += mil;
    result.reticle_distance_m = sph2_distance_for_mil(result.mil, arc);
    return result;
}

}  // namespace wardogs
