#ifndef FEM_EVENT_DEFINITION_H
#define FEM_EVENT_DEFINITION_H

#include "event_manager.h"

namespace fem {

#define FEM_DEFINE_EVENT(EventTypeName)                                                             \
    void EventTypeName::trigger() const {                                                           \
        EventManager::get().trigger_event(*this);                                                   \
    }                                                                                               \
    void EventTypeName::enqueue() const {                                                           \
        EventManager::get().enqueue_event(*this);                                                   \
    }                                                                                               \
    EventHandlerType EventTypeName::subscribe(const EventDelegate<EventTypeName>& event_delegate) { \
        return EventManager::get().subscribe<EventTypeName>(event_delegate);                        \
    }                                                                                               \
    void EventTypeName::unsubscribe(const EventHandlerType& event_handler_type) {                   \
        EventManager::get().unsubscribe<EventTypeName>(event_handler_type);                         \
    }

}

#endif 