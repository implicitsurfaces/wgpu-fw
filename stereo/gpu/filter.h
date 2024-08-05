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

private:
    
    wgpu::BindGroup _src_bindgroup = nullptr;
    std::vector<wgpu::BindGroup> _dst_bind_groups;
    
    void _init();
    void _release();
    
public:
    
    FilteredTexture() = default;
    FilteredTexture(wgpu::Texture source, wgpu::Device device, Filter3x3& filter);
    FilteredTexture(const FilteredTexture&);
    FilteredTexture(FilteredTexture&&);
    ~FilteredTexture();
    
    FilteredTexture& operator=(const FilteredTexture&);
    FilteredTexture& operator=(FilteredTexture&&);
    
    wgpu::BindGroup source_bindgroup();
    wgpu::BindGroup target_bindgroup(size_t level);
    
    void process();
};

struct Filter3x3 {
    
    static constexpr uint32_t max_mip_levels = 24;
    
private:
    wgpu::Device          _device             = nullptr;
    // uniforms
    wgpu::Buffer          _uniform_buffer     = nullptr;
    // bind group layouts
    wgpu::BindGroupLayout _src_layout         = nullptr;
    wgpu::BindGroupLayout _dst_layout         = nullptr;
    wgpu::BindGroupLayout _uniform_layout     = nullptr;
    // bind group
    wgpu::BindGroup       _uniform_bind_group = nullptr;
    // pipeline
    wgpu::ComputePipeline _pipeline           = nullptr;
    
    void _init();
    void _release();
    
    friend class FilteredTexture;
    
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
