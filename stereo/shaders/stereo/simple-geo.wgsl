// #include "camera.wgsl"

struct V {
    @location(0) x : vec3f,
    @location(1) c : vec3f,
}

struct VOut {
    @builtin(position) p: vec4f,
    @location(0)       c: vec4f,
}

struct U {
    view_cam: CameraState,
};

@group(0) @binding(0) var<uniform>       uniforms: U;
@group(0) @binding(1) var<storage, read> verts:    array<V>;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VOut {
    let vert: V = verts[in_vertex_index];
    let M: mat4x3f = world_to_cam(uniforms.view_cam);
    let P: mat3x3f = pinhole_projection(uniforms.view_cam.lens);
    let p_clip:   vec3f = P * M * vec4f(vert.x, 1.);
    let p_screen: vec2f = 2. * (p_clip.xy / p_clip.z) - 1.;
    return VOut(
        vec4f(p_screen, 0.5, 1.),
        vec4f(vert.c, 1.),
    );
}

@fragment
fn fs_main(v: VOut) -> @location(0) vec4f {
    return v.c;
}
