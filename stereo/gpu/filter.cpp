#include <stereo/gpu/filter.h>
#include <stereo/gpu/shader.h>

namespace stereo {

struct FilterUniforms {
    int32_t mip_level = 0;
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
    wgpu::ShaderModule shader = shader_from_file(_device, "shaders/filter3x3.wgsl");
    
    std::array<wgpu::BindGroupLayoutEntry, 5> bindings;
    
    wgpu::BindGroupLayoutEntry& src_entry = bindings[0];
    src_entry.binding               = 0;
    src_entry.visibility            = wgpu::ShaderStage::Compute;
    src_entry.texture.sampleType    = wgpu::TextureSampleType::Float;
    src_entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutEntry& dst_df_dx = bindings[1];
    dst_df_dx.binding                      = 1;
    dst_df_dx.visibility                   = wgpu::ShaderStage::Compute;
    dst_df_dx.storageTexture.access        = wgpu::StorageTextureAccess::WriteOnly;
    dst_df_dx.storageTexture.format        = wgpu::TextureFormat::RGBA16Float;
    dst_df_dx.storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;
    
    wgpu::BindGroupLayoutEntry& dst_df_dy = bindings[2];
    dst_df_dy         = dst_df_dx;
    dst_df_dy.binding = 2;
    
    wgpu::BindGroupLayoutEntry& dst_laplace = bindings[3];
    dst_laplace         = dst_df_dx;
    dst_laplace.binding = 3;
    
    wgpu::BindGroupLayoutEntry& uniform_entry = bindings[4];
    uniform_entry.binding     = 2;
    uniform_entry.visibility  = wgpu::ShaderStage::Compute;
    uniform_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniform_entry.buffer.minBindingSize = sizeof(FilterUniforms);
    
    wgpu::BindGroupLayoutDescriptor desc = {};
    desc.entryCount = bindings.size();
    desc.entries    = bindings.data();
    
    _bind_group_layout = _device.createBindGroupLayout(desc);
    
    wgpu::PipelineLayoutDescriptor pl_desc = {};
    pl_desc.bindGroupLayoutCount = 1;
    pl_desc.bindGroupLayouts     = (WGPUBindGroupLayout*) &_bind_group_layout;
    wgpu::PipelineLayout pl = _device.createPipelineLayout(pl_desc);
    
    wgpu::ComputePipelineDescriptor cp_desc;
    cp_desc.compute.constantCount = 0;
    cp_desc.compute.constants     = nullptr;
    cp_desc.compute.entryPoint    = "filter_main";
    cp_desc.compute.module        = shader;
    cp_desc.layout                = pl;
    
    // set up the uniform buffer
    wgpu::BufferDescriptor bd;
    bd.size = sizeof(FilterUniforms) * max_mip_levels;
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

static wgpu::TextureView view_2d_from_texture(wgpu::Texture tex) {
    wgpu::TextureViewDescriptor src_view_desc = {};
    src_view_desc.aspect          = wgpu::TextureAspect::All;
    src_view_desc.dimension       = wgpu::TextureViewDimension::_2D;
    src_view_desc.format          = tex.getFormat();
    src_view_desc.mipLevelCount   = tex.getMipLevelCount();
    src_view_desc.baseArrayLayer  = 0;
    src_view_desc.baseMipLevel    = 0;
    src_view_desc.arrayLayerCount = 1;
    return tex.createView(src_view_desc);
}

void Filter3x3::apply(
        wgpu::Texture src,
        wgpu::Texture df_dx_dst,
        wgpu::Texture df_dy_dst,
        wgpu::Texture laplace_dst)
{
    wgpu::TextureView src_view   = view_2d_from_texture(src);
    wgpu::TextureView df_dx_view = view_2d_from_texture(df_dx_dst);
    wgpu::TextureView df_dy_view = view_2d_from_texture(df_dy_dst);
    wgpu::TextureView lap_view   = view_2d_from_texture(laplace_dst);
    
    std::array<wgpu::BindGroupEntry, 5> bindings;
    
    // bind all the inputs:
    // src entry
    bindings[0].binding = 0;
    bindings[0].textureView = src_view;
    // df_dx entry
    bindings[1].binding = 1;
    bindings[1].textureView = df_dx_view;
    // df_dy entry
    bindings[2].binding = 2;
    bindings[2].textureView = df_dy_view;
    // laplace entry
    bindings[3].binding = 3;
    bindings[3].textureView = lap_view;
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
    
    // one exec for each mip level
    int mips = src.getMipLevelCount();
    for (int i = 0; i < mips; i++) {
        bindings[4].offset = i * sizeof(FilterUniforms);
        
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

}  // namespace stereo
