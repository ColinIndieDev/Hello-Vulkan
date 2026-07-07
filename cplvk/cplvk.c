#include "cplvk.h"

#include "logging.h"
#include "vulkan.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include <GLFW/glfw3.h>

#include <cpstd/vector.h>
#include <cpstd/mathplus.h>
#include <cpstd/arena.h>

#define VULKAN_VERSION VK_API_VERSION_1_4

GLFWwindow *window = NULL;
const char *title = "Hello World";
uint32_t scr_width = 800, scr_height = 800;
bool vsync = true;
bool req_swapchain_recreate = false;

cam2D_t cam2D = {
    .pos = VEC2F(0, 0),
    .zoom = 1.0f,
    .rot = 0.0f
};
bool is_mode2D;
draw_mode_t cur_draw_mode = DRAW_MODE_NONE;
mat4f projection2D;

shader_t shape2D_shader;

color_t bg_color_norm = RGB(0, 0, 0);

vertex_t *vertices = NULL;
instance_t *instances = NULL;
uint32_t world_vertices_cnt = 0;
uint32_t world_instances_cnt = 0;

void enable_vsync(bool enabled) {
    vsync = enabled;
}

cam2D_t *get_cam2D() {
    return &cam2D;
}

mat4f *cam_2D_get_view_mat(cam2D_t *cam) {
    mat4f *view = malloc(sizeof(mat4f));
    mat4f_identity(view);

    mat4f_translate(view, VEC3F(-cam->pos.x, -cam->pos.y, 0.0f));

    mat4f_translate(view, VEC3F(cam->pos.x, cam->pos.y, 0.0f));
    mat4f_rotate(view, math_rad(cam->rot), VEC3F(0.0f, 0.0f, 1.0f));
    mat4f_translate(view, VEC3F(-cam->pos.x, -cam->pos.y, 0.0f));

    mat4f_translate(view, VEC3F(cam->pos.x, cam->pos.y, 0.0f));
    mat4f_scale(view, VEC3F(cam->zoom, cam->zoom, 1.0f));
    mat4f_translate(view, VEC3F(-cam->pos.x, -cam->pos.y, 0.0f));

    return view;
}

// {{{ Time

unsigned int nb_frames = 0;
float last_time = 0.0f;
float last_frame = 0.0f;
float dt = 0.0f;
float target_frame_time = 0.0f;
unsigned int fps = 0;

float get_time() {
    static struct timespec start_ts;
    static bool initialized = false;
    struct timespec cur_ts;
    if (!initialized) {
        clock_gettime(CLOCK_MONOTONIC, &start_ts);
        initialized = true;
    }
    clock_gettime(CLOCK_MONOTONIC, &cur_ts);
    return (float)((double)(cur_ts.tv_sec - start_ts.tv_sec) + ((double)(cur_ts.tv_nsec - start_ts.tv_nsec) * 1e-9));
}
void calc_fps() {
    float cur_time = get_time();
    nb_frames++;
    if (cur_time - last_time >= 1.0) {
        fps = nb_frames;
        nb_frames = 0;
        last_time += 1.0f;
    }
}
unsigned int get_fps() { 
    return fps; 
}
void calc_dt() {
    float cur_frame = get_time();
    dt = cur_frame - last_frame;
    last_frame = cur_frame;
}
float get_dt() { 
    return dt; 
}

// }}}

// {{{ Input

bool key_states[KEY_LAST - KEY_SPACE + 1];
bool prev_key_states[KEY_LAST - KEY_SPACE + 1];
bool mouse_button_states[MOUSE_BUTTON_LAST + 1];
bool prev_mouse_button_states[MOUSE_BUTTON_LAST + 1];

void input_update() {
    for (unsigned int i = 0; i < KEY_LAST - KEY_SPACE; i++) {
        prev_key_states[i] = key_states[i];
    }
    for (unsigned int key = KEY_SPACE; key <= KEY_LAST; key++) {
        key_states[key - KEY_SPACE] = glfwGetKey(window, (int)key) == GLFW_PRESS;
    }

    for (unsigned int i = 0; i < MOUSE_BUTTON_LAST - MOUSE_BUTTON_1; i++) {
        prev_mouse_button_states[i] = mouse_button_states[i];
    }
    for (unsigned int button = MOUSE_BUTTON_1; button <= MOUSE_BUTTON_LAST; button++) {
        mouse_button_states[button - MOUSE_BUTTON_1] = glfwGetMouseButton(window, (int)button) == GLFW_PRESS;
    }
}
bool is_key_down(key_buttons key) { 
    return key_states[(int)key - KEY_SPACE]; 
}
bool is_key_up(key_buttons key) { 
    return !key_states[(int)key - KEY_SPACE]; 
}
bool is_key_pressed(key_buttons key) {
    return key_states[(int)key - KEY_SPACE] && !prev_key_states[(int)key - KEY_SPACE];
}
bool is_key_released(key_buttons key) {
    return !key_states[(int)key - KEY_SPACE] && prev_key_states[(int)key - KEY_SPACE];
}
bool is_mouse_down(mouse_buttons button) {
    return mouse_button_states[(int)button - MOUSE_BUTTON_1];
}
bool is_mouse_pressed(mouse_buttons button) {
    return mouse_button_states[(int)button - MOUSE_BUTTON_1] && !prev_mouse_button_states[(int)button - MOUSE_BUTTON_1];
}
bool is_mouse_released(mouse_buttons button) {
    return !mouse_button_states[(int)button - MOUSE_BUTTON_1] && prev_mouse_button_states[(int)button - MOUSE_BUTTON_1];
}

// }}}

// {{{ Window Init

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    (void)window;
    scr_width = width;
    scr_height = height;

    req_swapchain_recreate = true;

    mat4f_ortho(&projection2D, 0.0f, (float)scr_width,(float)scr_height, 0.0f, -1.0f, 1.0f);
}

void window_init(uint32_t width, uint32_t height, const char *title) {
    scr_width = width;
    scr_height = height;
    title = title;
    
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    window = glfwCreateWindow((int)scr_width, (int)scr_height, title, NULL, NULL);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    log_msg(SEVERITY_INFO, "Initialized window!\n");

    mat4f_ortho(&projection2D, 0.0f, (float)scr_width, (float)scr_height, 0.0f, -1.0f, 1.0f);

    vulkan_init(window, title, VULKAN_VERSION, scr_width, scr_height, vsync);

    vulkan_load_shader(&shape2D_shader, "shaders/shader.vert", "shaders/shader.frag");

    vertices = vec_init(vertices, 1000);
    instances = vec_init(instances, 100);
}

// }}}

bool window_should_close() {
    calc_fps();
    calc_dt();
    input_update();
    return glfwWindowShouldClose(window);
}

uint32_t get_screen_width() {
    return scr_width;
}   
uint32_t get_screen_height() {
    return scr_height;
}   

void window_close() {
    vulkan_destroy_shader(&shape2D_shader);
    
    vulkan_destroy();

    vec_destroy(vertices);
    vec_destroy(instances);

    glfwDestroyWindow(window);
    glfwTerminate();
    log_msg(SEVERITY_INFO, "Closing window and terminating!\n");
}

// {{{ Drawing

void clear_background(color_t color) {
    bg_color_norm = NORM_COLOR(color);
}

void draw_rect(vec2f pos, vec2f size, color_t color, float rotation) {
    color_t norm_color = NORM_COLOR(color);

    vertex_t rectangle[6] = {
        VERTEX(size.x, 0.0f,   0.0f, world_instances_cnt), 
        VERTEX(size.x, size.y, 0.0f, world_instances_cnt), 
        VERTEX(0.0f,   0.0f,   0.0f, world_instances_cnt), 
    
        VERTEX(size.x, size.y, 0.0f, world_instances_cnt),
        VERTEX(0.0f,   size.y, 0.0f, world_instances_cnt), 
        VERTEX(0.0f,   0.0f,   0.0f, world_instances_cnt)
    };

    mat4f transform;
    mat4f_identity(&transform);
    mat4f_translate(&transform, VEC3F(pos.x, pos.y, 0.0f));
    mat4f_translate(&transform, VEC3F(size.x * 0.5f, size.y * 0.5f, 0.0f));
    mat4f_rotate(&transform, math_rad(rotation), VEC3F(0.0f, 0.0f, 1.0f));
    mat4f_translate(&transform, VEC3F(-size.x * 0.5f, -size.y * 0.5f, 0.0f));

    for (int i = 0; i < 6; i++) {
        vec_push(vertices, rectangle[i]);
    }
    world_vertices_cnt += 6;

    instance_t inst = {
        .color = norm_color
    };
    memcpy(inst.transform, transform, sizeof(mat4f));
    vec_push(instances, inst);
    world_instances_cnt++;
}

void draw_triangle(vec2f pos, vec2f size, color_t color, float rotation) {
    color_t norm_color = NORM_COLOR(color);

    vertex_t triangle[3] = {
        VERTEX(0.0f,            0.0f,   0.0f, world_instances_cnt), 
        VERTEX(size.x,          0.0f,   0.0f, world_instances_cnt), 
        VERTEX(size.x * 0.5f,   size.y, 0.0f, world_instances_cnt), 
    };

    mat4f transform;
    mat4f_identity(&transform);
    mat4f_translate(&transform, VEC3F(pos.x, pos.y, 0.0f));
    mat4f_translate(&transform, VEC3F(size.x * 0.5f, size.y * 0.5f, 0.0f));
    mat4f_rotate(&transform, math_rad(rotation), VEC3F(0.0f, 0.0f, 1.0f));
    mat4f_translate(&transform, VEC3F(-size.x * 0.5f, -size.y * 0.5f, 0.0f));

    for (int i = 0; i < 3; i++) {
        vec_push(vertices, triangle[i]);
    }
    world_vertices_cnt += 3;

    instance_t inst = {
        .color = norm_color
    };
    memcpy(inst.transform, transform, sizeof(mat4f));
    vec_push(instances, inst);
    world_instances_cnt++;
}

void draw_circle(vec2f pos, float radius, color_t color) {
    color_t norm_color = NORM_COLOR(color);

    float max_error = 0.25f; 
    float segments_cnt = MATH_PI / acosf(1.0f - (max_error / radius));
    uint32_t segments = (uint32_t)(ceilf(segments_cnt));
    math_clamp(segments, 8, 128);
    uint32_t vertices_cnt = segments * 3;
    vertex_t circle[vertices_cnt];
    int idx = 0;
    float step = (float)(2 * MATH_PI) / (float)segments;

    for (int i = 0; i < segments; i++) {
        float alpha1 = (float)i * step;
        float alpha2 = (float)(i + 1) * step;

        circle[idx++] = VERTEX(0.0f, 0.0f, 0.0f, world_instances_cnt);
        circle[idx++] = VERTEX(cosf(alpha1) * radius, sinf(alpha1) * radius, 0.0f, world_instances_cnt);
        circle[idx++] = VERTEX(cosf(alpha2) * radius, sinf(alpha2) * radius, 0.0f, world_instances_cnt);
    }

    mat4f transform;
    mat4f_identity(&transform);
    mat4f_translate(&transform, VEC3F(pos.x, pos.y, 0.0f));

    for (int i = 0; i < vertices_cnt; i++) {
        vec_push(vertices, circle[i]);
    }
    world_vertices_cnt += vertices_cnt;

    instance_t inst = {
        .color = norm_color
    };
    memcpy(inst.transform, transform, sizeof(mat4f));
    vec_push(instances, inst);
    world_instances_cnt++;
}

// }}}

void begin_draw(draw_mode_t mode) {
    if (mode == DRAW_MODE_NONE) {
        log_msg(SEVERITY_ERROR, "Cannot change draw mode to DRAW_MODE_NONE!\n");
        exit(-1);
    }
    
    if (cur_draw_mode == DRAW_MODE_NONE) {
        vulkan_begin(&req_swapchain_recreate, bg_color_norm, scr_width, scr_height, vsync);
        cur_draw_mode = mode;

        // Since there are not any more shaders we bind always the same
        vulkan_shader_bind(&shape2D_shader);

        return;
    }

    if (cur_draw_mode != mode) {
        mat4f view_projection;
        if (cur_draw_mode == DRAW_MODE_2D) {
            mat4f *view = cam_2D_get_view_mat(&cam2D);
            mat4f_mul(&projection2D, view, &view_projection);
            free(view);
        } else {
            memcpy(&view_projection, &projection2D, sizeof(mat4f));
        }
        vulkan_end(vertices, instances, world_vertices_cnt, world_instances_cnt, &view_projection, cur_draw_mode);
        vec_clear(vertices);
        vec_clear(instances);
        world_vertices_cnt = 0;
        world_instances_cnt = 0;
        cur_draw_mode = mode;
    }
}

void end_draw() {
    if (cur_draw_mode == DRAW_MODE_NONE) {
        glfwPollEvents();
        return;
    }

    mat4f view_projection;
    if (cur_draw_mode == DRAW_MODE_2D) {
        mat4f *view = cam_2D_get_view_mat(&cam2D);
        mat4f_mul(&projection2D, view, &view_projection);
        free(view);
    } else {
        memcpy(&view_projection, &projection2D, sizeof(mat4f));
    }
    vulkan_end(vertices, instances, world_vertices_cnt, world_instances_cnt, &view_projection, cur_draw_mode);
    vulkan_end_frame();

    vec_clear(vertices);
    vec_clear(instances);
    world_vertices_cnt = 0;
    world_instances_cnt = 0;
    cur_draw_mode = DRAW_MODE_NONE;

    glfwPollEvents();
}
