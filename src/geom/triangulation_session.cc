#include "triangulation_session.h"
#include "triangulation_session_events.h"

#include <glm/glm.hpp>
#include <glm/geometric.hpp>

namespace fem {

void TriangulationSession::init() {
    mesh_generator_ = std::make_shared<DelaunayMeshGenerator>();
    mesh_generator_->init();

    subscribe_to_events();
}

void TriangulationSession::shutdown() {
    mesh_generator_->shutdown();
}

void TriangulationSession::clear_all() {

}

void TriangulationSession::subscribe_to_events() {
    fem::PlanarTriangulationRequest::subscribe([this](const fem::PlanarTriangulationRequest& event){
        mesh_generator_->set_density_function(event.config().density_function);
        mesh_generator_->generate_mesh(event.config().mesh_generator_config, event.config().mesh);
    });
}

}
