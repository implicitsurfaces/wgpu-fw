// #include "camera.wgsl"
// #include "inverse.wgsl"

struct Vert {
    @location(0) p:  vec3f,
    @location(1) n:  vec3f,
    @location(2) uv: vec2f,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0)       p_world:  vec3f,
    @location(1)       n_world:  vec3f,
    @location(2)       uv:       vec2f,
}

struct CameraUniforms {
    cam2world: mat4x4f,
    world2cam: mat4x4f,
    proj:      mat4x4f,
}

/*
struct Material {
    diffuse:   vec3f,
    specular:  vec3f,
    roughness: f32,
    metallic:  f32,
};
*/

// prim binding
@group(0) @binding(0) var<uniform> u_camera:  CameraUniforms;
@group(0) @binding(1) var<uniform> u_model2w: mat4x4f;
@group(0) @binding(2) var u_sampler:          sampler;

// material binding
@group(1) @binding(0) var t_diffuse: texture_2d<f32>;
// @group(1) @binding(1) var t_normal:    texture_2d<f32>;
// @group(1) @binding(2) var t_specular:  texture_2d<f32>;
// @group(1) @binding(3) var t_roughness: texture_2d<f32>;
// @group(1) @binding(4) var t_metallic:  texture_2d<f32>;


@vertex
fn vs_main(v: Vert) -> VertexOutput {
    var out: VertexOutput;
    let p_world: vec4f = u_model2w * vec4f(v.p, 1.);
    let M: mat4x4f = u_model2w;
    let N: mat3x3f = transpose(inverse3x3(mat3x3f(M[0].xyz, M[1].xyz, M[2].xyz)));
    out.position = u_camera.proj * u_camera.world2cam * p_world;
    out.p_world  = p_world.xyz;
    out.n_world  = normalize(N * v.n);
    out.uv       = v.uv;
    return out;
}

@fragment
fn fs_main(v: VertexOutput) -> @location(0) vec4f {
    let diffuse: vec3f = textureSample(t_diffuse, u_sampler, v.uv).xyz;
    let normal:  vec3f = normalize(v.n_world);
    let light:   vec3f = normalize(vec3f(1., 1., 1.));
    let NdotL:   f32   = 1.; // max(dot(normal, light), 0.);
    let color:   vec3f = diffuse * NdotL;
    return vec4f(color, 1.);
}
