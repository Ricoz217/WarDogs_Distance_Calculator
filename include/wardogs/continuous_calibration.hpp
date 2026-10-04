#pragma once

#include "wardogs/vehicle_ballistics.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace wardogs {

// Captures the firing settings used for one observed landing. Callers may
// supply actual settings if they differ from the displayed solution.
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
    [[nodiscard]] FiringSnapshot firing_snapshot(
        Point target, Arc arc, double height_delta_m = 0.0) const;
    ObservationAssessment add_landing(
        FiringSnapshot firing, Point impact,
        double impact_height_delta_m = 0.0);
    void clear();
    [[nodiscard]] std::size_t sample_count() const noexcept;
    [[nodiscard]] const PlatformCalibration& global_calibration() const noexcept;
    [[nodiscard]] double global_rotation_adjustment_deg() const noexcept;

private:
    struct Sample {
        Point target;
        Arc arc;
        double target_bearing_deg;
        double target_range_m;
        double target_height_delta_m;
        double bearing_offset_deg;
        double mil_offset;
        double firing_bearing_deg;
        double firing_mil;
        Point impact;
        double impact_height_delta_m;
        Vector3 local_direction;
        Vector3 world_direction;
    };

    [[nodiscard]] double confidence(std::size_t index) const;
    [[nodiscard]] double proximity(const Sample& sample, double bearing_deg,
                                   double range_m, double height_delta_m) const;
    [[nodiscard]] std::pair<double, double> correction(
        Point target, Arc arc, double height_delta_m) const;
    void refresh_offsets();
    void refit_global_calibration();

    Point base_;
    PlatformCalibration baseline_;
    PlatformCalibration active_;
    std::vector<Sample> samples_;
    std::vector<double> confidence_scores_;
};

}  // namespace wardogs
