#include "gl.h"

#include <GLFW/glfw3.h>

int main() {
    gl_init();

    while (!window_should_close()) {
        gl_run();
        glfwPollEvents();
    }
    gl_close();
}
