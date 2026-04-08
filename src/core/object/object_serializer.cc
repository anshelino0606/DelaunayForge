#include "object_serializer.h"

#include "object.h"
#include "property.h"
#include "property_attribute.h"
#include "core/file_system/archive.h"

namespace fem {

void ObjectSerializer::serialize(Object* object, Archive& archive) {
    archive << object->get_type_info()->get_name();

    for (Property* property : object->get_properties()) {
        if (property->has_attribute<Transient>()) {
            continue;
        }

        property->serialize(object, archive);
    }
}

void ObjectSerializer::deserialize(Object* object, Archive& archive) {
    for (Property* property : object->get_properties()) {
        if (property->has_attribute<Transient>()) {
            continue;
        }

        property->deserialize(object, archive);
    }
}

}