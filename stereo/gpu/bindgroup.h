#pragma once

#include <stereo/defs.h>

namespace stereo {

struct BindGroup {
private:
    wgpu::BindGroup _bindgroup = nullptr;
    
public:
    
    BindGroup() = default;
    BindGroup(wgpu::BindGroup&& bindgroup);
    BindGroup(const BindGroup& other);
    BindGroup(BindGroup&& other);
    
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
    
    ~BindGroupLayout();
    
    BindGroupLayout& operator=(const BindGroupLayout& other);
    BindGroupLayout& operator=(BindGroupLayout&& other);
    
    operator wgpu::BindGroupLayout() const { return _layout; }
    operator WGPUBindGroupLayout()   const { return _layout; }
    
};

}  // namespace stereo
