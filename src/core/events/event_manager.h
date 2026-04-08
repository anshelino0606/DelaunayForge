#ifndef FEM_EVENT_MANAGER_H
#define FEM_EVENT_MANAGER_H

#include "event_handler.h"
#include <mutex>
#include <queue>

namespace fem {
    
class EventManager {
public:
    static EventManager& get() {
        static EventManager instance; 
        return instance;
    }

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;
    EventManager(EventManager&&) = delete;
    EventManager& operator=(EventManager&&) = delete;

    template<typename EventType>
    EventHandlerType subscribe(uint64_t event_id, const EventDelegate<EventType>& event_delegate) {
        std::scoped_lock<std::mutex> locker(handlers_by_event_id_mutex_);

        auto it = handlers_by_event_id_.find(event_id);
        std::unique_ptr<EventHandler<EventType>> event_handler(new EventHandler<EventType>(event_delegate));

        EventHandlerType new_handler_type = event_handler->get_type();

        if (it != handlers_by_event_id_.end()) {
            for (auto& handler : handlers_by_event_id_[event_id]) {
                if (handler->get_type() == new_handler_type) {
                    fprintf(stderr, "EventManager::subscribe(): Engine subscribed the event to the same event handler.\n"); fflush(stderr);
                    return g_invalid_event_handler_type;
                }
            }
        }

        handlers_by_event_id_[event_id].emplace_back(std::move(event_handler));

        return new_handler_type;
    }

    template<typename EventType>
    EventHandlerType subscribe(const EventDelegate<EventType>& event_delegate) {
        return subscribe(EventType::get_type_id_static(), event_delegate);
    }

    void unsubscribe(uint64_t event_id, const EventHandlerType& event_handler_type);

    template<typename EventType>
    void unsubscribe(const EventHandlerType& event_handler_type) {
        unsubscribe(EventType::get_type_id_static(), event_handler_type);
    }

    template<typename CustomEvent>
    void enqueue_event(const CustomEvent& event) {
        std::scoped_lock<std::mutex> locker(event_queue_mutex_);
        events_queue_.emplace(new CustomEvent(event));
    }

    void trigger_event(const IEvent& event);
    void dispatch_events();

private:
    using EventHandlerArray = std::vector<std::unique_ptr<IEventHandler>>;

    std::queue<std::unique_ptr<IEvent>> events_queue_;
    std::unordered_map<uint64_t, EventHandlerArray> handlers_by_event_id_;
    
    std::mutex event_queue_mutex_;
    std::mutex handlers_by_event_id_mutex_;

    EventManager() = default;
    ~EventManager() = default;
};

}

#endif // FEM_EVENT_MANAGER_H