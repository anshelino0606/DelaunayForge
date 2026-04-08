#ifndef FEM_DOCKING_WINDOW_H
#define FEM_DOCKING_WINDOW_H

namespace fem {

struct DockingWindowDrawInfo {

};

class DockingWindow {
public:
    void draw(const DockingWindowDrawInfo& draw_info);
};

}

#endif // FEM_DOCKING_WINDOW_H