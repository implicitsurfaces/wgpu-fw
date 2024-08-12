// correlate.wgsl
// Shader to correlate two windows of samples and accumulate the result
// to a running correlogram.

// #include "dft.wgsl"
// #include "structs.wgsl"

@group(0) @binding(0) var<storage,read>       window_pairs:        array<WindowPair>;
@group(0) @binding(1) var<storage,read_write> correlation_windows: array<CorrelationWindow>;

// issue: correlation is done with two channels, but we have three.
//   do we waste a channel, or do we try to pull in a second layer for each eye?

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.x;
    if feature_idx >= arrayLength(&correlation_windows) { return; }
    
    var one_v: vec4f   = vec4f(1.);
    var one_m: mat4x4f = mat4x4f(one_v, one_v, one_v, one_v);
    
    let window_a: SampleWindow = window_pairs[feature_idx].a;
    let window_b: SampleWindow = window_pairs[feature_idx].b;
    let a_0: cmat4 = cmat4(window_a.r, window_a.g);
    let a_1: cmat4 = cmat4(window_a.b, mat4x4f()); // waste a channel :|
    let b_0: cmat4 = cmat4(window_b.r, window_b.g);
    let b_1: cmat4 = cmat4(window_b.b, mat4x4f());
    
    // xxx: when we are accumulating multiple passes of correlation, we need to
    //   to initialize this only once to 1, otherwise accumulate to the previous result.
    var xcor:  mat4x4f = one_m; // correlation_windows[feature_idx].correlation;
    var xcors: array<mat4x4f, 2> = array<mat4x4f, 2>(
        normed_cross_correlation(a_0, b_0),
        normed_cross_correlation(a_1, b_1),
    );
    for (var i: u32 = 0u; i < 2; i++) {
        // multiply in the correlation for this layer
        let xcor_i: mat4x4f = hadamard_mm(xcor, xcors[i]);
        // renormalize the correlation
        let area:   f32     = dot(vec4f(1.), xcor_i * vec4f(1.));
        // update the correlation
        xcor = xcor_i * (1. / area);
    }
    
    // write back the correlation
    correlation_windows[feature_idx].correlation = xcor;
}
