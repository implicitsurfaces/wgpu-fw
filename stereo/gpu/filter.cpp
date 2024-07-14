#include <stereo/gpu/filter.h>
#include <stereo/gpu/shader.h>

namespace stereo {

#define MIN_UNIFORM_PADDING_BYTES 256

/*************************
 * FilteredTexture       *
 *************************/

static wgpu::Texture _clone_texture(
        wgpu::Texture source,
        wgpu::Device device,
        const char* label)
{
    wgpu::TextureDescriptor desc = {};
    desc.size.width    = source.getWidth();
    desc.size.height   = source.getHeight();
    desc.size.depthOrArrayLayers = source.getDepthOrArrayLayers();
    desc.mipLevelCount = source.getMipLevelCount();
    desc.sampleCount   = source.getSampleCount();
    desc.dimension     = source.getDimension();
    desc.format        = wgpu::TextureFormat::RGBA8Snorm;
    desc.usage         = 
        wgpu::TextureUsage::TextureBinding |
        wgpu::TextureUsage::StorageBinding;
    desc.label         = label;
    if (std::bit_width(desc.size.width) > desc.mipLevelCount or
        std::bit_width(desc.size.height) > desc.mipLevelCount)
    {
        std::cerr << 
            "Warning: Texture dimension for " << label << " is too large "
            << "to accommodate all mip levels (maximum mips = "
            << Filter3x3::max_mip_levels << ")" << std::endl;
    }
    return device.createTexture(desc);
}

FilteredTexture::FilteredTexture(wgpu::Texture source, wgpu::Device device, Filter3x3& filter):
    source(source, device, MipSetting{MipViews::Single, MipLevels::All}),
    df_dx(
        _clone_texture(source, device, "df_dx texture"),
        device,
        MipSetting {MipViews::Multiple, MipLevels::All}),
    df_dy(
        _clone_texture(source, device, "df_dy texture"),
        device,
        MipSetting {MipViews::Multiple, MipLevels::All}
    ),
    laplace(
        _clone_texture(source, device, "laplace texture"),
        device,
        MipSetting {MipViews::Multiple, MipLevels::All}
    ),
    filter(&filter) {}

void FilteredTexture::process() {
    filter->apply(*this);
}


/*************************
 * Filter 3x3            *
 *************************/

struct FilterUniforms {
    int32_t mip_level = 0;
    uint8_t _pad[MIN_UNIFORM_PADDING_BYTES - 4]; // align
};

Filter3x3::Filter3x3(wgpu::Device device):
    _device(device)
{
    _device.reference();
    _init();
}

Filter3x3::~Filter3x3() {
    _release();
}

void Filter3x3::_release() {
    if (_pipeline)          _pipeline.release();
    if (_bind_group_layout) _bind_group_layout.release();
    if (_uniform_buffer)    _uniform_buffer.release();
    if (_device)            _device.release();
}

void Filter3x3::_init() {
    wgpu::ShaderModule shader = shader_from_file(_device, "resource/shaders/filter3x3.wgsl");
    
    if (shader == nullptr) {
        std::cerr << "Failed to load filter3x3 shader" << std::endl;
        std::abort();
    }
    
    std::array<wgpu::BindGroupLayoutEntry, 5> bindings;
    
    wgpu::BindGroupLayoutEntry& src_entry = bindings[0];
    src_entry.binding               = 0;
    src_entry.visibility            = wgpu::ShaderStage::Compute;
    src_entry.texture.sampleType    = wgpu::TextureSampleType::Float;
    src_entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutEntry& dst_df_dx  = bindings[1];
    dst_df_dx.binding                      = 1;
    dst_df_dx.visibility                   = wgpu::ShaderStage::Compute;
    dst_df_dx.storageTexture.access        = wgpu::StorageTextureAccess::WriteOnly;
    dst_df_dx.storageTexture.format        = wgpu::TextureFormat::RGBA8Snorm;
    dst_df_dx.storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutEntry& dst_df_dy  = bindings[2];
    dst_df_dy = dst_df_dx;
    dst_df_dy.binding = 2;
    
    wgpu::BindGroupLayoutEntry& dst_laplace  = bindings[3];
    dst_laplace = dst_df_dx;
    dst_laplace.binding = 3;
    
    wgpu::BindGroupLayoutEntry& uniform_entry = bindings[4];
    uniform_entry.binding     = 4;
    uniform_entry.visibility  = wgpu::ShaderStage::Compute;
    uniform_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniform_entry.buffer.minBindingSize = sizeof(FilterUniforms);
    
    wgpu::BindGroupLayoutDescriptor desc = {};
    desc.label       = "filter3x3";
    desc.entryCount  = bindings.size();
    desc.entries     = bindings.data();
    desc.nextInChain = nullptr;
    
    _bind_group_layout = _device.createBindGroupLayout(desc);
    
    wgpu::PipelineLayoutDescriptor pl_desc = {};
    pl_desc.bindGroupLayoutCount = 1;
    pl_desc.bindGroupLayouts     = (WGPUBindGroupLayout*) &_bind_group_layout;
    wgpu::PipelineLayout pl      = _device.createPipelineLayout(pl_desc);
    
    wgpu::ComputePipelineDescriptor cp_desc;
    cp_desc.compute.constantCount = 0;
    cp_desc.compute.constants     = nullptr;
    cp_desc.compute.entryPoint    = "filter_main";
    cp_desc.compute.module        = shader;
    cp_desc.layout                = pl;
    
    // set up the uniform buffer
    wgpu::BufferDescriptor bd;
    bd.size  = sizeof(FilterUniforms) * max_mip_levels;
    bd.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    bd.mappedAtCreation = false;
    _uniform_buffer = _device.createBuffer(bd);
    
    // fill the uniform buffer
    wgpu::Queue queue = _device.getQueue();
    std::array<FilterUniforms, max_mip_levels> uniforms;
    for (int i = 0; i < max_mip_levels; i++) {
        uniforms[i].mip_level = i;
    }
    queue.writeBuffer(
        _uniform_buffer,
        0,
        uniforms.data(),
        sizeof(FilterUniforms) * uniforms.size()
    );
    queue.release();
    
    _pipeline = _device.createComputePipeline(cp_desc);
}

void Filter3x3::apply(FilteredTexture& tex) {
    wgpu::TextureView src_view = tex.source.view_for_mip(0);
    wgpu::Texture src = tex.source.texture;
    
    std::array<wgpu::BindGroupEntry, 5> bindings;
    
    // bind all the inputs:
    // src entry
    bindings[0].binding = 0;
    bindings[0].textureView = src_view;
    // destination texture bindings
    bindings[1].binding = 1;
    bindings[2].binding = 2;
    bindings[3].binding = 3;
    // uniform entry
    bindings[4].binding = 4;
    bindings[4].buffer  = _uniform_buffer;
    bindings[4].offset  = 0;
    bindings[4].size    = sizeof(FilterUniforms);
    
    // pass the mip levels
    wgpu::Queue queue = _device.getQueue();
    
    // start encoding the compute pass
    wgpu::CommandEncoderDescriptor ed = wgpu::Default;
    wgpu::CommandEncoder encoder = _device.createCommandEncoder(ed);
    wgpu::ComputePassDescriptor cpd;
    wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(cpd);
    compute_pass.setPipeline(_pipeline);
    
    vec2ui src_res = {src.getWidth(), src.getHeight()};
    std::vector<wgpu::TextureView> mip_views;
    
    // one exec for each mip level
    int mips = src.getMipLevelCount();
    for (int i = 0; i < mips; i++) {
        bindings[1].textureView = tex.df_dx.view_for_mip(i);
        bindings[2].textureView = tex.df_dy.view_for_mip(i);
        bindings[3].textureView = tex.laplace.view_for_mip(i);
        bindings[4].offset      = i * sizeof(FilterUniforms);
        
        wgpu::BindGroupDescriptor bgd;
        bgd.layout     = _bind_group_layout;
        bgd.entryCount = bindings.size();
        bgd.entries    = (WGPUBindGroupEntry*) bindings.data();
        wgpu::BindGroup bind_group = _device.createBindGroup(bgd);
        compute_pass.setBindGroup(0, bind_group, 0, nullptr);
        
        uint32_t bucket_xy = 8;
        uint32_t invocations_x = src_res.x >> i;
        uint32_t invocations_y = src_res.y >> i;
        uint32_t x_buckets = geom::ceil_div(invocations_x, bucket_xy);
        uint32_t y_buckets = geom::ceil_div(invocations_y, bucket_xy);
        
        compute_pass.dispatchWorkgroups(x_buckets, y_buckets, 1);
    }
    
    compute_pass.end();
    wgpu::CommandBuffer commands = encoder.finish();
    queue.submit(commands);
    
    encoder.release();
    commands.release();
    compute_pass.release();
    queue.release();
}

wgpu::Device Filter3x3::device() {
    return _device;
}

}  // namespace stereo
