// #include "camera.wgsl"

struct ProjectUniforms {
    cam_a: CameraState,
    cam_b: CameraState,
    depth: u32,
}

// nb: different bindgroups below!
@group(0) @binding(0) var<storage,read>       src_scene_features: array<SceneFeature>;
@group(1) @binding(0) var<storage,read_write> dst_img_features:   array<FeaturePair>;
@group(2) @binding(0) var<uniform>            uniforms:           ProjectUniforms;
@group(3) @binding(0) var<storage,read>       feature_idx_buffer: array<u32>;


@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let instance_id: u32 = global_id.x;
    if instance_id >= arrayLength(&feature_idx_buffer) { return; }
    let feature_idx: u32 = feature_idx_buffer[instance_id];
    if feature_idx >= arrayLength(&src_scene_features) { return; }
    
    let src_scene_feature: SceneFeature = src_scene_features[feature_idx];
    
    let feat_a: ImageFeature = project_scene_feature(uniforms.cam_a, src_scene_feature);
    let feat_b: ImageFeature = project_scene_feature(uniforms.cam_b, src_scene_feature);
    
    dst_img_features[instance_id] = FeaturePair(feat_a, feat_b);
}
