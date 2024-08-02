// #include "structs.wgsl"

// sample the window around each feature in a pair

@group(0) @binding(0) var<storage,read> features:      array<FeaturePair>;
@group(0) @binding(1) var tex_a:                       texture_2d<f32>;
@group(0) @binding(2) var tex_b:                       texture_2d<f32>;
@group(0) @binding(3) var<storage,write> window_pairs: array<WindowPair>;
@group(0) @binding(4) var sampler_a:                   sampler;
@group(0) @binding(5) var sampler_b:                   sampler;

@compute @workgroup_size(4,4,32)
fn main(
        @builtin(local_invocation_id)  local_id:  vec3u,
        @builtin(global_invocation_id) global_id: vec3u)
{
    let feature_idx: u32 = global_id.z;
    if feature_idx >= u32(features.length()) {
        return;
    }
    let feature: FeaturePair = features[feature_idx];
    let xy:       vec2u = local_id.xy
    let st:       vec2f = vec2f(xy) / vec2f(2.) - vec2f(1.);
    let coords_a: vec2u = vec2u(feature.a.st) + feature.a.basis * xy;
    let coords_b: vec2u = vec2u(feature.b.st) + feature.b.basis * xy;
    let a: vec3f = textureSampleGrad(tex_a, sampler_a, coords_a, feature.a.basis[0] / 2., feature.a.basis[1] / 2.).rgb;
    let b: vec3f = textureSampleGrad(tex_b, sampler_b, coords_b, feature.b.basis[0] / 2., feature.b.basis[1] / 2.).rgb;
    
    // todo: use the chain rule to compute the gradient of the texture sample,
    //   if a uniform flag requests it.
    
    window_pairs[feature_idx].a.r[xy.x][xy.y] = a.r;
    window_pairs[feature_idx].a.g[xy.x][xy.y] = a.g;
    window_pairs[feature_idx].a.b[xy.x][xy.y] = a.b;
    
    window_pairs[feature_idx].b.r[xy.x][xy.y] = b.r;
    window_pairs[feature_idx].b.g[xy.x][xy.y] = b.g;
    window_pairs[feature_idx].b.b[xy.x][xy.y] = b.b;
}
