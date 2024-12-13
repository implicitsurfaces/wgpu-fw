// #include "stereo_structs.wgsl"

// sample the window around each feature in a pair

@group(0) @binding(0) var<storage,read_write> kernel_pairs: array<KernelPair>;
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

    var f_a:     vec4f = vec4f(0.);
    var df_dx_a: vec4f = vec4f(0.);
    var df_dy_a: vec4f = vec4f(0.);
    var lapl_a:  vec4f = vec4f(0.);

    var f_b:     vec4f = vec4f(0.);
    var df_dx_b: vec4f = vec4f(0.);
    var df_dy_b: vec4f = vec4f(0.);
    var lapl_b:  vec4f = vec4f(0.);

    // the samples are taken from [-1, 1]^2 in the feature's local coordinate system.
    // the edge samples are at |x| = 1 exactly.
    for (var i: u32 = 0; i < 4; i++) {
        var y: u32 = i;
        let feature: FeaturePair = features[feature_idx];
        let xy:       vec2f = vec2f(vec2u(x, y)) / vec2f(3.); // [ 0, 1]
        let st:       vec2f = 2. * xy - vec2f(1.);            // [-1, 1]
        let coords_a: vec2f = feature.a.st + feature.a.basis * st;
        let coords_b: vec2f = feature.b.st + feature.b.basis * st;

        let a: vec4f = textureSampleGrad(
            tex_a,
            tex_sampler,
            coords_a,
            feature.a.basis[0] / 2.,
            feature.a.basis[1] / 2.);
        let b: vec4f = textureSampleGrad(
            tex_b,
            tex_sampler,
            coords_b,
            feature.b.basis[0] / 2.,
            feature.b.basis[1] / 2.);

        f_a[i]     = a.r;
        df_dx_a[i] = a.g;
        df_dy_a[i] = a.b;
        lapl_a[i]  = a.a;

        f_b[i]     = b.r;
        df_dx_b[i] = b.g;
        df_dy_b[i] = b.b;
        lapl_b[i]  = b.a;
    }

    // todo: use the chain rule to compute the gradient of the texture sample,
    //   if a uniform flag requests it.
    kernel_pairs[feature_idx].a.f[x]       = f_a;
    kernel_pairs[feature_idx].a.df_dx[x]   = df_dx_a;
    kernel_pairs[feature_idx].a.df_dy[x]   = df_dy_a;
    kernel_pairs[feature_idx].a.laplace[x] = lapl_a;

    kernel_pairs[feature_idx].b.f[x]       = f_b;
    kernel_pairs[feature_idx].b.df_dx[x]   = df_dx_b;
    kernel_pairs[feature_idx].b.df_dy[x]   = df_dy_b;
    kernel_pairs[feature_idx].b.laplace[x] = lapl_b;
}
