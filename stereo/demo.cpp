#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>
#include <webgpu/glfw3webgpu.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    // Init WebGPU
    WGPUInstanceDescriptor desc;
    desc.nextInChain = NULL;
    WGPUInstance instance = wgpuCreateInstance(&desc);
    
    // Init GLFW
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
    
    // Here we create our WebGPU surface from the window!
    WGPUSurface surface = glfwGetWGPUSurface(instance, window);
    printf("surface = %p", surface);
    
    // Terminate GLFW
    while (!glfwWindowShouldClose(window)) glfwPollEvents();
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
