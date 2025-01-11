#pragma once

#include <stereo/defs.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
namespace stereo {

struct Window {
    GLFWwindow*          window         = nullptr;
    wgpu::Instance       instance       = nullptr;
    wgpu::Device         device         = nullptr;
    wgpu::Queue          queue          = nullptr;
    wgpu::Surface        surface        = nullptr;
	wgpu::TextureFormat  surface_format = wgpu::TextureFormat::Undefined;
	vec2u                dims           = {1024, 1024};
    
    Window(wgpu::Instance instance, wgpu::Device device, vec2u dims);
    ~Window();
    
    void init();
    void release();
    wgpu::TextureView next_target();
};

}  // namespace stereo
