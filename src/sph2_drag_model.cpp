#include "sph2_drag_model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

// Ported and adapted from WARDOGS Artillery Calculator's
// js/workers/terrain-height-solver.js at commit
// f43183c0747afa3dc33ec9af93a153a53766bf7f.
// https://github.com/apollyon-sys/wardogs-calculator
// Copyright (c) 2026 Apollyon, MIT License.
//
// These are fitted effective trajectory families, not native game constants.
// The flat firing table remains authoritative; the model supplies only the
// height-dependent command delta and an effective elevation for calibration.

namespace wardogs::sph2_drag {
namespace {

struct Model {
    double speed;
    double gravity;
    double drag;
    double offset;
};

constexpr std::array high_models{
    Model{9389.43881652221, 147.2063947858103, 0.0008516971618643472,
          -960.0413878819479},
    Model{9849.20362952773, 150.86543234319186, 0.0008557959979161821,
          -985.1206130285715},
    Model{9538.83646613938, 148.641633460151, 0.0008527226093807684,
          -968.4137716340553},
    Model{9382.501472445045, 148.5944458354111, 0.0008496679095904272,
          -960.0394378351399},
    Model{9357.938150456344, 150.47135225076096, 0.0008467547170072617,
          -958.5760116042985},
    Model{9434.587865435868, 147.3517620197302, 0.0008527036302749736,
          -959.7586038157854},
};

constexpr std::array low_models{
    Model{300, 8.85397505384648, 0.0007351626163587786,
          95.03019643400405},
    Model{300, 8.855984443192355, 0.0007350661856769885,
          95.09033125524101},
    Model{300, 8.85460617869732, 0.0007351378032591812,
          95.04909745846172},
    Model{300, 8.85634575638418, 0.000735050816746845,
          95.07216670372487},
    Model{300, 8.852773782663528, 0.0007351990161210038,
          94.99348250194096},
    Model{300, 8.85228338555638, 0.0007352570328084649,
          94.97988436225965},
    Model{300, 9.774639751933924, 0.0006836064277465947,
          107.38706982639987},
    Model{300, 9.80665, 0.0006827669131158775, 108.27014958304952},
    Model{300, 9.80665, 0.0006836064277465947, 108.7138149309282},
};

constexpr std::array<double, 21> drag_speed{
    0,   30,  61,  91,  122, 150, 183, 213, 244, 274, 300,
    335, 366, 396, 427, 450, 488, 518, 549, 579, 600};
constexpr std::array<double, 21> drag_coefficient_table{
    .71, .69, .66, .64, .65, .66, .68, .72, .80, .92, 1.10,
    1.25, 1.33, 1.39, 1.44, 1.48, 1.50, 1.54, 1.57, 1.58, 1.60};

double drag_coefficient(double speed) {
    if (speed <= drag_speed.front()) return drag_coefficient_table.front();
    if (speed >= drag_speed.back()) return drag_coefficient_table.back();
    const auto right = std::upper_bound(drag_speed.begin(), drag_speed.end(),
                                        speed);
    const std::size_t high =
        static_cast<std::size_t>(right - drag_speed.begin());
    const std::size_t low = high - 1;
    const double fraction =
        (speed - drag_speed[low]) / (drag_speed[high] - drag_speed[low]);
    return drag_coefficient_table[low] +
        fraction * (drag_coefficient_table[high] -
                    drag_coefficient_table[low]);
}

double drag_factor(double vx, double vz, const Model& model) {
    const double speed = std::hypot(vx, vz);
    return model.drag * drag_coefficient(speed) * speed;
}

std::optional<double> height_at_distance(double angle, double distance,
                                         const Model& model,
                                         double time_step = 0.02) {
    if (!std::isfinite(angle) || angle <= 0.0 ||
        angle >= std::numbers::pi / 2.0 || !std::isfinite(distance) ||
        distance <= 0.0 || !std::isfinite(time_step) || time_step <= 0.0 ||
        time_step > 0.02)
        return std::nullopt;

    double x = 0.0;
    double z = 0.0;
    double vx = model.speed * std::cos(angle);
    double vz = model.speed * std::sin(angle);
    const int steps = static_cast<int>(std::ceil(120.0 / time_step));
    for (int index = 0; index < steps; ++index) {
        const double q1 = drag_factor(vx, vz, model);
        const double a1x = -q1 * vx;
        const double a1z = -model.gravity - q1 * vz;
        const double v2x = vx + a1x * time_step / 2.0;
        const double v2z = vz + a1z * time_step / 2.0;
        const double q2 = drag_factor(v2x, v2z, model);
        const double a2x = -q2 * v2x;
        const double a2z = -model.gravity - q2 * v2z;
        const double v3x = vx + a2x * time_step / 2.0;
        const double v3z = vz + a2z * time_step / 2.0;
        const double q3 = drag_factor(v3x, v3z, model);
        const double a3x = -q3 * v3x;
        const double a3z = -model.gravity - q3 * v3z;
        const double v4x = vx + a3x * time_step;
        const double v4z = vz + a3z * time_step;
        const double q4 = drag_factor(v4x, v4z, model);
        const double next_x =
            x + time_step * (vx + 2.0 * v2x + 2.0 * v3x + v4x) / 6.0;
        const double next_z =
            z + time_step * (vz + 2.0 * v2z + 2.0 * v3z + v4z) / 6.0;
        if (!std::isfinite(next_x) || !std::isfinite(next_z) ||
            !std::isfinite(v4x) || !std::isfinite(v4z))
            return std::nullopt;
        if (next_x >= distance) {
            return z + (next_z - z) * (distance - x) / (next_x - x) +
                model.offset;
        }
        vx += time_step *
            (a1x + 2.0 * a2x + 2.0 * a3x - q4 * v4x) / 6.0;
        vz += time_step *
            (a1z + 2.0 * a2z + 2.0 * a3z - model.gravity - q4 * v4z) /
            6.0;
        x = next_x;
        z = next_z;
        if (vx <= 1e-7) return std::nullopt;
    }
    return std::nullopt;
}

using ScalarFunction = std::function<std::optional<double>(double)>;

std::optional<double> bisect(const ScalarFunction& function, double left,
                             double right, double tolerance) {
    auto left_value = function(left);
    auto right_value = function(right);
    if (!left_value || !right_value || *left_value * *right_value > 0.0)
        return std::nullopt;
    if (std::abs(*left_value) < 1e-8) return left;
    if (std::abs(*right_value) < 1e-8) return right;
    for (int index = 0; index < 40 && right - left > tolerance; ++index) {
        const double middle = (left + right) / 2.0;
        const auto middle_value = function(middle);
        if (!middle_value) return std::nullopt;
        if (std::abs(*middle_value) < 1e-8) return middle;
        if (*left_value * *middle_value <= 0.0) {
            right = middle;
            right_value = middle_value;
        } else {
            left = middle;
            left_value = middle_value;
        }
    }
    return (left + right) / 2.0;
}

bool on_arc(Arc arc, const ScalarFunction& function, double root,
            double step) {
    const auto center = function(root);
    const auto left = function(root - step);
    const auto right = function(root + step);
    if (!center) return false;
    std::optional<double> slope;
    if (left && right)
        slope = *right - *left;
    else if (left)
        slope = *center - *left;
    else if (right)
        slope = *right - *center;
    return slope && std::isfinite(*slope) &&
        (arc == Arc::low ? *slope > 0.0 : *slope < 0.0);
}

std::optional<double> reference_angle(Arc arc, double distance,
                                      double flat_mil,
                                      const Model& model) {
    const double center = flat_mil / 1000.0;
    const double minimum = arc == Arc::low ? 0.0001 : 0.3;
    const double maximum = arc == Arc::low ? 0.61 : 1.55;
    const ScalarFunction function = [&](double angle) {
        return height_at_distance(angle, distance, model);
    };
    const auto center_value = function(center);
    if (center_value && std::abs(*center_value) < 1e-8 &&
        on_arc(arc, function, center, 1e-5))
        return center;

    constexpr std::array spans{0.002, 0.005, 0.01, 0.02, 0.04,
                               0.08,  0.12,  0.2,  0.3};
    for (const double span : spans) {
        const double left = std::max(minimum, center - span);
        const double right = std::min(maximum, center + span);
        std::vector<double> candidates;
        if (center_value) {
            for (const double edge : {left, right}) {
                const auto value = function(edge);
                if (value && *value * *center_value <= 0.0) {
                    const auto root = bisect(function, std::min(edge, center),
                                             std::max(edge, center), 1e-7);
                    if (root && on_arc(arc, function, *root, 1e-5))
                        candidates.push_back(*root);
                }
            }
        }
        if (!candidates.empty()) {
            return *std::min_element(
                candidates.begin(), candidates.end(), [&](double first,
                                                           double second) {
                    return std::abs(first - center) <
                        std::abs(second - center);
                });
        }
    }

    std::optional<std::pair<double, double>> previous;
    std::optional<double> best;
    for (int index = 0; index <= 100; ++index) {
        const double angle =
            minimum + (maximum - minimum) * index / 100.0;
        const auto value = function(angle);
        if (value && previous && previous->second * *value <= 0.0) {
            const auto root =
                bisect(function, previous->first, angle, 1e-7);
            if (root && on_arc(arc, function, *root, 1e-5) &&
                (!best || std::abs(*root - center) <
                         std::abs(*best - center)))
                best = root;
        }
        if (value)
            previous = std::pair{angle, *value};
        else
            previous.reset();
    }
    return best;
}

std::optional<double> command_for_height(Arc arc, double distance,
                                         double flat_mil,
                                         double height_delta,
                                         const Model& model,
                                         double reference) {
    if (height_delta == 0.0) return flat_mil;
    const double minimum = arc == Arc::low ? 20.0 : 610.0;
    const double maximum = arc == Arc::low ? 600.0 : 1390.0;
    const ScalarFunction function = [&](double command) {
        const auto height = height_at_distance(
            reference + (command - flat_mil) / 1000.0, distance, model);
        if (!height) return std::optional<double>{};
        return std::optional<double>{*height - height_delta};
    };
    std::vector<double> samples{minimum, maximum, flat_mil};
    for (double command = minimum; command < maximum; command += 10.0)
        samples.push_back(command);
    std::sort(samples.begin(), samples.end());
    samples.erase(std::unique(samples.begin(), samples.end()), samples.end());
    std::optional<std::pair<double, double>> previous;
    std::optional<double> best;
    for (const double command : samples) {
        const auto value = function(command);
        if (value && std::abs(*value) < 1e-8 &&
            on_arc(arc, function, command, 0.05))
            return command;
        if (value && previous && previous->second * *value <= 0.0) {
            const auto root =
                bisect(function, previous->first, command, 0.025);
            if (root && on_arc(arc, function, *root, 0.05) &&
                (!best || std::abs(*root - flat_mil) <
                         std::abs(*best - flat_mil)))
                best = root;
        }
        if (value)
            previous = std::pair{command, *value};
        else
            previous.reset();
    }
    return best;
}

std::span<const Model> models_for(Arc arc) {
    return arc == Arc::low ? std::span<const Model>{low_models}
                           : std::span<const Model>{high_models};
}

struct ReferenceCacheEntry {
    Arc arc;
    double distance;
    double flat_mil;
    std::vector<double> references;
};

const std::vector<double>& cached_references(Arc arc, double distance,
                                             double flat_mil) {
    thread_local std::vector<ReferenceCacheEntry> cache;
    const auto found = std::find_if(
        cache.begin(), cache.end(), [&](const ReferenceCacheEntry& entry) {
            return entry.arc == arc && entry.distance == distance &&
                entry.flat_mil == flat_mil;
        });
    if (found != cache.end()) return found->references;

    ReferenceCacheEntry entry{arc, distance, flat_mil, {}};
    const auto models = models_for(arc);
    entry.references.reserve(models.size());
    for (const auto& model : models) {
        const auto reference = reference_angle(arc, distance, flat_mil, model);
        entry.references.push_back(
            reference.value_or(std::numeric_limits<double>::quiet_NaN()));
    }
    if (cache.size() >= 32) cache.erase(cache.begin());
    cache.push_back(std::move(entry));
    return cache.back().references;
}

struct SolutionCacheEntry {
    Arc arc;
    double distance;
    double flat_mil;
    double height_delta;
    Solution solution;
};

}  // namespace

Solution solve(Arc arc, double horizontal_distance_m, double flat_mil,
               double height_delta_m) {
    if (!std::isfinite(horizontal_distance_m) ||
        horizontal_distance_m < 780.0 ||
        horizontal_distance_m > sph2_maximum_range_m ||
        !std::isfinite(flat_mil) ||
        flat_mil < (arc == Arc::low ? 20.0 : 610.0) ||
        flat_mil > (arc == Arc::low ? 600.0 : 1390.0) ||
        !std::isfinite(height_delta_m))
        throw std::invalid_argument("弹道模型输入超出支持范围");

    thread_local std::vector<SolutionCacheEntry> cache;
    const auto cached = std::find_if(
        cache.begin(), cache.end(), [&](const SolutionCacheEntry& entry) {
            return entry.arc == arc && entry.distance == horizontal_distance_m &&
                entry.flat_mil == flat_mil &&
                entry.height_delta == height_delta_m;
        });
    if (cached != cache.end()) return cached->solution;

    std::vector<double> commands;
    std::optional<double> full_elevation;
    const auto models = models_for(arc);
    const auto& references =
        cached_references(arc, horizontal_distance_m, flat_mil);
    commands.reserve(models.size());
    for (std::size_t index = 0; index < models.size(); ++index) {
        const double reference = references[index];
        if (!std::isfinite(reference))
            throw std::invalid_argument("弹道模型无法建立平地参考解");
        const auto command = command_for_height(
            arc, horizontal_distance_m, flat_mil, height_delta_m,
            models[index], reference);
        if (!command)
            throw std::invalid_argument("目标超出拟合弹道包线");
        commands.push_back(*command);
        if (index == 0)
            full_elevation =
                reference + (*command - flat_mil) / 1000.0;
    }
    std::sort(commands.begin(), commands.end());
    const std::size_t middle = commands.size() / 2;
    const double command = commands.size() % 2 != 0
        ? commands[middle]
        : (commands[middle - 1] + commands[middle]) / 2.0;
    const Solution result{
        command, *full_elevation, commands.front(), commands.back()};
    if (cache.size() >= 64) cache.erase(cache.begin());
    cache.push_back({arc, horizontal_distance_m, flat_mil, height_delta_m,
                     result});
    return result;
}

double full_model_elevation(Arc arc, double horizontal_distance_m,
                            double flat_mil, double height_delta_m) {
    const Model& model = models_for(arc).front();
    const double reference =
        cached_references(arc, horizontal_distance_m, flat_mil).front();
    if (!std::isfinite(reference))
        throw std::invalid_argument("弹道模型无法建立平地参考解");
    if (std::abs(height_delta_m) < 1e-12) return reference;

    const double minimum = arc == Arc::low ? 0.0001 : 0.3;
    const double maximum = arc == Arc::low ? 0.61 : 1.55;
    const ScalarFunction function = [&](double angle) {
        const auto height = height_at_distance(angle, horizontal_distance_m,
                                               model);
        if (!height) return std::optional<double>{};
        return std::optional<double>{*height - height_delta_m};
    };
    std::optional<std::pair<double, double>> previous;
    std::optional<double> best;
    for (int index = 0; index <= 160; ++index) {
        const double angle =
            minimum + (maximum - minimum) * index / 160.0;
        const auto value = function(angle);
        if (value && previous && previous->second * *value <= 0.0) {
            const auto root =
                bisect(function, previous->first, angle, 1e-7);
            if (root && on_arc(arc, function, *root, 1e-5) &&
                (!best || std::abs(*root - reference) <
                         std::abs(*best - reference)))
                best = root;
        }
        if (value)
            previous = std::pair{angle, *value};
        else
            previous.reset();
    }
    if (!best) throw std::invalid_argument("目标超出拟合弹道包线");
    return *best;
}

}  // namespace wardogs::sph2_drag
