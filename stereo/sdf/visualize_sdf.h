#pragma once

#include <stereo/gpu/window.h>
#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/render_pipeline.h>
#include <stereo/sdf/sdf_eval.h>

namespace stereo {

struct VisualizeSdf {
private:
    
    Window       _window;
    SdfEvaluator _sdf_eval;
    SdfGpuExpr   _sdf_expr;
    
    wgpu::Device    _device;
    // we need our own bindgroup because the layout effectively specifies
    // whether it talks to a compute or a render pipeline
    BindGroupLayout _vis_expr_layout;
    BindGroup       _vis_expr_bindgroup;
    RenderPipeline  _vis_pipeline;

public:
    
    VisualizeSdf(
            wgpu::Instance instance,
            wgpu::Device device,
            SdfNodeRef<Dual<float>> expr);
    
    void do_frame();
    
    const Window* window() const { return &_window; }
    
};

} // namespace stereo
