#include "application/application.h"
#include <iostream>
#include "log_categories.h"

DEFINE_LOG_CATEGORY(MAIN);

int main() {
    fem::Application app;
    if (!app.init()) {
        LOGT_ERROR(MAIN, "Failed to initialize application");
        return -1;
    }
    LOGT_INFO(MAIN, "Starting app");
    app.run(); 
    LOGT_INFO(MAIN, "Exiting app");

    logger::shutdown();
    return 0;
}