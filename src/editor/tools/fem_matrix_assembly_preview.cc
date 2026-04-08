#include "fem_matrix_assembly_preview.h"
#include "math/fem/fem_problem.h"
#include "math/fem/fem_mesh.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace fem {
namespace {

template <class Node>
inline double node_x(const Node& n) noexcept {
    if constexpr (requires { n.x(); }) return static_cast<double>(n.x());
    else if constexpr (requires { n.x; }) return static_cast<double>(n.x);
    else if constexpr (requires { n.p.x; }) return static_cast<double>(n.p.x);
    else if constexpr (requires { n.p.x(); }) return static_cast<double>(n.p.x());
    else return 0.0;
}

template <class Node>
inline double node_y(const Node& n) noexcept {
    if constexpr (requires { n.y(); }) return static_cast<double>(n.y());
    else if constexpr (requires { n.y; }) return static_cast<double>(n.y);
    else if constexpr (requires { n.p.y; }) return static_cast<double>(n.p.y);
    else if constexpr (requires { n.p.y(); }) return static_cast<double>(n.p.y());
    else return 0.0;
}

template <class Elem>
inline std::int32_t elem_v(const Elem& e, int k) noexcept {
    if constexpr (requires { e.v[k]; }) return static_cast<std::int32_t>(e.v[k]);
    else if constexpr (requires { e.verts[k]; }) return static_cast<std::int32_t>(e.verts[k]);
    else if constexpr (requires { e.idx[k]; }) return static_cast<std::int32_t>(e.idx[k]);
    else return -1;
}

template <class Mesh>
inline const auto& mesh_nodes(const Mesh& m) {
    if constexpr (requires { m.nodes; }) return m.nodes;
    else if constexpr (requires { m.points; }) return m.points;
    else {
        static_assert(sizeof(Mesh) == 0, "FEMMesh: expected .nodes or .points");
    }
}

template <class Mesh>
inline const auto& mesh_elems(const Mesh& m) {
    if constexpr (requires { m.elems; }) return m.elems;
    else if constexpr (requires { m.elements; }) return m.elements;
    else if constexpr (requires { m.triangles; }) return m.triangles;
    else {
        static_assert(sizeof(Mesh) == 0, "FEMMesh: expected .elems/.elements/.triangles");
    }
}

inline double tri_area(double x1, double y1,
                       double x2, double y2,
                       double x3, double y3) noexcept
{
    return 0.5 * std::abs((x2 - x1) * (y3 - y1) - (x3 - x1) * (y2 - y1));
}

inline void p1_bc(double x1, double y1,
                  double x2, double y2,
                  double x3, double y3,
                  double b[3], double c[3]) noexcept
{
    b[0] = y2 - y3; c[0] = x3 - x2;
    b[1] = y3 - y1; c[1] = x1 - x3;
    b[2] = y1 - y2; c[2] = x2 - x1;
}

inline int clamp_int(int v, int lo, int hi) noexcept {
    return (v < lo) ? lo : (v > hi ? hi : v);
}

} // namespace

template <std::floating_point Real>
AssemblyResult<Real> FEMMatrixAssembler<Real>::assemble(const FEMProblem& problem) {
    AssemblyResult<Real> out{};

    const FEMMesh* mesh = problem.mesh;
    if (!mesh) return out;

    const auto& nodes = mesh_nodes(*mesh);
    const auto& elems = mesh_elems(*mesh);

    const std::size_t n = nodes.size();
    const std::size_t m = elems.size();

    out.stats.total_dofs     = n;
    out.stats.total_elements = m;

    out.K.rows = static_cast<Index>(n);
    out.K.cols = static_cast<Index>(n);
    out.rhs.assign(n, Real(0));

    out.K.reserve(m * 9);

    const auto t0 = std::chrono::high_resolution_clock::now();

    for (std::size_t e = 0; e < m; ++e) {
        assemble_element(problem, e, out.K, out.rhs);
    }

    apply_boundary_conditions(problem, out.K, out.rhs);
    compute_basic_statistics(out.K, out.stats);

    const auto t1 = std::chrono::high_resolution_clock::now();
    out.stats.assembly_time_ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    return out;
}

template <std::floating_point Real>
void FEMMatrixAssembler<Real>::assemble_element(const FEMProblem& problem,
                                               std::size_t elem_idx,
                                               Matrix& K,
                                               Vector& rhs)
{
    const FEMMesh* mesh = problem.mesh;
    if (!mesh) return;

    const auto& nodes = mesh_nodes(*mesh);
    const auto& elems = mesh_elems(*mesh);
    if (elem_idx >= elems.size()) return;

    const auto& el = elems[elem_idx];

    const Index i0 = elem_v(el, 0);
    const Index i1 = elem_v(el, 1);
    const Index i2 = elem_v(el, 2);

    if (i0 < 0 || i1 < 0 || i2 < 0) return;
    if (static_cast<std::size_t>(i0) >= nodes.size()) return;
    if (static_cast<std::size_t>(i1) >= nodes.size()) return;
    if (static_cast<std::size_t>(i2) >= nodes.size()) return;

    const auto& n0 = nodes[static_cast<std::size_t>(i0)];
    const auto& n1 = nodes[static_cast<std::size_t>(i1)];
    const auto& n2 = nodes[static_cast<std::size_t>(i2)];

    const double x0 = node_x(n0), y0 = node_y(n0);
    const double x1 = node_x(n1), y1 = node_y(n1);
    const double x2 = node_x(n2), y2 = node_y(n2);

    const double A = tri_area(x0, y0, x1, y1, x2, y2);
    if (A <= 1e-30) return;

    const double cx = (x0 + x1 + x2) / 3.0;
    const double cy = (y0 + y1 + y2) / 3.0;

    const double a = static_cast<double>(problem.a(cx, cy));
    const double c = static_cast<double>(problem.c(cx, cy));
    const double f = static_cast<double>(problem.f(cx, cy));

    double b[3], cc[3];
    p1_bc(x0, y0, x1, y1, x2, y2, b, cc);

    // Local stiffness: a * (b_i b_j + c_i c_j) / (4A)
    // Local mass: c * A/12 * [2 1 1; 1 2 1; 1 1 2]
    const double inv4A = 1.0 / (4.0 * A);
    const double mfac  = (c * A) / 12.0;

    const Index gi[3] = { i0, i1, i2 };

    for (int r = 0; r < 3; ++r) {
        for (int s = 0; s < 3; ++s) {
            const double kij = a * (b[r] * b[s] + cc[r] * cc[s]) * inv4A;
            const double mij = mfac * ((r == s) ? 2.0 : 1.0);
            K.add_entry(gi[r], gi[s], static_cast<Real>(kij + mij));
        }
    }

    // Load vector with centroid quadrature: ∫ f φ_i ≈ f(c) * A/3
    const double lf = f * A / 3.0;
    rhs[static_cast<std::size_t>(i0)] += static_cast<Real>(lf);
    rhs[static_cast<std::size_t>(i1)] += static_cast<Real>(lf);
    rhs[static_cast<std::size_t>(i2)] += static_cast<Real>(lf);
}

template <std::floating_point Real>
void FEMMatrixAssembler<Real>::apply_boundary_conditions(const FEMProblem&,
                                                        Matrix&,
                                                        Vector&)
{
    // If later add BCs to FEMProblem this is the hook.
}

template <std::floating_point Real>
void FEMMatrixAssembler<Real>::compute_basic_statistics(const Matrix& K,
                                                       AssemblyStatistics& stats) noexcept
{
    stats.matrix_nnz = K.nnz();
    stats.symmetric_hint = (K.rows == K.cols);

    std::size_t bw = 0;
    const std::size_t nnz = K.nnz();
    for (std::size_t k = 0; k < nnz; ++k) {
        const auto r = static_cast<std::int64_t>(K.row[k]);
        const auto c = static_cast<std::int64_t>(K.col[k]);
        const auto d = (r >= c) ? (r - c) : (c - r);
        if (static_cast<std::size_t>(d) > bw) bw = static_cast<std::size_t>(d);
    }
    stats.bandwidth = bw;
}

template <std::floating_point Real>
MatrixPreviewData FEMMatrixAssembler<Real>::make_preview(const Matrix& K,
                                                        const AssemblyStatistics& stats,
                                                        PreviewConfig cfg)
{
    return downsample_occupancy(K, stats, cfg);
}

template <std::floating_point Real>
MatrixPreviewData FEMMatrixAssembler<Real>::downsample_occupancy(const Matrix& K,
                                                                const AssemblyStatistics& stats,
                                                                PreviewConfig cfg)
{
    MatrixPreviewData P;
    P.stats = stats;

    if (!K.consistent() || K.rows <= 0 || K.cols <= 0) return P;

    const int maxs = clamp_int(cfg.max_size, 16, 1024);
    P.w = maxs;
    P.h = maxs;
    P.occ.assign(static_cast<std::size_t>(P.w * P.h), 0);

    P.has_values = cfg.include_values;
    if (P.has_values) {
        P.values.assign(static_cast<std::size_t>(P.w * P.h), 0.0f);
        P.min_value = std::numeric_limits<float>::infinity();
        P.max_value = 0.0f;
    }

    const std::int64_t R = K.rows;
    const std::int64_t C = K.cols;

    auto cell = [&](std::int64_t r, std::int64_t c) noexcept -> std::size_t {
        const int y = static_cast<int>((r * P.h) / R);
        const int x = static_cast<int>((c * P.w) / C);
        const int yy = clamp_int(y, 0, P.h - 1);
        const int xx = clamp_int(x, 0, P.w - 1);
        return static_cast<std::size_t>(yy * P.w + xx);
    };

    const std::size_t nnz = K.nnz();
    for (std::size_t k = 0; k < nnz; ++k) {
        const std::int64_t r = K.row[k];
        const std::int64_t c = K.col[k];
        if (r < 0 || c < 0 || r >= R || c >= C) continue;

        const std::size_t idx = cell(r, c);
        P.occ[idx] = 1;

        if (P.has_values) {
            const float av = static_cast<float>(std::abs(K.val[k]));
            if (av > P.values[idx]) P.values[idx] = av;
        }
    }

    if (P.has_values) {
        float mx = 0.0f;
        for (float v : P.values) if (v > mx) mx = v;
        P.max_value = mx;
        P.min_value = 0.0f;

        if (mx > 0.0f) {
            const float inv = 1.0f / mx;
            for (auto& v : P.values) v *= inv;
        }
    }

    return P;
}

template class FEMMatrixAssembler<double>;

} // namespace fem
