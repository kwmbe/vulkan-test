cmake_minimum_required(VERSION 3.29)
project(HelloTriangleApplication)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package (glfw3 REQUIRED)
find_package (Vulkan REQUIRED)

add_executable(HelloTriangleApplication main.cpp)

target_link_libraries(HelloTriangleApplication PRIVATE
    glfw
    Vulkan::Vulkan
)

target_compile_features(HelloTriangleApplication PRIVATE cxx_std_17)
