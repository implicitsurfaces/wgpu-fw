// #include "structs.wgsl"
// #include "sample.wgsl"

struct VertexOutput {
    @builtin(position) position: vec4<f32>;
    @location(0)       uv:       vec2<f32>;
}

struct Uniforms {
    x_tiles: u32,
    y_tiles: u32,
    n:       u32,
}


@group(0) @binding(0) var<storage,read> correlation_windows: array<CorrelationWindow>;
@group(0) @binding(1) var<storage,read> uniforms: Uniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    // emit a quad
    var v = array<vec2f, 4>(
        vec2f(-1,  1),
        vec2f(-1, -1),
        vec2f( 1,  1),
        vec2f( 1, -1)
    );
	var p = v[in_vertex_index % 4];
    var out: VertexOutput;
	out.position = vec4f(p, 0.0, 1.0);
    out.uv       = p * 0.5 + 0.5;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // figure out which tile we're inside of
    let uv: vec2f = vec2f(1.) - in.uv;
    let st: vec2f = uv * vec2f(uniforms.x_tiles, uniforms.y_tiles);
    let ij: vec2u = vec2u(st);
    let xy: vec2f = st - vec2f(ij);
    let idx: u32 = ij.y * uniforms.x_tiles + ij.x;
    
    // if we're out of bounds, return transparent black
    if idx >= arrayLength(&correlation_windows) { return vec4<f32>(0.); }
    
    // otherwise, evaluate the correlation at this point inside the tile
    let corr: mat4x4f = correlation_windows[idx].correlation;
    let c:    f32     = eval_interp_image(corr, xy);
    
    return vec4<f32>(vec3f(c), 1.);
}
