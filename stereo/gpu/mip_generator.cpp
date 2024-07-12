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
        device(gen.get_device()),
        texture(texture),
        generator(&gen)
{
    device.reference();
    texture.reference();
    _init();
}

MipTexture::MipTexture(const MipTexture& other):
    device(other.device),
    texture(other.texture),
    generator(other.generator),
    views(other.views)
{
    if (device)   device.reference();
    if (texture)  texture.reference();
    for (auto view : views) {
        view.reference();
    }
}

MipTexture::MipTexture(MipTexture&& other):
    device(other.device),
    texture(other.texture),
    generator(other.generator),
    views(std::move(other.views))
{
    other.device   = nullptr;
    other.texture  = nullptr;
    other.generator = nullptr;
}

MipTexture::~MipTexture() {
    _release();
}

MipTexture& MipTexture::operator=(MipTexture&& other) {
    std::swap(device, other.device);
    std::swap(texture, other.texture);
    std::swap(generator, other.generator);
    std::swap(views, other.views);
    return *this;
}

MipTexture& MipTexture::operator=(const MipTexture& other) {
    _release();
    device    = other.device;
    texture   = other.texture;
    generator = other.generator;
    views     = other.views;
    if (device)   device.reference();
    if (texture)  texture.reference();
    for (auto view : views) {
        view.reference();
    }
    return *this;
}

void MipTexture::generate() {
    wgpu::Queue queue = device.getQueue();
    
    // construct the mipmap processing pass
    wgpu::CommandEncoderDescriptor encoder_descriptor = wgpu::Default;
    wgpu::CommandEncoder encoder = device.createCommandEncoder(encoder_descriptor);
    
    wgpu::ComputePassDescriptor compute_pass_descriptor;
    wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(compute_pass_descriptor);
    compute_pass.setPipeline(generator->_pipeline);
    
    uint32_t res_x = texture.getWidth();
    uint32_t res_y = texture.getHeight();
    wgpu::BindGroupEntry mip_entries[2] = { wgpu::Default, wgpu::Default };
    for (size_t level = 1; level < texture.getMipLevelCount(); ++level) {
        // bind the mip level N and N + 1 texture views
        mip_entries[0].binding = 0;
        mip_entries[0].textureView = views[level - 1];
        
        mip_entries[1].binding = 1;
        mip_entries[1].textureView = views[level];
        
        wgpu::BindGroupDescriptor bgd;
        bgd.layout     = generator->_bind_group_layout;
        bgd.entryCount = 2;
        bgd.entries    = (WGPUBindGroupEntry*) mip_entries;
        bind_group     = device.createBindGroup(bgd);
        compute_pass.setBindGroup(0, bind_group, 0, nullptr);
        
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

void MipTexture::_init() {
    // create the mip-level views of the texture
    wgpu::TextureViewDescriptor view_descriptor;
    view_descriptor.aspect          = wgpu::TextureAspect::All;
    view_descriptor.baseArrayLayer  = 0;
    view_descriptor.arrayLayerCount = 1;
    view_descriptor.dimension       = wgpu::TextureViewDimension::_2D;
    view_descriptor.format          = texture.getFormat();
    view_descriptor.mipLevelCount   = 1; // each view looks into only ONE mip level
    
    size_t mip_levels = texture.getMipLevelCount();
    views.reserve(mip_levels);
    for (size_t level = 0; level < mip_levels; ++level) {
        std::string label = "frame source view (mip = " + std::to_string(level) + ")";
        view_descriptor.label = label.c_str();
        view_descriptor.baseMipLevel = level;
        views.push_back(texture.createView(view_descriptor));
    }
}

void MipTexture::_release() {
    for (auto view : views) {
        view.release();
    }
    views.clear();
    if (texture)    texture.release();
    if (bind_group) bind_group.release();
    if (device)     device.release();
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
