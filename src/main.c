#include "gl.h"

#include <stdio.h>

int main(void) {
    enable_vsync(false);

    window_init(800, 800, "Hello Triangle!");
    while (!window_should_close()) {
        clear_background(RGB(10, 10, 10));
        begin_draw();
    
        draw_rect(VEC2F(100, 100), VEC2F(400, 300), RGB(255, 255, 0));

        end_draw();
    }
    window_close();
}
