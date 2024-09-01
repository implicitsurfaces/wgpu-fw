// #include "structs.wgsl"
// #include "camera.wgsl"
// #include "mat_helpers.wgsl"

struct VertexOut {
    @builtin(position) p:  vec4f,
    @location(0)       st: vec2f,
    @location(1)       wt: f32,
    @location(2)  world_p: vec3f,
}

struct SplatUniforms {
    view_cam: CameraState,
    w2cam:    mat4x4f,
    P:        mat4x4f,
}

@group(0) @binding(0) var<storage,read>  features: array<SceneFeature>;
@group(0) @binding(1) var<uniform>       uniforms: SplatUniforms;
@group(0) @binding(2) var gauss_tex:     texture_2d<f32>;
@group(0) @binding(3) var gauss_sampler: sampler;

@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> VertexOut {
    // reindexing for 1-quad triangle strip
    var reindex = array<u32,6>(0u, 1u, 2u, 2u, 1u, 3u,);
    var Verts_Per_Feature: u32 = 6u;
    let feature_idx: u32 = in_vertex_index / Verts_Per_Feature;
    let vert_idx:    u32 = reindex[in_vertex_index % Verts_Per_Feature];
    if feature_idx >= arrayLength(&features) {
        return VertexOut(
            vec4f(0.), vec2f(0.), 0., vec3f(0.),
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
    // var img_to_clip = mat3x3f(
    //     vec3f(2., 0., 0.),
    //     vec3f(0., 2., 0.),
    //     vec3f(vec2f(-1.), 1.),
    // );
    // Scaling factor for the covariance matrix; take to be 2 * 3 * I.
    // Factor of 2 for the rescaling to [-1, 1]^2 above.
    // Factor of 3 because we want to scale the covariance ellipse to be
    // 3 standard deviations wide.
    var k = 6.;
    let p: vec2f   = (2. * img_feat.st - 1.).xy;
    // matrix transforming a unit circle to the 3SD ellipse of the covariance:
    let J: mat2x2f = k * sqrt_2x2(img_feat.cov);
    let pt: vec2f  = p + J * card_pts[vert_idx];
    
    let B: mat4x3f = world_to_cam(uniforms.view_cam);
    let C = mat4x4f(
        vec4f(B[0], 0.),
        vec4f(B[1], 0.),
        vec4f(B[2], 0.),
        vec4f(B[3], 1.),
    );
    let M: mat4x4f =  uniforms.P * C; // uniforms.w2cam;
    let ctr: vec4f = M * vec4f(feature.x, 1.);
    
    let Q: mat3x3f = pinhole_projection(uniforms.view_cam.lens);
    let d = Q * (C * vec4f(feature.x, 1.)).xyz;
    var b = 2. * (d.xy / d.z) - 1.;
    
    return VertexOut(
        vec4f(pt, 0.1, 1.),
        // vec4f(b + card_pts[vert_idx] * 0.05, 0.5, 1.),
        // ctr + vec4f(card_pts[vert_idx] * 0.05, 0., 0.),
        // vec4f(p + card_pts[vert_idx] * 0.05, 0., 1.),
        card_pts[vert_idx] * 0.5 + vec2f(0.5),
        feature.wt,
        // feature.x,
        vec3f(b, 0.5),
    );
}


@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let v:  f32 = textureSample(gauss_tex, gauss_sampler, in.st).r;
    return vec4f(vec3f(in.world_p), in.wt * v * 0.05);
}
