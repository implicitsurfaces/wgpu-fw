#include <stereo/sdf/sdf_eval.h>
#include <stereo/gpu/shader.h>

namespace stereo {

constexpr gpu_size_t Wg_W = 8;
constexpr gpu_size_t Wg_H = 8;

size_t SdfGpuExpr::ExprBuffer::size() const { return params_x.size(); }

size_t SdfGpuExpr::ExprBuffer::extend(const float* v, size_t n) {
    params_x.insert(params_x.end(), v, v + n);
    params_dx.insert(params_dx.end(), n, 0.);
    return params_x.size();
}

size_t SdfGpuExpr::ExprBuffer::extend(const Dual<float>* v, size_t n) {
    params_x.reserve(size() + n);
    params_dx.reserve(size() + n);
    for (size_t i = 0; i < n; ++i) {
        params_x.push_back(v[i].x);
        params_dx.push_back(v[i].dx);
    }
    return params_x.size();
}

template <typename T>
requires (std::is_same_v<T, float> || std::is_same_v<T, Dual<float>>)
SdfGpuExpr::SdfGpuExpr(
        SdfEvaluator& evaluator,
        SdfNodeRef<T> expr,
        gpu_size_t param_x_variations,
        gpu_size_t param_dx_variations):
    _n_param_x_variations(param_x_variations),
    _n_param_dx_variations(param_dx_variations)
{
    wgpu::Device device = evaluator.device();
    
    // serialize expression
    ExprBuffer buf;
    _push_expr(buf, expr);
    
    // initialize buffers
    _ops = DataBuffer<SdfGpuOp>(
        device,
        buf.ops.size(),
        BufferKind::Storage,
        wgpu::BufferUsage::CopyDst
    );
    _params_x = DataBuffer<float>(
        device,
        param_x_variations * buf.size(),
        BufferKind::Storage,
        wgpu::BufferUsage::CopyDst
    );
    _params_dx = DataBuffer<float>(
        device,
        param_dx_variations * buf.size(),
        BufferKind::Storage,
        wgpu::BufferUsage::CopyDst
    );
    
    // upload data
    _ops.submit_write(buf.ops);
    int32_t n = buf.size();
    for (int32_t i = 0; i < param_x_variations; ++i) {
        // write a copy of the parameters for each variation
        _params_x.submit_write(buf.params_x.data(), {i * n, (i + 1) * n - 1});
    }
    for (int32_t i = 0; i < param_dx_variations; ++i) {
        // ...
        _params_dx.submit_write(buf.params_dx.data(), {i * n, (i + 1) * n - 1});
    }
    
    // init bindgroup
    _bindgroup = {
        device,
        evaluator.expr_layout(),
        {
            buffer_entry<SdfGpuOp>(0, _ops, _ops.size()),
            buffer_entry<float>(1, _params_x,  _params_x.size()),
            buffer_entry<float>(2, _params_dx, _params_dx.size()),
        },
        "SDF GPU expression bindgroup",
    };
}

// explicit template instantiation
template SdfGpuExpr::SdfGpuExpr(
    SdfEvaluator& evaluator,
    SdfNodeRef<float> expr,
    gpu_size_t param_x_variations,
    gpu_size_t param_dx_variations
);

template SdfGpuExpr::SdfGpuExpr(
    SdfEvaluator& evaluator,
    SdfNodeRef<Dual<float>> expr,
    gpu_size_t param_x_variations,
    gpu_size_t param_dx_variations
);

template <typename T>
void SdfGpuExpr::_push_expr(ExprBuffer& buf, SdfNodeRef<T> expr) {
    size_t n_params = expr->n_params();
    gpu_size_t param_start = buf.size();
    gpu_size_t param_end   = param_start;
    if (n_params > 0) {
        param_end = buf.extend(expr->params(), n_params);
    }
    uint32_t op_index = buf.size(); 
    buf.ops.push_back({
        .op          = expr->op(),
        .variant     = SdfOpVariant::None,
        .param_start = param_start,
        .param_end   = param_end
    });
    for (size_t i = 0; i < expr->n_children(); ++i) {
        _push_expr(buf, expr->child(i));
    }
    if (expr->transforms_domain()) {
        buf.ops.push_back({
            .op          = SdfOp::PopDomain,
            .push_index  = op_index,
            .param_start = 0,
            .param_end   = 0,
        });
    }
}

// explicit template instantiation
template void SdfGpuExpr::_push_expr(ExprBuffer&, SdfNodeRef<float>);
template void SdfGpuExpr::_push_expr(ExprBuffer&, SdfNodeRef<Dual<float>>);

SdfInput::SdfInput(SdfEvaluator& evaluator, gpu_size_t samples_x, gpu_size_t samples_dx):
    _samples_x (evaluator.device(), samples_x,  BufferKind::Storage, wgpu::BufferUsage::CopyDst),
    _samples_dx(evaluator.device(), samples_dx, BufferKind::Storage, wgpu::BufferUsage::CopyDst),
    _read_bindgroup {
        evaluator.device(),
        evaluator.samples_layout(),
        {
            buffer_entry<vec3gpu>(0, _samples_x,  _samples_x.size()),
            buffer_entry<vec3gpu>(1, _samples_dx, _samples_dx.size()),
        },
        "SDF input samples bindgroup",
    } {}

void SdfInput::write_samples_x(const vec3gpu* samples) {
    _samples_x.submit_write(samples, {0, n_samples_x() - 1});
}

void SdfInput::write_samples_dx(const vec3gpu* samples) {
    _samples_dx.submit_write(samples, {0, n_samples_dx() - 1});
}
    
SdfOutput::SdfOutput(SdfEvaluator& evaluator, gpu_size_t n_samples):
    _sdf_x    (evaluator.device(), n_samples, BufferKind::Storage, wgpu::BufferUsage::CopySrc),
    _sdf_dx   (evaluator.device(), n_samples, BufferKind::Storage, wgpu::BufferUsage::CopySrc),
    _normal_x (evaluator.device(), n_samples, BufferKind::Storage, wgpu::BufferUsage::CopySrc),
    _normal_dx(evaluator.device(), n_samples, BufferKind::Storage, wgpu::BufferUsage::CopySrc),
    _bindgroup {
        evaluator.device(),
        evaluator.output_layout(),
        {
            buffer_entry<float>  (0, _sdf_x,    _sdf_x.size()),
            buffer_entry<float>  (1, _sdf_dx,   _sdf_dx.size()),
            buffer_entry<vec3gpu>(2, _normal_x, _normal_x.size()),
            buffer_entry<vec3gpu>(3, _normal_dx,_normal_dx.size()),
        },
        "SDF output values bindgroup"
    } {}

std::pair<gpu_size_t, gpu_size_t> SdfEvaluator::_prepare_ranges(
    gpu_size_t samples,
    gpu_size_t n_variations,
    ParamVariation variation_scheme)
{
    // todo: handle "no variation" case and break up
    //   samples into smaller ranges with identical parameters.
    if (not _point_offsets or _point_offsets.size() < n_variations) {
        // buffer not created to the correct size. (re)create
        _point_offsets = DataBuffer<ParamOffset>(
            _device,
            n_variations,
            BufferKind::Storage,
            wgpu::BufferUsage::CopyDst
        );
        _param_offsets = DataBuffer<ParamOffset>(
            _device,
            n_variations,
            BufferKind::Storage,
            wgpu::BufferUsage::CopyDst
        );
        // make the bindgroup
        _offsets_bindgroup = {
            _device,
            _offsets_layout,
            {
                // point variation
                buffer_entry<ParamOffset>(0, _point_offsets, n_variations),
                // parameter variation
                buffer_entry<ParamOffset>(1, _param_offsets, n_variations),
                // work range uniform
                buffer_entry<PaddedWorkRange>(2, _work_range, 1),
            },
            "SDF parameter offsets bindgroup",
        };
    }
    std::vector<ParamOffset> buf {n_variations};
    // upload the offsets.
    // point variation first
    // (we are not doing point variation for now, so they all have zero offset
    _point_offsets.submit_write(buf.data(), {0, (int32_t) n_variations - 1});
    gpu_size_t x_stride  = variation_scheme == ParamVariation::VaryDerivative ? 0 : samples;
    gpu_size_t dx_stride = variation_scheme == ParamVariation::VaryParam      ? 0 : samples;
    for (gpu_size_t i = 0; i < n_variations; ++i) {
        buf[i] = {i * x_stride, i * dx_stride};
    }
    _param_offsets.submit_write(
        buf.data() + n_variations, 
        {
            0,
            (int32_t) n_variations - 1
        }
    );
    
    return {samples, n_variations};
}

SdfEvaluator::SdfEvaluator(wgpu::Device device):
    _device(device),
    _expr_layout {
        _device,
        {
            compute_r_buffer_layout<SdfGpuOp>(0),
            compute_r_buffer_layout<float>(1),
            compute_r_buffer_layout<float>(2),
        },
        "SDF expression bindgroup layout",
    },
    _samples_layout {
        _device,
        {
            compute_r_buffer_layout<vec3gpu>(0),
            compute_r_buffer_layout<vec3gpu>(1),
        },
        "SDF input samples layout",
    },
    _output_layout {
        _device,
        {
            compute_rw_buffer_layout<float>(0),
            compute_rw_buffer_layout<float>(1),
            compute_rw_buffer_layout<vec3gpu>(2),
            compute_rw_buffer_layout<vec3gpu>(3),
        },
        "SDF output values layout",
    },
    _offsets_layout {
        _device,
        {
            compute_r_buffer_layout<ParamOffset>(0),
            compute_r_buffer_layout<ParamOffset>(1),
            uniform_layout<PaddedWorkRange>(2),
        },
        "SDF parameter offsets layout",
    },
    _work_range {
        _device,
        1,
        BufferKind::Uniform,
        wgpu::BufferUsage::CopyDst,
    }
{
    wgpu::ShaderModule shader = shader_from_file(
        _device,
        "resource/shaders/sdf/sdf_eval_main.wgsl"
    );
    if (not shader) {
        std::cerr << "Failed to load SDF evaluation shader." << std::endl;
        std::abort();
    }
    
    _eval_pipeline = create_compute_pipeline(
        _device,
        shader,
        {
            _expr_layout,
            _samples_layout,
            _output_layout,
            _offsets_layout,
        },
        "SDF evaluation pipeline"
    );
}

SdfOutputRef SdfEvaluator::evaluate(
    const SdfGpuExpr& expr,
    const SdfInput& input,
    gpu_size_t param_variations,
    ParamVariation variation_scheme,
    SdfOutputRef output)
{
    // set up the output
    gpu_size_t sample_points = input.n_samples_x() * param_variations;
    if (output == nullptr or output->n_samples() < sample_points) {
        output = std::make_shared<SdfOutput>(*this, sample_points);
    }
    // set up the parameter ranges
    auto [samples, variations] = _prepare_ranges(
        input.n_samples_x(),
        param_variations,
        variation_scheme
    );
    // pass the explicit range to the shader
    PaddedWorkRange wr {samples, variations};
    _work_range.submit_write(wr, 0);
    
    gpu_size_t wg_x = ceil_div(samples,    Wg_W);
    gpu_size_t wg_y = ceil_div(variations, Wg_H);
    
    wgpu::CommandEncoder encoder = _device.createCommandEncoder();
    wgpu::ComputePassEncoder pass = encoder.beginComputePass();
    pass.setPipeline(_eval_pipeline);
    pass.setBindGroup(0, expr.bindgroup(),       0, nullptr);
    pass.setBindGroup(1, input.read_bindgroup(), 0, nullptr);
    pass.setBindGroup(2, output->bindgroup(),    0, nullptr);
    pass.setBindGroup(3, _offsets_bindgroup,     0, nullptr);
    pass.dispatchWorkgroups(wg_x, wg_y, 1);
    pass.end();
    wgpu::CommandBuffer commands = encoder.finish();
    
    wgpu::Queue queue = _device.getQueue();
    pass.release();
    encoder.release();
    queue.submit(commands);
    queue.release();
    
    return output;
}
    
} // namespace stereo
