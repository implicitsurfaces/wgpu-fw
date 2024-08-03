#include <iostream>
#include <thread>
#include <chrono>

#include <stereo/gpu/window.h>
#include <stereo/app/solver.h>

using namespace stereo;
using namespace std::chrono_literals;

#define RUN_ONCE 0

// todo: mip generation probably does not handle edges correctly

const char* WGSL_SHD_SRC = R"(
@group(0) @binding(0) var tex_image: texture_2d<f32>;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0)       uv:       vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    var v = array<vec2f, 4>(
        vec2f(-1,  1),
        vec2f(-1, -1),
        vec2f( 1,  1),
        vec2f( 1, -1)
    );
	var p = v[in_vertex_index % 4];
    var out: VertexOutput;
	out.position = vec4f(p, 0.0, 1.0);
    out.uv       = p * 0.5 + 0.5;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let mip_level: i32 = 4;
    let tex_size: vec2f = vec2f(textureDimensions(tex_image, mip_level));
    let uv = vec2f(1. - in.uv.x, 1. - in.uv.y);
    let st: vec2<u32> = vec2<u32>(uv * tex_size);
    let c = textureLoad(tex_image, st, mip_level).rgb;
    return vec4f(pow(c, vec3f(2.2)), 1.);
}

)";

struct Viewer {
    Window                window;
    StereoSolver*         solver;
    wgpu::BindGroupLayout bind_layout = nullptr;
    
    Viewer(StereoSolver* solver):
        window(solver->instance(), solver->device()),
        solver(solver)
    {
        init();
    }
    
    ~Viewer() {
        if (bind_layout) bind_layout.release();
    }
    
    void init() {
        wgpu::ShaderModuleDescriptor shd_dsc;
        shd_dsc.hintCount = 0;
        shd_dsc.hints     = nullptr;
        
        // load shader
        wgpu::ShaderModuleWGSLDescriptor wgsl_dsc;
        wgsl_dsc.chain.next  = nullptr;
        wgsl_dsc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
        shd_dsc.nextInChain  = &wgsl_dsc.chain;
        wgsl_dsc.code        = WGSL_SHD_SRC;
        wgpu::ShaderModule shd = window.device.createShaderModule(shd_dsc);
        
        // set up vertex shader
        wgpu::RenderPipelineDescriptor pl_dsc;
        pl_dsc.vertex.bufferCount   = 0;
        pl_dsc.vertex.buffers       = nullptr;
        pl_dsc.vertex.module        = shd;
        pl_dsc.vertex.entryPoint    = "vs_main";
        pl_dsc.vertex.constantCount = 0;
        pl_dsc.vertex.constants     = nullptr;
        
        // define geometry type
        pl_dsc.primitive.topology = wgpu::PrimitiveTopology::TriangleStrip;
        pl_dsc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
        pl_dsc.primitive.frontFace = wgpu::FrontFace::CCW;
        pl_dsc.primitive.cullMode  = wgpu::CullMode::None;
        
        // set up fragment shader
        wgpu::FragmentState frg_state;
        frg_state.module        = shd;
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
        
        // output format
        wgpu::ColorTargetState target;
        target.format         = window.surface_format;
        target.blend          = &blend;
        target.writeMask      = wgpu::ColorWriteMask::All;
        frg_state.targetCount = 1;
        frg_state.targets     = &target;
        pl_dsc.fragment       = &frg_state;
        
        // depth/stencil state
        pl_dsc.depthStencil      = nullptr;
        pl_dsc.multisample.count = 1;
        pl_dsc.multisample.mask  = ~0u;
        pl_dsc.multisample.alphaToCoverageEnabled = false;
        pl_dsc.layout = nullptr;
        
        // define texture and uniform binding layout
        std::array<wgpu::BindGroupLayoutEntry, 1> bind_entries;
        bind_entries[0].binding               = 0;
        bind_entries[0].texture.sampleType    = wgpu::TextureSampleType::Float;
        bind_entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
        bind_entries[0].visibility            = wgpu::ShaderStage::Fragment;
        
        wgpu::BindGroupLayoutDescriptor bind_layout_dsc {};
        bind_layout_dsc.entryCount = bind_entries.size();
        bind_layout_dsc.entries    = bind_entries.data();
        bind_layout = window.device.createBindGroupLayout(bind_layout_dsc);
        
        wgpu::PipelineLayoutDescriptor pl_layout_dsc;
        pl_layout_dsc.bindGroupLayoutCount = 1;
        pl_layout_dsc.bindGroupLayouts     = (WGPUBindGroupLayout*) &bind_layout;
        pl_dsc.layout = window.device.createPipelineLayout(pl_layout_dsc);
        
        window.pipeline = window.device.createRenderPipeline(pl_dsc);
        shd.release();
    }
    
    void do_frame() {
        // Get the next target texture view
        wgpu::TextureView target_view = window.next_target();
        if (not target_view) return;
        
        // Create a command encoder for the draw call
        wgpu::CommandEncoderDescriptor encoder_desc = {};
        encoder_desc.label = "Render command encoder";
        wgpu::CommandEncoder encoder = window.device.createCommandEncoder(encoder_desc);
        
        // Create the render pass that clears the screen with our color
        wgpu::RenderPassDescriptor pass_desc;
        
        // The attachment part of the render pass descriptor describes the target texture of the pass
        wgpu::RenderPassColorAttachment render_attachment = {};
        render_attachment.view          = target_view;
        render_attachment.resolveTarget = nullptr;
        render_attachment.loadOp        = wgpu::LoadOp::Clear;
        render_attachment.storeOp       = wgpu::StoreOp::Store;
        render_attachment.clearValue    = WGPUColor{ 0.5, 0.5, 0.5, 1.0 };
        
        pass_desc.colorAttachmentCount   = 1;
        pass_desc.colorAttachments       = &render_attachment;
        pass_desc.depthStencilAttachment = nullptr;
        pass_desc.timestampWrites        = nullptr;
        
        wgpu::RenderPassEncoder render_pass = encoder.beginRenderPass(pass_desc);
        
        // select which render pipeline to use
        render_pass.setPipeline(window.pipeline);
        
        // bind the texture to the fragment shader
        // wgpu::TextureView tex_view = solver->frame_source(0).mip.views[0];
        
        // make a texture view
        wgpu::Texture src_tex = solver->frame_source(0).filtered.laplace.texture();
        wgpu::TextureViewDescriptor view_desc = wgpu::Default;
        view_desc.aspect           = wgpu::TextureAspect::All;
        view_desc.baseArrayLayer   = 0;
        view_desc.arrayLayerCount  = 1;
        view_desc.baseMipLevel     = 0;
        view_desc.mipLevelCount    = src_tex.getMipLevelCount();
        view_desc.label            = "presentation texture view";
        view_desc.dimension        = wgpu::TextureViewDimension::_2D;
        view_desc.format           = src_tex.getFormat();
        wgpu::TextureView tex_view = src_tex.createView(view_desc);
        
        wgpu::BindGroupEntry bind_entry = {};
        bind_entry.binding     = 0;
        bind_entry.textureView = tex_view;
        
        wgpu::BindGroupDescriptor bgd;
        bgd.layout     = bind_layout;
        bgd.entryCount = 1;
        bgd.entries    = &bind_entry;
        
        wgpu::BindGroup bind_group = window.device.createBindGroup(bgd);
        render_pass.setBindGroup(0, bind_group, 0, nullptr);
        
        // draw one quad
        render_pass.draw(4, 1, 0, 0);
        render_pass.end();
        render_pass.release();
        
        // Finally encode and submit the render pass
        wgpu::CommandBufferDescriptor cmd_dsc = {};
        cmd_dsc.label = "Command buffer";
        wgpu::CommandBuffer command = encoder.finish(cmd_dsc);
        encoder.release();
        
        window.queue.submit(1, &command);
        command.release();
        
        target_view.release();
        bind_group.release();
        
    #if not defined(__EMSCRIPTEN__)
        window.surface.present();
    #endif
    }
    
};

int main(int argc, char** argv) {
    CaptureRef cap = std::make_shared<cv::VideoCapture>(0);
    while (not cap->isOpened()) {
        // this can happen if the host's security policy requires
        // user approval to start the camera. wait for it to be ready.
        cap->open(0);
        std::this_thread::sleep_for(100ms);
    }
    StereoSolver solver {{cap}};
    Viewer viewer {&solver};
    solver.capture(0);
#if RUN_ONCE
    viewer.do_frame();
#else
    while (not glfwWindowShouldClose(viewer.window.window)) {
        glfwPollEvents();
        solver.capture(0);
        viewer.do_frame();
        solver.device().poll(false); // xxx what does this do...?
        // todo: control frame rate
    }
#endif
    std::cout << "finished!" << std::endl;
}
