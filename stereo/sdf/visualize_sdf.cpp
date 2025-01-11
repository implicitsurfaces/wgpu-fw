#include <stereo/sdf/visualize_sdf.h>
#include <stereo/gpu/shader.h>

namespace stereo {

VisualizeSdf::VisualizeSdf(
        wgpu::Instance instance,
        wgpu::Device device,
        SdfNodeRef<Dual<float>> expr):
    _window(instance, device, vec2u(2048, 2048)),
    _sdf_eval(device),
    _sdf_expr(_sdf_eval, expr),
    _device(device),
    _vis_expr_layout {
        device,
        {
            storage_buffer_layout<SdfGpuOp>(0, BufferTarget::Read, wgpu::ShaderStage::Fragment),
            storage_buffer_layout<float>(1, BufferTarget::Read, wgpu::ShaderStage::Fragment),
            storage_buffer_layout<float>(2, BufferTarget::Read, wgpu::ShaderStage::Fragment),
        },
        "SDF visualizer expression layout",
    },
    _vis_pipeline(
        device,
        shader_from_file(device, "resource/shaders/sdf/visualize_sdf.wgsl"),
        wgpu::PrimitiveTopology::TriangleStrip,
        {_window.surface_format},
        {_sdf_eval.expr_layout()}
    ) {}

void VisualizeSdf::do_frame() {
    wgpu::TextureView backbuffer = _window.next_target();
    if (not backbuffer) return;
    wgpu::CommandEncoderDescriptor enc_desc;
    enc_desc.label = "Visualize SDF command encoder";
    wgpu::CommandEncoder encoder = _device.createCommandEncoder(enc_desc);
    
    wgpu::RenderPassDescriptor pass_desc;
    wgpu::RenderPassColorAttachment render_attachment = wgpu::Default;
    render_attachment.view = backbuffer;
    render_attachment.resolveTarget = nullptr;
    render_attachment.loadOp = wgpu::LoadOp::Clear;
    render_attachment.storeOp = wgpu::StoreOp::Store;
    float c = 5 / 255;
    render_attachment.clearValue = WGPUColor{c, c, c, 1.};
    
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &render_attachment;
    pass_desc.depthStencilAttachment = nullptr;
    pass_desc.timestampWrites = nullptr;
    wgpu::RenderPassEncoder render_pass = encoder.beginRenderPass(pass_desc);
    
    // render stuff
    render_pass.setPipeline(_vis_pipeline);
    render_pass.setBindGroup(0, _sdf_expr.bindgroup(), 0, nullptr);
    render_pass.draw(4, 1, 0, 0); // draw one quad
    
    // clean up
    render_pass.end();
    wgpu::CommandBuffer cmd_buf = encoder.finish();
    render_pass.release();
    encoder.release();
    wgpu::Queue queue = _device.getQueue();
    queue.submit(cmd_buf);
    backbuffer.release();
    queue.release();
    
#if not defined(__EMSCRIPTEN__)
    _window.surface.present();
#endif
}

} // namespace stereo
