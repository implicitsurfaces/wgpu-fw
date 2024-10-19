// correlate.wgsl
// Shader to correlate two windows of samples and accumulate the result
// to a running correlogram.

// #include "dft.wgsl"
// #include "structs.wgsl"

// todo: should we keep the log of the magnitude for better precision?

struct CorrelateUniforms {
    do_init: u32,
}

@group(0) @binding(0) var<storage,read>       kernel_pairs:        array<KernelPair>;
@group(0) @binding(1) var<storage,read_write> correlation_kernels: array<CorrelationKernel>;
@group(0) @binding(2) var<uniform>            uniforms:            CorrelateUniforms;

const Use_Intermediate_Norm: bool = false;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.x;
    if feature_idx >= arrayLength(&correlation_kernels) { return; }

    var one_v: vec4f   = vec4f(1.);
    var one_m: mat4x4f = mat4x4f(one_v, one_v, one_v, one_v);

    let kernel_a: SampleKernel = kernel_pairs[feature_idx].a;
    let kernel_b: SampleKernel = kernel_pairs[feature_idx].b;

    var a: array<cmat4, 2> = array<cmat4, 2>(
        cmat4(kernel_a.f,     kernel_a.laplace),
        cmat4(kernel_a.df_dx, kernel_a.df_dy),
    );
    var b: array<cmat4, 2> = array<cmat4, 2>(
        cmat4(kernel_b.f,     kernel_b.laplace),
        cmat4(kernel_b.df_dx, kernel_b.df_dy),
    );

    var xcor:    mat4x4f = one_m;
    var mag2_ab: f32     = 1.;

    if uniforms.do_init == 0u {
        // when we are accumulating multiple passes of correlation, we need to
        //   to initialize this only once to 1, otherwise accumulate to the previous result.
        xcor    = correlation_kernels[feature_idx].correlation;
        mag2_ab = correlation_kernels[feature_idx].mag2;
    }

    for (var i: u32 = 0u; i < 2; i++) {
        if Use_Intermediate_Norm {
            // update the raw correlation, with a basic element-by-element product
            let xcor_i: mat4x4f = normed_cross_correlation(a[i], b[i]);
            xcor = hadamard_mm(xcor, xcor_i);
            // renormalize the correlation to have unit integral
            let area: f32 = dot(vec4f(1.), xcor * vec4f(1.));
            xcor = (1. / area) * xcor;
            mag2_ab *= cnorm2(a[i]) * cnorm2(b[i]);
        } else {
            let xcor_i: Correlation = quantified_cross_correlation(a[i], b[i]);
            xcor = hadamard_mm(xcor, xcor_i.correlation);
            // also update the magnitudes of all the multiplicands;
            // we will use this to compute the cosine distance
            mag2_ab *= xcor_i.mag2_ab;
        }
    }

    // write back the correlation
    correlation_kernels[feature_idx] = CorrelationKernel(
        xcor,
        mag2_ab
    );
}
