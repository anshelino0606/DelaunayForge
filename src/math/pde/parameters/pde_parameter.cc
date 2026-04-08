#include "pde_parameter.h"

namespace fem {

FEM_DEFINE_OBJECT(PDEParameter, Object);
FEM_BEGIN_PROPERTY_REGISTER(PDEParameter)
{

}
FEM_END_PROPERTY_REGISTER(PDEParameter);

FEM_DEFINE_OBJECT(PDEScalarParameter, PDEParameter);
FEM_BEGIN_PROPERTY_REGISTER(PDEScalarParameter)
{

}
FEM_END_PROPERTY_REGISTER(PDEScalarParameter);

}