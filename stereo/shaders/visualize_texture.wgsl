// #include "structs.wgsl"

alias TexVisMode = u32;

const TexVisMode_Stereo: TexVisMode = 0;
const TexVisMode_Mono_A: TexVisMode = 1;
const TexVisMode_Mono_B: TexVisMode = 2;

struct TexDisplayUniforms {
    mode:          TexVisMode,
    signed_signal: u32,  // no bool
}

@group(0) @binding(0) var a_tex:     texture_2d<f32>;
@group(0) @binding(1) var b_tex:     texture_2d<f32>;
@group(0) @binding(2) var u_sampler: sampler;
@group(0) @binding(3) var<uniform> u_tex_uniforms: TexDisplayUniforms;

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
    if u_tex_uniforms.signed_signal != 0 {
        c_a = c_a * 2. - 1.;
        c_b = c_b * 2. - 1.;
    }
    var c: vec3f;
    if u_tex_uniforms.mode == TexVisMode_Mono_A {
        c = c_a;
    } else if u_tex_uniforms.mode == TexVisMode_Mono_B {
        c = c_b;
    } else if u_tex_uniforms.mode == TexVisMode_Stereo {
        // convert to luminance
        let a = dot(c_a, L);
        let b = dot(c_b, L);
        c = vec3f(a, b, 0.5);
    }
    return vec4f(c, 1.); // cheapass gamma
}
