#pragma once

#include <compare>

namespace geom {

struct Point2D {
    double x = 0.0;
    double y = 0.0;

    auto operator<=>(const Point2D&) const = default;
};

struct Vec2D {
    double x = 0.0;
    double y = 0.0;

    auto operator<=>(const Vec2D&) const = default;
};

}  // namespace geom