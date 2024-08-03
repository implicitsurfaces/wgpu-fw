#include <stereo/gpu/mip_generator.h>
#include <stereo/gpu/shader.h>

namespace stereo {

constexpr const char* MIP_SHADER_SRC = R"(
@group(0) @binding(0) var previous_mip: texture_2d<f32>;
@group(0) @binding(1) var current_mip:  texture_storage_2d<bgra8unorm,write>;

@compute @workgroup_size(8, 8)
fn compute_mipmap(@builtin(global_invocation_id) id: vec3<u32>) {
    let offset = vec2<u32>(0u, 1u);
    let color = (
        textureLoad(previous_mip, 2u * id.xy + offset.xx, 0) +
        textureLoad(previous_mip, 2u * id.xy + offset.xy, 0) +
        textureLoad(previous_mip, 2u * id.xy + offset.yx, 0) +
        textureLoad(previous_mip, 2u * id.xy + offset.yy, 0)
    ) * 0.25;
    textureStore(current_mip, id.xy, color);
}
)";


/***********************
 * MipTexture          *
 ***********************/

MipTexture::MipTexture() {}

MipTexture::MipTexture(wgpu::Texture texture, MipGenerator& gen):
        texture(
            texture,
            gen.get_device()
        ),
        generator(&gen)
{
    _init();
}

MipTexture::MipTexture(const MipTexture& other):
    texture(other.texture),
    bind_groups(other.bind_groups),
    generator(other.generator)
{
    for (auto bg : bind_groups) {
        bg.reference();
    }
}

MipTexture::MipTexture(MipTexture&& other):
    texture(std::move(other.texture)),
    bind_groups(other.bind_groups),
    generator(other.generator)
{
    other.bind_groups.clear();
}

MipTexture::~MipTexture() {
    _release();
}

MipTexture& MipTexture::operator=(MipTexture&& other) {
    std::swap(texture, other.texture);
    std::swap(bind_groups, other.bind_groups);
    std::swap(generator, other.generator);
    return *this;
}

MipTexture& MipTexture::operator=(const MipTexture& other) {
    _release();
    texture   = other.texture;
    generator = other.generator;
    for (auto bg : bind_groups) {
        bg.reference();
    }
    return *this;
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
        bgd.layout     = generator->_bind_group_layout;
        bgd.entryCount = 2;
        bgd.entries    = (WGPUBindGroupEntry*) mip_entries;
        bind_groups.push_back(texture.device().createBindGroup(bgd));
    }
}

void MipTexture::_release() {
    for (auto bg : bind_groups) {
        bg.release();
    }
    bind_groups.clear();
}

void MipTexture::generate() {
    wgpu::Queue queue = texture.device().getQueue();
    
    // construct the mipmap processing pass
    wgpu::CommandEncoderDescriptor encoder_descriptor = wgpu::Default;
    wgpu::CommandEncoder encoder = texture.device().createCommandEncoder(encoder_descriptor);
    
    wgpu::ComputePassDescriptor compute_pass_descriptor;
    wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(compute_pass_descriptor);
    compute_pass.setPipeline(generator->_pipeline);
    
    uint32_t res_x = texture.texture().getWidth();
    uint32_t res_y = texture.texture().getHeight();
    for (size_t level = 1; level < texture.texture().getMipLevelCount(); ++level) {
        compute_pass.setBindGroup(0, bind_groups[level - 1], 0, nullptr);
        uint32_t invocations_x = res_x >> level;
        uint32_t invocations_y = res_y >> level;
        uint32_t bucket_xy     = 8;
        uint32_t buckets_x     = geom::ceil_div(invocations_x, bucket_xy);
        uint32_t buckets_y     = geom::ceil_div(invocations_y, bucket_xy);
        compute_pass.dispatchWorkgroups(buckets_x, buckets_y, 1);
    }
    compute_pass.end();
    wgpu::CommandBuffer commands = encoder.finish(wgpu::CommandBufferDescriptor{});
    queue.submit(commands);
    
    encoder.release();
    commands.release();
    compute_pass.release();
    queue.release();
}


/***********************
 * MipGenerator        *
 ***********************/

MipGenerator::MipGenerator(wgpu::Device device):
    _device(device)
{
    _device.reference();
    _init();
}

MipGenerator::~MipGenerator() {
    _release();
    _device.release();
}

void MipGenerator::_init() {
    // init bind group layout
    // input image: mip level N
    wgpu::BindGroupLayoutEntry mip_entries[2];
    mip_entries[0].binding               = 0;
    mip_entries[0].texture.sampleType    = wgpu::TextureSampleType::Float;
    mip_entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
    mip_entries[0].visibility            = wgpu::ShaderStage::Compute;
    
    // output image: mip level N + 1
    mip_entries[1].binding               = 1;
    mip_entries[1].storageTexture.access = wgpu::StorageTextureAccess::WriteOnly;
    mip_entries[1].storageTexture.format = wgpu::TextureFormat::BGRA8Unorm;
    mip_entries[1].storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;
    mip_entries[1].visibility            = wgpu::ShaderStage::Compute;
    
    wgpu::BindGroupLayoutDescriptor layout_descriptor;
    layout_descriptor.entryCount = 2;
    layout_descriptor.entries    = mip_entries;
    _bind_group_layout = _device.createBindGroupLayout(layout_descriptor);
    
    // load the shader + build its pipeline
    wgpu::ShaderModule compute_shader_module = shader_from_str(_device, MIP_SHADER_SRC);
    
    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts     = (WGPUBindGroupLayout*) &_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout = _device.createPipelineLayout(pipeline_layout_descriptor);
    
    wgpu::ComputePipelineDescriptor cpd;
    cpd.compute.constantCount = 0;
    cpd.compute.constants     = nullptr;
    cpd.compute.entryPoint    = "compute_mipmap";
    cpd.compute.module        = compute_shader_module;
    cpd.layout                = pipeline_layout;
    _pipeline = _device.createComputePipeline(cpd);
}

void MipGenerator::_release() {
    _bind_group_layout.release();
    _pipeline.release();
}

wgpu::Device MipGenerator::get_device() {
    return _device;
}


MipTexture MipGenerator::create_mip_texture(wgpu::Texture texture) {
    return MipTexture(texture, *this);
}

}  // namespace stereo
