#pragma once

#include <stereo/defs.h>

namespace stereo {

wgpu::ShaderModule shader_from_file(wgpu::Device device, const char* fpath);

wgpu::ShaderModule shader_from_str(wgpu::Device device, const char* source, const char* label);

} // namespace stereo
