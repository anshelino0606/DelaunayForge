#include "pde_rhs.h"
#include "pde_rhs_kinds.h"

namespace fem {

FEM_DEFINE_OBJECT(PDE_RHS, PDEParameter, BaseClass(), AbstractClass());
FEM_BEGIN_PROPERTY_REGISTER(PDE_RHS)
{
    
}
FEM_END_PROPERTY_REGISTER(PDE_RHS)

FEM_DEFINE_OBJECT(PDEDynamicRHS, PDEParameter, BaseClass(), AbstractClass())
FEM_BEGIN_PROPERTY_REGISTER(PDEDynamicRHS)
{

}
FEM_END_PROPERTY_REGISTER(PDEDynamicRHS)

PDE_RHS* PDE_RHS::default_rhs() {
    return create_object<RHS_FConstant>();
}

PDEDynamicRHS* PDEDynamicRHS::default_rhs() {
    return create_object<RHS_FConstantDynamic>();
}

}