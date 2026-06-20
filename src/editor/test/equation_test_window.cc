#include "equtaion_test_window.h"
#include "editor/widgets/object_widget.h"
#include "math/pde/pde_component.h"

namespace fem {

EquationTestWindow::EquationTestWindow() {
    component_ = create_object<PDEComponent>();
}

EquationTestWindow::~EquationTestWindow() {
    destroy_object(component_);
}

void EquationTestWindow::draw() {
    ImGui::Begin("Equation Test Window");
    ObjectWidget().draw(component_);
    ImGui::End();
}

void draw_equation_test_window() {
    static EquationTestWindow window;
    window.draw();
}

}