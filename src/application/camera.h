#ifndef FEM_ENGINE_CAMERA_H
#define FEM_ENGINE_CAMERA_H


#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace fem {

enum class CameraDirection {
    NONE = 0,
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};

class Camera {
public:
    glm::vec3 cameraPos;

    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 cameraRight;

    // camera rotational values
    float yaw; // x-axis
    float pitch; // y-axis

    float speed;
    float sensitivity;
    float zoom;

    /*
        constructor
    */

    // default and initialize with position
    Camera(glm::vec3 position = glm::vec3(0.0f));

    /*
        modifiers
    */

    // change camera direction (mouse movement)
    void updateCameraDirection(double dx, double dy);

    // change camera position in certain direction (keyboard)
    void updateCameraPos(CameraDirection direction, double dt);

    // change camera zoom (scroll wheel)
    void updateCameraZoom(double dy);

    /*
        accessors
    */

    // get view matrix for camera
    glm::mat4 getViewMatrix();

    // get zoom value for camera
    float getZoom();
    void lookAt(const glm::vec3& target);

private:
    /*
        private modifier
    */

    // change camera directional vectors based on movement
    void updateCameraVectors();
};

}

#endif //FEM_ENGINE_CAMERA_H
