#pragma once

#include <stereo/defs.h>

// todo: very few overrides for settings exposed now,
//   but we should add this as needed (e.g. with a Settings class having
//   optional fields).

namespace stereo {

wgpu::BlendState get_blend_over();

wgpu::BlendState get_blend_additive();

wgpu::VertexBufferLayout vertex_buffer(std::initializer_list<wgpu::VertexAttribute> attrs);

struct RenderPipeline {
    wgpu::Device            _device   = nullptr;
    wgpu::RenderPipeline    _pipeline = nullptr;
    wgpu::PrimitiveTopology _topology = wgpu::PrimitiveTopology::TriangleList;

    RenderPipeline(
        wgpu::Device device,
        wgpu::ShaderModule shader,
        wgpu::PrimitiveTopology topology,
        std::initializer_list<wgpu::TextureFormat>      target_formats,
        std::initializer_list<wgpu::BindGroupLayout>    bind_group_layouts,
        std::initializer_list<wgpu::VertexBufferLayout> vertex_buffer_layouts = {},
        std::optional<wgpu::BlendState> blending = std::nullopt);

    /// Construct a render pipeline with a single vertex buffer, having the
    /// listed fields in order. The fields must be members of the same class,
    /// the vertex class.
    /// Usage: RenderPipeline<Vert>(..., &Vert::field_1, &Vert::field_2, ...)
    template <typename C, typename F, typename... Fields>
    RenderPipeline(
            wgpu::Device device,
            wgpu::ShaderModule shader,
            wgpu::PrimitiveTopology topology,
            std::initializer_list<wgpu::TextureFormat> target_formats,
            std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts,
            std::optional<wgpu::BlendState> blending,
            F C::* field,
            Fields C::*... fields):
        _device(device),
        _topology(topology)
    {
        wgpu::VertexAttribute attrs[] = {
            vertex_attribute(0, field),
            vertex_attribute(1, fields)...
        };
        for (size_t i = 2; i <= sizeof...(fields); ++i) {
            attrs[i].shaderLocation = i;
        }
        wgpu::VertexBufferLayout layout = wgpu::Default;
        layout.attributeCount = sizeof...(fields) + 1;
        layout.attributes     = attrs;
        layout.arrayStride    = sizeof(C);
        layout.stepMode       = wgpu::VertexStepMode::Vertex;
        // init
        _init(
            shader,
            target_formats,
            bind_group_layouts,
            {layout},
            blending.value_or(get_blend_over())
        );
    }

    RenderPipeline(const RenderPipeline& other) = delete;
    RenderPipeline(RenderPipeline&& other) = delete;

    ~RenderPipeline();

    RenderPipeline& operator=(const RenderPipeline& other) = delete;
    RenderPipeline& operator=(RenderPipeline&& other)      = delete;

    void _init(
        wgpu::ShaderModule shader,
        std::initializer_list<wgpu::TextureFormat> target_formats,
        std::initializer_list<wgpu::BindGroupLayout> bind_group_layouts,
        std::initializer_list<wgpu::VertexBufferLayout> vertex_buffers,
        wgpu::BlendState blending
    );

    void _release();

    wgpu::RenderPipeline pipeline() const;
    operator wgpu::RenderPipeline() const { return pipeline(); }
};

} // namespace stereo
