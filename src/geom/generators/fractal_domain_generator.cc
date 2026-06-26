#include "fractal_domain_generator.h"
#include "geom/configs/fractal_domain_config.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>

namespace fem {

static inline glm::dvec2 rot90(const glm::dvec2& v) { return { -v.y, v.x }; }
static inline glm::dvec2 rotm90(const glm::dvec2& v) { return { v.y, -v.x }; } // right normal


static inline double norm2(const glm::dvec2& v) { return v.x*v.x + v.y*v.y; }
static inline double norm(const glm::dvec2& v)  { return std::sqrt(norm2(v)); }

static inline glm::dvec2 lerp(const glm::dvec2& a, const glm::dvec2& b, double t) {
    return a + t * (b - a);
}

static inline glm::dvec2 outward_normal_from_area(double area, const glm::dvec2& t_unit) {
    return (area >= 0.0) ? rotm90(t_unit) : rot90(t_unit);
}

static inline glm::dvec2 rot60_signed(const glm::dvec2& v, double area) {
    const double c = 0.5;
    const double s = std::sqrt(3.0) / 2.0;
    if (area >= 0.0) { // -60
        return { c*v.x + s*v.y, -s*v.x + c*v.y };
    } else { // +60
        return { c*v.x - s*v.y,  s*v.x + c*v.y };
    }
}

static inline double orient(const glm::dvec2& a, const glm::dvec2& b, const glm::dvec2& c) {
    return (b.x-a.x)*(c.y-a.y) - (b.y-a.y)*(c.x-a.x);
}

static bool seg_intersect_strict(const glm::dvec2& a, const glm::dvec2& b,
                                const glm::dvec2& c, const glm::dvec2& d,
                                double eps = 1e-12)
{
    auto o1 = orient(a,b,c);
    auto o2 = orient(a,b,d);
    auto o3 = orient(c,d,a);
    auto o4 = orient(c,d,b);

    if (std::abs(o1) < eps || std::abs(o2) < eps || std::abs(o3) < eps || std::abs(o4) < eps) {
        return false;
    }
    return (o1 * o2 < 0.0) && (o3 * o4 < 0.0);
}

static bool poly_self_intersects(const std::vector<glm::dvec2>& p) {
    const int n = (int)p.size();
    if (n < 4) return false;
    for (int i = 0; i < n; ++i) {
        glm::dvec2 a = p[i];
        glm::dvec2 b = p[(i+1)%n];
        for (int j = i+1; j < n; ++j) {
            if (j == i) continue;
            if ((j == i+1) || ((i==0) && (j==n-1))) continue;

            glm::dvec2 c = p[j];
            glm::dvec2 d = p[(j+1)%n];
            if (seg_intersect_strict(a,b,c,d)) return true;
        }
    }
    return false;
}

static void remove_consecutive_duplicates(FractalDomainGenerator::Polyline& poly, double eps2 = 1e-24) {
    if (poly.size() < 2) return;
    FractalDomainGenerator::Polyline out;
    out.reserve(poly.size());
    out.push_back(poly[0]);
    for (size_t i=1;i<poly.size();++i) {
        if (norm2(poly[i]-out.back()) > eps2) out.push_back(poly[i]);
    }
    poly.swap(out);
}

static void generate_sierpinski_carpet_holes(
    const glm::dvec2& center,
    double half_size,
    int depth,
    int max_holes,
    double min_hole_half_size,
    std::vector<std::pair<glm::dvec2, double>>& out_holes
) {
    if (depth <= 0) return;

    const double child_half = half_size / 3.0;
    if (child_half < std::max(0.0, min_hole_half_size)) return;
    const double step = 2.0 * child_half;

    for (int iy = -1; iy <= 1; ++iy) {
        for (int ix = -1; ix <= 1; ++ix) {
            glm::dvec2 c = center + glm::dvec2((double)ix * step, (double)iy * step);
            if (ix == 0 && iy == 0) {
                if (max_holes >= 0 && (int)out_holes.size() >= max_holes) {
                    return;
                }
                out_holes.emplace_back(c, child_half);
            } else {
                generate_sierpinski_carpet_holes(c, child_half, depth - 1, max_holes, min_hole_half_size, out_holes);
                if (max_holes >= 0 && (int)out_holes.size() >= max_holes) {
                    return;
                }
            }
        }
    }
}




std::optional<std::string> FractalDomainGenerator::generate(
    const FractalDomainConfig& cfg,
    std::vector<Point2D>& out_points
) {
    out_points.clear();

    if (cfg.iterations < 0) return "iterations must be >= 0";
    if (cfg.sample_count < 16) return "sample_count must be >= 16";

    Polyline poly = make_base_polygon(cfg);
    if (poly.size() < 3) return "internal: base polygon too small";

    switch (cfg.preset.value) {
        case FractalPreset::KochSnowflake:
            refine_koch_snowflake(poly, cfg.iterations);
            break;
        case FractalPreset::QuadraticKochIsland:
            refine_quadratic_koch(poly, cfg.iterations);
            break;
        case FractalPreset::MinkowskiIsland:
            refine_minkowski(poly, cfg.iterations);
            break;
        case FractalPreset::MidpointDisplacementLoop:
            midpoint_displacement_loop(poly, cfg.iterations, cfg.seed, cfg.roughness);
            break;
        default:
            return "preset not implemented";
    }

    drop_duplicate_last(poly);
    ensure_ccw(poly);

    FractalDomainGenerator::Polyline sampled = resample_closed_by_arclength(poly, cfg.sample_count);

    if (poly_self_intersects(sampled)) {
        return "boundary self-intersects (reduce iterations / roughness / size, or increase sample_count)";
    }

    out_points.reserve(sampled.size());
    for (int i = 0; i < (int)sampled.size(); ++i) {
        Point2D pt;
        pt.p[0] = sampled[i].x;
        pt.p[1] = sampled[i].y;
        pt.id = i;
        pt.on_boundary = true;
        out_points.push_back(pt);
    }

    return std::nullopt;
}

void FractalDomainGenerator::polyline_to_points(const Polyline& poly, std::vector<Point2D>& out_points) {
    out_points.clear();
    out_points.reserve(poly.size());
    for (int i = 0; i < (int)poly.size(); ++i) {
        Point2D pt;
        pt.p[0] = poly[i].x;
        pt.p[1] = poly[i].y;
        pt.id = i;
        pt.on_boundary = true;
        out_points.push_back(pt);
    }
}

FractalDomainGenerator::Polyline
FractalDomainGenerator::make_square_ccw(glm::dvec2 center, double half_size) {
    half_size = std::max(1e-9, half_size);
    Polyline poly;
    poly.reserve(4);
    poly.push_back(center + glm::dvec2(-half_size, -half_size));
    poly.push_back(center + glm::dvec2( half_size, -half_size));
    poly.push_back(center + glm::dvec2( half_size,  half_size));
    poly.push_back(center + glm::dvec2(-half_size,  half_size));
    ensure_ccw(poly);
    return poly;
}

void FractalDomainGenerator::ensure_cw(Polyline& poly) {
    if (signed_area(poly) > 0.0) std::reverse(poly.begin(), poly.end());
}

std::optional<std::string> FractalDomainGenerator::generate_boundary_loops(
    const FractalDomainConfig& cfg,
    std::vector<Point2D>& out_outer,
    std::vector<std::vector<Point2D>>& out_holes
) {
    out_outer.clear();
    out_holes.clear();

    if (cfg.preset.value != FractalPreset::SierpinskiCarpet) {
        std::vector<Point2D> boundary;
        if (auto err = generate(cfg, boundary)) return err;
        out_outer = std::move(boundary);
        return std::nullopt;
    }

    if (cfg.iterations < 0) return "iterations must be >= 0";

    const double half = std::max(1e-6, cfg.size);
    Polyline outer = make_square_ccw(cfg.center, half);

    // Densify the outer boundary to get a smoother / more uniform triangulation near the outer edge.
    // Keep holes cheap (4 corners) to avoid blowing up constraint recovery cost.
    const int outer_samples = std::max(4, cfg.sample_count);
    if (outer_samples > (int)outer.size()) {
        outer = resample_closed_by_arclength(outer, outer_samples);
        remove_consecutive_duplicates(outer);
    }

    std::vector<std::pair<glm::dvec2, double>> holes;
    holes.reserve(128);
    generate_sierpinski_carpet_holes(cfg.center, half, cfg.iterations, cfg.carpet_max_holes, cfg.carpet_min_hole_half_size, holes);

    if (cfg.carpet_max_holes >= 0 && (int)holes.size() >= cfg.carpet_max_holes) {
        return "Sierpinski carpet: reached Max Holes limit (increase Max Holes or reduce Iterations)";
    }

    polyline_to_points(outer, out_outer);

    out_holes.reserve(holes.size());
    for (const auto& h : holes) {
        Polyline hole = make_square_ccw(h.first, h.second);
        ensure_cw(hole);
        std::vector<Point2D> loop;
        polyline_to_points(hole, loop);
        out_holes.push_back(std::move(loop));
    }

    return std::nullopt;
}

FractalDomainGenerator::Polyline
FractalDomainGenerator::make_base_polygon(const FractalDomainConfig& cfg) {
    Polyline poly;

    if (cfg.preset.value == FractalPreset::KochSnowflake) {
        const double s = std::max(1e-6, cfg.snowflake_side_length);
        const double R = s / std::sqrt(3.0);
        for (int k = 0; k < 3; ++k) {
            double a = (2.0 * math::PI) * (double)k / 3.0 + math::PI / 2.0;
            poly.push_back(cfg.center + glm::dvec2(R * std::cos(a), R * std::sin(a)));
        }
        return poly;
    }

    const double s = std::max(1e-6, cfg.size);
    poly.push_back(cfg.center + glm::dvec2(-s, -s));
    poly.push_back(cfg.center + glm::dvec2( s, -s));
    poly.push_back(cfg.center + glm::dvec2( s,  s));
    poly.push_back(cfg.center + glm::dvec2(-s,  s));
    return poly;
}


void FractalDomainGenerator::drop_duplicate_last(Polyline& poly, double eps) {
    if (poly.size() < 2) return;
    if (norm2(poly.front() - poly.back()) < eps) poly.pop_back();
}

double FractalDomainGenerator::signed_area(const Polyline& poly) {
    if (poly.size() < 3) return 0.0;
    double A = 0.0;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& p = poly[i];
        const auto& q = poly[(i + 1) % poly.size()];
        A += p.x * q.y - q.x * p.y;
    }
    return 0.5 * A;
}

void FractalDomainGenerator::ensure_ccw(Polyline& poly) {
    if (signed_area(poly) < 0.0) std::reverse(poly.begin(), poly.end());
}

static inline glm::dvec2 rot60(const glm::dvec2& v) {
    const double c = 0.5;
    const double s = std::sqrt(3.0) / 2.0;
    return { c*v.x - s*v.y, s*v.x + c*v.y };
}

void FractalDomainGenerator::refine_koch_snowflake(Polyline& poly, int iterations) {
    ensure_ccw(poly);

    for (int it = 0; it < iterations; ++it) {
        Polyline next;
        next.reserve(poly.size() * 4);

        for (size_t i = 0; i < poly.size(); ++i) {
            glm::dvec2 A = poly[i];
            glm::dvec2 B = poly[(i + 1) % poly.size()];
            glm::dvec2 v = (B - A);
            
            glm::dvec2 P1 = A + v / 3.0;
            glm::dvec2 P3 = A + 2.0 * v / 3.0;
            double area = signed_area(poly);
            glm::dvec2 P2 = P1 + rot60_signed(v / 3.0, area);
            // glm::dvec2 P2 = P1 + rot60(v / 3.0);

            next.push_back(A);
            next.push_back(P1);
            next.push_back(P2);
            next.push_back(P3);
        }

        poly = std::move(next);
    }
}

void FractalDomainGenerator::refine_quadratic_koch(Polyline& poly, int iterations) {
    ensure_ccw(poly);

    for (int it = 0; it < iterations; ++it) {
        Polyline next;
        next.reserve(poly.size() * 8);

        for (size_t i = 0; i < poly.size(); ++i) {
            glm::dvec2 A = poly[i];
            glm::dvec2 B = poly[(i + 1) % poly.size()];
            glm::dvec2 v = (B - A);

            // local basis
            glm::dvec2 t = v;
            double L = norm(t);
            if (L < 1e-12) continue;
            t /= L;
            glm::dvec2 n = rot90(t);

            double h = L / 4.0;

            glm::dvec2 P0 = A;
            glm::dvec2 P1 = P0 + h * t;
            glm::dvec2 P2 = P1 + h * n;
            glm::dvec2 P3 = P2 + h * t;
            glm::dvec2 P4 = P3 - h * n;
            glm::dvec2 P5 = P4 + h * t;
            glm::dvec2 P6 = P5 + h * n;
            glm::dvec2 P7 = P6 + h * t;

            next.push_back(P0);
            next.push_back(P1);
            next.push_back(P2);
            next.push_back(P3);
            next.push_back(P4);
            next.push_back(P5);
            next.push_back(P6);
            next.push_back(P7);
        }

        poly = std::move(next);
    }
}

void FractalDomainGenerator::refine_minkowski(Polyline& poly, int iterations) {
    ensure_ccw(poly);

    for (int it = 0; it < iterations; ++it) {
        Polyline next;
        next.reserve(poly.size() * 8);

        for (size_t i = 0; i < poly.size(); ++i) {
            glm::dvec2 A = poly[i];
            glm::dvec2 B = poly[(i + 1) % poly.size()];
            glm::dvec2 v = (B - A);

            double L = norm(v);
            if (L < 1e-12) continue;

            glm::dvec2 t = v / L;
            glm::dvec2 n = rot90(t);

            double h = L / 4.0;

            glm::dvec2 P0 = A;
            glm::dvec2 P1 = P0 + h * t;
            glm::dvec2 P2 = P1 + h * n;
            glm::dvec2 P3 = P2 + h * t;
            glm::dvec2 P4 = P3 - h * n;
            glm::dvec2 P5 = P4 - h * n;
            glm::dvec2 P6 = P5 + h * t;
            glm::dvec2 P7 = P6 + h * n;

            next.push_back(P0);
            next.push_back(P1);
            next.push_back(P2);
            next.push_back(P3);
            next.push_back(P4);
            next.push_back(P5);
            next.push_back(P6);
            next.push_back(P7);
        }

        poly = std::move(next);
    }
}

void FractalDomainGenerator::midpoint_displacement_loop(
    Polyline& poly,
    int iterations,
    uint32_t seed,
    double roughness
) {
    roughness = std::clamp(roughness, 0.0, 1.0);
    ensure_ccw(poly);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> U(-1.0, 1.0);

    for (int it = 0; it < iterations; ++it) {
        Polyline next;
        next.reserve(poly.size() * 2);

        double A0 = 0.05;
        double amp = A0 * std::pow(roughness, (double)it);

        for (size_t i = 0; i < poly.size(); ++i) {
            glm::dvec2 A = poly[i];
            glm::dvec2 B = poly[(i + 1) % poly.size()];

            glm::dvec2 v = B - A;
            double L = norm(v);
            if (L < 1e-12) continue;

            glm::dvec2 t = v / L;
            glm::dvec2 n = rot90(t);

            glm::dvec2 M = 0.5 * (A + B);
            glm::dvec2 M2 = M + (amp * L) * U(rng) * n;

            next.push_back(A);
            next.push_back(M2);
        }

        poly = std::move(next);
    }
}

FractalDomainGenerator::Polyline
FractalDomainGenerator::resample_closed_by_arclength(const Polyline& poly, int sample_count) {
    Polyline out;
    if (poly.size() < 2 || sample_count < 2) return out;

    std::vector<double> s(poly.size() + 1, 0.0);
    for (size_t i = 0; i < poly.size(); ++i) {
        glm::dvec2 A = poly[i];
        glm::dvec2 B = poly[(i + 1) % poly.size()];
        s[i + 1] = s[i] + norm(B - A);
    }
    const double L = s.back();
    if (L < 1e-12) return out;

    out.reserve(sample_count);

    for (int k = 0; k < sample_count; ++k) {
        double target = (L * (double)k) / (double)sample_count;

        auto it = std::upper_bound(s.begin(), s.end(), target);
        size_t j = (it == s.begin()) ? 0 : (size_t)(it - s.begin() - 1);
        j = std::min(j, poly.size() - 1);

        double seg_s0 = s[j];
        double seg_s1 = s[j + 1];
        double t = (seg_s1 > seg_s0) ? (target - seg_s0) / (seg_s1 - seg_s0) : 0.0;

        glm::dvec2 A = poly[j];
        glm::dvec2 B = poly[(j + 1) % poly.size()];
        out.push_back(lerp(A, B, t));
    }

    return out;
}

} // namespace fem
