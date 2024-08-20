// #include "structs.wgsl"

// sample the window around each feature in a pair

@group(0) @binding(0) var<storage,read_write> window_pairs: array<WindowPair>;
@group(0) @binding(1) var tex_sampler: sampler;

@group(1) @binding(0) var tex_a: texture_2d<f32>;
@group(1) @binding(1) var tex_b: texture_2d<f32>;

@group(2) @binding(0) var<storage,read> features: array<FeaturePair>;

// we "vectorize" over only one of the axes, because vector element accesses may
// read the entire vector, creating a race condition along the columns of the matrix.
@compute @workgroup_size(16,4)
fn main(
        @builtin(local_invocation_id)  local_id:  vec3u,
        @builtin(global_invocation_id) global_id: vec3u)
{
    let feature_idx: u32 = global_id.x;
    if feature_idx >= arrayLength(&features) { return; }
    var x: u32 = global_id.y;
    
    var r_a: vec4f = vec4f(0.);
    var g_a: vec4f = vec4f(0.);
    var b_a: vec4f = vec4f(0.);
    
    var r_b: vec4f = vec4f(0.);
    var g_b: vec4f = vec4f(0.);
    var b_b: vec4f = vec4f(0.);
    
    for (var i: u32 = 0; i < 4; i++) {
        var y: u32 = i;
        let feature: FeaturePair = features[feature_idx];
        let st:       vec2f = vec2f(vec2u(x, y)) / vec2f(2.) - vec2f(1.); // [-1, 1]
        let coords_a: vec2f = feature.a.st + feature.a.basis * st;
        let coords_b: vec2f = feature.b.st + feature.b.basis * st;
        
        let a: vec3f = textureSampleGrad(
            tex_a,
            tex_sampler,
            coords_a,
            feature.a.basis[0] / 2.,
            feature.a.basis[1] / 2.).rgb;
        let b: vec3f = textureSampleGrad(
            tex_b,
            tex_sampler,
            coords_b,
            feature.b.basis[0] / 2.,
            feature.b.basis[1] / 2.).rgb;
        
        r_a[i] = a.r;
        g_a[i] = a.g;
        b_a[i] = a.b;
        
        r_b[i] = b.r;
        g_b[i] = b.g;
        b_b[i] = b.b;
    }
    
    // todo: use the chain rule to compute the gradient of the texture sample,
    //   if a uniform flag requests it.
    window_pairs[feature_idx].a.r[x] = r_a;
    window_pairs[feature_idx].a.g[x] = g_a;
    window_pairs[feature_idx].a.b[x] = b_a;
    
    window_pairs[feature_idx].b.r[x] = r_b;
    window_pairs[feature_idx].b.g[x] = g_b;
    window_pairs[feature_idx].b.b[x] = b_b;
}
