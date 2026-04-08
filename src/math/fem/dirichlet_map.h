#ifndef DIRICHLET_MAP
#define DIRICHLET_MAP

#include <vector>
#include "fem_mesh.h"
#include "fem_assembler.h"
#include "bc_value.h"

namespace fem {

struct DirichletData {
    std::vector<uint8_t> is_dirichlet; // 0/1
    std::vector<double>  value;        // uD
};

inline DirichletData build_dirichlet_data(const FEMMesh& mesh) {
    const int N = mesh.dof_count();
    DirichletData D;
    D.is_dirichlet.assign(N, 0);
    D.value.assign(N, 0.0);

    // If multiple edges touch a vertex, average;
    std::vector<int> cnt(N, 0);

    for (const auto& e : mesh.edges_bc) {
        if (e.type != fem::BCType::Dirichlet) continue;
        D.is_dirichlet[e.a] = 1; D.is_dirichlet[e.b] = 1;
        D.value[e.a] += e.uD;    D.value[e.b] += e.uD;
        cnt[e.a]++; cnt[e.b]++;
    }

    for (int i = 0; i < N; ++i) {
        if (D.is_dirichlet[i] && cnt[i] > 0) D.value[i] /= (double)cnt[i];
    }
    return D;
}

}

#endif