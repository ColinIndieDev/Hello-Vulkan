#include "gl.h"

#include <cpstd/mathplus.h>
#include <stdio.h>

#include <cpstd/rand.h>

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

    pcg_rand_seed();

    clear_background(BLUE);
    while (!window_should_close()) {
        float speed = 250.0f;
        if (is_key_down(KEY_LETTER_A)) {
            pos.x -= speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_D)) {
            pos.x += speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_W)) {
            pos.y -= speed * get_dt();
        }
        if (is_key_down(KEY_LETTER_S)) {
            pos.y += speed * get_dt();
        }
        ball_update();

        begin_draw();
    
        draw_rect(VEC2F(0, 0), VEC2F(100.0f, 100.0f), DARK_GRAY, 0.0f);
        draw_circle(ball_pos, radius, YELLOW);
        draw_rect(pos, VEC2F(50.0f, 50.0f), PURPLE, get_time() * 360);

        end_draw();
    }
    window_close();
}
