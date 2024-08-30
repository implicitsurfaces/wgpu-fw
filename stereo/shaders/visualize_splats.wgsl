// #include "structs.wgsl"
// #include "camera.wgsl"
// #include "mat_helpers.wgsl"

struct VertexOut {
    @builtin(position) p:  vec4f,
    @location(0)       st: vec2f,
    @location(1)       wt: f32,
}

@group(0) @binding(0) var<storage,read>  features: array<SceneFeature>;
@group(0) @binding(1) var<uniform>       view_cam: CameraState;
@group(0) @binding(2) var gauss_tex:     texture_2d<f32>;
@group(0) @binding(3) var gauss_sampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut {
    var Verts_Per_Feature: u32 = 4u;
    let feature_idx: u32 = in_vertex_index / Verts_Per_Feature;
    let vert_idx:    u32 = in_vertex_index % Verts_Per_Feature;
    if feature_idx >= arrayLength(&features) { return VertexOut(vec4f(0.), vec2f(0.), 0.); }
    
    let feature: SceneFeature = features[feature_idx];
    var card_pts = array<vec2f,4>(
        // triangle strip order:
        vec2f(-1.,  1.),
        vec2f(-1., -1.),
        vec2f( 1.,  1.),
        vec2f( 1., -1.),
    );
    
    let img_feat    = project_scene_feature(view_cam, feature);
    var img_to_clip = mat3x3f(
        vec3f(2., 0., 0.),
        vec3f(0., 2., 0.),
        vec3f(vec2f(-1.), 1.),
    );
    // Scaling factor for the covariance matrix; take to be 2 * 3 * I.
    // Factor of 2 for the rescaling to [-1, 1]^2 above.
    // Factor of 3 because we want to scale the covariance ellipse to be
    // 3 standard deviations wide. Then account for the fact that we multiply on
    // both sides, thus squaring the factor.
    var k = 36.; // (3 * 2)^2
    let p: vec2f   = (img_to_clip * vec3f(img_feat.st, 1.)).xy;
    // matrix transforming a unit circle to the 3SD ellipse of the covariance:
    let J: mat2x2f = sqrt_2x2(k * img_feat.cov);
    let pt: vec2f  = p + J * card_pts[vert_idx];
    
    return VertexOut(
        vec4f(pt, 0., 1.),
        card_pts[vert_idx] * 0.5 + vec2f(0.5),
        feature.wt,
    );
}


@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let v:  f32 = textureSample(gauss_tex, gauss_sampler, in.st).r;
    return vec4f(vec3f(1.), in.wt * v);
}
