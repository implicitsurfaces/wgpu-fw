#pragma once

#include <stereo/gpu/texture.h>

namespace stereo {

struct Filter3x3;

struct FilteredTexture {
    Texture source;
    Texture df_dx;
    Texture df_dy;
    Texture laplace;
    
    Filter3x3* filter = nullptr;
    
    FilteredTexture() = default;
    FilteredTexture(wgpu::Texture source, wgpu::Device device, Filter3x3& filter);
    
    void process();
};

struct Filter3x3 {
    
    static constexpr uint32_t max_mip_levels = 24;
    
private:
    wgpu::Device          _device            = nullptr;
    wgpu::BindGroupLayout _bind_group_layout = nullptr;
    wgpu::ComputePipeline _pipeline          = nullptr;
    wgpu::Buffer          _uniform_buffer    = nullptr;
    
    void _init();
    void _release();

public:
    
    Filter3x3(wgpu::Device device);
    Filter3x3(const Filter3x3&) = delete;
    Filter3x3(Filter3x3&&) = delete;
    ~Filter3x3();
    
    Filter3x3& operator=(const Filter3x3&) = delete;
    Filter3x3& operator=(Filter3x3&&) = delete;
    
    void apply(FilteredTexture& src);
    
    wgpu::Device device();
};

}  // namespace stereo
