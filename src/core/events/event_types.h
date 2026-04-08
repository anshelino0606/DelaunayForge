#ifndef FEM_EVENT_TYPES_H
#define FEM_EVENT_TYPES_H

#include <string>
#include <functional>

namespace fem {

template<typename EventType>
using EventDelegate = std::function<void(const EventType& event)>;

using EventHandlerType = std::string;
const EventHandlerType g_invalid_event_handler_type = "";

}

#endif 