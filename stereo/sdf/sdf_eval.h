#pragma once

#include "stereo/gpu/uniform.h"
#include <geomc/linalg/Quaternion.h>

#include <stereo/sdf/sdf_structs.h>
#include <stereo/gpu/bindgroup.h>
#include <stereo/gpu/buffer.h>

// todo: handle case of disused point/variation threads
//   - more important: variation
//   - if zero variation, break up the points into eight groups (stride = samples / 8)
//     and hold variation constant (stride = 0)
//   > return from the setup calc the exact X and Y dimensions, bc of the above
// todo: readback functionality
// todo: means of going from a parameters back to an expr tree

namespace stereo {

enum struct ParamVariation {
    VaryParam,
    VaryDerivative,
    VaryBoth,
};

struct SdfGpuOp {
    SdfOp op;
    union {
        SdfOpVariant variant;
        uint32_t     push_index;
        int32_t      int_param;
    };
    gpu_size_t param_start;
    gpu_size_t param_end;
};

struct ParamOffset {
    gpu_size_t x_offset  = 0;
    gpu_size_t dx_offset = 0;
};

struct SdfEvaluator;
struct SdfGpuExpr;
struct SdfInput;
struct SdfOutput;

using SdfOutputRef = std::shared_ptr<SdfOutput>;

/**
 * Keeps a pipeline for evaluating SDFs.
 */
struct SdfEvaluator {
private:
    
    struct WorkRange {
        gpu_size_t n_samples;
        gpu_size_t n_variations;
    };
    
    using PaddedWorkRange = UniformBox<WorkRange>;
    
    wgpu::Device _device;
    
    // layouts
    BindGroupLayout _expr_layout;
    BindGroupLayout _samples_layout;
    BindGroupLayout _output_layout;
    BindGroupLayout _offsets_layout;
    
    // selection ranges binding
    DataBuffer<ParamOffset>     _param_offsets;
    DataBuffer<ParamOffset>     _point_offsets; // must have the same size as _param_offsets
    DataBuffer<PaddedWorkRange> _work_range;
    BindGroup                   _offsets_bindgroup;
    
    // compute pipeline
    wgpu::ComputePipeline _eval_pipeline;
    
    friend class SdfGpuExpr;
    friend class SdfInput;
    friend class SdfOutput;
    
protected:
    // return the sample count (x) by parameter variation count (y)
    std::pair<gpu_size_t, gpu_size_t> _prepare_ranges(
        gpu_size_t samples,
        gpu_size_t n_variations,
        ParamVariation variation_scheme
    );
    
public:
    
    SdfEvaluator(wgpu::Device device);

    wgpu::Device device() const { return _device; }
    
    SdfOutputRef evaluate(
        const SdfGpuExpr& expr,
        const SdfInput& input,
        gpu_size_t param_variations,
        ParamVariation variation_scheme=ParamVariation::VaryDerivative,
        SdfOutputRef output=nullptr
    );
    
    BindGroupLayout& expr_layout()     { return _expr_layout;    }
    BindGroupLayout& samples_layout()  { return _samples_layout; }
    BindGroupLayout& output_layout()   { return _output_layout;  }
    BindGroupLayout& offsets_layout()  { return _offsets_layout;  }
    
};


/**
 * @brief Represents an SDF expression tree that can be evaluated on the GPU.
 */
struct SdfGpuExpr {
private:
    DataBuffer<SdfGpuOp> _ops;
    DataBuffer<float>    _params_x;
    DataBuffer<float>    _params_dx;
    gpu_size_t           _n_param_x_variations;
    gpu_size_t           _n_param_dx_variations;
    BindGroup            _bindgroup;
    
    // temporary buffer for serializing an sdf expr
    struct ExprBuffer {
        std::vector<SdfGpuOp> ops;
        std::vector<float>    params_x;
        std::vector<float>    params_dx;
        
        size_t size() const;
        size_t extend(const float* v, size_t n);
        size_t extend(const Dual<float>* v, size_t n);
    };
    
    // serialize a sub-expr
    template <typename T>
    void _push_expr(ExprBuffer& buf, SdfNodeRef<T> expr);
    
public:
    
    template <typename T>
    requires (std::is_same_v<T, float> || std::is_same_v<T, Dual<float>>)
    SdfGpuExpr(
            SdfEvaluator& evaluator,
            SdfNodeRef<T> expr,
            gpu_size_t param_x_variations=1,
            gpu_size_t param_dx_variations=1);
    
    gpu_size_t n_x_variations()  const { return _n_param_x_variations; }
    gpu_size_t n_dx_variations() const { return _n_param_dx_variations; }
    
    DataBuffer<float>& params_x()  { return _params_x; }
    DataBuffer<float>& params_dx() { return _params_dx; }
    
    const BindGroup& bindgroup() const { return _bindgroup; }
};


/**
 * @brief Represents a sampling of points at which to evaluate an SDF.
 */
struct SdfInput {
private:
    DataBuffer<vec3gpu> _samples_x;
    DataBuffer<vec3gpu> _samples_dx;
    BindGroup           _read_bindgroup;
    
public:
    SdfInput(SdfEvaluator& evaluator, gpu_size_t samples_x, gpu_size_t samples_dx);
    
          DataBuffer<vec3gpu>& n_samples_x()        { return _samples_x; }
    const DataBuffer<vec3gpu>& n_samples_x()  const { return _samples_x; }
    
          DataBuffer<vec3gpu>& n_samples_dx()       { return _samples_dx; }
    const DataBuffer<vec3gpu>& n_samples_dx() const { return _samples_dx; }
    
    // write the entire buffer. must provide samples_x() samples
    void write_samples_x(const vec3gpu* samples);
    void write_samples_dx(const vec3gpu* samples);
    
    const BindGroup& read_bindgroup() const { return _read_bindgroup; }
};

/**
 * @brief represents the result of an SDF evaluation.
 */
struct SdfOutput {
private:
    DataBuffer<float>   _sdf_x;
    DataBuffer<float>   _sdf_dx;
    DataBuffer<vec3gpu> _normal_x;
    DataBuffer<vec3gpu> _normal_dx;
    BindGroup           _bindgroup;

public:
    
    SdfOutput(SdfEvaluator& evaluator, gpu_size_t n_samples);
    
    gpu_size_t n_samples() const { return _sdf_x.size(); }
    const BindGroup& bindgroup() const { return _bindgroup; }
};

} // namespace stereo
