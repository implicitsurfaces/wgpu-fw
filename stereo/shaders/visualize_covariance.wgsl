// #include "structs.wgsl"
// #include "mat_helpers.wgsl"

const Tau: f32 = 6.283185307179586;

struct CovarianceUniforms {
    x_tiles: u32,
    y_tiles: u32,
    verts_per_ellipse: u32,
}

struct VertexOut {
    @builtin(position) p: vec4f,
    @location(0)       c: vec4f,
    @location(1)       q: f32,
}

@group(0) @binding(0) var<storage,read> features: array<DebugFeature2D>;
@group(0) @binding(1) var<uniform>      uniforms: CovarianceUniforms;


fn tile_xf(tile_idx: u32, x_tiles: u32) -> mat3x3f {
    let tile_x: u32 = tile_idx % uniforms.x_tiles;
    let tile_y: u32 = tile_idx / uniforms.x_tiles;
    let tile_xy     = vec2f(vec2u(tile_x, tile_y));
    let tile_xymax  = vec2f(vec2u(uniforms.x_tiles, uniforms.y_tiles));
    let tile_wh     = 1. / tile_xymax;
    // in window coords: [0, 1]^2
    let tile_center: vec2f = (tile_xy + vec2(0.5)) / tile_xymax;
    
    return mat3x3f(
        tile_wh.x / 2., 0., 0.,
        0., tile_wh.y / 2., 0.,
        tile_center.x, tile_center.y, 1.,
    );
}

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut {
    let feature_idx: u32 = in_vertex_index / (2 * uniforms.verts_per_ellipse);
    let seg_idx:     u32 = in_vertex_index % (2 * uniforms.verts_per_ellipse);
    if feature_idx >= arrayLength(&features) { return VertexOut(vec4f(0.), vec4f(0.), 0.); }
    
    // two verts per segment!
    let vert_idx: u32 = (seg_idx + 1) / 2;
    
    let feature: DebugFeature2D = features[feature_idx];
    let center:  vec2f = feature.x;
    let A:     mat2x2f = sqrt_2x2(feature.sigma);
    
    let s:        f32 = f32(vert_idx) / f32(uniforms.verts_per_ellipse);
    let theta:    f32 = Tau * s;
    let p_tile: vec2f = A * vec2f(cos(theta), sin(theta)) + center;
    let xf:   mat3x3f = tile_xf(feature_idx, uniforms.x_tiles);
    let p:      vec2f = 2. * (xf * vec3f(p_tile, 1.)).xy - 1.;
    
    return VertexOut(
        // invert ST for visualization
        vec4f(-p, 0.25, 1.0),
        vec4f(vec3f(1.), 0.25),
        1.,
    );
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    return in.c;
}
