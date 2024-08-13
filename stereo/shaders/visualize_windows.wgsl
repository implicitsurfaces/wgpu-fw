// #include "structs.wgsl"
// #include "sample.wgsl"
// #include "mat_helpers.wgsl"

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0)       uv:       vec2<f32>,
}

struct Uniforms {
    x_tiles: u32,
    y_tiles: u32,
    n:       u32,
}

@group(0) @binding(0) var<storage,read> correlation_windows: array<CorrelationWindow>;
@group(0) @binding(1) var<uniform>      uniforms: Uniforms;
@group(0) @binding(2) var<storage,read> sample_windows: array<WindowPair>;
@group(0) @binding(3) var tex_sampler: sampler;
@group(0) @binding(4) var tex: texture_2d<f32>;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOutput {
    // emit a quad
    var v = array<vec2f, 4>(
        vec2f(-1,  1),
        vec2f(-1, -1),
        vec2f( 1,  1),
        vec2f( 1, -1)
    );
	var p = v[in_vertex_index % 4];
    var out: VertexOutput;
	out.position = vec4f(p, 0.5, 1.0);
    out.uv       = p * 0.5 + 0.5;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // figure out which tile we're inside of
    let uv:         vec2f = vec2f(1.) - in.uv;
    let tile_coord: vec2f = uv * vec2f(f32(uniforms.x_tiles), f32(uniforms.y_tiles));
    let tile_ij:    vec2u = vec2u(tile_coord);
    let tile_xy:    vec2f = tile_coord - vec2f(tile_ij);
    let idx:        u32 = tile_ij.y * uniforms.x_tiles + tile_ij.x;
    
    // if we're out of bounds, return transparent black
    if idx >= arrayLength(&correlation_windows) { return vec4<f32>(0.); }
    
    // var v:    vec4f   = vec4f(1., 2., 3., 4.) / 4.;
    // var test: mat4x4f = outer4x4(v);
    // 
    // let theta: f32 = 0.125 * 3.14159265358979;
    // let rot: mat2x2f = mat2x2f(
    //     cos(theta), -sin(theta),
    //     sin(theta),  cos(theta)
    // );
    
    // var a: vec3f = textureSampleGrad(
    //     tex,
    //     tex_sampler,
    //     vec2f(1.) - in.uv,
    //     rot * vec2<f32>(0.05, 0.),
    //     rot * vec2<f32>(0., 0.001)
    // ).rgb;
    // var dims: vec2u = textureDimensions(tex, 5);
    // var a: vec3f = textureLoad(tex, vec2i(uv * vec2f(dims)), 5).rgb;
    // a = textureSampleLevel(tex, tex_sampler, vec2f(1.) - in.uv, 5.).rgb;
    // a = a * 0.5 + 0.5;
    
    
    // otherwise, evaluate the correlation at this point inside the tile
    var corr:   mat4x4f = correlation_windows[idx].correlation;
    var smps = sample_windows[idx].a.r;
    var rotate = mat4x4f(
        0, 0, 1, 0,
        0, 0, 0, 1,
        1, 0, 0, 0,
        0, 1, 0, 0,
    );
    let q: f32 = eval_interp_image(rotate * corr * rotate, tile_xy);
    let b: f32 = eval_4x4_image(smps, tile_xy);
    
    if (tile_xy.x < 0.05 || tile_xy.y < 0.05) {
        return vec4f(0.25, 0.12, 0.12, 1.);
    }
    
    return vec4f(q, b, q, 1.);
}
