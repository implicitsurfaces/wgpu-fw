// #include "structs.wgsl"
// #include "camera.wgsl"
// #include "mat_helpers.wgsl"

struct VertexOut {
    @builtin(position) p:  vec4f,
    @location(0)       st: vec2f,
    @location(1)       wt: f32,
    @location(2)  world_p: vec3f,
    @location(3)   scr_sz: f32,
}

struct SplatUniforms {
    view_cam: CameraState,
    w2cam:    mat4x4f,
    P:        mat4x4f,
}

@group(0) @binding(0) var<uniform>       uniforms: SplatUniforms;
@group(0) @binding(1) var gauss_tex:     texture_2d<f32>;
@group(0) @binding(2) var gauss_sampler: sampler;

@group(1) @binding(0) var video_tex:     texture_2d<f32>;
@group(2) @binding(0) var<storage,read>  features: array<SceneFeature>;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut {
    // reindexing for 1-quad triangle strip
    var reindex = array<u32,6>(0u, 1u, 2u, 2u, 1u, 3u,);
    var Verts_Per_Feature: u32 = 6u;
    let feature_idx: u32 = in_vertex_index / Verts_Per_Feature;
    let vert_idx:    u32 = reindex[in_vertex_index % Verts_Per_Feature];
    if feature_idx >= arrayLength(&features) {
        return VertexOut(
            vec4f(0.), vec2f(0.), 0., vec3f(0.), 1.,
        );
    }
    
    let feature: SceneFeature = features[feature_idx];
    var card_pts = array<vec2f,4>(
        // triangle strip order:
        vec2f(-1.,  1.),
        vec2f(-1., -1.),
        vec2f( 1.,  1.),
        vec2f( 1., -1.),
    );
    
    let img_feat = project_scene_feature(uniforms.view_cam, feature);
    // Scaling factor for the covariance matrix; take to be 2 * 3 * I.
    // Factor of 2 for the rescaling to [-1, 1]^2 above.
    // Factor of 3 because we want to scale the covariance ellipse to be
    // 3 standard deviations wide.
    var k = 6.;
    let p: vec2f   = (2. * img_feat.st - 1.).xy;
    // matrix transforming a unit circle to the 3SD ellipse of the covariance:
    let J: mat2x2f = k * sqrt_2x2(img_feat.cov);
    let pt: vec2f  = p + J * card_pts[vert_idx];
    
    return VertexOut(
        vec4f(pt, 0.5, 1.),
        card_pts[vert_idx] * 0.5 + vec2f(0.5),
        feature.wt,
        feature.x,
        determinant(J),
    );
}


@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let v: f32 = textureSample(gauss_tex, gauss_sampler, in.st).r;
    let area_norm: f32 = 1. / (256. * sqrt(in.scr_sz));
    let k: f32 = 0.5 * area_norm;
    let c: vec3f = mix(vec3f(1., 1., 0.), vec3f(0., 1., 1.), 4. * sqrt(in.wt));
    return vec4f(c, v * k);
}
