
#ifndef CURVE_H
#define CURVE_H


#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "geom/delaunay_types.h"
#include <algorithm>
#include <cmath>
#include "editor/viewport.h"

namespace fem {

inline double polyline_length(const std::vector<glm::dvec2>& P) {
    double L=0; for (size_t i=1;i<P.size();++i) L += glm::length(P[i]-P[i-1]); return L;
}
inline std::vector<glm::dvec2> resample_polyline_by_count(
    const std::vector<glm::dvec2>& P, int N, bool closed)
{
    if (P.empty() || N <= 0) return {};
    if (P.size() == 1) return std::vector<glm::dvec2>(N, P[0]);

    std::vector<glm::dvec2> Q = P;
    if (closed) Q.push_back(P.front());
    std::vector<double> S(Q.size(), 0.0);
    for (size_t i=1;i<Q.size();++i) S[i] = S[i-1] + glm::length(Q[i]-Q[i-1]);
    double L = S.back();
    if (L < 1e-12) return std::vector<glm::dvec2>(N, Q.front());

    std::vector<glm::dvec2> out; out.reserve(N);
    for (int k=0;k<N;++k) {
        double s = (L * k) / N;
        // find segment
        size_t j = size_t(std::lower_bound(S.begin(), S.end(), s) - S.begin());
        if (j == 0) { out.push_back(Q.front()); continue; }
        if (j >= S.size()) { out.push_back(Q.back()); continue; }
        double t = (s - S[j-1]) / std::max(1e-12, (S[j]-S[j-1]));
        out.push_back(Q[j-1] + t*(Q[j]-Q[j-1]));
    }
    return out;
}

inline double signed_area2(const std::vector<glm::dvec2>& P) {
    double a=0; for (size_t i=0;i<P.size();++i){auto& A=P[i]; auto& B=P[(i+1)%P.size()]; a += A.x*B.y - A.y*B.x;} return a;
}
inline void ensure_ccw(std::vector<glm::dvec2>& P){
    if (P.size()>=3 && signed_area2(P) < 0.0) std::reverse(P.begin(), P.end());
}

inline std::vector<Point2D> as_points_ccw(const std::vector<glm::dvec2>& P){
    std::vector<glm::dvec2> Q = P;
    ensure_ccw(Q);
    std::vector<Point2D> out; out.reserve(Q.size());
    for (size_t i=0;i<Q.size();++i){ Point2D p(Q[i].x, Q[i].y, (int)i); p.on_boundary = true; out.push_back(p); }
    return out;
}

struct CatmullRom {
    static glm::dvec2 eval(const glm::dvec2& P0,const glm::dvec2& P1,
                           const glm::dvec2& P2,const glm::dvec2& P3, double t)
    {
        double t2=t*t, t3=t2*t;
        glm::dvec2 a = 2.0*P1;
        glm::dvec2 b = (P2 - P0);
        glm::dvec2 c = 2.0*P0 - 5.0*P1 + 4.0*P2 - P3;
        glm::dvec2 d = -P0 + 3.0*P1 - 3.0*P2 + P3;
        return 0.5*(a + b*t + c*t2 + d*t3);
    }

    static std::vector<glm::dvec2> sample_closed(const std::vector<glm::dvec2>& ctrl, int per_seg=12){
        if (ctrl.size()<3) return ctrl;
        std::vector<glm::dvec2> out;
        const int n = (int)ctrl.size();
        for (int i=0;i<n;++i){
            auto& P0 = ctrl[(i-1+n)%n];
            auto& P1 = ctrl[i];
            auto& P2 = ctrl[(i+1)%n];
            auto& P3 = ctrl[(i+2)%n];
            for (int k=0;k<per_seg;++k){
                double t = double(k)/per_seg;
                out.push_back(eval(P0,P1,P2,P3,t));
            }
        }
        return out;
    }

    static std::vector<glm::dvec2> sample_open(const std::vector<glm::dvec2>& ctrl, int per_seg=12){
        if (ctrl.size()<2) return ctrl;
        std::vector<glm::dvec2> ext = ctrl;
        // pad endpoints
        ext.insert(ext.begin(), ctrl.front());
        ext.push_back(ctrl.back());
        std::vector<glm::dvec2> out; out.reserve((ctrl.size()-1)*per_seg+1);
        for (int i=0;i+3<= (int)ext.size(); ++i){
            auto P0=ext[i], P1=ext[i+1], P2=ext[i+2], P3=ext[i+3];
            for (int k=0;k<per_seg;++k){
                double t = double(k)/per_seg;
                out.push_back(eval(P0,P1,P2,P3,t));
            }
        }
        out.push_back(ext.back());
        return out;
    }
};

struct ParametricCurve {
    std::function<double(double)> fx;
    std::function<double(double)> fy;
    double t0, t1;
    bool closed = true;

    std::vector<glm::dvec2> sample_by_count(int N) const {
        const int M = std::max(4*N, 64);
        std::vector<glm::dvec2> tmp; tmp.reserve(M + (closed?1:0));
        for (int i=0;i<M;++i) {
            double t = t0 + (t1 - t0) * (double(i)/M);
            tmp.emplace_back(fx(t), fy(t));
        }
        if (closed) tmp.push_back(tmp.front());
        auto R = resample_polyline_by_count(tmp, N, closed);
        return R;
    }
};



static inline double signed_area_dvec2(const std::vector<glm::dvec2>& P) {
    if (P.size() < 3) return 0.0;
    double a = 0.0;
    for (size_t i = 0; i + 1 < P.size(); ++i) {
        a += P[i].x * P[i+1].y - P[i+1].x * P[i].y;
    }
    return a;
}

static inline std::vector<glm::dvec2> simplify_min_dist(
    const std::vector<glm::dvec2>& P, double min_dist)
{
    if (P.empty()) return {};
    const double md2 = min_dist * min_dist;

    std::vector<glm::dvec2> out;
    out.reserve(P.size());
    out.push_back(P.front());

    for (size_t i = 1; i < P.size(); ++i) {
        const glm::dvec2 d = P[i] - out.back();
        if (d.x*d.x + d.y*d.y >= md2) out.push_back(P[i]);
    }
    return out;
}

static inline std::vector<glm::dvec2> stroke_screen_to_world(
    const Viewport& vp, const std::vector<glm::dvec2>& screen_pts)
{
    std::vector<glm::dvec2> world;
    world.reserve(screen_pts.size());
    for (const auto& sp : screen_pts) {
        world.push_back(vp.to_world(ImVec2((float)sp.x, (float)sp.y)));
    }
    return world;
}


static inline std::vector<Point2D> make_closed_smooth_loop(
    std::vector<glm::dvec2> world_ctrl,
    int boundary_sample_count,
    double close_thr_world,
    double min_dist_world,
    int catmull_per_seg)
{
    if ((int)world_ctrl.size() < 3) return {};

    world_ctrl = simplify_min_dist(world_ctrl, min_dist_world);
    if ((int)world_ctrl.size() < 3) return {};

    // If you want pure-legacy behavior, you can delete this entire block.
    {
        const glm::dvec2 a = world_ctrl.front();
        const glm::dvec2 b = world_ctrl.back();
        const glm::dvec2 d = a - b;
        if (d.x*d.x + d.y*d.y > close_thr_world * close_thr_world) {
            // NOTE: we can also do nothing like in legacySmoothStrokeTool
        }
    }

    auto smooth    = CatmullRom::sample_closed(world_ctrl, catmull_per_seg);
    auto resampled = resample_polyline_by_count(smooth, boundary_sample_count, /*closed=*/true);

    if (resampled.size() < 3 || std::abs(signed_area2(resampled)) < 1e-9) return {};

    return as_points_ccw(resampled);
}


static inline glm::dvec2 catmull_rom(const glm::dvec2& p0,
                                     const glm::dvec2& p1,
                                     const glm::dvec2& p2,
                                     const glm::dvec2& p3,
                                     double t)
{
    const double t2 = t * t;
    const double t3 = t2 * t;

    return 0.5 * ((2.0 * p1) +
                  (-p0 + p2) * t +
                  (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                  (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

static inline std::vector<glm::dvec2> sample_open_catmull_rom(const std::vector<glm::dvec2>& pts,
                                                              int samples_per_seg)
{
    std::vector<glm::dvec2> out;
    const int n = (int)pts.size();
    if (n < 2) return out;
    if (n == 2) return pts;

    out.reserve((n - 1) * samples_per_seg + 1);

    for (int i = 0; i < n - 1; ++i) {
        const glm::dvec2& p0 = pts[std::max(0, i - 1)];
        const glm::dvec2& p1 = pts[i];
        const glm::dvec2& p2 = pts[i + 1];
        const glm::dvec2& p3 = pts[std::min(n - 1, i + 2)];

        for (int s = 0; s < samples_per_seg; ++s) {
            const double t = (double)s / (double)samples_per_seg;
            out.push_back(catmull_rom(p0, p1, p2, p3, t));
        }
    }
    out.push_back(pts.back());
    return out;
}

}

#endif // CURVE_H