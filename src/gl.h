#pragma once

#include <stdbool.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h>

#include <cpstd/mathplus.h>

typedef vec4f color_t;
#define RGB(r, g, b) (color_t){r, g, b, 255.0f}
#define RGBA(r, g, b, a) (color_t){r, g, b, a}
#define NORM_COLOR(color) (color_t){(color).r / 255.0f, (color).g / 255.0f, (color).b / 255.0f, (color).a / 255.0f}

#define WHITE RGB(255, 255, 255)
#define BLACK RGB(0, 0, 0)
#define RED RGB(255, 0, 0)
#define ORANGE RGB(255, 127, 0)
#define YELLOW RGB(255, 255, 0)
#define LIME_GREEN RGB(0, 255, 0)
#define GREEN RGB(0, 150, 25)
#define BLUE RGB(0, 0, 255)
#define LIGHT_BLUE RGB(0, 255, 255)
#define PURPLE RGB(127, 0, 255)
#define PINK RGB(255, 0, 255)
#define LIGHT_GRAY RGB(200, 200, 200)
#define DARK_GRAY RGB(64, 64, 64)
#define BROWN RGB(150, 76, 0)

#define ROTATION_NONE (vec3f){0.0f, 0.0f, 0.0f}
#define ROTATION_X(r) (vec3f){r, 0.0f, 0.0f}
#define ROTATION_Y(r) (vec3f){0.0f, r, 0.0f}
#define ROTATION_Z(r) (vec3f){0.0f, 0.0f, r}

typedef struct {
    vec3f pos;
    int instance_id;
} vertex;
#define VERTEX(x, y, z, id) (vertex){{x, y, z}, id}

typedef struct {
    mat4f transform;
    color_t color;
} object_instance;

// shaders use std430 which expects 16 byte alignment
typedef struct __attribute((aligned(16))) {
    uint64_t vertex_buffer_addr;
    uint64_t instance_buffer_addr;
    mat4f projection;       
} shader_push_consts_t;

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer buffer;
    VkSemaphore img_aquired_sem;
} frame_resources_t;

typedef enum : uint8_t {
    SEVERITY_INFO = 0,
    SEVERITY_WARNING,
    SEVERITY_ERROR
} severity_t;

void log_msg(severity_t severtiy, const char *msg, ...);

void enable_vsync(bool enabled);
unsigned int get_fps();
float get_time();
float get_dt();

// {{{ Key Inputs

typedef enum {
    MOUSE_BUTTON_1 = 0,
    MOUSE_BUTTON_2,
    MOUSE_BUTTON_3,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,
    MOUSE_BUTTON_LAST = MOUSE_BUTTON_8,
    MOUSE_BUTTON_LEFT = MOUSE_BUTTON_1,
    MOUSE_BUTTON_RIGHT = MOUSE_BUTTON_2,
    MOUSE_BUTTON_MIDDLE = MOUSE_BUTTON_3,
} mouse_buttons;

typedef enum {
    KEY_SPACE = 32,
    KEY_APOSTROPHE = 39,
    KEY_COMMA = 44,
    KEY_MINUS,
    KEY_PERIOD,
    KEY_SLASH,
    KEY_DIGIT_0,
    KEY_DIGIT_1,
    KEY_DIGIT_2,
    KEY_DIGIT_3,
    KEY_DIGIT_4,
    KEY_DIGIT_5,
    KEY_DIGIT_6,
    KEY_DIGIT_7,
    KEY_DIGIT_8,
    KEY_DIGIT_9,
    KEY_SEMICOLON = 59,
    KEY_EQUAL = 61,
    KEY_LETTER_A = 65,
    KEY_LETTER_B,
    KEY_LETTER_C,
    KEY_LETTER_D,
    KEY_LETTER_E,
    KEY_LETTER_F,
    KEY_LETTER_G,
    KEY_LETTER_H,
    KEY_LETTER_I,
    KEY_LETTER_J,
    KEY_LETTER_K,
    KEY_LETTER_L,
    KEY_LETTER_M,
    KEY_LETTER_N,
    KEY_LETTER_O,
    KEY_LETTER_P,
    KEY_LETTER_Q,
    KEY_LETTER_R,
    KEY_LETTER_S,
    KEY_LETTER_T,
    KEY_LETTER_U,
    KEY_LETTER_V,
    KEY_LETTER_W,
    KEY_LETTER_X,
    KEY_LETTER_Y,
    KEY_LETTER_Z,
    KEY_LEFT_BRACKET,
    KEY_BACKSLASH,
    KEY_RIGHT_BRACKET,
    KEY_GRAVE_ACCENT = 96,
    KEY_WORLD_1 = 161,
    KEY_WORLD_2,
    KEY_ESCAPE = 256,
    KEY_ENTER,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,
    KEY_RIGHT_ARROW,
    KEY_LEFT_ARROW,
    KEY_DOWN_ARROW,
    KEY_UP_ARROW,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,
    KEY_HOME,
    KEY_END,
    KEY_CAPS_LOCK = 280,
    KEY_SCROLL_LOCK,
    KEY_NUM_LOCK,
    KEY_PRINT_SCREEN,
    KEY_PAUSE,
    KEY_F1 = 290,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_F25,
    KEY_KP_0 = 320,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,
    KEY_LEFT_SHIFT = 340,
    KEY_LEFT_CONTROL,
    KEY_LEFT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SHIFT,
    KEY_RIGHT_CONTROL,
    KEY_RIGHT_ALT,
    KEY_RIGHT_SUPER,
    KEY_MENU,
    KEY_LAST = GLFW_KEY_MENU
} key_buttons;

// }}}

bool is_key_down(key_buttons key);
bool is_key_up(key_buttons key);
bool is_key_pressed(key_buttons key);
bool is_key_released(key_buttons key);
bool is_mouse_down(mouse_buttons button);
bool is_mouse_pressed(mouse_buttons button);
bool is_mouse_released(mouse_buttons button);

void draw_rect(vec2f pos, vec2f size, color_t color, float rotation);
void draw_circle(vec2f pos, float radius, color_t color);

void window_init(uint32_t width, uint32_t height, const char *title);
bool window_should_close();
uint32_t get_screen_width();
uint32_t get_screen_height();
void clear_background(color_t color);
void begin_draw();
void end_draw();
void window_close();
