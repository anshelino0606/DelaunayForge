#include "property_attribute.h"
#include "property.h"
#include "type_info.h"

namespace fem {

static int enum_ordinal(const EnumTypeInfo* eti, Enum* e) {
    if (!eti || !e) return 0;
    const std::string_view cur = eti->get_value(e);
    int idx = 0;
    int found = 0;
    eti->for_each_element([&](std::string_view name) {
        if (!found) {
            if (name == cur) found = 1;
            else ++idx;
        }
    });
    return found ? idx : 0;
}

bool EditConditionFlags::evaluate(const void* owner, const StructTypeInfo* type_info) const {
    if (!owner || !type_info) return true;
    const Property* prop = type_info->get_property(property_name);
    if (!prop || prop->get_type() != PropertyType::ENUM) {
        return true;
    }
    auto* owner_obj = static_cast<Object*>(const_cast<void*>(owner));
    const EnumContext ec = prop->get_value_as_enum(owner_obj);
    const int ord = enum_ordinal(ec.type_info, ec.object);
    return (allowed_flags & (1u << ord)) != 0;
}

} // namespace fem