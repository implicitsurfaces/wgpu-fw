// #include "structs.wgsl"
// #include "kalman.wgsl"
// #include "camera.wgsl"

// xxx todo: we should just output the fused difference.
//   - this will be used in different ways depending on the solver stage
//   - the L/R registration will Kalman un-project the difference to the original state
//   - the time registration will do a Kalman update of the current estimate only
// todo: could do basis-conditioning steps on both parent and child;
//   ensure: child < min_size(updated parent cov) < parent < max_size(updated parent cov)
// todo: need to figure and store the "quality" estimate (probs the normalization factor).
//   - could also use the quantified_update formula, and incorporate this

// number of invocations for samples
// @id(1000) override Sample_Invocations: u32 = 4;
// number of samples per invocation
// @id(1001) override Sample_Multiple: u32 = 4;

const Sample_Invocations: u32 = 4;

alias FuseMode = u32;

const FuseMode_TimeUpdate:   FuseMode = 0;
const FuseMode_StereoUpdate: FuseMode = 1;
const FuseMode_StereoInit:   FuseMode = 2;

struct FuseUniforms {
    fuse_mode:   FuseMode,
    multiple:    u32,
    cam_a:       CameraState,
    cam_b:       CameraState,
}

@group(0) @binding(0) var<storage,read> samples:       array<WeightedSample>;
@group(0) @binding(1) var<uniform>      uniforms:      FuseUniforms;
@group(0) @binding(2) var<uniform>      feature_range: FeatureRange;

// nb: different bindgroups below!
@group(1) @binding(0) var<storage,read>       src_img_features:   array<FeaturePair>;
@group(2) @binding(0) var<storage,read>       src_scene_features: array<SceneFeature>;
@group(3) @binding(0) var<storage,read_write> dst_scene_features: array<SceneFeature>;
@group(4) @binding(0) var<storage,read>       feature_idx_buffer: array<u32>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    var sample_count: u32 = Sample_Invocations * uniforms.multiple * 2;
    let feature_idx:  u32 = global_id.x;
    let i_idx:        u32 = global_id.x + feature_range.feature_start;
    if i_idx >= arrayLength(&feature_idx_buffer) || i_idx > feature_range.feature_end {
        return;
    }
    let scene_feature_idx: u32 = feature_idx_buffer[i_idx];
    if scene_feature_idx >= arrayLength(&src_scene_features) { return; }
    
    // compute the "frequency weighted" covariance and mean of the sample set
    // see https://stats.stackexchange.com/questions/193046/online-weighted-covariance
    // this is a best guess of the distribution of the displacement between the
    // feature endpoints, accounting for both prior predictions and the actual
    // image registration.
    var mu:   vec2f = vec2f(0.);
    var w_sum:  f32 = 0.;
    var running_cov = mat2x2f(); // zero matrix
    for (var i: u32 = 0u; i < sample_count; i++) {
        let sample_index: u32 = feature_idx * sample_count + i;
        let sample: WeightedSample = samples[sample_index];
        let w: f32 = sample.w;
        w_sum += w;
        let dx: vec2f = sample.x - mu;
        mu += dx * w / w_sum;
        running_cov += w * outer2x2(dx);
    }
    let k: f32 = 1. / w_sum;
    let est_cov: mat2x2f = k * running_cov;
    
    let src_img_feature:   FeaturePair  = src_img_features[feature_idx];
    let src_scene_feature: SceneFeature = src_scene_features[scene_feature_idx];
    
    if uniforms.fuse_mode == FuseMode_TimeUpdate {
        // perform a time update
        
        // xxx todo: this
        
    } else if uniforms.fuse_mode == FuseMode_StereoUpdate {
        // perform a stereo update.
        // the correlogram is just a *correction* to the difference between
        // the two views; add them together. take the difference in kernel-space.
        let tex2kern_a: mat2x2f = inverse2x2(src_img_feature.a.basis);
        let tex2kern_b: mat2x2f = inverse2x2(src_img_feature.b.basis);
        let dx: vec2f = tex2kern_b * src_img_feature.b.st - tex2kern_a * src_img_feature.a.st;
        let updated: Estimate3D = unproject_kalman_view_difference(
            Estimate3D(src_scene_feature.x, src_scene_feature.x_cov),
            Estimate2D(mu + dx, est_cov),
            tex2kern_a,
            tex2kern_b,
            uniforms.cam_a,
            uniforms.cam_b
        );
        dst_scene_features[scene_feature_idx] = SceneFeature(
            // todo: no update to feature orientation for now
            src_scene_feature.q,
            src_scene_feature.q_cov,
            updated.x,
            updated.sigma,
            1. // xxx todo: update quality estimate
        );
    } else if uniforms.fuse_mode == FuseMode_StereoInit {
        
        // xxx todo: use update_ekf_unproject_initial_2d_3d()
    }
    
}
