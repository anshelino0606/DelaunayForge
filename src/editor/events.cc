#include "events.h"
#include "core/events/event_definition.h"

namespace fem {

FEM_DEFINE_EVENT(EntityRemovalRequest);
FEM_DEFINE_EVENT(ProjectOpenRequest);
FEM_DEFINE_EVENT(ProjectCreationRequest);
FEM_DEFINE_EVENT(ProjectSaveAsRequest);
FEM_DEFINE_EVENT(ProjectSaveRequest);

}