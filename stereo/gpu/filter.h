#pragma once

#include <stereo/defs.h>

namespace stereo {

struct Filter3x3 {
    
    static constexpr uint32_t max_mip_levels = 48;
    
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
    
    void apply(
        wgpu::Texture src,
        wgpu::Texture df_dx_dst,
        wgpu::Texture df_dy_dst,
        wgpu::Texture laplace_dst
    );
    
    wgpu::Device device();
};

}  // namespace stereo
