#include "event_manager.h"

namespace fem {

void EventManager::unsubscribe(uint64_t event_id, const EventHandlerType& event_handler_type) {
    if (event_handler_type == g_invalid_event_handler_type) {
        fprintf(stderr, "EventManager::unsubscribe(): Event handler type is invalid!\n"); fflush(stderr);
        return;
    }

    std::scoped_lock<std::mutex> locker(handlers_by_event_id_mutex_);
    auto it = handlers_by_event_id_.find(event_id);

    if (it == handlers_by_event_id_.end()) {
        fprintf(stderr, "EventManager::unsubscribe(): EventManager doesn't know event with!\n"); fflush(stderr);
        return;
    }

    auto& handlers = it->second;

    for (auto begin_it = handlers.begin(); begin_it != handlers.end(); ++begin_it) {
        if (begin_it->get()->get_type() == event_handler_type) {
            handlers.erase(begin_it);
            return;
        }
    }
}

void EventManager::trigger_event(const IEvent& event) {
    std::scoped_lock<std::mutex> locker(handlers_by_event_id_mutex_);

    auto it = handlers_by_event_id_.find(event.get_type_id());
    if (it == handlers_by_event_id_.end())
        return;

    for (auto& handler : it->second)
        handler->execute(event);
}

void EventManager::dispatch_events()
{
    while (!events_queue_.empty()) {
        IEvent* event = events_queue_.front().get();
        auto it = handlers_by_event_id_.find(event->get_type_id());
        if (it == handlers_by_event_id_.end()) {
            events_queue_.pop();
            continue;
        }
        
        auto& handlers = it->second;
        for (auto& handler : handlers) {
            handler->execute(*event);
        }

        events_queue_.pop();
    }
}  

}