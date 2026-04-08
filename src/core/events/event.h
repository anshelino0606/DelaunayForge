#ifndef FEM_EVENT_H
#define FEM_EVENT_H

#include "event_types.h"
#include "core/compile_time_hash.h"

namespace fem {

#define FEM_EVENT_TYPE_HASH(x) compile_time_fnv1(#x)

#define FEM_DECLARE_EVENT(EventTypeName)							                        \
    enum class EventType : uint64_t	{					                                    \
        TYPE_ID = FEM_EVENT_TYPE_HASH(EventTypeName)				                        \
    };														                                \
    static inline constexpr uint64_t get_type_id_static() {                                 \
        return static_cast<uint64_t>(EventType::TYPE_ID);	                                \
    }														                                \
    virtual uint64_t get_type_id() const override {		                                    \
        return static_cast<uint64_t>(EventType::TYPE_ID);	                                \
    }                                                                                       \
    void trigger() const;                                                                   \
    void enqueue() const;                                                                   \
    static EventHandlerType subscribe(const EventDelegate<EventTypeName>& event_delegate);  \
    static void unsubscribe(const EventHandlerType& event_handler_type);                    \

class IEvent {
public:
    virtual ~IEvent() { }
    virtual uint64_t get_type_id() const = 0;
};

#define FEM_TRIGGER_EVENT(EventType, ...) \
    EventType event{__VA_ARGS__};         \
    event.trigger();

}

#endif // FEM_EVENT_H