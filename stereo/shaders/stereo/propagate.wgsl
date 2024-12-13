// #include "stereo_structs.wgsl"
// #include "../quat.wgsl"
// #include "../mat_helpers.wgsl"

@group(0) @binding(0) var<storage, read> feature_idx_buffer:     array<u32>;
@group(1) @binding(0) var<storage, read> src_parent_nodes:       array<TreeNode>;
@group(2) @binding(0) var<storage, read> src_parent_features_t0: array<SceneFeature>;
@group(3) @binding(0) var<storage, read> src_parent_features_t1: array<SceneFeature>;
@group(4) @binding(0) var<storage, read> src_child_features_t0:  array<SceneFeature>;

@group(5) @binding(0) var<storage, read_write> dst_child_features_t1: array<SceneFeature>;

const Process_Noise_Scale: f32 = 1. / 30.;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let index_number: u32 = global_id.x;
    if index_number    >= arrayLength(&feature_idx_buffer) { return; }
    let src_feature_idx: u32 = feature_idx_buffer[index_number];
    if src_feature_idx >= arrayLength(&src_parent_features_t0) { return; }
    
    let src_parent_node:   TreeNode = src_parent_nodes[src_feature_idx];
    let src_parent_t0: SceneFeature = src_parent_features_t0[src_feature_idx];
    let src_parent_t1: SceneFeature = src_parent_features_t1[src_feature_idx];
    
    var dx: vec3f = src_parent_t1.x - src_parent_t0.x;
    var dq: vec4f = qmult(src_parent_t1.q, qconj(src_parent_t0.q));
    
    // iterate over each child; apply the rigid transform of the parent.
    var first_child_idx: u32 = src_parent_node.child_begin;
    var end_child_idx:   u32 = src_parent_node.child_end;
    for (var i: u32 = first_child_idx; i < end_child_idx; i++) {
        let src_child_t0: SceneFeature = src_child_features_t0[i];
        let process_noise: mat3x3f = src_child_t0.scale * Process_Noise_Scale * I_3x3;
        // not handled: time prediction
        dst_child_features_t1[i] = SceneFeature(
            qmult(dq, src_child_t0.q),
            src_child_t0.q_cov,
            src_child_t0.x + dx,
            src_child_t0.x_cov + process_noise,
            src_child_t0.scale,
            src_child_t0.wt,
            // xxx debug
            src_parent_t0.color,
        );
    }
}
