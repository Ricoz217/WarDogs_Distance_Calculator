#pragma once

#include "wardogs/vehicle_ballistics.hpp"

namespace wardogs::sph2_drag {

struct Solution {
    double command_mil{};
    // Internal trajectory direction paired with command_mil.  The fitted
    // full-model flat reference is shifted by the ensemble-median command
    // delta so calibration and the displayed solution use one definition.
    double effective_elevation_rad{};
    double full_model_elevation_rad{};
    double minimum_command_mil{};
    double maximum_command_mil{};
};

[[nodiscard]] Solution solve(Arc arc, double horizontal_distance_m,
                             double flat_mil, double height_delta_m);

[[nodiscard]] double full_model_elevation(Arc arc,
                                          double horizontal_distance_m,
                                          double flat_mil,
                                          double height_delta_m);

}  // namespace wardogs::sph2_drag
