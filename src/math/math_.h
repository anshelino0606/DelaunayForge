#pragma once

#include <limits>
#include <numbers>
#include "math/types.h"

namespace fem::math {

constexpr double DMIN = std::numeric_limits<double>::lowest();
constexpr double DMAX = std::numeric_limits<double>::max();
constexpr double DNAN = std::numeric_limits<double>::quiet_NaN();
constexpr double DINF = std::numeric_limits<double>::infinity();

constexpr double PI = std::numbers::pi;
constexpr float F_PI = std::numbers::pi_v<float>;
constexpr double E = std::numbers::e;
constexpr double TWO_PI = 2.0 * PI;

// Clamps x value in range [-1; 1] before acos
double safe_acos(double x);
bool equals(double x, double y, double epsilon = 1e-12);

int32_t sign_eps(double value, double epsilon = 1e-9);

}