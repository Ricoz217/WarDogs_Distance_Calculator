#pragma once

#include <string>
#include <string_view>

namespace wardogs {

struct Point {
    double x{};
    double y{};

    bool operator==(const Point&) const = default;
};

struct Shot {
    Point base;
    Point target;
    double dx{};
    double dy{};
    double distance{};
    double angle{};
};

inline constexpr std::wstring_view default_ocr_coordinate_pattern =
    LR"(x\s*[:=]?\s*([-+]?\s*[0-9liI|Oo](?:[0-9liI|Oo\s]*[0-9liI|Oo])?\s*\.\s*[0-9liI|Oo]\s*[0-9liI|Oo](?:\s*[0-9liI|Oo])*)[\s,，;；:*&#.·]*y\s*[:=]?\s*([-+]?\s*[0-9liI|Oo](?:[0-9liI|Oo\s]*[0-9liI|Oo])?\s*\.\s*[0-9liI|Oo]\s*[0-9liI|Oo](?:\s*[0-9liI|Oo])*))";

Shot calculate_shot(Point base, Point target);
Point parse_ocr_coordinate(std::wstring_view text,
                           std::wstring_view pattern = default_ocr_coordinate_pattern);
Point parse_manual_coordinate(std::wstring_view text);
std::wstring format_point(Point point);
std::wstring format_distance_meters(double distance);
std::wstring format_bearing(double angle);
std::wstring format_raw_distance(double distance);

}  // namespace wardogs
