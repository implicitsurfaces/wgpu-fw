// #include "dft.wgsl"

struct SampleWindow {
    r: mat4x4f,
    g: mat4x4f,
    b: mat4x4f,
}

struct WindowPair {
    a: SampleWindow,
    b: SampleWindow,
}

struct CorrelationWindow {
    correlation: mat4x4,
};

struct MatcherUniforms {
    tree_depth:     i32,
    feature_offset: u32,
    refine_parent:  bool,
}

@group(0) @binding(0) var<storage,read>       window_pairs;        array<WindowPair>;
@group(0) @binding(1) var<storage,read_write> correlation_windows: array<CorrelationWindow>;
@group(0) @binding(2) var<uniform>            uniforms:            MatcherUniforms;

// issue: correlation is done with two channels, but we have three.
//   do we waste a channel, or do we try to pull in a second layer for each eye?

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.z + uniforms.feature_offset;
    if (feature_idx >= u32(features.length())) {
        return;
    }
    let window_a: SampleWindow = window_pairs[feature_idx].a;
    let window_b: SampleWindow = window_pairs[feature_idx].b;
    let a_0: cmat4 = cmat4(window_a.r, window_a.g);
    let a_1: cmat4 = cmat4(window_a.b, mat4x4f(0.)); // waste a channel :|
    let b_0: cmat4 = cmat4(window_b.r, window_b.g);
    let b_1: cmat4 = cmat4(window_b.b, mat4x4f(0.));
    let xcor_0: mat4x4f = normed_cross_correlation(a_0, b_0);
    let xcor_1: mat4x4f = normed_cross_correlation(a_1, b_1);
    let xcor: mat4x4f = hadamard(xcor_0, xcor_1);
    correlation_windows[feature_idx].correlation = hardamard(
        correlation_windows[feature_idx].correlation,
        xcor
    );
}
