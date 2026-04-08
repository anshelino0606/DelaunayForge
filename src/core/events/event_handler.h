#ifndef FEM_EVENT_HANDLER_H
#define FEM_EVENT_HANDLER_H

#include "event.h"

namespace fem {

class IEventHandler {
public:
    virtual ~IEventHandler() { }

    void execute(const IEvent& event)
    {
        internal_execute(event);
    }

    virtual EventHandlerType get_type() = 0;

protected:
    virtual void internal_execute(const IEvent& event) = 0;
};

template<typename EventType>
class EventHandler : public IEventHandler {
public:
    explicit EventHandler(const EventDelegate<EventType>& event_delegate)
        : event_delegate_(event_delegate), type_(event_delegate_.target_type().name()) { }

    virtual EventHandlerType get_type() override
    {
        return type_;
    }

private:
    EventDelegate<EventType> event_delegate_;
    EventHandlerType type_;

    virtual void internal_execute(const IEvent& event) override
    {
        if (EventType::get_type_id_static() == event.get_type_id())
            event_delegate_(static_cast<const EventType&>(event));
    }
};

}

#endif // FEM_EVENT_HANDLER_H