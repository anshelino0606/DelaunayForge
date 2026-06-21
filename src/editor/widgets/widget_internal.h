#pragma once

namespace fem {

class Object;
class Struct;
class StructTypeInfo;

class WidgetInternal {
public:
    static bool draw_members(Object* object, const StructTypeInfo* type_info);
    static bool draw_members(Struct* object, const StructTypeInfo* type_info);
};

}