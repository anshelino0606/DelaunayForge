#ifndef FEM_OBJECT_SERIALIZER_H
#define FEM_OBJECT_SERIALIZER_H

namespace fem {

class Object;
class Archive;

class ObjectSerializer {
public:
    static void serialize(Object* object, Archive& archive);
    static void deserialize(Object* object, Archive& archive);
};

}

#endif // FEM_OBJECT_SERIALIZER_H