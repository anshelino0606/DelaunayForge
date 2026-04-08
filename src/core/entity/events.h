#ifndef FEM_ENTITY_EVENTS_H
#define FEM_ENTITY_EVENTS_H

#include "core/events/event.h"

namespace fem {

class ObjectTypeInfo;

class EntityCreationRequest : public IEvent {
public:
    FEM_DECLARE_EVENT(EntityCreationRequest);

    EntityCreationRequest(const ObjectTypeInfo* entity_type_info) 
        : entity_type_info_(entity_type_info) {

    }

    const ObjectTypeInfo* entity_type_info() const { 
        return entity_type_info_;
    }

private:
    const ObjectTypeInfo* entity_type_info_ = nullptr;
};

}

#endif // FEM_ENTITY_EVENTS_H