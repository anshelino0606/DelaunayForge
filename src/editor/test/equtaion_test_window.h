#ifndef FEM_EQUATION_TEST_WINDOW_H
#define FEM_EQUATION_TEST_WINDOW_H

namespace fem {

class PDEComponent;

class EquationTestWindow {
public:
    EquationTestWindow();
    ~EquationTestWindow();

    void draw();

private:
    PDEComponent* component_;
};

void draw_equation_test_window();

}

#endif // FEM_EQUATION_TEST_WINDOW_H