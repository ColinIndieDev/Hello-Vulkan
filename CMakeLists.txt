cmake_minimum_required(VERSION 3.31)

# Name of executable
project(Example C CXX)

# {{{ ===== CMAKE SETTINGS =====

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# }}}

# {{{ ===== BUILD MODES =====

#### Debug ####

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -fno-omit-frame-pointer)
    add_compile_options(
        -fsanitize=address,undefined
        -fsanitize-recover=address
        -fno-omit-frame-pointer
        -g
        -O0
    )
    add_link_options(
        -fsanitize=address,undefined
    )
endif()

#### Dev ####

if (CMAKE_BUILD_TYPE STREQUAL "Dev")
    add_compile_options(-g -O2 -fno-omit-frame-pointer -U NDEBUG)
endif()

#### Release ####

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-g -O3 -ffast-math -DNDEBUG)
endif()

# }}}

# {{{ ===== DEPENDENCIES =====

# {{{ GLFW

if (EMSCRIPTEN)
    set(GLFW_BUILD_WSI_EMSCRIPTEN ON CACHE BOOL "" FORCE)
    set(GLFW_USE_POSIX_TIME OFF CACHE BOOL "" FORCE)
    set(GLFW_USE_EMSCRIPTEN ON CACHE BOOL "" FORCE)

    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_COCOA OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WIN32 OFF CACHE BOOL "" FORCE)
    set(GLFW_USE_OSMESA OFF CACHE BOOL "" FORCE)
endif()

include(FetchContent)
if (NOT EMSCRIPTEN)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG master
    )
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)
endif()

# }}}

# Volk
FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG        master
)

# VMA
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        master
)

FetchContent_MakeAvailable(volk vma)

# }}}

# {{{ ===== LINKING SRC =====

# Is used here but normally never do 
# it name the files manually

file(GLOB_RECURSE SOURCE_FILES CONFIGURE DEPENDS src/*.c src/*.cpp cplvk/*.c)
message(STATUS "============================================")
message(STATUS "Included files")
message(STATUS "============================================")
foreach(f IN LISTS SOURCE_FILES)
    file(RELATIVE_PATH rel ${CMAKE_SOURCE_DIR} ${f})
    message(STATUS "    ${rel}")
endforeach()
message(STATUS "============================================")

# }}}

add_executable(${CMAKE_PROJECT_NAME}
    ${SOURCE_FILES}
)

# {{{ ===== LIBRARIES =====

# pthread
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)

# Vulkan
find_package(Vulkan REQUIRED)

# shaderc
find_package(PkgConfig REQUIRED)
pkg_check_modules(SHADERC REQUIRED shaderc)

# cpstd
find_package(cpstd REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ${SHADERC_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Threads::Threads Vulkan::Vulkan ${SHADERC_LIBRARIES} volk VulkanMemoryAllocator cpstd::cpstd)

# }}}

# {{{ ===== WEB EMSCRIPTEN =====

if (NOT EMSCRIPTEN)
    set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw)
endif()

if (EMSCRIPTEN)
    set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
    set(SHADERS_DIR "${CMAKE_SOURCE_DIR}/shaders")
    set(SHADERS_CUSTOM_DIR "${CMAKE_SOURCE_DIR}/shaders_custom")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
        # Add folders with files to Web Build
        "SHELL:--preload-file ${ASSETS_DIR}@/assets"
        "SHELL:--preload-file ${SHADERS_DIR}@/shaders"

        "-sSTACK_SIZE=8mb"
	    "-sMIN_WEBGL_VERSION=2"
	    "-sMAX_WEBGL_VERSION=2"
        "-sUSE_GLFW=3"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sFORCE_FILESYSTEM=1"
    )

    # Debug
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(Example PRIVATE
            "-sSAFE_HEAP=1"
            "-gsource-map"
            "-sASSERTIONS=2"
            "-sSTACK_SIZE=1mb"
            "-sDISABLE_EXCEPTION_CATCHING=0"
            "-fexceptions"
            "-sGL_DEBUG=1"
            "-g"
        )
    endif()

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES SUFFIX ".html")
endif()

# }}}
