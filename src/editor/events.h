#ifndef FEM_EDITOR_EVENTS_H
#define FEM_EDITOR_EVENTS_H

#include "core/events/event.h"

namespace fem {

class Entity;

class EntityRemovalRequest : public IEvent
{
public:
    FEM_DECLARE_EVENT(EntityRemovalRequest);

    EntityRemovalRequest(Entity* entity) : m_entity(entity) { }

    Entity* entity() const { return m_entity; }

private:
    Entity* m_entity;
};

class ProjectOpenRequest : public IEvent {
public:
    FEM_DECLARE_EVENT(ProjectOpenRequest);

    ProjectOpenRequest(const std::string& path) 
        : path_(path){

    }

    const std::string& path() const { 
        return path_;
    }

private:
    std::string path_;
};

class ProjectCreationRequest : public ProjectOpenRequest {
public:
    FEM_DECLARE_EVENT(ProjectCreationRequest);

    ProjectCreationRequest(const std::string& path, const std::string& project_name) 
        : ProjectOpenRequest(path), project_name_(project_name) { }

    const std::string& project_name() const { return project_name_; }

private:
    std::string project_name_;
};

class ProjectSaveAsRequest : public ProjectCreationRequest {
public:
    FEM_DECLARE_EVENT(ProjectSaveAsRequest);

    using ProjectCreationRequest::ProjectCreationRequest;
};

class ProjectSaveRequest : public IEvent {
public:
    FEM_DECLARE_EVENT(ProjectSaveRequest);
};


}

#endif // FEM_EDITOR_EVENTS_H