#pragma once

#include <stdbool.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <GLFW/glfw3.h>

#include <cpstd/mathplus.h>

#define MAX_FRAME_IN_FLIGHT 2

typedef vec4f color_t;
#define NORM_COLOR(color) (color_t){(color).r / 255.0f, (color).g / 255.0f, (color).b / 255.0f, (color).a / 255.0f}

typedef struct {
    vec3f pos;
    uint32_t instance_id;
} vertex_t;
#define VERTEX(x, y, z, id) (vertex_t){{x, y, z}, id}

typedef struct {
    mat4f transform;
    color_t color;
} instance_t;

typedef struct __attribute((aligned(16))) {
    uint64_t vertex_buffer_addr;
    uint64_t instance_buffer_addr;
    mat4f projection;       
} shader_push_constants_t;

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer buffer;
    VkSemaphore img_aquired_sem;
} frame_resources_t;

typedef struct {
    VkShaderEXT shader;
    VkShaderStageFlagBits stage;
} shader_module_t;

typedef struct {
    shader_module_t vert;
    shader_module_t frag;
    VkPushConstantRange push_const_range;
} shader_t;

typedef enum {
    DRAW_MODE_NONE = -1,
    DRAW_MODE_2D,
    DRAW_MODE_UI,
    DRAW_MODES_SIZE
} draw_mode_t;

void vulkan_init(GLFWwindow *window, const char *title, uint32_t vk_version, uint32_t width, uint32_t height, bool vsync);
void vulkan_destroy();

void vulkan_load_shader(shader_t *shader, char *vert_path, char *frag_path);
void vulkan_shader_bind(shader_t *shader);
void vulkan_destroy_shader(shader_t *shader);

void vulkan_create_vertex_buffer(vertex_t *vertices, size_t size, draw_mode_t mode);
void vulkan_create_object_instance_buffer(instance_t *instances, size_t size, draw_mode_t mode);

bool vulkan_begin(bool *req_swapchain_recreate, color_t bg_color_norm, uint32_t width, uint32_t height, bool vsync);
void vulkan_end(vertex_t *vertices, instance_t *instances, uint32_t vertices_size, uint32_t instances_size, mat4f *projection2D, draw_mode_t mode);
void vulkan_end_frame();
