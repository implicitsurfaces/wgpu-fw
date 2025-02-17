#include <stereo/gpu/simple_render.h>

namespace stereo {

static constexpr size_t TexSize = 32;
    
static wgpu::Sampler _make_sampler(wgpu::Device device) {
    wgpu::SamplerDescriptor tex_sampler = wgpu::Default;
    tex_sampler.addressModeU  = wgpu::AddressMode::ClampToEdge;
    tex_sampler.addressModeV  = wgpu::AddressMode::ClampToEdge;
    tex_sampler.addressModeW  = wgpu::AddressMode::ClampToEdge;
    tex_sampler.magFilter     = wgpu::FilterMode::Linear;
    tex_sampler.minFilter     = wgpu::FilterMode::Linear;
    tex_sampler.mipmapFilter  = wgpu::MipmapFilterMode::Linear;
    tex_sampler.compare       = wgpu::CompareFunction::Undefined; // ???
    tex_sampler.maxAnisotropy = 16;
    return device.createSampler(tex_sampler);
}

static void _fill_texture(TextureRef tex, vec4 color) {
    gpu_size_t mips = tex->num_mip_levels();
    gpu_size_t w = tex->width();
    gpu_size_t h = tex->height();
    std::vector<uint8_t> tex_data(w * h * 4);
    for (size_t i = 0; i < w * h; ++i) {
        tex_data[i * 4 + 0] = color[0] * 255;
        tex_data[i * 4 + 1] = color[1] * 255;
        tex_data[i * 4 + 2] = color[2] * 255;
        tex_data[i * 4 + 3] = color[3] * 255;
    }
    for (size_t i = 0; i < mips; ++i) {
        tex->submit_write(tex_data.data(), 4, i);
        w = std::max<gpu_size_t>(w / 2, 1);
        h = std::max<gpu_size_t>(w / 2, 1);
    }
}

SimpleRender::GpuVert& SimpleRender::GpuVert::operator=(const Model::Vert& v) {
    p  = v.p;
    n  = v.n;
    uv = v.uv;
    return *this;
}

SimpleRender::SimpleRender(wgpu::Device device):
    _device(device),
    _sampler(_make_sampler(device)),
    // uniform buffers
    _camera_uniforms {device, 1, BufferKind::Uniform, wgpu::BufferUsage::CopyDst},
    _lighting_buffer {device, 1, BufferKind::Uniform, wgpu::BufferUsage::CopyDst},
    // binding layouts
    _globals_layout {
        device,
        {
            uniform_layout<UniformBox<CameraUniforms>>(
                0, false, wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment
            ),
            uniform_layout<UniformBox<Lighting>>(
                1, false, wgpu::ShaderStage::Fragment
            ),
            sampler_layout(2, wgpu::ShaderStage::Fragment),
        },
        "simple render globals layout",
    },
    _material_layout {
        device,
        {
            uniform_layout<UniformBox<MaterialCoeffs>>(
                0, false, wgpu::ShaderStage::Fragment
            ),
            texture_layout(1, wgpu::ShaderStage::Fragment), // diffuse
            texture_layout(2, wgpu::ShaderStage::Fragment), // spec
            texture_layout(3, wgpu::ShaderStage::Fragment), // emit
            texture_layout(4, wgpu::ShaderStage::Fragment), // rough
            texture_layout(5, wgpu::ShaderStage::Fragment), // metal
            texture_layout(6, wgpu::ShaderStage::Fragment), // alpha
            texture_layout(7, wgpu::ShaderStage::Fragment), // normal
        },
        "simple render material layout"
    },
    _object_layout {
        device,
        {
            uniform_layout<UniformBox<ObjectUniforms>>(
                0,
                true, 
                wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment
            ),
        },
        "simple render object layout"
    },
    _pipeline {
        device,
        shader_from_file(device, "resource/shaders/simple_render.wgsl"),
        wgpu::PrimitiveTopology::TriangleList,
        {wgpu::TextureFormat::BGRA8UnormSrgb}, // todo: more attachments
        {
            _globals_layout,
            _material_layout,
            _object_layout,
        },
        get_blend_over(),
        // vertex buffer attrs
        &GpuVert::p,
        &GpuVert::n,
        &GpuVert::uv,
    },
    // textures:
    _white_tex {
        std::make_shared<Texture>(
            device,
            vec2ui{TexSize, TexSize},
            wgpu::TextureFormat::RGBA8Unorm,
            "white texture"
        )
    },
    _black_tex {
        std::make_shared<Texture>(
            device,
            vec2ui{TexSize, TexSize},
            wgpu::TextureFormat::RGBA8Unorm,
            "black texture"
        )
    },
    _grey_tex {
        std::make_shared<Texture>(
            device,
            vec2ui{TexSize, TexSize},
            wgpu::TextureFormat::RGBA8Unorm,
            "grey texture"
        )
    },
    _default_material {
        std::make_shared<Material>(
            _white_tex, // diffuse
            _white_tex, // spec
            _black_tex, // emit
            _white_tex, // rough
            _white_tex, // metal
            _white_tex, // alpha
            _grey_tex,  // normal
            // coeffs:
            MaterialCoeffs {
                vec3(0.18), // diffuse
                vec3(0.25), // spec
                vec3(0.),   // emit
                0.15,       // rough
                1.,         // metal
                1.,         // alpha
            }
        ),
        _material_layout
    },
    // bindings
    _globals_binding {
        _device,
        _globals_layout,
        {
            buffer_entry(0, _camera_uniforms, 1),
            buffer_entry(1, _lighting_buffer, 1),
            sampler_entry(2, _sampler),
        },
        "simple render globals bind group"
    }
{
    // populate the textures
    _fill_texture(_white_tex, vec4(1.));
    _fill_texture(_black_tex, vec4(0.));
    _fill_texture(_grey_tex,  vec4(0.5, 0.5, 0.5, 1.));
    
    // populate the lighting
    UniformBox<Lighting> lighting = Lighting {
        {
            2.,                 // key intensity
            vec3(1, 1, 2),      // sun direction
            vec3(1, 0.98, 0.9), // sun color
        },
        {
            1.,                     // ambient intensity
            vec3(0, 1, 0),          // gradient axis
            vec3(.627, .741, 1.00), // +axis color (blue / sky)
            vec3(.343, .282, .176), // -axis color (tan / ground)
        }
    };
    _lighting_buffer.submit_write(lighting, 0);
}

void SimpleRender::_update_geometry(ModelData &data) {
    if (not data.context.model) return;
    const ModelRef &model = data.context.model;

    size_t vcount = model->verts.size();
    size_t icount = model->indices.size();

    if (not data.vertex_buffer or data.vertex_buffer.size() != vcount) {
        data.vertex_buffer = DataBuffer<GpuVert>(
            _device, 
            vcount, 
            BufferKind::Vertex, 
            wgpu::BufferUsage::CopyDst
        );
    }
    if (not data.index_buffer or data.index_buffer.size() != icount) {
        data.index_buffer = DataBuffer<uint32_t>(
            _device, 
            icount, 
            BufferKind::Index, 
            wgpu::BufferUsage::CopyDst
        );
    }

    std::vector<GpuVert> gpu_verts(vcount);
    for (size_t i = 0; i < vcount; ++i) {
        gpu_verts[i] = model->verts[i];
    }

    data.vertex_buffer.submit_write(gpu_verts);
    data.index_buffer.submit_write(model->indices);
}

void SimpleRender::_update_prims(ModelData& data) {
    if (not data.context.model) return;
    const ModelRef &model = data.context.model;
    
    // Ensure we have enough uniform buffer space for all primitives
    size_t prim_count = model->prims.size();
    if (not data.object_uniforms or data.object_uniforms.size() != prim_count) {
        data.object_uniforms = DataBuffer<UniformBox<ObjectUniforms>>(
            _device,
            prim_count,
            BufferKind::Uniform,
            wgpu::BufferUsage::CopyDst
        );
        data.binding = BindGroup(
            _device,
            _object_layout,
            {
                buffer_entry(0, data.object_uniforms, 1),
            },
            "simple render object bind group"
        );
    }

    // Pre-compute model transform
    const xf3d& model_xf = data.context.xf;
    
    // Update uniforms for each primitive
    std::vector<UniformBox<ObjectUniforms>> uniforms(prim_count);
    for (size_t i = 0; i < prim_count; i++) {
        const Model::Prim& prim = model->prims[i];
        
        // Compose the model transform with the primitive's local transform
        xf3d combined_xf = model_xf * (xf3d) prim.obj_to_world;
        
        uniforms[i]->obj2w   = combined_xf.mat;
        uniforms[i]->w2obj   = combined_xf.inv;
        uniforms[i]->overlay = data.context.overlay;
    }
    
    // Upload all uniform data at once
    data.object_uniforms.submit_write(uniforms);
}


////////////////////////////////////////////////////////////////////////////////
// Material API
////////////////////////////////////////////////////////////////////////////////

SimpleRender::MaterialData::MaterialData(
        MaterialRef material,
        BindGroupLayout& material_layout):
    buffer {
        material->diffuse->device(),
        1,
        BufferKind::Uniform, 
        wgpu::BufferUsage::CopyDst
    }
{
    update(material, material_layout);
}

void SimpleRender::MaterialData::update(MaterialRef material, BindGroupLayout& layout) {
    this->material = material;
    // 1) Upload the MaterialCoeffs
    UniformBox<MaterialCoeffs> coeffs = material->coeffs;
    buffer.submit_write(coeffs, 0);

    // 2) Re-create or update the bind group with the textures
    binding = BindGroup(
        material->diffuse->device(),
        layout,
        {
            buffer_entry (0, buffer, 1),
            texture_entry(1, *material->diffuse),
            texture_entry(2, *material->specular),
            texture_entry(3, *material->emission),
            texture_entry(4, *material->roughness),
            texture_entry(5, *material->metallic),
            texture_entry(6, *material->alpha),
            texture_entry(7, *material->normal),
        },
        "simple_render material bind group"
    );
}

MaterialId SimpleRender::add_material(MaterialRef material) {
    // Allocate a new MaterialId
    MaterialId new_id = ++_max_material_id;
    // Delegate to set_material to handle creation
    set_material(new_id, material);
    return new_id;
}

void SimpleRender::set_material(MaterialId id, MaterialRef material) {
    _max_material_id = std::max(_max_material_id, id);
    // If material does not exist in the map, create it; otherwise update it
    auto it = _materials.find(id);
    if (it == _materials.end()) {
        // Create new material data
        _materials.emplace(id, MaterialData(material, _material_layout));
    } else {
        // Update existing material data
        MaterialData& md = it->second;
        md.update(material, _material_layout);
    }
}

MaterialRef SimpleRender::get_material(MaterialId id) const {
    auto it = _materials.find(id);
    if (it == _materials.end()) {
        return _default_material.material;
    }
    return it->second.material;
}

std::optional<MaterialRef> SimpleRender::remove_material(MaterialId id) {
    auto it = _materials.find(id);
    if (it == _materials.end()) {
        return std::nullopt;
    } else {
        MaterialRef mat = it->second.material;
        _materials.erase(it);
        return mat;
    }
}

SimpleRender::OverlayUniforms::OverlayUniforms(const OverlayColor& overlay):
    tint(overlay.tint.value_or(vec4(1.))),
    additive(overlay.additive.value_or(vec4(0.))),
    over(overlay.over.value_or(vec4(0.))) {}

SimpleRender::OverlayUniforms& SimpleRender::OverlayUniforms::operator=(
        const OverlayColor& overlay)
{
    tint     = overlay.tint.value_or(vec4(1.));
    additive = overlay.additive.value_or(vec4(0.));
    over     = overlay.over.value_or(vec4(0.));
    return *this;
}

////////////////////////////////////////////////////////////////////////////////
// Model API
////////////////////////////////////////////////////////////////////////////////

ModelId SimpleRender::add_model(ModelRef model, xf3d xf, OverlayColor overlay) {
    ModelId id = ++_max_model_id;

    ModelContext ctx;
    ctx.model   = model;
    ctx.xf      = xf;
    ctx.overlay = overlay;

    // Delegate creation to set_model
    set_model(id, ctx);
    return id;
}

void SimpleRender::set_model(ModelId id, ModelContext ctx) {
    _max_model_id = std::max(_max_model_id, id);
    auto it = _models.find(id);
    if (it == _models.end()) {
        // Insert new ModelData
        ModelData data;
        data.context = ctx;
        
        _update_geometry(data);
        _update_prims(data);
        
        _models[id] = std::move(data);
    } else {
        // Update existing
        ModelData& data = it->second;
        data.context = ctx;

        _update_geometry(data);
        _update_prims(data);
    }
}

void SimpleRender::set_model_xf(ModelId id, xf3d xf) {
    auto it = _models.find(id);
    if (it == _models.end()) {
        return;
    } else {
        it->second.context.xf = xf;
        // update all the prims' transforms
        _update_prims(it->second);
    }
}

void SimpleRender::set_model_overlay(ModelId id, OverlayColor overlay) {
    auto it = _models.find(id);
    if (it == _models.end()) {
        return;
    } else {
        it->second.context.overlay = overlay;
        // update all the per-prim overlays
        _update_prims(it->second);
    }
}

const SimpleRender::ModelContext* SimpleRender::get_model(ModelId id) const {
    auto it = _models.find(id);
    if (it == _models.end()) {
        return nullptr;
    }
    return &it->second.context;
}

std::optional<SimpleRender::ModelContext> SimpleRender::remove_model(ModelId id) {
    auto it = _models.find(id);
    if (it == _models.end()) {
        return std::nullopt;
    } else {
        ModelContext ctx = std::move(it->second.context);
        _models.erase(it);
        return ctx;
    }
}

void SimpleRender::clear() {
    _models.clear();
    _materials.clear();
}

void SimpleRender::clear_models() {
    _models.clear();
}

////////////////////////////////////////////////////////////////////////////////
// Environment API
////////////////////////////////////////////////////////////////////////////////

void SimpleRender::set_lighting(const Lighting& lighting) {
    UniformBox<Lighting> lighting_uniform = lighting;
    _lighting_buffer.submit_write(lighting_uniform, 0);
}

////////////////////////////////////////////////////////////////////////////////
// Rendering
////////////////////////////////////////////////////////////////////////////////

void SimpleRender::render(
        const Camera<double>& cam,
        float exposure,
        wgpu::TextureView target_view,
        wgpu::TextureView depth_view)
{
    // submit camera uniforms
    UniformBox<CameraUniforms> cam_uniforms = CameraUniforms {
        .w2cam    = cam.cam_to_world.inv,
        .cam2w    = cam.cam_to_world.mat,
        .P        = cam.compute_projection(),
        .exposure = exposure,
    };
    _camera_uniforms.submit_write(cam_uniforms, 0);
    
    // set up render pass
    wgpu::RenderPassColorAttachment color_attachment = wgpu::Default;
    color_attachment.view       = target_view;
    color_attachment.loadOp     = wgpu::LoadOp::Clear;
    color_attachment.storeOp    = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.07, 0.07, 0.07, 1};
    
    wgpu::RenderPassDepthStencilAttachment depth_attachment = wgpu::Default;
    depth_attachment.view = depth_view;
    depth_attachment.depthLoadOp     = wgpu::LoadOp::Clear;
    depth_attachment.depthStoreOp    = wgpu::StoreOp::Store;
    depth_attachment.depthClearValue = 1.0f; // "far"
    
    // Stencil setup, mandatory but unused
    depth_attachment.stencilClearValue = 0;
    depth_attachment.stencilLoadOp     = wgpu::LoadOp::Clear;
    depth_attachment.stencilStoreOp    = wgpu::StoreOp::Store;
    depth_attachment.stencilReadOnly   = true;
    
    wgpu::RenderPassDescriptor pass_desc = wgpu::Default;
    pass_desc.colorAttachments       = &color_attachment;
    pass_desc.colorAttachmentCount   = 1;
    pass_desc.depthStencilAttachment = &depth_attachment;
    
    // set up the render pass
    wgpu::CommandEncoder encoder = _device.createCommandEncoder(wgpu::Default);
    wgpu::RenderPassEncoder pass = encoder.beginRenderPass(pass_desc);
    
    // set up the pipeline
    pass.setPipeline(_pipeline.pipeline());
    pass.setBindGroup(0, _globals_binding, 0, nullptr);
    
    // render each model
    for (const auto& [id, data] : _models) {
        if (not data.context.model) continue;
        
        // set up vertex buffers
        pass.setVertexBuffer(
            0,
            data.vertex_buffer.buffer(),
            0,
            data.vertex_buffer.size() * sizeof(GpuVert)
        );
        pass.setIndexBuffer(
            data.index_buffer.buffer(),
            wgpu::IndexFormat::Uint32,
            0,
            data.index_buffer.size() * sizeof(uint32_t)
        );
        
        // draw each primitive
        for (size_t i = 0; i < data.context.model->prims.size(); ++i) {
            const Model::Prim& prim = data.context.model->prims[i];
            
            range1ui prim_range = prim.index_range;
            gpu_size_t uniform_offset = i * sizeof(UniformBox<ObjectUniforms>);
            
            auto material_it = _materials.find(prim.material_id);
            const BindGroup* material_binding = &_default_material.binding;
            if (material_it != _materials.end()) {
                material_binding = &material_it->second.binding;
            }
            
            pass.setBindGroup(1, *material_binding, 0, nullptr);
            pass.setBindGroup(2, data.binding, 1, &uniform_offset);
            
            // (n verts, n instances, start index, vertex offset, instance offset)
            pass.drawIndexed(prim_range.dimensions(), 1, prim_range.lo, 0, 0);
        }
    }
    
    pass.end();
    wgpu::CommandBuffer commands = encoder.finish(wgpu::Default);
    pass.release();
    encoder.release();
    
    wgpu::Queue queue = _device.getQueue();
    queue.submit(commands);
    queue.release();
}

} // namespace stereo
