// #include "structs.wgsl"

struct SamplePtUniforms {
    samples_per_feature: u32,
    point_scale: f32,
    x_tiles: u32,
    y_tiles: u32,
}

struct VertexOut {
    @builtin(position) p:         vec4f,
    @location(0)       sample_id: u32,
    @location(1)       weight:    f32,
}

@group(0) @binding(0) var<storage,read> samples:  array<WeightedSample>;
@group(0) @binding(1) var<uniform>      uniforms: SamplePtUniforms;
// @group(0) @binding(2) var<storage,read> features: array<FeaturePair>;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut {
    let point_idx:   u32 = in_vertex_index / 4;
    let vert_idx:    u32 = in_vertex_index % 4;
    let feature_idx: u32 = point_idx / uniforms.samples_per_feature;
    let sample_idx:  u32 = point_idx % uniforms.samples_per_feature;
    if point_idx >= arrayLength(&samples) { return VertexOut(vec4f(0.), 0, 0.); }
    let sample:  WeightedSample = samples[point_idx];
    
    // "+" shape:
    var local_pts = array<vec2f,4>(
        vec2f( 0.,  1.),
        vec2f( 0., -1.),
        vec2f( 1.,  0.),
        vec2f(-1.,  0.)
    );
    
    let tile_x: u32 = feature_idx % uniforms.x_tiles;
    let tile_y: u32 = feature_idx / uniforms.x_tiles;
    let tile_xy     = vec2f(vec2u(tile_x, tile_y));
    let tile_xymax  = vec2f(vec2u(uniforms.x_tiles, uniforms.y_tiles));
    let tile_wh     = 1. / tile_xymax;
    let tile_center:   vec2f = (tile_xy + vec2(0.5)) / tile_xymax;
    let vert_pt: vec2f = samples[point_idx].x + uniforms.point_scale * local_pts[vert_idx];
    let p: vec2f = tile_wh * vert_pt + tile_center;
    let q: vec2f = tile_center + local_pts[vert_idx] * tile_wh * uniforms.point_scale;
    
    return VertexOut(
        vec4f(p * 2. - 1., 0.25, 1.),
        sample_idx,
        sample.w,
    );
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    var c: vec3f;
    if (in.sample_id & 1u) != 0u {
        c = vec3f(1., 0.4118, 0.8118);
    } else {
        c = vec3f(0.6392, 0.4118, 1.);
    }
    return vec4f(c, 1.); // in.weight);
}
