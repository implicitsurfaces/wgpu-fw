#include <iostream>

#include <stereo/gpu/window.h>
#include <stereo/app/solver.h>

using namespace stereo;

const char* SHD_SRC = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
    var v = array<vec2f, 4>(
        vec2f(-1,  1),
        vec2f(-1, -1),
        vec2f( 1,  1),
        vec2f( 1, -1)
    );
	var p = v[in_vertex_index];
	return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";


void setup_pipeline(Window* window) {
    wgpu::ShaderModuleDescriptor shd_dsc;
    shd_dsc.hintCount = 0;
    shd_dsc.hints     = nullptr;
    
    wgpu::ShaderModuleWGSLDescriptor wgsl_dsc;
    wgsl_dsc.chain.next  = nullptr;
    wgsl_dsc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shd_dsc.nextInChain  = &wgsl_dsc.chain;
    wgsl_dsc.code        = SHD_SRC;
    wgpu::ShaderModule shd = window->device.createShaderModule(shd_dsc);
    
    wgpu::RenderPipelineDescriptor pl_dsc;
    pl_dsc.vertex.bufferCount = 0;
    pl_dsc.vertex.buffers     = nullptr;
    
    pl_dsc.vertex.module        = shd;
    pl_dsc.vertex.entryPoint    = "vs_main";
    pl_dsc.vertex.constantCount = 0;
    pl_dsc.vertex.constants     = nullptr;
    
    pl_dsc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
    pl_dsc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
    pl_dsc.primitive.frontFace = wgpu::FrontFace::CCW;
    pl_dsc.primitive.cullMode  = wgpu::CullMode::None;
    
    wgpu::FragmentState frg_state;
    frg_state.module        = shd;
    frg_state.entryPoint    = "fs_main";
    frg_state.constantCount = 0;
    frg_state.constants     = nullptr;
    
    wgpu::BlendState blend;
	blend.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
	blend.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
	blend.color.operation = wgpu::BlendOperation::Add;
	blend.alpha.srcFactor = wgpu::BlendFactor::Zero;
	blend.alpha.dstFactor = wgpu::BlendFactor::One;
	blend.alpha.operation = wgpu::BlendOperation::Add;
    
	wgpu::ColorTargetState target;
	target.format    = window->surface_format;
	target.blend     = &blend;
	target.writeMask = wgpu::ColorWriteMask::All;
    
    frg_state.targetCount = 1;
    frg_state.targets     = &target;
    pl_dsc.fragment       = &frg_state;
    
    pl_dsc.depthStencil      = nullptr;
    pl_dsc.multisample.count = 1;
    pl_dsc.multisample.mask  = ~0u;
    pl_dsc.multisample.alphaToCoverageEnabled = false;
    pl_dsc.layout = nullptr;
    
    window->pipeline = window->device.createRenderPipeline(pl_dsc);
    shd.release();
}


void do_frame(Window* window) {
    // Get the next target texture view
    wgpu::TextureView target_view = window->next_target();
    if (not target_view) return;
    
    // Create a command encoder for the draw call
    wgpu::CommandEncoderDescriptor encoder_desc = {};
    encoder_desc.label = "Render command encoder";
    wgpu::CommandEncoder encoder = window->device.createCommandEncoder(encoder_desc);
    
    // Create the render pass that clears the screen with our color
    wgpu::RenderPassDescriptor pass_desc;
    
    // The attachment part of the render pass descriptor describes the target texture of the pass
    wgpu::RenderPassColorAttachment render_attachment = {};
    render_attachment.view          = target_view;
    render_attachment.resolveTarget = nullptr;
    render_attachment.loadOp        = wgpu::LoadOp::Clear;
    render_attachment.storeOp       = wgpu::StoreOp::Store;
    render_attachment.clearValue    = WGPUColor{ 0.9, 0.1, 0.2, 1.0 };
    
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &render_attachment;
    pass_desc.depthStencilAttachment = nullptr;
    pass_desc.timestampWrites = nullptr;
    
    wgpu::RenderPassEncoder render_pass = encoder.beginRenderPass(pass_desc);
    
    // select which render pipeline to use
    render_pass.setPipeline(window->pipeline);
    // draw one quad
    render_pass.draw(4, 1, 0, 0);
    render_pass.end();
    render_pass.release();
    
    // Finally encode and submit the render pass
    wgpu::CommandBufferDescriptor cmd_dsc = {};
    cmd_dsc.label = "Command buffer";
    wgpu::CommandBuffer command = encoder.finish(cmd_dsc);
    encoder.release();
    
    window->queue.submit(1, &command);
    command.release();
    
    target_view.release();
    
#ifndef __EMSCRIPTEN__
    window->surface.present();
#endif
}


int main(int argc, char** argv) {
    CaptureRef cap = std::make_shared<cv::VideoCapture>(0);
    StereoSolver solver {{cap}};
    Window w {solver.instance(), solver.device()};
    setup_pipeline(&w);
    solver.capture(0);
    while (not glfwWindowShouldClose(w.window)) {
        glfwPollEvents();
        do_frame(&w);
        w.device.poll(false); // xxx what does this do...?
    }
    std::cout << "finished!" << std::endl;
}
