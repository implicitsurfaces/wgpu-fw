#pragma once

#include <stereo/defs.h>

namespace stereo {
    
// fwd decl
struct BindGroupLayout;


struct BindGroup {
private:
    wgpu::BindGroup _bindgroup = nullptr;
    
public:
    
    BindGroup() = default;
    BindGroup(wgpu::BindGroup&& bindgroup);
    BindGroup(const BindGroup& other);
    BindGroup(BindGroup&& other);
    BindGroup(
        wgpu::Device device,
        BindGroupLayout& layout,
        std::initializer_list<wgpu::BindGroupEntry> entries,
        std::string_view label
    );
    
    ~BindGroup();
    
    BindGroup& operator=(const BindGroup& other);
    BindGroup& operator=(BindGroup&& other);
    
    static BindGroup create(
        wgpu::Device device,
        wgpu::BindGroupDescriptor descriptor);
    
    operator wgpu::BindGroup() const { return _bindgroup; }
    operator WGPUBindGroup()   const { return _bindgroup; }
    
};

struct BindGroupLayout {
private:
    wgpu::BindGroupLayout _layout = nullptr;
    
public:
    
    BindGroupLayout() = default;
    BindGroupLayout(wgpu::BindGroupLayout&& bindgroup);
    BindGroupLayout(const BindGroupLayout& other);
    BindGroupLayout(BindGroupLayout&& other);
    BindGroupLayout(
        wgpu::Device device,
        std::initializer_list<wgpu::BindGroupLayoutEntry> entries,
        std::string_view label);
    
    ~BindGroupLayout();
    
    BindGroupLayout& operator=(const BindGroupLayout& other);
    BindGroupLayout& operator=(BindGroupLayout&& other);
    
    operator wgpu::BindGroupLayout() const { return _layout; }
    operator WGPUBindGroupLayout()   const { return _layout; }
    
};

}  // namespace stereo
