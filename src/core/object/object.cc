#include "object.h"
#include "object_serializer.h"
#include "core/file_system/file_system.h"

namespace fem {

FEM_DEFINE_ROOT_OBJECT(Object);

void Object::serialize(Archive& archive) {
    ObjectSerializer::serialize(this, archive);
}

void Object::deserialize(Archive& archive) {
    ObjectSerializer::deserialize(this, archive);
    last_path_ = archive.path();
}

void Object::serialize() {

}

}