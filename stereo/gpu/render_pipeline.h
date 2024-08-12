#pragma once

#include <stereo/defs.h>

// todo: very few overrides for settings exposed now,
//   but we should add this as needed (e.g. with a Settings class having
//   optional fields).

namespace stereo {

struct RenderPipeline {
    wgpu::Device            _device   = nullptr;
    wgpu::RenderPipeline    _pipeline = nullptr;
    wgpu::PrimitiveTopology _topology = wgpu::PrimitiveTopology::TriangleList;
    
    RenderPipeline(
        wgpu::Device device,
        wgpu::ShaderModule shader,
        wgpu::PrimitiveTopology topology,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts);
    
    RenderPipeline(const RenderPipeline& other) = delete;
    RenderPipeline(RenderPipeline&& other) = delete;
    
    ~RenderPipeline();
    
    RenderPipeline& operator=(const RenderPipeline& other) = delete;
    RenderPipeline& operator=(RenderPipeline&& other)      = delete;
    
    void _init(
        wgpu::ShaderModule shader,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts
    );
    
    void _release();
    
    wgpu::RenderPipeline pipeline() const;
};

} // namespace stereo
