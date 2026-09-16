#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace wardogs {

// The displayed firing settings must be frozen before the shot. If the player
// adjusts them manually, the adjusted values can be supplied instead.
struct FiringSnapshot {
    Point target;
    Arc arc{Arc::low};
    double bearing_deg{};
    double mil{};
    double target_height_delta_m{};
};

struct ObservationAssessment {
    // Relative, uncalibrated model-consistency score in [0, 1].
    double confidence{};
    std::size_t observation_count{};
};

class ContinuousCalibration {
public:
    ContinuousCalibration(Point base, PlatformCalibration baseline);

    [[nodiscard]] CorrectedSolution solution(
        Point target, Arc arc, double height_delta_m = 0.0) const;
    ObservationAssessment add_landing(
        FiringSnapshot firing, Point impact,
        double impact_height_delta_m = 0.0);
    void clear();
    [[nodiscard]] std::size_t sample_count() const noexcept;

private:
    struct Sample {
        Point target;
        Arc arc;
        double target_bearing_deg;
        double target_range_m;
        double target_height_delta_m;
        double bearing_offset_deg;
        double mil_offset;
    };

    [[nodiscard]] double confidence(std::size_t index) const;
    [[nodiscard]] double proximity(const Sample& sample, double bearing_deg,
                                   double range_m, double height_delta_m) const;
    [[nodiscard]] std::pair<double, double> correction(
        Point target, Arc arc, double height_delta_m) const;

    Point base_;
    PlatformCalibration baseline_;
    std::vector<Sample> samples_;
    std::vector<double> confidence_scores_;
};

}  // namespace wardogs
