#ifndef FEM_PLANAR_MATH_ENTITY_H
#define FEM_PLANAR_MATH_ENTITY_H

#include "math_entity.h"

namespace fem {

class PlanarMathEntity : public MathEntity {
public:
    FEM_DECLARE_OBJECT(PlanarMathEntity);
    FEM_DECLARE_PROPERTY_REGISTER(PlanarMathEntity);
    
    virtual void init() override;
    virtual MeshComponent* add_mesh() override;
};

}

#endif // FEM_PLANAR_MATH_ENTITY_H