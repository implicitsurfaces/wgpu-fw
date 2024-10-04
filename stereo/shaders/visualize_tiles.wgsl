// #include "structs.wgsl"
// #include "camera.wgsl"

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0)       uv:       vec2<f32>,
}

struct Uniforms {
    x_tiles: u32,
    y_tiles: u32,
    n:       u32,
    cam_a:   CameraState,
    cam_b:   CameraState,
}

@group(0) @binding(0) var<uniform>      uniforms: Uniforms;
@group(0) @binding(1) var tex_sampler:  sampler;
@group(0) @binding(2) var feed_a_tex:   texture_2d<f32>;
@group(0) @binding(3) var feed_b_tex:   texture_2d<f32>;

@group(1) @binding(0) var<storage,read> scene_features: array<SceneFeature>;

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
    let tile_uv:    vec2f = tile_coord - vec2f(tile_ij); // [0, 1]
    let tile_xy:    vec2f = 2. * tile_uv - 1.;           // [-1, 1]
    let idx:        u32   = tile_ij.y * uniforms.x_tiles + tile_ij.x;

    // if we're out of bounds, return transparent black
    if idx >= arrayLength(&scene_features) { return vec4f(0.); }

    let scene_feature: SceneFeature = scene_features[idx];
    let img_a_feature: ImageFeature = project_scene_feature(uniforms.cam_a, scene_feature);
    let img_b_feature: ImageFeature = project_scene_feature(uniforms.cam_b, scene_feature);

    let uv_a: vec2f = img_a_feature.st + img_a_feature.basis * tile_uv;
    let uv_b: vec2f = img_b_feature.st + img_b_feature.basis * tile_uv;
    let c_a: vec3f = textureSample(feed_a_tex, tex_sampler, uv_a).rgb;
    let c_b: vec3f = textureSample(feed_b_tex, tex_sampler, uv_b).rgb;

    // draw tile borders
    if (tile_uv.x < 0.05 || tile_uv.y < 0.05) {
        return vec4f(0.25, 0.12, 0.12, 1.);
    }

    let L: vec3f = vec3f(0.2126, 0.7152, 0.0722);
    let lum_a: f32 = dot(c_a, L);
    let lum_b: f32 = dot(c_b, L);

    return vec4f(lum_a * lum_a, lum_b * lum_b, 0.5, 1.);
}
