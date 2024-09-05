// #include "camera.wgsl"

struct ProjectUniforms {
    cam_a: CameraState,
    cam_b: CameraState,
    depth: u32,
}

// nb: different bindgroups below!
@group(0) @binding(0) var<storage,read>       src_scene_features_a: array<SceneFeature>;
@group(1) @binding(0) var<storage,read>       src_scene_features_b: array<SceneFeature>;
@group(2) @binding(0) var<storage,read_write> dst_img_features:   array<FeaturePair>;
@group(3) @binding(0) var<uniform>            uniforms:           ProjectUniforms;
@group(3) @binding(1) var<uniform>            feature_range:      FeatureRange;
@group(4) @binding(0) var<storage,read>       feature_idx_buffer: array<u32>;


@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let index_number: u32 = global_id.x + feature_range.feature_start;
    if index_number >= arrayLength(&feature_idx_buffer) ||
       index_number > feature_range.feature_end
    {
        return;
    }
    let src_feature_idx: u32 = feature_idx_buffer[index_number];
    let dst_feature_idx: u32 = global_id.x;
    if src_feature_idx >= arrayLength(&src_scene_features_a) ||
       src_feature_idx >= arrayLength(&src_scene_features_b) {
        return;
    }
    if dst_feature_idx >= arrayLength(&dst_img_features)   { return; }
    
    let src_scene_feature_a: SceneFeature = src_scene_features_a[src_feature_idx];
    let src_scene_feature_b: SceneFeature = src_scene_features_b[src_feature_idx];
    
    let feat_a: ImageFeature = project_scene_feature(uniforms.cam_a, src_scene_feature_a);
    let feat_b: ImageFeature = project_scene_feature(uniforms.cam_b, src_scene_feature_b);
    
    dst_img_features[dst_feature_idx] = FeaturePair(feat_a, feat_b);
}
