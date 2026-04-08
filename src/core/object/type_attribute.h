#ifndef FEM_TYPE_ATTRIBUTE_H
#define FEM_TYPE_ATTRIBUTE_H

#include "attribute.h"

namespace fem {

struct TypeProperty {

};

struct Config : public TypeProperty {
    FEM_DEFINE_ATTRIBUTE(Config);
};

// For enums
struct DrawAsToggles : public TypeProperty {
    FEM_DEFINE_ATTRIBUTE(DrawAsToggles);

    constexpr DrawAsToggles() = default;
    constexpr DrawAsToggles(bool in_same_line_with_label) 
        : same_line_with_label(in_same_line_with_label) { }

    bool same_line_with_label = false;
};

struct BaseClass : public TypeProperty {
    FEM_DEFINE_ATTRIBUTE(BaseClass);
};

// Will not appear in types combo box if property/type has BaseClass attribute
struct AbstractClass : public TypeProperty {
    FEM_DEFINE_ATTRIBUTE(AbstractClass);
};

}

#endif // FEM_TYPE_ATTRIBUTE_H