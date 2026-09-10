#include "wardogs/core.hpp"

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace wardogs {
namespace {

std::wstring trimmed(double value) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(4) << value;
    std::wstring result = stream.str();
    while (!result.empty() && result.back() == L'0') {
        result.pop_back();
    }
    if (!result.empty() && result.back() == L'.') {
        result.pop_back();
    }
    return result;
}

}  // namespace

std::wstring format_point(Point point) {
    return L"x" + trimmed(point.x) + L", y" + trimmed(point.y);
}

std::wstring format_distance_meters(double distance) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(0) << distance * 100.0 << L" m";
    return stream.str();
}

std::wstring format_bearing(double angle) {
    static constexpr std::array<std::wstring_view, 8> directions{
        L"N", L"NE", L"E", L"SE", L"S", L"SW", L"W", L"NW"};
    double normalized = std::fmod(angle, 360.0);
    if (normalized < 0) {
        normalized += 360.0;
    }
    double rounded = std::round(normalized * 10.0) / 10.0;
    if (rounded >= 360.0) {
        rounded = 0.0;
    }
    const auto sector = static_cast<std::size_t>(
        std::floor((rounded + 22.5) / 45.0)) % directions.size();
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(1) << rounded << L"° "
           << directions[sector];
    return stream.str();
}

std::wstring format_raw_distance(double distance) {
    std::wostringstream stream;
    stream << L"原始距离 " << std::fixed << std::setprecision(4) << distance
           << L" 单位 · 1 单位 = 100 m";
    return stream.str();
}

}  // namespace wardogs
