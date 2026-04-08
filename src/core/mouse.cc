// mouse.cpp
#include "mouse.h"
#include <cstring> // for memset

double Mouse::x = 0.0;
double Mouse::y = 0.0;
double Mouse::lastX = 0.0;
double Mouse::lastY = 0.0;
double Mouse::dx = 0.0;
double Mouse::dy = 0.0;
double Mouse::scrollDx = 0.0;
double Mouse::scrollDy = 0.0;
bool Mouse::firstMouse = true;
bool Mouse::buttons[GLFW_MOUSE_BUTTON_LAST+1];
bool Mouse::buttonsChanged[GLFW_MOUSE_BUTTON_LAST+1];

void Mouse::cursorPosCallback(GLFWwindow* window, double _x, double _y) {
    if (firstMouse) {
        lastX = _x;
        lastY = _y;
        firstMouse = false;
    }

    dx = _x - lastX;
    dy = lastY - _y; // invert y for typical camera usage
    lastX = _x;
    lastY = _y;

    x = _x;
    y = _y;
}

void Mouse::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return;

    if (action == GLFW_PRESS) {
        buttons[button] = true;
    } else if (action == GLFW_RELEASE) {
        buttons[button] = false;
    }
    buttonsChanged[button] = true;
}

void Mouse::mouseWheelCallback(GLFWwindow* window, double dx_, double dy_) {
    scrollDx = dx_;
    scrollDy = dy_;
}

double Mouse::getMouseX() { return x; }
double Mouse::getMouseY() { return y; }

double Mouse::getDX() {
    double temp = dx;
    dx = 0.0;
    return temp;
}

double Mouse::getDY() {
    double temp = dy;
    dy = 0.0;
    return temp;
}

double Mouse::getScrollDX() {
    double temp = scrollDx;
    scrollDx = 0;
    return temp;
}

double Mouse::getScrollDY() {
    double temp = scrollDy;
    scrollDy = 0;
    return temp;
}

bool Mouse::button(int button) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return buttons[button];
}

bool Mouse::buttonChanged(int button) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    bool ret = buttonsChanged[button];
    buttonsChanged[button] = false;
    return ret;
}

bool Mouse::buttonWentUp(int btn) {
    return buttonChanged(btn) && !button(btn);
}

bool Mouse::buttonWentDown(int btn) {
    return buttonChanged(btn) && button(btn);
}
