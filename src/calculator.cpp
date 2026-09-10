#include "wardogs/core.hpp"

#include <cmath>
#include <numbers>

namespace wardogs {

Shot calculate_shot(Point base, Point target) {
    const double dx = target.x - base.x;
    const double dy = target.y - base.y;
    const double distance = std::hypot(dx, dy);
    double angle = std::atan2(dx, dy) * 180.0 / std::numbers::pi;
    if (angle < 0) {
        angle += 360.0;
    }
    return {base, target, dx, dy, distance, angle};
}

}  // namespace wardogs
