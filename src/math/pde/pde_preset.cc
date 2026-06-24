#include "pde_preset.h"
#include "pde_presets.h"

namespace fem {

FEM_DEFINE_OBJECT(PDEPreset, Object, AbstractClass());
FEM_BEGIN_PROPERTY_REGISTER(PDEPreset)
{

}
FEM_END_PROPERTY_REGISTER(PDEPreset);

PDEPreset* PDEPreset::default_preset() {
    return create_object<PDEPreset_Laplace>();
}

} // namespace fem
