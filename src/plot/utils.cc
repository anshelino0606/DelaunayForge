#include "utils.h"
#include <cmath>
#include <format>

namespace fem::plot {

inline std::string fmt_no_plus(double x) {
    std::string s = std::format("{:.6g}", x);
    if (auto p = s.find("e+"); p != std::string::npos) s.erase(p + 1, 1);
    return s;
}

// Compute "nice" rounded tick values a la MATLAB / matplotlib
inline double nice_num(double x, bool round_it) {
    if (x == 0.0) return 0.0;
    const double exp_v = std::floor(std::log10(std::abs(x)));
    const double frac = std::abs(x) / std::pow(10.0, exp_v);
    double nice;
    if (round_it) {
        if (frac < 1.5)       nice = 1.0;
        else if (frac < 3.0)  nice = 2.0;
        else if (frac < 7.0)  nice = 5.0;
        else                  nice = 10.0;
    } else {
        if (frac <= 1.0)      nice = 1.0;
        else if (frac <= 2.0) nice = 2.0;
        else if (frac <= 5.0) nice = 5.0;
        else                  nice = 10.0;
    }
    return std::copysign(nice * std::pow(10.0, exp_v), x);
}

inline std::vector<double> nice_ticks(double lo, double hi, int32_t target_ticks) {
    if (hi - lo < 1e-14) return { lo };
    const double range = nice_num(hi - lo, false);
    const double d = nice_num(range / std::max(1, target_ticks - 1), true);
    const double graph_min = std::floor(lo / d) * d;
    const double graph_max = std::ceil(hi / d) * d;
    std::vector<double> ticks;
    for (double v = graph_min; v <= graph_max + 0.5 * d; v += d) {
        if (v >= lo - 1e-10 * (hi - lo) && v <= hi + 1e-10 * (hi - lo))
            ticks.push_back(v);
    }
    if (ticks.empty()) ticks.push_back(0.5 * (lo + hi));
    return ticks;
}

// Format tick value: use integer when possible, otherwise compact float
inline std::string fmt_tick(double v) {
    if (std::abs(v) < 1e-12) return "0";
    if (std::abs(v) >= 1.0 && std::abs(v - std::round(v)) < 1e-9)
        return std::format("{}", (long long)std::round(v));
    // Choose precision based on magnitude
    int prec = std::max(0, (int)(2 - std::floor(std::log10(std::abs(v)))));
    prec = std::min(prec, 6);
    std::string s = std::format("{:.{}f}", v, prec);
    // Strip trailing zeros after decimal point
    if (s.find('.') != std::string::npos) {
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
    }
    return s;
}

bool glyph_5x7(char c, uint8_t rows[7]) {
    auto set = [&](uint8_t r0, uint8_t r1, uint8_t r2, uint8_t r3, uint8_t r4, uint8_t r5, uint8_t r6) {
        rows[0] = r0; rows[1] = r1; rows[2] = r2; rows[3] = r3; rows[4] = r4; rows[5] = r5; rows[6] = r6;
    };
    switch (c) {
    case '0': set(0x0E,0x11,0x13,0x15,0x19,0x11,0x0E); return true;
    case '1': set(0x04,0x0C,0x04,0x04,0x04,0x04,0x0E); return true;
    case '2': set(0x0E,0x11,0x01,0x02,0x04,0x08,0x1F); return true;
    case '3': set(0x0E,0x11,0x01,0x06,0x01,0x11,0x0E); return true;
    case '4': set(0x02,0x06,0x0A,0x12,0x1F,0x02,0x02); return true;
    case '5': set(0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E); return true;
    case '6': set(0x06,0x08,0x10,0x1E,0x11,0x11,0x0E); return true;
    case '7': set(0x1F,0x01,0x02,0x04,0x08,0x08,0x08); return true;
    case '8': set(0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E); return true;
    case '9': set(0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C); return true;
    case '.': set(0x00,0x00,0x00,0x00,0x00,0x06,0x06); return true;
    case '-': set(0x00,0x00,0x00,0x1F,0x00,0x00,0x00); return true;
    case 'e':
    case 'E': set(0x00,0x0E,0x11,0x1F,0x10,0x11,0x0E); return true;
    default:
        return false;
    }
}

ImVec2 measure_text_5x7(std::string_view text, int px_scale) {
    const int cw = 5 * px_scale;
    const int ch = 7 * px_scale;
    const int sp = 1 * px_scale;
    int w = 0;
    for (char c : text) {
        w += cw;
        w += sp;
        (void)c;
    }
    if (!text.empty()) w -= sp;
    return ImVec2((float)w, (float)ch);
}

}