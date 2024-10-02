#pragma once

#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/texture.h>

namespace stereo {

struct Filter3x3;
using Filter3x3Ref = std::shared_ptr<Filter3x3>;

struct FilteredTexture {
    Texture source;
    Texture df_dx;
    Texture df_dy;
    Texture laplace;

    Filter3x3Ref filter = nullptr;

private:

    BindGroup _src_bindgroup;
    std::vector<BindGroup> _dst_bind_groups;

    void _init();

public:

    FilteredTexture() = default;
    FilteredTexture(Texture source, wgpu::Device device, Filter3x3Ref filter);

    BindGroup source_bindgroup();
    BindGroup target_bindgroup(size_t level);

    void process();
};


struct Filter3x3 {

    static constexpr uint32_t max_mip_levels = 24;

private:
    wgpu::Device    _device = nullptr;
    // uniforms
    wgpu::Buffer    _uniform_buffer = nullptr;
    // bind group layouts
    BindGroupLayout _src_layout;
    BindGroupLayout _dst_layout;
    BindGroupLayout _uniform_layout;
    // bind group
    BindGroup       _uniform_bind_group;
    // pipeline
    wgpu::ComputePipeline _pipeline = nullptr;

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
