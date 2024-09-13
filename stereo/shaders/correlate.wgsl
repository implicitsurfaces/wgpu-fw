// correlate.wgsl
// Shader to correlate two windows of samples and accumulate the result
// to a running correlogram.

// #include "dft.wgsl"
// #include "structs.wgsl"

// todo: should we keep the log of the magnitude for better precision?

@group(0) @binding(0) var<storage,read>       kernel_pairs:        array<KernelPair>;
@group(0) @binding(1) var<storage,read_write> correlation_kernels: array<CorrelationKernel>;

const Use_Intermediate_Norm: bool = false;

// issue: correlation is done with two channels, but we have three.
//   do we waste a channel, or do we try to pull in a second layer for each eye?

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.x;
    if feature_idx >= arrayLength(&correlation_kernels) { return; }
    
    var one_v: vec4f   = vec4f(1.);
    var one_m: mat4x4f = mat4x4f(one_v, one_v, one_v, one_v);
    
    let kernel_a: SampleKernel = kernel_pairs[feature_idx].a;
    let kernel_b: SampleKernel = kernel_pairs[feature_idx].b;
    
    var a: array<cmat4, 2> = array<cmat4, 2>(
        cmat4(kernel_a.r, kernel_a.g),
        cmat4(kernel_a.b, mat4x4f()),  // waste a channel :|
    );
    var b: array<cmat4, 2> = array<cmat4, 2>(
        cmat4(kernel_b.r, kernel_b.g),
        cmat4(kernel_b.b, mat4x4f()),
    );
    
    // xxx: when we are accumulating multiple passes of correlation, we need to
    //   to initialize this only once to 1, otherwise accumulate to the previous result.
    var xcor:    mat4x4f = one_m; // correlation_kernels[feature_idx].correlation;
    var mag2_ab: f32     = 1.;
    
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
