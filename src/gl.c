#include "gl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <vk_mem_alloc.h>

char *title = "Hello Vulkan!";

GLFWwindow *window = NULL;

VkInstance instance = NULL;
VkSurfaceKHR surface = NULL;
VkFormat swapchain_fmt = VK_FORMAT_B8G8R8_SRGB;
VkPhysicalDevice physical_device = NULL;
VkDevice device = NULL;
uint32_t gfx_queue_fam_idx = UINT32_MAX;
VkQueue gfx_queue = NULL;

void gl_create_instance() {
    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "Failed to init Volk!\n");
        exit(-1);
    }
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title,
        .applicationVersion = VK_API_VERSION_1_4,
        .apiVersion = VK_API_VERSION_1_4
    };

    uint32_t count;
    const char **extensions = glfwGetRequiredInstanceExtensions(&count);

    const char *requested_layers[1] = {
        "VK_LAYER_KHRONOS_validation"
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = requested_layers,
        .enabledExtensionCount = count,
        .ppEnabledExtensionNames = extensions
    };

    if (vkCreateInstance(&create_info, NULL, &instance) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create Vulkan instance!\n");
        exit(-1);
    }

    volkLoadInstance(instance);
}

void gl_create_surface() {
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create window surface!\n");
        exit(-1);
    }
}

VkPhysicalDevice gl_find_physical_device() {
    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, NULL);
    VkPhysicalDevice devices[device_cnt];
    vkEnumeratePhysicalDevices(instance, &device_cnt, devices);

    VkPhysicalDevice device = NULL;
    if (device_cnt > 0) {
        device = devices[0];

        for (int i = 0; i < device_cnt; i++) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i], &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                device = devices[i];
                break;
            }
        }
    }

    uint32_t fmt_cnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmt_cnt, NULL);
    VkSurfaceFormatKHR surface_fmts[fmt_cnt];
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &fmt_cnt, surface_fmts);

    bool fmt_supported = false;
    for (int i = 0; i < fmt_cnt; i++) {
        if (surface_fmts[i].format == swapchain_fmt) {
            fmt_supported = true;
            break;
        }
    }

    if (!fmt_supported) {
        fprintf(stderr, "Swapchain format is not supported by surface!\n");
        return NULL;
    }

    return device;
}

void gl_find_graphics_queue() {
    uint32_t queue_fam_cnt = 0;
    vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &queue_fam_cnt, NULL);
    VkQueueFamilyProperties2 queue_fam_props[queue_fam_cnt];
    for (int i = 0; i < queue_fam_cnt; i++) {
        queue_fam_props[i] = (VkQueueFamilyProperties2){VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2};
    }
    vkGetPhysicalDeviceQueueFamilyProperties2(physical_device, &queue_fam_cnt, queue_fam_props);

    for (int i = 0; i < queue_fam_cnt; i++) {
        VkBool32 has_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &has_support);
        
        if (queue_fam_props[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && has_support) {
            gfx_queue_fam_idx = i;
            return;
        }
    }
    fprintf(stderr, "Could not find graphics queue!\n");
    exit(-1);
}

void gl_create_device() {
    float priority = 1.0f;
    uint32_t queue_families[1] = {
        gfx_queue_fam_idx
    };

    VkDeviceQueueCreateInfo gfx_queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = gfx_queue_fam_idx,
        .queueCount = 1,
        .pQueuePriorities = &priority
    };

    VkPhysicalDeviceVulkan14Features supported_features14 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = NULL
    };
    VkPhysicalDeviceVulkan13Features supported_features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &supported_features14
    };
    VkPhysicalDeviceVulkan12Features supported_features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &supported_features13
    };

    VkPhysicalDeviceFeatures2 supported_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, 
        .pNext = &supported_features12 
    };
    vkGetPhysicalDeviceFeatures2(physical_device, &supported_features);

    if (!supported_features13.dynamicRendering || !supported_features13.synchronization2 || !supported_features12.timelineSemaphore) {
        fprintf(stderr, "Physical device does not meet feature requirements!\n");
        exit(-1);
    }

    VkPhysicalDeviceVulkan14Features features14 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
        .pNext = NULL
    };
    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features14,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE
    };
    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .timelineSemaphore = VK_TRUE
    };

    VkPhysicalDeviceFeatures2 features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, 
        .pNext = &features12
    };

    const char *device_extensions[1] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    VkDeviceCreateInfo dev_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &gfx_queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = NULL
    };

    if (vkCreateDevice(physical_device, &dev_create_info, NULL, &device) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create device!\n");
        exit(-1);
    }

    vkGetDeviceQueue(device, gfx_queue_fam_idx, 0, &gfx_queue);
    if (!gfx_queue) {
        fprintf(stderr, "Cannot get graphics queue\n!");
        exit(-1);
    }
}

void gl_init_vma() {
    
}

void gl_init() {
    gl_create_instance();
    gl_create_surface();
    physical_device = gl_find_physical_device();
    gl_find_graphics_queue();
    gl_create_device();
    volkLoadDevice(device);
}
