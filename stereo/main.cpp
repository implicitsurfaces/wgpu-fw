#include <iostream>
#include <thread>
#include <chrono>

#include <stereo/gpu/render_pipeline.h>
#include <stereo/gpu/shader.h>
#include <stereo/gpu/window.h>
#include <stereo/app/solver.h>
#include <stereo/app/visualize.h>

using namespace stereo;
using namespace std::chrono_literals;

#define RUN_ONCE 0
#define ABORT_ON_ERROR 1

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
    let mip_level: i32 = 2;
    let tex_size: vec2f = vec2f(textureDimensions(tex_image, mip_level));
    let uv = vec2f(1. - in.uv.x, 1. - in.uv.y);
    let st: vec2<u32> = vec2<u32>(uv * tex_size);
    let c = 0.5 * -textureLoad(tex_image, st, mip_level).rgb + 0.5;
    return vec4f(pow(c, vec3f(2.2)), 1.);
}

)";

struct Viewer {
    Window                window;
    StereoSolver*         solver;
    wgpu::BindGroupLayout bind_layout = nullptr;
    wgpu::BindGroup       bind_group = nullptr;
    RenderPipeline        pipeline;
    
    Viewer(StereoSolver* solver):
        window(solver->instance(), solver->device()),
        solver(solver),
        bind_layout(create_bind_group_layout()),
        pipeline(
            solver->device(),
            shader_from_str(solver->device(), WGSL_SHD_SRC, "filter visualizer shader"),
            wgpu::PrimitiveTopology::TriangleStrip,
            {window.surface_format},
            {bind_layout}
        )
    {
        _init();
    }
    
    ~Viewer() {
        _release();
    }
    
    void _init() {
        wgpu::BindGroupEntry bind_entry = {};
        bind_entry.binding     = 0;
        bind_entry.textureView = solver->frame_source(0).filtered.laplace.view();
        
        wgpu::BindGroupDescriptor bgd;
        bgd.layout     = bind_layout;
        bgd.entryCount = 1;
        bgd.entries    = &bind_entry;
        
        bind_group = window.device.createBindGroup(bgd);
    }
    
    void _release() {
        release_all(bind_layout, bind_group);
    }
    
    wgpu::BindGroupLayout create_bind_group_layout() {
        std::array<wgpu::BindGroupLayoutEntry, 1> bind_entries;
        bind_entries[0].binding               = 0;
        bind_entries[0].texture.sampleType    = wgpu::TextureSampleType::Float;
        bind_entries[0].texture.viewDimension = wgpu::TextureViewDimension::_2D;
        bind_entries[0].visibility            = wgpu::ShaderStage::Fragment;
        
        wgpu::BindGroupLayoutDescriptor bind_layout_dsc {};
        bind_layout_dsc.entryCount = bind_entries.size();
        bind_layout_dsc.entries    = bind_entries.data();
        return window.device.createBindGroupLayout(bind_layout_dsc);
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
        
        // The attachment part of the render pass descriptor
        // describes the target texture of the pass
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
        render_pass.setPipeline(pipeline.pipeline());
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
        // bind_group.release();
        
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
    // Viewer viewer {&solver};
    Visualizer viewer {{cap}};
    viewer.solver->capture(0);
#if RUN_ONCE
    viewer.do_frame();
#else
    bool ok = true;
    while (not glfwWindowShouldClose(viewer.window.window)) {
        glfwPollEvents();
        viewer.solver->capture(0);
        viewer.do_frame();
        viewer.solver->device().poll(false); // xxx what does this do...?
        // todo: control frame rate
        if (ABORT_ON_ERROR and viewer.solver->has_error()) {
            ok = false;
            break;
        }
    }
#endif
    if (ok) std::cout << "finished!" << std::endl;
    else    std::cerr << "Aborted."   << std::endl;
}
