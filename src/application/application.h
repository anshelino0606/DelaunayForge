#ifndef FEM_APPLICATION_H
#define FEM_APPLICATION_H

#include "camera.h"
#include "core/window_.h"
#include "editor/editor.h"
#include "renderer/renderer.h"
#include <memory>

namespace fem {

class World;

class Application {
public:
    bool init();
    void shutdown();

    void run();

    bool should_close() const;
    void poll_events() const;

private:
    Window window_;
    World* world_;
    Editor editor_;
    Renderer renderer_;
    TriangulationSession triangulation_session_;

    std::unique_ptr<Camera> camera_;

    void update();
    void subscribe_to_events();

    void reset_app();
    void create_new_project(const std::string& base_path, const std::string& project_name);
    void save_project();
    void open_project(const std::string& project_file_path);
};

}

#endif // FEM_APPLICATION_H