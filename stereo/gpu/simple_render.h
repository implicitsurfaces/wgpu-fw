#pragma once

#include <stereo/gpu/render_pipeline.h>
#include <stereo/gpu/texture.h>
#include <stereo/gpu/buffer.h>
#include <stereo/gpu/uniform.h>
#include <stereo/gpu/model.h>
#include <stereo/gpu/camera.h>
#include <stereo/gpu/shader.h>
#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/material.h>
#include <stereo/gpu/uniform.h>

namespace stereo {

using ModelId = uint32_t;

struct SimpleRender {
    
    struct DistantLight {
        float   intensity;
        vec3gpu dir;
        vec3gpu color;
    };
    
    struct AmbientLight {
        float intensity;
        vec3gpu axis;
        vec3gpu color_a; // color along +axis
        vec3gpu color_b; // color along -axis
    };
    
    struct Lighting {
        DistantLight key;
        AmbientLight ambient;
    };
    
    struct ModelContext {
        ModelRef     model;
        xf3d         xf;
        OverlayColor overlay;
    };
    
private:

    struct OverlayUniforms {
        vec4gpu tint     = vec4(1.);
        vec4gpu additive = vec4(0.);
        vec4gpu over     = vec4(0.);
        
        OverlayUniforms() = default;
        OverlayUniforms(const OverlayColor& oc);
        
        OverlayUniforms& operator=(const OverlayColor& oc);
    };
    
    struct ObjectUniforms {
        mat4gpu         obj2w;
        mat4gpu         w2obj;
        OverlayUniforms overlay;
    };
    
    struct CameraUniforms {
        mat4gpu w2cam;
        mat4gpu cam2w;
        mat4gpu P;
        float   exposure = 0.;
    };
    
    struct GpuVert {
        vec3gpu p;
        vec3gpu n;
        vec2gpu uv;
        GpuVert& operator=(const Model::Vert& other);
    };
    
    struct ModelData {
        ModelContext         context;
        DataBuffer<GpuVert>  vertex_buffer;
        DataBuffer<uint32_t> index_buffer;
        DataBuffer<UniformBox<ObjectUniforms>> object_uniforms;
        BindGroup            binding;
    };
    
    struct MaterialData {
        MaterialRef material;
        DataBuffer<UniformBox<MaterialCoeffs>> buffer;
        BindGroup binding;
        
        MaterialData(MaterialRef material, BindGroupLayout& material_layout);
        void update(MaterialRef material, BindGroupLayout& material_layout);
    };
    
    friend struct MaterialData;
    
    wgpu::Device  _device;
    wgpu::Sampler _sampler;
    
    DataBuffer<UniformBox<CameraUniforms>> _camera_uniforms;
    DataBuffer<UniformBox<Lighting>>       _lighting_buffer;
    
    BindGroupLayout _globals_layout;
    BindGroupLayout _material_layout;
    BindGroupLayout _object_layout;
    
    RenderPipeline  _pipeline;
    
    DenseMap<ModelId, ModelData>       _models;
    DenseMap<MaterialId, MaterialData> _materials;
    MaterialId _max_material_id = 1; // 0 is reserved for the default material
    ModelId    _max_model_id = 1;
    
    TextureRef   _white_tex;
    TextureRef   _black_tex;
    TextureRef   _grey_tex;
    MaterialData _default_material;
    
    BindGroup _globals_binding;
    
    void _update_prims(ModelData& data);
    void _update_geometry(ModelData& data);

public:
    
    SimpleRender(wgpu::Device device);
    
    MaterialId  add_material(MaterialRef material);
    void        set_material(MaterialId id, MaterialRef material);
    MaterialRef get_material(MaterialId id) const;
    std::optional<MaterialRef> remove_material(MaterialId id);
    
    ModelId add_model(ModelRef model, xf3d xf={}, OverlayColor overlay={});
    void    set_model(ModelId id, ModelContext ctx);
    void    set_model_xf(ModelId id, xf3d xf);
    void    set_model_overlay(ModelId id, OverlayColor overlay);
    std::optional<ModelContext> remove_model(ModelId id);
    void    clear_models();
    // the the model, or null if no such model
    const ModelContext* get_model(ModelId id) const;
    
    void clear();
    
    void set_lighting(const Lighting& lighting);
    
    TextureRef white_tex() const { return _white_tex; }
    TextureRef black_tex() const { return _black_tex; }
    TextureRef grey_tex()  const { return _grey_tex; }
    
    wgpu::Device& device() { return _device; }
    
    void render(
        const Camera<double>& cam,
        float exposure,
        wgpu::TextureView target_view,
        wgpu::TextureView depth_view
    );
    
};

} // namespace stereo
