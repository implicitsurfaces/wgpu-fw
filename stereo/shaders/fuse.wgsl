// #include "structs.wgsl"
// #include "kalman.wgsl"

// todo: could do basis-conditioning steps on both parent and child;
//   ensure: child < min_size(updated parent cov) < parent < max_size(updated parent cov)
// todo: need to figure and store the "quality" estimate (probs the normalization factor).
//   - could also use the quantified_update formula, and incorporate this

// number of invocations for samples
// @id(1000) override Sample_Invocations: u32 = 4;
// number of samples per invocation
// @id(1001) override Sample_Multiple: u32 = 4;

const Sample_Invocations: u32 = 4;
const Sample_Multiple:    u32 = 4;
const Sample_Count:       u32 = Sample_Invocations * Sample_Multiple * 2;

// nb: each input in a different group!
@group(0) @binding(0) var<storage,read>       samples:      array<WeightedSample>;
@group(1) @binding(0) var<storage,read>       src_features: array<FeaturePair>;
@group(2) @binding(0) var<storage,read_write> dst_features: array<FeaturePair>;

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.x;
    
    // compute the "frequency weighted" covariance and mean of the sample set
    // see https://stats.stackexchange.com/questions/193046/online-weighted-covariance
    // this is a best guess of the distribution of the displacement between the
    // feature endpoints, accounting for both prior predictions and the actual
    // image registration.
    var mu:   vec2f = vec2f(0.);
    var w_sum:  f32 = 0.;
    var running_cov = mat2x2f(); // zero matrix
    for (var i: u32 = 0u; i < Sample_Count; i++) {
        let sample_index: u32 = feature_idx * Sample_Count + i;
        let sample: WeightedSample = samples[sample_index];
        let w: f32 = sample.w;
        w_sum += w;
        let dx: vec2f = sample.x - mu;
        mu += dx * w / w_sum;
        running_cov += w * outer2x2(dx);
    }
    let k: f32 = 1. / w_sum;
    let est_sqrt_cov: mat2x2f = sqrt_2x2(k * running_cov);
    
    let src_feature: FeaturePair = src_features[feature_idx];
    
    // perform symmetrical update— the measured displacement between
    // the two features constrains/informs where the endpoints can be.
    // init starting points:
    let a_prior: Estimate2D = Estimate2D(
        src_feature.a.st,
        src_feature.a.sqrt_cov,
    );
    let b_prior: Estimate2D = Estimate2D(
        src_feature.b.st,
        src_feature.b.sqrt_cov,
    );
    // B thinks A should be at -mu relative to itself
    let a_update: Estimate2D = Estimate2D(
        a_prior.x - mu,
        a_prior.sqrt_sigma + est_sqrt_cov, // uncertainty of starting pt + delta compound
    );
    // A thinks B should be at +mu relative to itself
    let b_update: Estimate2D = Estimate2D(
        b_prior.x + mu,
        b_prior.sqrt_sigma + est_sqrt_cov,
    );
    // fused estimates:
    let a_post: Estimate2D = update(a_prior, a_update);
    let b_post: Estimate2D = update(b_prior, b_update);
    let updated_feature = FeaturePair(
        src_feature.id,
        src_feature.depth,
        src_feature.parent,
        ImageFeature(
            a_post.x,
            a_post.sqrt_sigma,
            src_feature.a.basis,
        ),
        ImageFeature(
            b_post.x,
            b_post.sqrt_sigma,
            src_feature.b.basis,
        )
    );
    // update the feature
    dst_features[feature_idx] = updated_feature;
}
