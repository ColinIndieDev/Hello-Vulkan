#include "../cplvk/cplvk.h"

#include <stdio.h>

vec2f pos = VEC2F(800, 0);

vec2f ball_pos = VEC2F(400, 400);
float radius = 10.0f;
vec2f dir = VEC2F(1, 1);

void ball_update() {
    float speed = 400.0f;
    ball_pos.x += speed * dir.x * get_dt();
    ball_pos.y += speed * dir.y * get_dt();
    if (ball_pos.x - radius < 0.0f || ball_pos.x + radius > (float)get_screen_width()) {
        dir.x *= -1;
    }
    if (ball_pos.y - radius < 0.0f || ball_pos.y + radius > (float)get_screen_height()) {
        dir.y *= -1;
    }
}

int main(void) {
    enable_vsync(false);
    window_init(800, 800, "Hello Triangle!");

    clear_background(BLUE);
    while (!window_should_close()) {
        float speed = 250.0f;
        if (is_key_down(KEY_LETTER_A)) {
            get_cam2D()->pos.x -= speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_D)) {
            get_cam2D()->pos.x += speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_W)) {
            get_cam2D()->pos.y -= speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_S)) {
            get_cam2D()->pos.y += speed * get_dt();
        }
        ball_update();

        begin_draw(DRAW_MODE_2D);
        draw_rect(VEC2F(0, 0), VEC2F(100, 100), DARK_GRAY, 0);
        draw_rect(pos, VEC2F(50, 50), PURPLE, get_time() * 360);
        draw_triangle(VEC2F(400, 400), VEC2F(80, 80), RED, 45);

        begin_draw(DRAW_MODE_UI);
        draw_circle(ball_pos, radius, YELLOW);
        
        end_draw();
    }
    window_close();
}
