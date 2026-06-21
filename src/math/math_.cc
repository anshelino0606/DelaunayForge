#include "math_.h"
#include <cmath>
#include <algorithm>

namespace fem {

double Math::safe_acos(double x) {
    return std::acos(std::clamp(x, -1.0, 1.0));
}

bool Math::equals(double x, double y, double epsilon) {
    return std::abs(x - y) < epsilon;
}

}