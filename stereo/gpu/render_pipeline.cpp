#include <stereo/gpu/render_pipeline.h>

namespace stereo {

RenderPipeline::RenderPipeline(
        wgpu::Device device,
        wgpu::ShaderModule shader,
        wgpu::PrimitiveTopology topology,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts):
    _device(device),
    _topology(topology)
{
    _init(shader, target_formats, bind_group_layouts);
}

RenderPipeline::~RenderPipeline() {
    _release();
}

void RenderPipeline::_init(
        wgpu::ShaderModule shader,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts
    )
{
    // set up vertex shader
    wgpu::RenderPipelineDescriptor pl_dsc;
    pl_dsc.vertex.bufferCount   = 0;
    pl_dsc.vertex.buffers       = nullptr; // todo: allow for buffers
    pl_dsc.vertex.module        = shader;
    pl_dsc.vertex.entryPoint    = "vs_main";
    pl_dsc.vertex.constantCount = 0;
    pl_dsc.vertex.constants     = nullptr;
    
    // define geometry type
    pl_dsc.primitive.topology  = _topology;
    pl_dsc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pl_dsc.primitive.frontFace = wgpu::FrontFace::CCW;
    pl_dsc.primitive.cullMode  = wgpu::CullMode::Back; // todo: expose this
    
    // depth and sampling
    pl_dsc.depthStencil      = nullptr;
    pl_dsc.multisample.count = 1;
    pl_dsc.multisample.mask  = ~0u;
    pl_dsc.multisample.alphaToCoverageEnabled = false;
    pl_dsc.layout = nullptr;
    
    // set up fragment shader
    wgpu::FragmentState frg_state;
    frg_state.module        = shader;
    frg_state.entryPoint    = "fs_main";
    frg_state.constantCount = 0;
    frg_state.constants     = nullptr;
    
    // blend state
    wgpu::BlendState blend;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    
    std::vector<wgpu::ColorTargetState> target_states {target_formats.size()};
    for (size_t i = 0; i < target_formats.size(); i++) {
        auto& target = target_states[i];
        target.format = target_formats.begin()[i];
        target.blend  = &blend;
        target.writeMask = wgpu::ColorWriteMask::All;
    }
    frg_state.targetCount = target_states.size();
    frg_state.targets     = target_states.data();
    pl_dsc.fragment       = &frg_state;
    
    wgpu::PipelineLayoutDescriptor pl_layout_dsc;
    pl_layout_dsc.bindGroupLayoutCount = bind_group_layouts.size();
    pl_layout_dsc.bindGroupLayouts     = (WGPUBindGroupLayout*) bind_group_layouts.begin();
    pl_dsc.layout = _device.createPipelineLayout(pl_layout_dsc);
    
    _pipeline = _device.createRenderPipeline(pl_dsc);
    
}

void RenderPipeline::_release() {
    release_all(
        _pipeline,
        _device
    );
}

wgpu::RenderPipeline RenderPipeline::pipeline() const {
    return _pipeline;
}

}  // namespace stereo
