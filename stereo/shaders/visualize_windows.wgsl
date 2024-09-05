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

@group(0) @binding(0) var<storage,read> correlation_kernels: array<CorrelationKernel>;
@group(0) @binding(1) var<uniform>      uniforms: Uniforms;
@group(0) @binding(2) var<storage,read> sample_kernels: array<KernelPair>;
@group(0) @binding(3) var tex_sampler: sampler;

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
    if idx >= arrayLength(&correlation_kernels) { return vec4<f32>(0.); }
    
    // otherwise, evaluate the correlation at this point inside the tile
    var corr:   mat4x4f = correlation_kernels[idx].correlation;
    var smps_a = sample_kernels[idx].a.r;
    var smps_b = sample_kernels[idx].b.r;
    var rotate = mat4x4f(
        0, 0, 1, 0,
        0, 0, 0, 1,
        1, 0, 0, 0,
        0, 1, 0, 0,
    );
    
    let q: f32 = eval_interp_image(rotate * corr * rotate, tile_xy);
    let a: f32 = eval_4x4_image(smps_a, tile_xy);
    let b: f32 = eval_4x4_image(smps_b, tile_xy);
    
    if (tile_xy.x < 0.05 || tile_xy.y < 0.05) {
        return vec4f(0.25, 0.12, 0.12, 1.);
    }
    
    return vec4f(0.66 * a, 0.66 * b, 2. * q, 1.);
}
