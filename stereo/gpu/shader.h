#pragma once

#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/buffer.h>

namespace stereo {

wgpu::ShaderModule shader_from_file(wgpu::Device device, const char* fpath);

wgpu::ShaderModule shader_from_str(wgpu::Device device, const char* source, const char* label);

wgpu::ComputePipeline create_pipeline(
        wgpu::Device          device,
        wgpu::ShaderModule    shader,
        std::initializer_list<wgpu::BindGroupLayout> layouts,
        const char*           label,
        std::initializer_list<wgpu::ConstantEntry> constants = {});


template <typename T>
wgpu::BindGroupLayoutEntry compute_r_buffer_layout(gpu_size_t binding) {
    wgpu::BindGroupLayoutEntry entry = wgpu::Default;
    entry.binding     = binding;
    entry.visibility  = wgpu::ShaderStage::Compute;
    entry.buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;
    entry.buffer.minBindingSize = sizeof(T);
    return entry;
}

template <typename T>
wgpu::BindGroupLayoutEntry compute_rw_buffer_layout(gpu_size_t binding) {
    wgpu::BindGroupLayoutEntry entry = wgpu::Default;
    entry.binding     = binding;
    entry.visibility  = wgpu::ShaderStage::Compute;
    entry.buffer.type = wgpu::BufferBindingType::Storage;
    entry.buffer.minBindingSize = sizeof(T);
    return entry;
}

template <typename T>
wgpu::BindGroupLayoutEntry uniform_layout(gpu_size_t binding, bool has_dynamic_offset=false) {
    wgpu::BindGroupLayoutEntry entry = wgpu::Default;
    entry.binding     = binding;
    entry.visibility  = wgpu::ShaderStage::Compute;
    entry.buffer.type = wgpu::BufferBindingType::Uniform;
    entry.buffer.minBindingSize = sizeof(T);
    entry.buffer.hasDynamicOffset = has_dynamic_offset;
    return entry;
}

wgpu::BindGroupLayoutEntry sampler_layout(gpu_size_t binding);

wgpu::BindGroupLayoutEntry texture_layout(gpu_size_t binding);

template <typename T>
wgpu::BindGroupEntry buffer_entry(
        gpu_size_t binding,
        DataBuffer<T>& buffer,
        gpu_size_t size,
        gpu_size_t offset=0)
{
    wgpu::BindGroupEntry entry = wgpu::Default;
    entry.binding = binding;
    entry.buffer  = buffer.buffer();
    entry.size    = sizeof(T) * size;
    entry.offset  = sizeof(T) * offset;
    return entry;
}

wgpu::BindGroupEntry sampler_entry(
    gpu_size_t binding,
    wgpu::Sampler sampler
);

} // namespace stereo
