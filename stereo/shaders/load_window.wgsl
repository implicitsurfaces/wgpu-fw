struct ImageFeature {
    depth:    u32,
    parent:   u32,
    a_xy:     vec2u,
    b_xy:     vec2u,
    offs:     vec2f,
    sqrt_cov: mat3x3f,
}

struct SampleWindow {
    r: mat4x4f,
    g: mat4x4f,
    b: mat4x4f,
}

struct WindowPair {
    a: SampleWindow,
    b: SampleWindow,
}

struct MatcherUniforms {
    tree_depth:     i32,
    feature_offset: u32,
    refine_parent:  bool,
}

@group(0) @binding(0) var<storage,read> features:      array<ImageFeature>;
@group(0) @binding(1) var tex_a:                       texture_2d<f32>;
@group(0) @binding(2) var tex_b:                       texture_2d<f32>;
@group(0) @binding(3) var<storage,write> window_pairs: array<WindowPair>;
@group(0) @binding(4) var<uniform> uniforms:           MatcherUniforms;

@compute @workgroup_size(4,4,32)
fn main(
        @builtin(local_invocation_id)  local_id: vec3u,
        @builtin(global_invocation_id) global_id: vec3u)
{
    let feature_idx: u32 = global_id.z + uniforms.feature_offset;
    if (feature_idx >= u32(features.length())) {
        return;
    }
    let feature: ImageFeature = features[feature_idx];
    let xy: vec2u = local_id.xy
    let coords_a: vec2u = feature.a_xy + xy;
    let coords_b: vec2u = feature.b_xy + xy;
    let a: vec3f = textureLoad(tex_a, coords_a, uniforms.tree_depth).rgb;
    let b: vec3f = textureLoad(tex_b, coords_b, uniforms.tree_depth).rgb;
    
    window_pairs[feature_idx].a.r[xy.x][xy.y] = a.r;
    window_pairs[feature_idx].a.g[xy.x][xy.y] = a.g;
    window_pairs[feature_idx].a.b[xy.x][xy.y] = a.b;
    
    window_pairs[feature_idx].b.r[xy.x][xy.y] = b.r;
    window_pairs[feature_idx].b.g[xy.x][xy.y] = b.g;
    window_pairs[feature_idx].b.b[xy.x][xy.y] = b.b;
}
