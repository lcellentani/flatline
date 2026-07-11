#include <cstdio>

import engine.app;

int main() {
    auto app_result = App::create();
    if (!app_result) {
        std::fprintf(stderr, "Fatal: %s\n", app_result.error().c_str());
        return 1;
    }

    app_result.value().run();
    return 0;
}
