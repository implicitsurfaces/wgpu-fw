#pragma once

#include <stereo/gpu/texture.h>
#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/uniform.h>
#include <stereo/gpu/buffer.h>
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
        vec2ui size,
        wgpu::TextureFormat format,
        std::string_view label,
        wgpu::TextureUsage extra_usage=wgpu::TextureUsage::None
    );
    MipTexture(MipGeneratorRef gen, Texture tex);
    
    void generate(float input_gamma=1.f, float output_gamma=1.f);
};

struct MipGenerator {
private:
    
    struct MipUniforms {
        float src_gamma = 1.f;
        float dst_gamma = 1.f;
    };
    
    wgpu::Device          _device = nullptr;
    DataBuffer<UniformBox<MipUniforms>> _uniform_buffer;
    BindGroupLayout       _uniforms_layout;
    BindGroupLayout       _levels_layout;
    BindGroup             _uniforms_binding;
    wgpu::ComputePipeline _pipeline = nullptr; // this has the shader
    
    friend struct MipTexture;
    
public:
    
    MipGenerator(wgpu::Device device);
    ~MipGenerator();
    
    wgpu::Device get_device();
};

void generate_mips(MipGeneratorRef generator, Texture& tex);

} // namespace stereo
