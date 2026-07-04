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
    int instance_id;
} vertex;
#define VERTEX(x, y, z, id) (vertex){{x, y, z}, id}

typedef struct {
    mat4f transform;
    color_t color;
} object_instance;

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

void vulkan_init(GLFWwindow *window, const char *title, uint32_t vk_version, uint32_t width, uint32_t height, bool vsync);
void vulkan_destroy();

void vulkan_create_vertex_buffer(vertex *verts, size_t cnt);
void vulkan_create_object_instance_buffer(object_instance *insts, size_t cnt);

void vulkan_begin(bool *req_swapchain_recreate, color_t bg_color_norm, uint32_t width, uint32_t height, bool vsync);
void vulkan_end(vertex *vertices, object_instance *instances, uint32_t *world_vertices_cnt, uint32_t *world_instances_cnt, mat4f *projection2D);
void vulkan_end_frame();
