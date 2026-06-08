#include "application.h"
#include "core/keyboard.h"
#include "core/mouse.h"
#include "core/entity/world.h"
#include "core/object/test.h"
#include "geom/planar_mesh/planar_mesh_component.h"
#include "core/events/event_manager.h"
#include "core/file_system/file_system.h"
#include "core/file_system/archive_test.h"
#include "core/project_file_extensions.h"
#include "math/entities/planar_math_entity.h"
#include "editor/events.h"
#include <fstream>

namespace fem {

namespace {
    constexpr const char* DEFAULT_ENGINE_NAME = "DelaunayForge";
    constexpr int WINDOW_WIDTH = 1200; //       fallback
    constexpr int WINDOW_HEIGHT = 800; //       case
}

bool Application::init() {
    FileSystem::init();

    if (!window_.init({
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,
        .title = DEFAULT_ENGINE_NAME
    })) {
        return false;
    }

    if (!editor_.init({
        .window = &window_,
        .draw_debug_info_callback = [&]() {
            renderer_.draw_debug_info();
        }
    })) {
        return false;
    }

    if (!renderer_.init({
        .window = &window_
    })) {
        return false;
    }

    camera_ = std::make_unique<Camera>(glm::vec3(380.0f, 180.0f, 750.0f));

    triangulation_session_.init();

    world_ = create_object<World>();

    world_->create_entity(PlanarMathEntity::get_static_type_info());

    subscribe_to_events();

    return true;
}

void Application::shutdown() {
    window_.shutdown();
    triangulation_session_.shutdown();
}

void Application::run() {
    while (!window_.should_close()) {
        update();

        window_.poll_events();

        world_->update_pre_entities_update();

        renderer_.begin_frame();

        EditorDrawResult editor_draw_result = editor_.draw({
            .entities = &world_->get_entities(),
            .viewport_texture_id = renderer_.get_viewport_texture_id()
        });

        renderer_.draw({
            .imgui_draw_data = ImGui::GetDrawData(),
            .editor_draw_result = &editor_draw_result,
            .camera = camera_.get()
        });

        EventManager::get().dispatch_events();
    }
}

void Application::update() {
    double dt = 1.0 / 60.0;
    
    if (Keyboard::key(GLFW_KEY_W)) {
        camera_->updateCameraPos(CameraDirection::FORWARD, dt);
    }
    if (Keyboard::key(GLFW_KEY_S)) {
        camera_->updateCameraPos(CameraDirection::BACKWARD, dt);
    }
    if (Keyboard::key(GLFW_KEY_A)) {
        camera_->updateCameraPos(CameraDirection::LEFT, dt);
    }
    if (Keyboard::key(GLFW_KEY_D)) {
        camera_->updateCameraPos(CameraDirection::RIGHT, dt);
    }
    if (Keyboard::key(GLFW_KEY_SPACE)) {
        camera_->updateCameraPos(CameraDirection::UP, dt);
    }
    if (Keyboard::key(GLFW_KEY_LEFT_SHIFT)) {
        camera_->updateCameraPos(CameraDirection::DOWN, dt);
    }
    
    double dx = Mouse::getDX();
    double dy = Mouse::getDY();
    
    if (Mouse::button(GLFW_MOUSE_BUTTON_RIGHT)) {
        camera_->updateCameraDirection(dx * 0.1, dy * 0.1);
    }
    
    double scroll = Mouse::getScrollDY();
    if (scroll != 0) {
        camera_->updateCameraZoom(scroll);
    }
}

void Application::subscribe_to_events() {
    EntityRemovalRequest::subscribe([this](const EntityRemovalRequest& request) {
        world_->remove_entity(request.entity());
    });

    ProjectOpenRequest::subscribe([this](const ProjectOpenRequest& request) {
        reset_app();
        open_project(request.path());
    });

    ProjectCreationRequest::subscribe([this](const ProjectCreationRequest& request) {
        create_new_project(request.path(), request.project_name());
        reset_app();
        world_->create_entity(MathEntity::get_static_type_info());
    });

    ProjectSaveRequest::subscribe([this](const ProjectSaveRequest& request) {
        if (FileSystem::get_project_path().empty()) {
            LOGT_ERROR(LogApplication, "Project is not created!");
            return;
        }
        save_project();
    });

    ProjectSaveAsRequest::subscribe([this](const ProjectSaveAsRequest& request) {
        create_new_project(request.path(), request.project_name());
        save_project();
    });
}

void Application::reset_app() {
    world_->reset();
    editor_.reset();
}

void Application::create_new_project(const std::string& base_path, const std::string& project_name) {
    std::string project_path = std::format("{}/{}", base_path, project_name);

    FileSystem::create_directories(project_path + "/entities");
    FileSystem::create_directories(project_path + "/configs");

    std::string full_project_name = std::format("{}/{}.{}", project_path, project_name, g_project_file_extension);

    std::ofstream file(full_project_name);
    file.close();

    FileSystem::set_project_path(project_path);
}

void Application::save_project() {
    world_->save_entities();
}

void Application::open_project(const std::string& project_file_path) {
    std::string project_path;
    size_t last_pos = project_file_path.find_last_of("/\\");

    if (last_pos != std::string::npos) {
        project_path = project_file_path.substr(0, last_pos);

        if (project_path.empty()) {
            LOGT_ERROR(LogApplication, "Project path [%s] is invalid", project_file_path.c_str());
            return;
        }
    }

    FileSystem::set_project_path(project_path);

    std::string entities_folder = std::format("{}/entities", project_path);

    FileSystem::for_each_file(entities_folder , { g_entity_file_extension }, [this](const DirectoryEntry& entry) {
        Archive archive(entry.path().string());
        if (!archive.ok()) {
            LOGT_ERROR(LogApplication, "Failed to open entity '%s': %s", entry.path().string().c_str(), archive.error().c_str());
            return;
        }

        world_->create_entity(archive);
        if (!archive.ok()) {
            LOGT_ERROR(LogApplication, "Failed to deserialize entity '%s': %s", entry.path().string().c_str(), archive.error().c_str());
        }
    });
}

}