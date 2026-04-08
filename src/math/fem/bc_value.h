#ifndef BC_VALUE_H
#define BC_VALUE_H

#include <cassert>


namespace fem {

enum class BCType : int { None=0, Dirichlet=1, Neumann=2, Robin=3 };

template <typename Real>
struct BoundaryValueT {
    BCType type = BCType::None;
    Real   value = Real(0);
    Real   value_beta = Real(0);

    static BoundaryValueT none() { return {}; }
    static BoundaryValueT dirichlet(Real uD) { return {BCType::Dirichlet, uD, Real(0)}; }
    static BoundaryValueT neumann(Real qN)   { return {BCType::Neumann,   qN, Real(0)}; }
    static BoundaryValueT robin(Real k, Real g) { return {BCType::Robin, k, g}; }
};

using BoundaryValue = BoundaryValueT<double>;


template<typename Real>
inline bool is_active(const BoundaryValueT<Real>& bc) { return bc.type != BCType::None; }

template<typename Real>
inline Real primary_value(const BoundaryValueT<Real>& bc) { return bc.value; }

template<typename Real>
inline void set_primary_value(BoundaryValueT<Real>& bc, Real v) { bc.value = v; }

} // fem


#endif
