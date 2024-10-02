// #include "structs.wgsl"

@group(0) @binding(0) var u_tex:     texture_2d<f32>;
@group(0) @binding(1) var u_sampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0)       uv:       vec2<f32>,
}

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
    var c: vec3f = textureSample(u_tex, u_sampler, 1. - in.uv).rgb;
    c = c * 0.5 + 0.5;
    return vec4f(c*c, 1.); // cheapass gamma
}
