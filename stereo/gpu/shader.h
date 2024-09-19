#pragma once

#include <stereo/defs.h>

namespace stereo {

wgpu::ShaderModule shader_from_file(wgpu::Device device, const char* fpath);

wgpu::ShaderModule shader_from_str(wgpu::Device device, const char* source, const char* label);

wgpu::ComputePipeline create_pipeline(
        wgpu::Device          device,
        wgpu::ShaderModule    shader,
        std::initializer_list<wgpu::BindGroupLayout> layouts,
        const char*           label,
        std::initializer_list<wgpu::ConstantEntry> constants = {});

} // namespace stereo
