#include "object_destruction_queue.h"
#include "object.h"

namespace fem {

void ObjectDestructionQueue::destroy(Object* object) {
    destroy_handlers_.emplace_back([object]{
        TypeManager::destroy_object(object);
    });
}

void ObjectDestructionQueue::destroy(const ObjectDestroyHandler& handler) {
    destroy_handlers_.push_back(handler);
}

void ObjectDestructionQueue::update() {
    for (const ObjectDestroyHandler& handler : destroy_handlers_) {
        handler();
    }
    destroy_handlers_.clear();
}

}