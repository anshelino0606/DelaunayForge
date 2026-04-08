#ifndef GUI_BC_UTILS_H
#define GUI_BC_UTILS_H

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <vector>
#include <algorithm>

#include "geom/delaunay2d.h"

namespace fem::gui::bc {

/// Build adjacency (vertex -> [(neighbor_vertex, edge_id)]) for boundary edges only.
inline std::unordered_map<int, std::vector<std::pair<int,int>>>
build_boundary_graph(const DelaunayTriangulationResult& R) {
    std::unordered_map<int, std::vector<std::pair<int,int>>> G;
    G.reserve(R.points.size());
    for (int eid = 0; eid < static_cast<int>(R.edges.size()); ++eid) {
        const auto& e = R.edges[eid];
        if (!e.on_boundary) continue;
        if ((size_t)e.a >= R.points.size() || (size_t)e.b >= R.points.size()) continue;
        G[e.a].push_back({e.b, eid});
        G[e.b].push_back({e.a, eid});
    }
    return G;
}

static bool is_boundary_vertex(const DelaunayTriangulationResult& R, int v)
{
    if (v < 0 || (size_t)v >= R.points.size()) return false;
    // Fast path: honor explicit flag if it exists
    if (R.points[v].on_boundary) return true;
    // Robust path: derive from incident edges
    for (const auto& e : R.edges) {
        if (!e.on_boundary) continue;
        if (e.a == v || e.b == v) return true;
    }
    return false;
}

/// Find any path along boundary from v0 to v1 returning edge ids (BFS on boundary graph).
inline std::vector<int>
find_boundary_path_edge_ids(const DelaunayTriangulationResult& R, int v0, int v1,
                            const std::unordered_set<int>* forbid = nullptr) {
    auto G = build_boundary_graph(R);
    if (!G.count(v0) || !G.count(v1)) return {};

    std::unordered_map<int, std::pair<int,int>> parent; // v -> (prev_v, via_eid)
    std::queue<int> q;
    q.push(v0);
    parent[v0] = {-1, -1};

    while (!q.empty()) {
        int v = q.front(); q.pop();
        if (v == v1) break;
        for (auto [nbr, eid] : G[v]) {
            if (forbid && forbid->count(eid)) continue;
            if (!parent.count(nbr)) {
                parent[nbr] = {v, eid};
                q.push(nbr);
            }
        }
    }
    if (!parent.count(v1)) return {}; // no path (different loops?)

    std::vector<int> eids;
    for (int cur=v1; cur!=v0; ) {
        auto it = parent.find(cur);
        if (it==parent.end()) break;
        int prev = it->second.first;
        int eid  = it->second.second;
        if (eid >= 0) eids.push_back(eid);
        cur = prev;
    }
    std::reverse(eids.begin(), eids.end());
    return eids;
}

/// Extract the boundary loop starting at v_start (assumes a simple loop of degree 2 for each vertex on the loop).
inline bool extract_boundary_loop(
    const DelaunayTriangulationResult& R,
    int v_start,
    std::vector<int>& loop_vs,
    std::vector<int>& loop_eids) {
    auto G = build_boundary_graph(R);
    if (!G.count(v_start) || G[v_start].size() < 1) return false;

    loop_vs.clear(); loop_eids.clear();
    int cur = v_start;
    int prev = -1;
    const int guard_max = static_cast<int>(R.edges.size()) + 5;
    int guard = 0;

    do {
        if (++guard > guard_max) return false;
        loop_vs.push_back(cur);
        const auto& nbrs = G[cur];
        int next = -1, via_eid = -1;
        if (nbrs.size() == 1) {
            return false; // open end => not a closed loop
        } else {
            auto [n0,e0] = nbrs[0];
            auto [n1,e1] = nbrs[1];
            if (prev == -1) { next = n0; via_eid = e0; }
            else if (n0 != prev) { next = n0; via_eid = e0; }
            else { next = n1; via_eid = e1; }
        }
        loop_eids.push_back(via_eid);
        prev = cur;
        cur = next;
    } while (cur != v_start);

    return true; // closed
}

inline std::vector<int> arc_between_on_loop(
    const std::vector<int>& loop_vs,
    const std::vector<int>& loop_eids,
    int v0, int v1,
    bool forward) {
    const int n = static_cast<int>(loop_vs.size());
    if (n == 0) return {};
    auto pos = [&](int v){ for (int i=0;i<n;++i) if (loop_vs[i]==v) return i; return -1; };
    int i0 = pos(v0), i1 = pos(v1);
    if (i0 < 0 || i1 < 0) return {};

    std::vector<int> arc;
    if (forward) {
        for (int i=i0; i != i1; i = (i+1)%n) arc.push_back(loop_eids[i]);
    } else {
        for (int i=i0; i != i1; i = (i-1+n)%n) {
            int prev = (i-1+n)%n;
            arc.push_back(loop_eids[prev]);
        }
    }
    return arc;
}

inline double polygon_area_sign(
    const DelaunayTriangulationResult& R,
    const std::vector<int>& loop_vs) {
    double A = 0.0;
    int n = static_cast<int>(loop_vs.size());
    for (int i=0;i<n;++i) {
        const auto& p = R.points[ loop_vs[i] ];
        const auto& q = R.points[ loop_vs[(i+1)%n] ];
        A += p.x()*q.y() - p.y()*q.x();
    }
    return A; // >0 => CCW
}

enum class PathMode { Shorter=0, Longer=1, CW=2, CCW=3 };

inline std::vector<int> choose_boundary_arc(
    const DelaunayTriangulationResult& R,
    int v0, int v1,
    PathMode mode) {
    std::vector<int> vs, eids;
    if (!extract_boundary_loop(R, v0, vs, eids)) return {};

    auto arc_fw = arc_between_on_loop(vs, eids, v0, v1, /*forward=*/true);
    auto arc_bw = arc_between_on_loop(vs, eids, v0, v1, /*forward=*/false);
    if (arc_fw.empty() || arc_bw.empty()) return {};

    switch (mode) {
        case PathMode::Longer:
            return (arc_fw.size() >= arc_bw.size()) ? arc_fw : arc_bw;
        case PathMode::CW: {
            double A = polygon_area_sign(R, vs);
            return (A < 0.0) ? arc_fw : arc_bw;
        }
        case PathMode::CCW: {
            double A = polygon_area_sign(R, vs);
            return (A > 0.0) ? arc_fw : arc_bw;
        }
        case PathMode::Shorter:
        default:
            return (arc_fw.size() <= arc_bw.size()) ? arc_fw : arc_bw;
    }
}

} // namespace gui::bc

#endif // GUI_BC_UTILS_H