#pragma once

#include <stdbool.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <cpstd/mathplus.h>

typedef vec4f color_t;
#define RGB(r, g, b) (color_t){r, g, b, 255.0f}
#define RGBA(r, g, b, a) (color_t){r, g, b, a}
#define NORM_COLOR(color) (color_t){(color).r / 255.0f, (color).g / 255.0f, (color).b / 255.0f, (color).a / 255.0f}

typedef struct {
    vec3f pos;
    vec4f color;
} vertex;

typedef struct {
    uint64_t vertex_buffer_addr;
    float projection[16];
} shader_push_consts_t;

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer buffer;
    VkSemaphore img_aquired_sem;
} frame_resources_t;

void enable_vsync(bool enabled);
unsigned int get_fps();
float get_time();

void draw_rect(vec2f pos, vec2f size, color_t color);

void window_init(uint32_t width, uint32_t height, const char *title);
bool window_should_close();
void clear_background(color_t color);
void begin_draw();
void end_draw();
void window_close();
