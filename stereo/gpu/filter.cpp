#include <stereo/gpu/filter.h>
#include <stereo/gpu/shader.h>

namespace stereo {

/*************************
 * FilteredTexture       *
 *************************/

// todo: the source texture does not need to build mip views
//   (we don't use them, but they take up space)

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

FilteredTexture::FilteredTexture(Texture source, wgpu::Device device, Filter3x3Ref filter):
    source(source),
    r_tex(
        _clone_texture(source.texture(), device, "df_dx texture"),
        device
    ),
    g_tex(
        _clone_texture(source.texture(), device, "df_dy texture"),
        device
    ),
    b_tex(
        _clone_texture(source.texture(), device, "laplace texture"),
        device
    ),
    filter(filter)
{
    _init();
}

void FilteredTexture::_init() {
    wgpu::BindGroupEntry src_entry = wgpu::Default;
    src_entry.binding     = 0;
    src_entry.textureView = source.view();

    wgpu::BindGroupDescriptor src_desc;
    src_desc.layout     = filter->_src_layout;
    src_desc.entryCount = 1;
    src_desc.entries    = &src_entry;
    src_desc.label      = "filter3x3 source bind group";
    _src_bindgroup = source.device().createBindGroup(src_desc);

    std::array<wgpu::BindGroupEntry, 3> dst_entries;
    dst_entries[0].binding = 0;
    dst_entries[1].binding = 1;
    dst_entries[2].binding = 2;

    int32_t mip_levels = source.num_mip_levels();
    _dst_bind_groups.reserve(mip_levels);
    for (int32_t i = 0; i < mip_levels; i++) {
        dst_entries[0].textureView = r_tex.view_for_mip(i);
        dst_entries[1].textureView = g_tex.view_for_mip(i);
        dst_entries[2].textureView = b_tex.view_for_mip(i);

        wgpu::BindGroupDescriptor dst_desc;
        std::string label = "filter3x3 destination bind group (mip=" + std::to_string(i) + ")";
        dst_desc.layout     = filter->_dst_layout;
        dst_desc.entryCount = dst_entries.size();
        dst_desc.entries    = dst_entries.data();
        dst_desc.label      = label.c_str();
        BindGroup bg = source.device().createBindGroup(dst_desc);
        _dst_bind_groups.push_back(std::move(bg));
    }
}

void FilteredTexture::process() {
    filter->apply(*this);
}

BindGroup FilteredTexture::source_bindgroup() {
    return _src_bindgroup;
}

BindGroup FilteredTexture::target_bindgroup(size_t level) {
    return _dst_bind_groups[level];
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
    release_all(
        _pipeline,
        _uniform_buffer,
        _device
    );
}

void Filter3x3::_init() {
    wgpu::ShaderModule shader = shader_from_file(_device, "resource/shaders/stereo/filter3x3.wgsl");

    if (shader == nullptr) {
        std::cerr << "Failed to load filter3x3 shader" << std::endl;
        std::abort();
    }

    // source bind group layout
    wgpu::BindGroupLayoutEntry src_entry = wgpu::Default;
    src_entry.binding               = 0;
    src_entry.visibility            = wgpu::ShaderStage::Compute;
    src_entry.texture.sampleType    = wgpu::TextureSampleType::Float;
    src_entry.texture.viewDimension = wgpu::TextureViewDimension::_2D;

    wgpu::BindGroupLayoutDescriptor src_desc = {};
    src_desc.label       = "filter3x3 source bind group";
    src_desc.entryCount  = 1;
    src_desc.entries     = &src_entry;
    src_desc.nextInChain = nullptr;
    _src_layout = _device.createBindGroupLayout(src_desc);

    // destination bind group layout
    std::array<wgpu::BindGroupLayoutEntry, 3> dst_bindings;
    wgpu::BindGroupLayoutEntry& dst_df_dx  = dst_bindings[0];
    dst_df_dx.binding                      = 0;
    dst_df_dx.visibility                   = wgpu::ShaderStage::Compute;
    dst_df_dx.storageTexture.access        = wgpu::StorageTextureAccess::WriteOnly;
    dst_df_dx.storageTexture.format        = wgpu::TextureFormat::RGBA8Snorm;
    dst_df_dx.storageTexture.viewDimension = wgpu::TextureViewDimension::_2D;

    wgpu::BindGroupLayoutEntry& dst_df_dy  = dst_bindings[1];
    dst_df_dy = dst_df_dx;
    dst_df_dy.binding = 1;

    wgpu::BindGroupLayoutEntry& dst_laplace  = dst_bindings[2];
    dst_laplace = dst_df_dx;
    dst_laplace.binding = 2;

    wgpu::BindGroupLayoutDescriptor dst_desc = {};
    dst_desc.label       = "filter3x3 destination bind group";
    dst_desc.entryCount  = dst_bindings.size();
    dst_desc.entries     = dst_bindings.data();
    dst_desc.nextInChain = nullptr;
    _dst_layout = _device.createBindGroupLayout(dst_desc);

    // uniform bind group layout
    wgpu::BindGroupLayoutEntry uniform_layout_entry = wgpu::Default;
    uniform_layout_entry.binding     = 0;
    uniform_layout_entry.visibility  = wgpu::ShaderStage::Compute;
    uniform_layout_entry.buffer.type = wgpu::BufferBindingType::Uniform;
    uniform_layout_entry.buffer.minBindingSize = sizeof(FilterUniforms);
    uniform_layout_entry.buffer.hasDynamicOffset = true;

    wgpu::BindGroupLayoutDescriptor uniform_desc = {};
    uniform_desc.label       = "filter3x3 uniforms bind group";
    uniform_desc.entryCount  = 1;
    uniform_desc.entries     = &uniform_layout_entry;
    uniform_desc.nextInChain = nullptr;
    _uniform_layout      = _device.createBindGroupLayout(uniform_desc);

    // pipeline setup
    std::array<wgpu::BindGroupLayout, 3> layouts = {
        _src_layout,
        _dst_layout,
        _uniform_layout
    };

    wgpu::PipelineLayoutDescriptor pl_desc = {};
    pl_desc.bindGroupLayoutCount = layouts.size();
    pl_desc.bindGroupLayouts     = (WGPUBindGroupLayout*) layouts.data();
    wgpu::PipelineLayout pl      = _device.createPipelineLayout(pl_desc);

    wgpu::ComputePipelineDescriptor cp_desc;
    cp_desc.label                 = "filter 3x3 pipeline";
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

    // create the bind group for the uniforms
    wgpu::BindGroupEntry uniform_entry = wgpu::Default;
    uniform_entry.binding = 0;
    uniform_entry.buffer  = _uniform_buffer;
    uniform_entry.offset  = 0;
    uniform_entry.size    = sizeof(FilterUniforms);

    wgpu::BindGroupDescriptor bgd;
    bgd.layout     = _uniform_layout;
    bgd.entryCount = 1;
    bgd.entries    = (WGPUBindGroupEntry*) &uniform_entry;
    bgd.label      = "filter 3x3 uniform bind group";
    _uniform_bind_group = _device.createBindGroup(bgd);
}

void Filter3x3::apply(FilteredTexture& tex) {
    wgpu::TextureView src_view = tex.source.view();
    wgpu::Texture src = tex.source.texture();

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

    vec2u src_res = {src.getWidth(), src.getHeight()};
    std::vector<wgpu::TextureView> mip_views;

    // one exec for each mip level
    int mips = tex.source.num_mip_levels();
    compute_pass.setBindGroup(0, tex.source_bindgroup(), 0, nullptr);
    for (int i = 0; i < mips; i++) {
        uint32_t uniform_offset = i * sizeof(FilterUniforms);
        compute_pass.setBindGroup(1, tex.target_bindgroup(i), 0, nullptr);
        compute_pass.setBindGroup(2, _uniform_bind_group,     1, &uniform_offset);

        uint32_t bucket_xy = 8;
        uint32_t invocations_x = src_res.x >> i;
        uint32_t invocations_y = src_res.y >> i;
        uint32_t x_buckets = geom::ceil_div(invocations_x, bucket_xy);
        uint32_t y_buckets = geom::ceil_div(invocations_y, bucket_xy);

        compute_pass.dispatchWorkgroups(x_buckets, y_buckets, 1);
    }

    compute_pass.end();
    wgpu::CommandBuffer commands = encoder.finish();
    compute_pass.release();
    encoder.release();

    queue.submit(commands);
    queue.release();
}

wgpu::Device Filter3x3::device() {
    return _device;
}

}  // namespace stereo
