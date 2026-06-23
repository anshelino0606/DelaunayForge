#pragma once

#include <limits>

namespace fem {

constexpr double DOUBLE_MIN = std::numeric_limits<double>::lowest();
constexpr double DOUBLE_MAX = std::numeric_limits<double>::max();
constexpr double DOUBLE_NAN = std::numeric_limits<double>::quiet_NaN();
constexpr double DOUBLE_INF = std::numeric_limits<double>::infinity();

class Math {
public:
    // Clamps x value in range [-1; 1] before acos
    static double safe_acos(double x);
    static bool equals(double x, double y, double epsilon = 1e-12);
};

}