#pragma once

#include <stereo/gpu/texture.h>
#include <stereo/gpu/bindgroup.h>
#include <string_view>

namespace stereo {

// todo: this does not handle edges correctly yet

struct MipGenerator;
using MipGeneratorRef = std::shared_ptr<MipGenerator>;

struct MipTexture {

    Texture                texture;
    std::vector<BindGroup> bind_groups;
    MipGeneratorRef        generator = nullptr;

    friend struct MipGenerator;

private:

    void _init();

public:

    MipTexture() = default;
    MipTexture(
        MipGeneratorRef gen,
        vec2u size,
        wgpu::TextureFormat format,
        std::string_view label,
        wgpu::TextureUsage extra_usage=wgpu::TextureUsage::None
    );
    MipTexture(MipGeneratorRef gen, Texture tex);

    void generate();
};

struct MipGenerator {
private:

    wgpu::Device          _device            = nullptr;
    wgpu::BindGroupLayout _bind_group_layout = nullptr;
    wgpu::ComputePipeline _pipeline          = nullptr; // this has the shader

    friend struct MipTexture;

    void _init();
    void _release();

public:

    MipGenerator(wgpu::Device device);
    ~MipGenerator();

    wgpu::Device get_device();
};

} // namespace stereo
