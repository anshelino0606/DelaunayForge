#pragma once

#include <vector>
#include <functional>

namespace fem {

class Object;

using ObjectDestroyHandler = std::function<void()>;

class ObjectDestructionQueue {
public:
    void destroy(Object* object);
    void destroy(const ObjectDestroyHandler& handler);

    void update();

private:
    std::vector<ObjectDestroyHandler> destroy_handlers_;
};

}