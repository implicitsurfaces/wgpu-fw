#include "webgpu/webgpu.hpp"
#include <stereo/gpu/mip_generator.h>
#include <stereo/gpu/shader.h>

namespace stereo {

constexpr const char* MIP_SHADER_SRC = R"(
struct MipUniforms {
    src_gamma: f32,
    dst_gamma: f32,
}

@group(0) @binding(0) var<uniform> mip_uniforms: MipUniforms;

@group(1) @binding(0) var previousMipLevel: texture_2d<f32>;
@group(1) @binding(1) var nextMipLevel: texture_storage_2d<rgba8unorm,write>;

@compute @workgroup_size(8, 8)
fn compute_mipmap(@builtin(global_invocation_id) id: vec3<u32>) {
    let offset = vec2<u32>(0u, 1u);
    let decode_gamma = vec4f(vec3f(     mip_uniforms.src_gamma), 1.);
    let encode_gamma = vec4f(vec3f(1. / mip_uniforms.dst_gamma), 1.);
    var samples: array<vec4f, 4> = array<vec4f, 4>(
        textureLoad(previousMipLevel, 2u * id.xy + offset.xx, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.xy, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.yx, 0),
        textureLoad(previousMipLevel, 2u * id.xy + offset.yy, 0),
    );
    if (mip_uniforms.src_gamma != 1.) {
        for (var i: u32 = 0u; i < 4u; i = i + 1u) {
            samples[i] = pow(samples[i], decode_gamma);
        }
    }
    var color = (
        samples[0] + samples[1] + samples[2] + samples[3]
    ) * 0.25;
    if (mip_uniforms.dst_gamma != 1.) {
        color = pow(color, encode_gamma);
    }
    textureStore(nextMipLevel, id.xy, color);
}
)";


/***********************
 * MipTexture          *
 ***********************/

MipTexture::MipTexture(
        MipGeneratorRef gen,
        vec2ui size,
        wgpu::TextureFormat format,
        std::string_view label,
        wgpu::TextureUsage extra_usage):
            texture(
                gen->get_device(),
                size,
                format,
                label.data(),
                extra_usage |
                    wgpu::TextureUsage::CopyDst |        // for upload
                    wgpu::TextureUsage::StorageBinding | // for mip write-out
                    wgpu::TextureUsage::TextureBinding   // for access during mip gen
            ),
        generator(gen)
{
    _init();
}

MipTexture::MipTexture(
        MipGeneratorRef gen,
        Texture tex):
    texture(tex),
    generator(gen)
{
    _init();
}

void MipTexture::_init() {
    // create + store bind groups for each mip level
    size_t mip_levels = texture.texture().getMipLevelCount();
    wgpu::BindGroupEntry mip_entries[2] = { wgpu::Default, wgpu::Default };
    for (size_t level = 1; level < mip_levels; ++level) {
        // bind the mip level N and N + 1 texture views
        mip_entries[0].binding = 0;
        mip_entries[0].textureView = texture.view_for_mip(level - 1);

        mip_entries[1].binding = 1;
        mip_entries[1].textureView = texture.view_for_mip(level);

        wgpu::BindGroupDescriptor bgd;
        bgd.layout     = generator->_levels_layout;
        bgd.entryCount = 2;
        bgd.entries    = (WGPUBindGroupEntry*) mip_entries;
        bind_groups.push_back(texture.device().createBindGroup(bgd));
    }
}

void MipTexture::generate(float src_gamma, float dst_gamma) {
    wgpu::Queue queue = texture.device().getQueue();

    // construct the mipmap processing pass
    wgpu::CommandEncoderDescriptor encoder_descriptor = wgpu::Default;
    wgpu::CommandEncoder encoder = texture.device().createCommandEncoder(encoder_descriptor);

    wgpu::ComputePassDescriptor compute_pass_descriptor;
    wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(compute_pass_descriptor);
    compute_pass.setPipeline(generator->_pipeline);
    
    UniformBox<MipGenerator::MipUniforms> uniforms = MipGenerator::MipUniforms {
        .src_gamma = src_gamma,
        .dst_gamma = dst_gamma,
    };
    generator->_uniform_buffer.submit_write(uniforms, 0);

    uint32_t res_x = texture.texture().getWidth();
    uint32_t res_y = texture.texture().getHeight();
    compute_pass.setBindGroup(0, generator->_uniforms_binding, 0, nullptr);
    for (size_t level = 1; level < texture.texture().getMipLevelCount(); ++level) {
        compute_pass.setBindGroup(1, bind_groups[level - 1], 0, nullptr);
        uint32_t invocations_x = res_x >> level;
        uint32_t invocations_y = res_y >> level;
        uint32_t bucket_xy     = 8;
        uint32_t buckets_x     = geom::ceil_div(invocations_x, bucket_xy);
        uint32_t buckets_y     = geom::ceil_div(invocations_y, bucket_xy);
        compute_pass.dispatchWorkgroups(buckets_x, buckets_y, 1);
    }
    compute_pass.end();
    wgpu::CommandBuffer commands = encoder.finish(wgpu::Default);
    compute_pass.release();
    encoder.release();
    queue.submit(commands);
    queue.release();
}


/***********************
 * MipGenerator        *
 ***********************/

MipGenerator::MipGenerator(wgpu::Device device):
    _device(device),
    _uniform_buffer(device, 1, BufferKind::Uniform),
    _uniforms_layout {
        device,
        {
            uniform_layout<UniformBox<MipUniforms>>(0, false)
        },
        "mip uniforms layout"
    },
    _levels_layout {
        device,
        {
            // input image: mip level N
            texture_layout(0),
            // output image: mip level N + 1
            texture_storage_layout(1),
        },
        "mip levels layout"
    },
    _uniforms_binding {
        device,
        _uniforms_layout,
        {
            buffer_entry(0, _uniform_buffer, 1),
        },
        "mip uniforms binding"
    }
{
    _device.reference();
    // load the shader + build its pipeline
    wgpu::ShaderModule compute_shader_module = shader_from_str(
        _device,
        MIP_SHADER_SRC,
        "mipmap shader"
    );
    WGPUBindGroupLayout layouts[2] = { _uniforms_layout, _levels_layout };
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 2;
    pipeline_layout_descriptor.bindGroupLayouts     = (WGPUBindGroupLayout*) layouts;
    wgpu::PipelineLayout pipeline_layout = _device.createPipelineLayout(pipeline_layout_descriptor);

    wgpu::ComputePipelineDescriptor cpd;
    cpd.label                 = "mip generator pipeline";
    cpd.compute.constantCount = 0;
    cpd.compute.constants     = nullptr;
    cpd.compute.entryPoint    = "compute_mipmap";
    cpd.compute.module        = compute_shader_module;
    cpd.layout                = pipeline_layout;
    _pipeline = _device.createComputePipeline(cpd);
}

MipGenerator::~MipGenerator() {
    _pipeline.release();
    _device.release();
}

wgpu::Device MipGenerator::get_device() {
    return _device;
}


void generate_mips(MipGeneratorRef generator, Texture& tex) {
    wgpu::Queue queue = tex.device().getQueue();
    wgpu::TextureFormat src_format = tex.texture().getFormat();
    wgpu::TextureFormat dst_format = wgpu::TextureFormat::RGBA8Unorm;
    float src_gamma = 1.f;
    if (src_format == wgpu::TextureFormat::RGBA32Float) {
        dst_format = wgpu::TextureFormat::RGBA32Float;
    } else if (src_format == wgpu::TextureFormat::Depth32Float) {
        dst_format = wgpu::TextureFormat::Depth32Float;
    } else if (src_format == wgpu::TextureFormat::BGRA8UnormSrgb or
        src_format == wgpu::TextureFormat::RGBA8UnormSrgb)
    {
        // src image needs to be linearized before downsampling,
        // and gamma'd before write
        src_gamma = 2.2f;
    }
    MipTexture mip_tex {
        generator,
        {tex.width(), tex.height()},
        dst_format,
        "mip generator temporary texture",
        wgpu::TextureUsage::CopySrc,
    };
    auto src_range = tex.mip_range();
    
    // copy data from the source texture to the first mip level
    wgpu::CommandEncoder base_copy_encoder = tex.device().createCommandEncoder(wgpu::Default);
    wgpu::ImageCopyTexture src;
    wgpu::ImageCopyTexture dst;
    src.texture  = tex.texture();
    src.aspect   = wgpu::TextureAspect::All;
    src.mipLevel = src_range.lo;
    src.origin   = {0, 0, 0};
    
    dst.texture  = mip_tex.texture.texture();
    dst.aspect   = wgpu::TextureAspect::All;
    dst.mipLevel = 0;
    dst.origin   = {0, 0, 0};
    base_copy_encoder.copyTextureToTexture(src, dst, {tex.width(), tex.height(), 1});
    wgpu::CommandBuffer commands = base_copy_encoder.finish(wgpu::Default);
    queue.submit(commands);
    base_copy_encoder.release();
    
    // generate the mips
    mip_tex.generate(src_gamma, src_gamma);
    
    // copy the mips back to the original texture
    wgpu::CommandEncoder copy_back_encoder = tex.device().createCommandEncoder(wgpu::Default);
    src.texture = mip_tex.texture.texture();
    dst.texture = tex.texture();
    for (int level = std::max(1, src_range.lo); level < src_range.hi; ++level) {
        gpu_size_t w = tex.width()  >> level;
        gpu_size_t h = tex.height() >> level;
        src.mipLevel = level;
        dst.mipLevel = level;
        copy_back_encoder.copyTextureToTexture(src, dst, {w, h, 1});
    }
    commands = copy_back_encoder.finish(wgpu::Default);
    queue.submit(commands);
    copy_back_encoder.release();
    queue.release();
}

}  // namespace stereo
