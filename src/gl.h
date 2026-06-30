#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <stdbool.h>

typedef struct {
    VkCommandPool pool;
    VkCommandBuffer buffer;
    VkSemaphore img_aquired_sem;
} frame_resources_t;

void gl_init();
void gl_run();
void gl_close();
bool window_should_close();
