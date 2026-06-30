#ifndef FEM_TRIANGULATION_SESSION_EVENTS
#define FEM_TRIANGULATION_SESSION_EVENTS

#include "core/events/event.h"
#include "triangulation_session_config.h"

namespace fem {

class PlanarTriangulationRequest : public IEvent {
public:
    FEM_DECLARE_EVENT(PlanarTriangulationRequest)

    PlanarTriangulationRequest(const PlanarTriangulationSessionConfig& config)
        : config_(config) { }

    const PlanarTriangulationSessionConfig& config() const { return config_; }

private:
    PlanarTriangulationSessionConfig config_;
};

}

#endif // FEM_TRIANGULATION_SESSION_EVENTS