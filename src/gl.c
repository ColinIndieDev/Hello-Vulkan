#include "gl.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#include <vk_mem_alloc.h>
#include <shaderc/shaderc.h>

#include <cpstd/vector.h>
#include <cpstd/mathplus.h>
#include <cpstd/arena.h>

#define VULKAN_VERSION VK_API_VERSION_1_4
#define MAX_FRAME_IN_FLIGHT 2

GLFWwindow *window = NULL;
const char *title = "Hello World";
uint32_t scr_width = 800, scr_height = 800;
bool vsync = true;

mat4f projection2D;

color_t bg_color_norm = RGB(0, 0, 0);

vertex *vertices = NULL;
object_instance *instances = NULL;
uint32_t world_vertices_cnt = 0;
uint32_t world_instances_cnt = 0;

void enable_vsync(bool enabled) {
    vsync = enabled;
}

// {{{ Logging

void log_msg(severity_t severtiy, const char *msg, ...) {
    char buffer[KiB(1)];
    va_list args;
    va_start(args, msg);
    vsnprintf(buffer, 1024, msg, args);
    va_end(args);

    time_t now = time(NULL);
    struct tm *time = localtime(&now); 

    switch (severtiy) {
    case SEVERITY_INFO:
        printf("[%02d:%02d:%02d INFO]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    case SEVERITY_WARNING:
        fprintf(stderr, "[%02d:%02d:%02d WARNING]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    case SEVERITY_ERROR:
        fprintf(stderr, "[%02d:%02d:%02d ERROR]: %s", time->tm_hour, time->tm_min, time->tm_sec, buffer);
        break;
    }
}

// }}}

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

// {{{ Vulkan Variables

VkInstance instance = NULL;
VkSurfaceKHR surface = NULL;
VkFormat depth_fmt = VK_FORMAT_D32_SFLOAT;
VkFormat swapchain_fmt = VK_FORMAT_B8G8R8_SRGB;
uint64_t frame_counter = 0;

VkCommandPool command_pool = NULL;

VkPhysicalDevice physical_device = NULL;
VkDevice device = NULL;

uint32_t gfx_queue_fam_idx = UINT32_MAX;
VkQueue gfx_queue = NULL;

VmaAllocator allocator = NULL;

uint32_t img_cnt = 0;
bool req_swapchain_recreate = false;
VkSwapchainKHR swapchain = NULL;
uint32_t swapchain_width = 0, swapchain_height = 0;
VkImage *swapchain_imgs = NULL;
VkImageView *swapchain_img_views = NULL;
VkSemaphore *render_complete_sems = NULL;

VkImage depth_img = NULL;
VkImageView depth_img_view = NULL;
VmaAllocation depth_img_alloc = NULL;

VkShaderModule vert_shader = NULL;
VkShaderModule frag_shader = NULL;

VkPipelineLayout pipeline_layout = NULL;
VkPipeline pipeline = NULL;

VkSemaphore timeline_sem = NULL;
uint64_t timeline_val = 0;
frame_resources_t frame_resources[MAX_FRAME_IN_FLIGHT] = {
    {
        .buffer = NULL,
        .pool = NULL,
        .img_aquired_sem = NULL
    },
    {
        .buffer = NULL,
        .pool = NULL,
        .img_aquired_sem = NULL
    }
};
frame_resources_t *cur_res = NULL;
uint32_t cur_img_idx = 0;
uint64_t cur_frame_id = 0; 

VkBuffer vertex_buffer = NULL;
VmaAllocation vertex_buffer_alloc = NULL;
uint64_t vertex_buffer_addr = 0;

VkBuffer instances_buffer = NULL;
VmaAllocation instances_buffer_alloc = NULL;
uint64_t instances_buffer_addr = 0;

// }}}

// {{{ Window Init

// {{{ vulkan_init() Helper

void vulkan_create_instance() {
    if (volkInitialize() != VK_SUCCESS) {
        log_msg(SEVERITY_ERROR, "Failed to init Volk!\n");
        exit(-1);
    }
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = title,
        .applicationVersion = VULKAN_VERSION,
        .apiVersion = VULKAN_VERSION
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
        log_msg(SEVERITY_ERROR, "Failed to create Vulkan instance!\n");
        exit(-1);
    }

    volkLoadInstance(instance);
    log_msg(SEVERITY_INFO, "Created Vulkan instance!\n");
}

void vulkan_create_surface() {
    if (glfwCreateWindowSurface(instance, window, NULL, &surface) != VK_SUCCESS) {
        log_msg(SEVERITY_ERROR, "Failed to create window surface!\n");
        exit(-1);
    }
    log_msg(SEVERITY_INFO, "Created window surface!\n");
}

// {{{ Device related

void vulkan_find_physical_device() {
    uint32_t device_cnt = 0;
    vkEnumeratePhysicalDevices(instance, &device_cnt, NULL);
    VkPhysicalDevice devices[device_cnt];
    vkEnumeratePhysicalDevices(instance, &device_cnt, devices);

    if (device_cnt > 0) {
        physical_device = devices[0];

        for (int i = 0; i < device_cnt; i++) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(devices[i], &props);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                physical_device = devices[i];
                break;
            }
        }
    }

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    log_msg(SEVERITY_INFO, "Found GPU: %s!\n", props.deviceName);

    uint32_t fmt_cnt = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &fmt_cnt, NULL);
    VkSurfaceFormatKHR surface_fmts[fmt_cnt];
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &fmt_cnt, surface_fmts);

    if (fmt_cnt == 0) {
        log_msg(SEVERITY_ERROR, "No surface formats are supported by GPU!\n");
        exit(-1);
    }

    VkSurfaceFormatKHR *formats = malloc(sizeof(VkSurfaceFormatKHR) * fmt_cnt);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &fmt_cnt, formats);

    bool format_found = false;
    for (uint32_t i = 0; i < fmt_cnt; i++) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB || 
            formats[i].format == VK_FORMAT_R8G8B8A8_SRGB ||
            formats[i].format == VK_FORMAT_B8G8R8_SRGB) {
        
            swapchain_fmt = formats[i].format;
            format_found = true;
            break;
        }
    }

    if (!format_found) {
        swapchain_fmt = formats[0].format;
    }

    free(formats);
    log_msg(SEVERITY_INFO, "Chosen surface format %d!\n", swapchain_fmt);

    bool fmt_supported = false;
    for (int i = 0; i < fmt_cnt; i++) {
        if (surface_fmts[i].format == swapchain_fmt) {
            fmt_supported = true;
            break;
        }
    }

    if (!fmt_supported) {
        log_msg(SEVERITY_ERROR, "Swapchain format is not supported by surface!\n");
        exit(-1);
    }
}

void vulkan_find_graphics_queue() {
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
    log_msg(SEVERITY_ERROR, "Could not find graphics queue!\n");
    exit(-1);
}

void vulkan_create_device() {
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

    if (!supported_features13.dynamicRendering || !supported_features13.synchronization2 || 
        !supported_features12.timelineSemaphore || !supported_features12.scalarBlockLayout || !supported_features12.bufferDeviceAddress) {
        log_msg(SEVERITY_ERROR, "Physical device does not meet feature requirements!\n");
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
        .timelineSemaphore = VK_TRUE,
        .scalarBlockLayout = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE
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
        log_msg(SEVERITY_ERROR, "Failed to create device!\n");
        exit(-1);
    }

    vkGetDeviceQueue(device, gfx_queue_fam_idx, 0, &gfx_queue);
    if (!gfx_queue) {
        log_msg(SEVERITY_ERROR, "Cannot get graphics queue\n!");
        exit(-1);
    }
}

// }}}

void vulkan_init_vma() {
    VmaVulkanFunctions vma_func_info;
	VmaAllocatorCreateInfo vma_alloc_info = {
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = physical_device,
		.device = device,
		.pVulkanFunctions = &vma_func_info,
		.instance = instance,
		.vulkanApiVersion = VULKAN_VERSION
	};

	vmaImportVulkanFunctionsFromVolk(&vma_alloc_info, &vma_func_info);

	if (vmaCreateAllocator(&vma_alloc_info, &allocator) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Cannot create allocator!\n");
        exit(-1);
	}
    log_msg(SEVERITY_INFO, "Created VMA allocator!\n");
}

// {{{ Swapchain

void vulkan_create_swapchain(uint32_t w, uint32_t h) {
    swapchain_width = w;
	swapchain_height = h;

	VkSurfaceCapabilitiesKHR surface_caps;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_caps) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Cannot get surface capabilities!\n");
		exit(-1);
	}

	VkSwapchainCreateInfoKHR swapchain_create_info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = surface,
		.minImageCount = surface_caps.minImageCount,
		.imageFormat = swapchain_fmt,
		.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
		.imageExtent = {
            .width = swapchain_width, 
            .height = swapchain_height 
        },
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR
	};

	if (vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Error creating swapchain!\n");
		exit(-1);
	}

	vkGetSwapchainImagesKHR(device, swapchain, &img_cnt, NULL);
	swapchain_imgs = malloc(img_cnt * sizeof(VkImage));
	vkGetSwapchainImagesKHR(device, swapchain, &img_cnt, swapchain_imgs);
	swapchain_img_views = malloc(img_cnt * sizeof(VkImageView));

	for (size_t i = 0; i < img_cnt; ++i) {
		VkImageViewCreateInfo img_view_info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = swapchain_imgs[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchain_fmt,
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		};

		if (vkCreateImageView(device, &img_view_info, NULL, &swapchain_img_views[i]) != VK_SUCCESS) {
			log_msg(SEVERITY_ERROR, "Error creating swapchain image view!\n");
			exit(-1);
		}
	}

	render_complete_sems = malloc(img_cnt * sizeof(VkSemaphore));
	for (int i = 0; i < img_cnt; i++) {
		VkSemaphoreCreateInfo sem_info = { 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 
        };
		if (vkCreateSemaphore(device, &sem_info, NULL, &render_complete_sems[i]) != VK_SUCCESS) {
			log_msg(SEVERITY_ERROR, "Error creating render-complete semaphore!\n");
			exit(-1);
		}
	}

	VkImageCreateInfo depth_create_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType = VK_IMAGE_TYPE_2D,
		.format = depth_fmt,
		.extent = {
            .width = swapchain_width, 
            .height = swapchain_height, 
            .depth = 1 
        },
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
	};

	VmaAllocationCreateInfo alloc_info = {
		.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
		.usage = VMA_MEMORY_USAGE_AUTO
	};
	if (vmaCreateImage(allocator, &depth_create_info, &alloc_info, &depth_img, &depth_img_alloc, NULL) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Error allocating depth image!\n");
		exit(-1);
	}

	VkImageViewCreateInfo depth_img_view_info = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image = depth_img,
		.viewType = VK_IMAGE_VIEW_TYPE_2D,
		.format = depth_fmt,
		.subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, 
            .levelCount = 1, 
            .layerCount = 1
        }
	};
	if (vkCreateImageView(device, &depth_img_view_info, NULL, &depth_img_view) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Error creating depth image view!\n");
		exit(-1);
	}
}

void vulkan_destroy_swapchain() {
    for (int i = 0; i < img_cnt; i++) {
		vkDestroyImageView(device, swapchain_img_views[i], NULL);
	}
	free(swapchain_img_views);

	for (int i = 0; i < img_cnt; i++) {
		vkDestroySemaphore(device, render_complete_sems[i], NULL);
	}
	free(render_complete_sems);

	if (swapchain) {
		vkDestroySwapchainKHR(device, swapchain, NULL);
		swapchain = NULL;
	}

	if (depth_img_view) {
		vkDestroyImageView(device, depth_img_view, NULL);
		vmaDestroyImage(allocator, depth_img, depth_img_alloc);
		depth_img_view = NULL;
	}
}

// }}}

// {{{ Shader

char *vulkan_shader_read_file(char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    unsigned int size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    unsigned int read = fread(buffer, 1, size, f);
    fclose(f);

    if (read != size) {
        free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

VkShaderModule vulkan_create_shader_mod(char *path, char *name, shaderc_shader_kind kind) {
    char *final_path = malloc(strlen(path) + strlen(name) + 1);
    strcpy(final_path, path);
    strcat(final_path, name);
    const char *src = vulkan_shader_read_file(final_path);

    if (!src) {
        log_msg(SEVERITY_ERROR, "Cannot find shader src file: %s!\n", final_path);
        return NULL;
    }

    log_msg(SEVERITY_INFO, "Compiling shader: %s!\n", final_path);
    free(final_path);

	shaderc_compiler_t compiler = shaderc_compiler_initialize();;
	shaderc_compile_options_t opts = shaderc_compile_options_initialize();
    shaderc_compile_options_set_target_env(opts, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
    shaderc_compile_options_set_target_spirv(opts, shaderc_spirv_version_1_6);
    shaderc_compile_options_set_optimization_level(opts, shaderc_optimization_level_performance);

    shaderc_compilation_result_t res = shaderc_compile_into_spv(compiler, src, strlen(src), kind, name, "main", opts);

	if (shaderc_result_get_compilation_status(res) != shaderc_compilation_status_success) {
		log_msg(SEVERITY_ERROR, "Shader Compilation Error: %s!\n", shaderc_result_get_error_message(res));
		return NULL;
	}

    size_t byte_len = shaderc_result_get_length(res);
    const uint32_t *spv = (const uint32_t *)shaderc_result_get_bytes(res);

	VkShaderModuleCreateInfo mod_create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = byte_len,
		.pCode = spv
	};
	VkShaderModule shader_mod = NULL;
	if (vkCreateShaderModule(device, &mod_create_info, NULL, &shader_mod) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Error creating shader module!\n");
		return NULL;
	}
    log_msg(SEVERITY_INFO, "Finished compiling shader module!\n");
	return shader_mod;
}

void vulkan_create_shader(char *vert_path, char *vert_name, char *frag_path, char *frag_name) {
    vert_shader = vulkan_create_shader_mod(vert_path, vert_name, shaderc_vertex_shader);
    frag_shader = vulkan_create_shader_mod(frag_path, frag_name, shaderc_fragment_shader);

    if (!vert_shader || !frag_shader) {
        log_msg(SEVERITY_ERROR, "Error creating shader!\n");
        exit(-1);
    }
    log_msg(SEVERITY_INFO, "Created shader!\n");
}

// }}}

// {{{ Pipeline

void vulkan_create_gfx_pipeline() {
	char *entry_point = "main";
	VkPipelineShaderStageCreateInfo shader_stages[2] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_shader,
			.pName = entry_point
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag_shader,
			.pName = entry_point
		}
	};

	VkPipelineVertexInputStateCreateInfo vert_in_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	VkPipelineInputAssemblyStateCreateInfo in_assembly_info =  {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
	};

	VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_FALSE,
		.depthWriteEnable = VK_TRUE,
		.depthCompareOp = VK_COMPARE_OP_LESS,
		.stencilTestEnable = VK_FALSE
	};

	VkPipelineViewportStateCreateInfo viewport_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = NULL,
		.scissorCount = 1,
		.pScissors = NULL
	};

	VkPipelineRasterizationStateCreateInfo raster_info =  {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_CLOCKWISE,
		.lineWidth = 1.0f
	};

	VkPipelineMultisampleStateCreateInfo multisample_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	VkPipelineColorBlendAttachmentState attach_state = {
		.blendEnable = VK_FALSE,
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
	};
	VkPipelineColorBlendStateCreateInfo blend_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.attachmentCount = 1,
		.pAttachments = &attach_state
	};

	VkDynamicState dynamic_state[2] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamic_state_info =  {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = dynamic_state
	};

	VkPipelineRenderingCreateInfo render_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount = 1,
		.pColorAttachmentFormats = &swapchain_fmt,
		.depthAttachmentFormat = depth_fmt,
	};

    // Push constants
    VkPushConstantRange push_const_range = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(shader_push_consts_t) + 8 // idk some shit padding
    };

	VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 0,
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &push_const_range
	};
	if (vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &pipeline_layout) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Unable to create pipeline layout!\n");
		exit(-1);
	}

	VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.pNext = &render_info,
		.stageCount = 2,
		.pStages = shader_stages,
		.pVertexInputState = &vert_in_info,
		.pInputAssemblyState = &in_assembly_info,
		.pViewportState = &viewport_info,
		.pRasterizationState = &raster_info,
		.pMultisampleState = &multisample_info,
		.pDepthStencilState = &depth_stencil_info,
		.pColorBlendState = &blend_info,
		.pDynamicState = &dynamic_state_info,
		.layout = pipeline_layout,
		.renderPass = VK_NULL_HANDLE,
	};
	if (vkCreateGraphicsPipelines(device, NULL, 1, &pipeline_info, NULL, &pipeline) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Error creating pipeline!\n");
	}
    log_msg(SEVERITY_INFO, "Created pipeline!\n");
}

// }}}

void vulkan_create_sync_resources() {
    VkSemaphoreTypeCreateInfo sem_type_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
		.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
		.initialValue = timeline_val
	};
    {
        VkSemaphoreCreateInfo sem_info =  {
        	.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        	.pNext = &sem_type_info
        };
        if (vkCreateSemaphore(device, &sem_info, NULL, &timeline_sem) != VK_SUCCESS) {
        	log_msg(SEVERITY_ERROR, "Unable to create timeline semaphore!\n");
        	exit(-1);
        }
    }

	for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
		VkSemaphoreCreateInfo sem_info = { 
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO 
        };
		if (vkCreateSemaphore(device, &sem_info, NULL, &frame_resources[i].img_aquired_sem) != VK_SUCCESS) {
			log_msg(SEVERITY_ERROR, "Error creating per-frame image-acquire semaphore!\n");
			exit(-1);
		}
	}
    log_msg(SEVERITY_INFO, "Created sync resources!\n");
}

void vulkan_create_command_buffers() {
    VkCommandPoolCreateInfo poolInfo = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.queueFamilyIndex = gfx_queue_fam_idx
	};
	if (vkCreateCommandPool(device, &poolInfo, NULL, &command_pool) != VK_SUCCESS) {
		log_msg(SEVERITY_ERROR, "Unable to create command buffer pool!\n");
		exit(-1);
    }

	for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
		VkCommandPoolCreateInfo pool_info =  {
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = gfx_queue_fam_idx
		};
		if (vkCreateCommandPool(device, &poolInfo, NULL, &frame_resources[i].pool) != VK_SUCCESS) {
			log_msg(SEVERITY_ERROR, "Unable to create command buffer pool!\n");
			exit(-1);
		}

		VkCommandBufferAllocateInfo cmd_alloc_info = {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = frame_resources[i].pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};
		if (vkAllocateCommandBuffers(device, &cmd_alloc_info, &frame_resources[i].buffer) != VK_SUCCESS) {
			log_msg(SEVERITY_ERROR, "Unable to allocate command buffer!\n");
			exit(-1);
		}
	}
    log_msg(SEVERITY_INFO, "Created command buffers!\n");
}

// }}}

void vulkan_init() {
    vulkan_create_instance();
    vulkan_create_surface();
    vulkan_find_physical_device();
    vulkan_find_graphics_queue();
    vulkan_create_device();
    volkLoadDevice(device);
    vulkan_init_vma();

    log_msg(SEVERITY_INFO, "VSync is set to %s!\n", vsync ? "true" : "false");
    vulkan_create_swapchain(scr_width, scr_height);
    log_msg(SEVERITY_INFO, "Created swapchain!\n");

    vulkan_create_shader("shaders/", "shader.vert", "shaders/", "shader.frag");

    vulkan_create_gfx_pipeline();
    vulkan_create_sync_resources();
    vulkan_create_command_buffers();
    log_msg(SEVERITY_INFO, "Initialized Vulkan!\n");
}

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

    vulkan_init();

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

// {{{ window_close() Helper

void vulkan_destroy() {
    if (device) {
        vkDeviceWaitIdle(device);
    }

    for (int i = 0; i < MAX_FRAME_IN_FLIGHT; i++) {
        if (frame_resources[i].pool) {
            vkDestroyCommandPool(device, frame_resources[i].pool, NULL);
            frame_resources[i].pool = NULL;
        }
        if (frame_resources[i].img_aquired_sem) {
            vkDestroySemaphore(device, frame_resources[i].img_aquired_sem, NULL);
            frame_resources[i].img_aquired_sem = NULL;
        }
    }

    if (command_pool) {
        vkDestroyCommandPool(device, command_pool, NULL);
        command_pool = NULL;
    }

    if (timeline_sem) {
        vkDestroySemaphore(device, timeline_sem, NULL);
        timeline_sem = NULL;
    }

    if (pipeline) {
        vkDestroyPipeline(device, pipeline, NULL);
        pipeline = NULL;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(device, pipeline_layout, NULL);
        pipeline_layout = NULL;
    }

    if (vert_shader) {
        vkDestroyShaderModule(device, vert_shader, NULL);
        vert_shader = NULL;
    }
    if (frag_shader) {
        vkDestroyShaderModule(device, frag_shader, NULL);
        frag_shader = NULL;
    }

    vulkan_destroy_swapchain();
    free(swapchain_imgs);

    if (vertex_buffer) {
        vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_alloc);
        vertex_buffer = NULL;
    }
    if (instances_buffer) {
        vmaDestroyBuffer(allocator, instances_buffer, instances_buffer_alloc);
        vertex_buffer = NULL;
    }

    if (allocator) {
        vmaDestroyAllocator(allocator);
        allocator = NULL;
    }

    if (device) {
        vkDestroyDevice(device, NULL);
        device = NULL;
    }

    if (surface) {
        vkDestroySurfaceKHR(instance, surface, NULL);
        surface = NULL;
    }
    if (instance) {
        vkDestroyInstance(instance, NULL);
        instance = NULL;
    }

    volkFinalize();
    log_msg(SEVERITY_INFO, "Closing Vulkan!\n");
}

// }}}

void window_close() {
    vulkan_destroy();

    vec_destroy(vertices);
    vec_destroy(instances);

    glfwDestroyWindow(window);
    glfwTerminate();
    log_msg(SEVERITY_INFO, "Closing window and terminating!\n");
}

void clear_background(color_t color) {
    bg_color_norm = NORM_COLOR(color);
}

// {{{ Buffers

void vulkan_create_vertex_buffer(vertex *verts, size_t cnt) {
    if (vertex_buffer) {
        vmaDestroyBuffer(allocator, vertex_buffer, vertex_buffer_alloc);
    }

    VkDeviceSize buffer_size = sizeof(vertex) * cnt;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    VmaAllocationInfo alloc_res_info;
    if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &vertex_buffer, &vertex_buffer_alloc, &alloc_res_info) != VK_SUCCESS) {
        log_msg(SEVERITY_ERROR, "Failed to create vertex buffer!\n");
        exit(-1);
    }
    memcpy(alloc_res_info.pMappedData, verts, buffer_size);
    VkBufferDeviceAddressInfo addr_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vertex_buffer
    };
    vertex_buffer_addr = vkGetBufferDeviceAddress(device, &addr_info);
}

void vulkan_create_object_instance_buffer(object_instance *insts, size_t cnt) {
    if (insts) {
        vmaDestroyBuffer(allocator, instances_buffer, instances_buffer_alloc);
    }

    VkDeviceSize buffer_size = sizeof(object_instance) * cnt;

    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = buffer_size,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT
    };

    VmaAllocationInfo alloc_res_info;
    if (vmaCreateBuffer(allocator, &buffer_info, &alloc_info, &instances_buffer, &instances_buffer_alloc, &alloc_res_info) != VK_SUCCESS) {
        log_msg(SEVERITY_ERROR, "Failed to create vertex buffer!\n");
        exit(-1);
    }
    memcpy(alloc_res_info.pMappedData, insts, buffer_size);
    VkBufferDeviceAddressInfo addr_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = instances_buffer
    };
    instances_buffer_addr = vkGetBufferDeviceAddress(device, &addr_info);
}

// }}}

void draw_rect(vec2f pos, vec2f size, color_t color, float rotation) {
    color_t norm_color = NORM_COLOR(color);

    vertex rectangle[6] = {
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

    object_instance obj = {
        .color = norm_color
    };
    memcpy(obj.transform, transform, sizeof(mat4f));
    vec_push(instances, obj);
    world_instances_cnt++;
}

void draw_circle(vec2f pos, float radius, color_t color) {
    color_t norm_color = NORM_COLOR(color);

    float max_error = 0.25f; 
    float segments_cnt = MATH_PI / acosf(1.0f - (max_error / radius));
    uint32_t segments = (uint32_t)(ceilf(segments_cnt));
    math_clamp(segments, 8, 128);
    uint32_t vertices_cnt = segments * 3;
    vertex circle[vertices_cnt];
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

    object_instance obj = {
        .color = norm_color
    };
    memcpy(obj.transform, transform, sizeof(mat4f));
    vec_push(instances, obj);
    world_instances_cnt++;
}

// {{{ Begin draw

void begin_draw() {
    // Update

    // Clear vertex buffer
    vec_clear(vertices);
    world_vertices_cnt = 0;
    vec_clear(instances);
    world_instances_cnt = 0;

    // Update swapchain if resize
    if (req_swapchain_recreate) {
        vkDeviceWaitIdle(device);
        vulkan_destroy_swapchain();
        vulkan_create_swapchain(scr_width, scr_height);
        req_swapchain_recreate = false;
    }

    // Update and get frame
    uint32_t frame_res_idx = frame_counter++ % MAX_FRAME_IN_FLIGHT;
    cur_frame_id = ++timeline_val; 
    uint64_t wait_for_id = cur_frame_id - MAX_FRAME_IN_FLIGHT;

    if (cur_frame_id > MAX_FRAME_IN_FLIGHT) {
        VkSemaphoreWaitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &timeline_sem,
            .pValues = &wait_for_id
        };

        vkWaitSemaphores(device, &wait_info, UINT64_MAX);
    }

    cur_res = &frame_resources[frame_res_idx];
    vkResetCommandPool(device, cur_res->pool, 0);

    cur_img_idx = 0;
    VkResult acquire_result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, cur_res->img_aquired_sem, VK_NULL_HANDLE, &cur_img_idx);

    if (acquire_result == VK_ERROR_OUT_OF_DATE_KHR) {
        req_swapchain_recreate = true;
        return;
    }
    if (acquire_result == VK_SUBOPTIMAL_KHR) {
        req_swapchain_recreate = true;
    }

    // Begin recording
    VkCommandBufferBeginInfo cmd_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cur_res->buffer, &cmd_begin_info);

    // Image layout barriers
    VkImageMemoryBarrier2 layout_barriers[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = swapchain_imgs[cur_img_idx],
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        },
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .image = depth_img,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        }
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 2,
        .pImageMemoryBarriers = layout_barriers
    };
    vkCmdPipelineBarrier2(cur_res->buffer, &dep_info);

    // Dynamic rendering
    VkRenderingAttachmentInfo color_attach_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain_img_views[cur_img_idx],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = { 
            .color = { 
                { bg_color_norm.r, bg_color_norm.g, bg_color_norm.b, bg_color_norm.a } 
            } 
        }
    };

    VkRenderingAttachmentInfo depth_attach_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth_img_view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = { 
            .depthStencil = { 
                1.0f, 
                0 
            } 
        }
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 
                .x = 0, 
                .y = 0 
            },
            .extent = { 
                .width = swapchain_width, 
                .height = swapchain_height 
            }
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attach_info,
        .pDepthAttachment = &depth_attach_info
    };

    vkCmdBeginRendering(cur_res->buffer, &rendering_info);
}

// }}}

// {{{ End draw

void end_draw() {
    {
        VkViewport viewport = {
            .x = 0, .y = 0,
            .width = (float)swapchain_width,
            .height = (float)swapchain_height,
            .minDepth = 0.0f, .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cur_res->buffer, 0, 1, &viewport);

        VkRect2D scissor = {
            .offset = { 
                .x = 0, 
                .y = 0 
            },
            .extent = { 
                .width = swapchain_width, 
                .height = swapchain_height }
        };
        vkCmdSetScissor(cur_res->buffer, 0, 1, &scissor);

        vkCmdBindPipeline(cur_res->buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        if (world_vertices_cnt > 0 && world_instances_cnt > 0) {
            vulkan_create_vertex_buffer(vertices, world_vertices_cnt);
            vulkan_create_object_instance_buffer(instances, world_instances_cnt);
        }

        shader_push_consts_t shader_push_consts = {
            .vertex_buffer_addr = vertex_buffer_addr,
            .instance_buffer_addr = instances_buffer_addr
        };
        memcpy(&shader_push_consts.projection, &projection2D, sizeof(mat4f));

        vkCmdPushConstants(cur_res->buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shader_push_consts_t), &shader_push_consts);

        if (world_vertices_cnt > 0) {
            vkCmdDraw(cur_res->buffer, world_vertices_cnt, 1, 0, 0);
        }
    }
    vkCmdEndRendering(cur_res->buffer);

    // Make image ready to draw
    VkImageMemoryBarrier2 present_layout_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = swapchain_imgs[cur_img_idx],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        }
    };
    VkDependencyInfo present_dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &present_layout_barrier
    };
    vkCmdPipelineBarrier2(cur_res->buffer, &present_dep_info);

    vkEndCommandBuffer(cur_res->buffer);

    // GFX queue submission
    VkSemaphoreSubmitInfo image_acquire_wait_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
        .semaphore = cur_res->img_aquired_sem,
        .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT
    };

    VkSemaphoreSubmitInfo semaphore_signals[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = render_complete_sems[cur_img_idx],
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = timeline_sem,
            .value = cur_frame_id,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
        }
    };

    VkCommandBufferSubmitInfo cmd_submit_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = cur_res->buffer,
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = &image_acquire_wait_info,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmd_submit_info,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos = semaphore_signals
    };
    vkQueueSubmit2(gfx_queue, 1, &submit_info, VK_NULL_HANDLE);

    // Draw to screen
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &render_complete_sems[cur_img_idx],
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &cur_img_idx
    };
    vkQueuePresentKHR(gfx_queue, &present_info);

    // GLFW
    glfwPollEvents();
}

// }}}
