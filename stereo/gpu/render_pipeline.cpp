#include <stereo/gpu/render_pipeline.h>

namespace stereo {

// blending helpers

wgpu::BlendState get_blend_over() {
    wgpu::BlendState blend;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    return blend;
}

wgpu::BlendState get_blend_additive() {
    wgpu::BlendState blend;
    blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
    blend.color.dstFactor = wgpu::BlendFactor::One;
    blend.color.operation = wgpu::BlendOperation::Add;
    blend.alpha.srcFactor = wgpu::BlendFactor::One;
    blend.alpha.dstFactor = wgpu::BlendFactor::One;
    blend.alpha.operation = wgpu::BlendOperation::Add;
    return blend;
}

// RenderPipeline class

RenderPipeline::RenderPipeline(
        wgpu::Device device,
        wgpu::ShaderModule shader,
        wgpu::PrimitiveTopology topology,
        std::initializer_list<wgpu::TextureFormat>      target_formats,
        std::initializer_list<wgpu::BindGroupLayout>    bind_group_layouts,
        std::initializer_list<wgpu::VertexBufferLayout> vertex_buffers,
        std::optional<wgpu::BlendState> blending):
    _device(device),
    _topology(topology)
{
    _init(
        shader,
        target_formats,
        bind_group_layouts,
        vertex_buffers,
        blending.value_or(get_blend_over())
    );
}

RenderPipeline::~RenderPipeline() {
    _release();
}

void RenderPipeline::_init(
        wgpu::ShaderModule shader,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts,
        std::initializer_list<wgpu::VertexBufferLayout> vertex_buffers,
        wgpu::BlendState blending
    )
{
    // set up vertex shader
    wgpu::RenderPipelineDescriptor pl_dsc;
    pl_dsc.vertex.bufferCount   = vertex_buffers.size();
    pl_dsc.vertex.buffers       = vertex_buffers.begin();
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

    std::vector<wgpu::ColorTargetState> target_states {target_formats.size()};
    for (size_t i = 0; i < target_formats.size(); i++) {
        auto& target = target_states[i];
        target.format = target_formats.begin()[i];
        target.blend  = &blending;
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
