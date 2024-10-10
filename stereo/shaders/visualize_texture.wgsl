// #include "structs.wgsl"

@group(0) @binding(0) var a_tex:     texture_2d<f32>;
@group(0) @binding(1) var b_tex:     texture_2d<f32>;
@group(0) @binding(2) var u_sampler: sampler;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0)       uv:       vec2f,
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
    var c_a: vec3f = textureSample(a_tex, u_sampler, 1. - in.uv).rgb;
    var c_b: vec3f = textureSample(b_tex, u_sampler, 1. - in.uv).rgb;
    let L: vec3f = vec3f(0.2126, 0.7152, 0.0722);
    var a: f32 = dot(c_a, L);
    var b: f32 = dot(c_b, L);
    // for signed signals:
    // a = a * 0.5 + 0.5;
    // b = b * 0.5 + 0.5;
    return vec4f(a * a, b * b, 0.5, 1.); // cheapass gamma
}
