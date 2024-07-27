// #include "sample.wgsl"
// #include "kalman.wgsl"

// todo: compute the sample covariance, and update it with the parent covariance
// todo: figure out how to compute the "kalman overlap" / normalization term for
//   an estimate of estimate quality
//   - figure out how to go from a pdf gradient to a covariance
//     (sigma pts?? outer product??)
//   - store this in the feature pair and use it for tree conditioning
// todo: include the sample pdf; use it to update sample quality
// todo: distinguish sample count and branching factor
//   - branching factor should be uniform.
//   - sample count should be
//     - based on the quality of the sample?
//     - override constant? (allows for fixed arrays of samples)
// todo: figure out how to estimate an update to the feature bases

// todo: is MIS out? are we not doing MIS?
//   > MIS is out

struct CorrelationWindow {
    correlation: mat4x4,
}

struct ImageFeature {
    st:        vec2f,
    sqrt_cov:  mat2x2f,
    basis:     mat2x2f,
}

struct FeaturePair {
    depth:  u32,
    parent: u32,
    a:      ImageFeature,
    b:      ImageFeature,
}

struct MatcherUniforms {
    tree_depth:          i32,
    feature_offset:      u32,
    refine_parent:       bool,
    correlation_samples: i32,
}

// @id(1000) override branching_factor: i32 = 4;

@group(0) @binding(0) var<storage,read>  correlations: array<CorrelationWindow>;
@group(0) @binding(1) var<storage,read>  src_features: array<FeaturePair>;
@group(0) @binding(2) var<storage,write> dst_features: array<FeaturePair>;
@group(0) @binding(3) var<uniform>       uniforms:     MatcherUniforms;

// xy is in [0, 1]
fn corr_to_displacement(xy: vec2f) -> vec2f {
    // cut up the square into 4 quadrants
    let uv = modf(xy * 2);
    // map to [-1, 1]
    return vec2f(uv.x.fract, uv.y.fract) - vec2f(uv.x.whole, uv.y.whole);
}

@compute @workgroup_size(64)
fn main(@builtin(global_invocation_id) global_id: vec3u) {
    let feature_idx: u32 = global_id.z + uniforms.feature_offset;
    if (feature_idx >= u32(src_features.length())) {
        return;
    }
    
    // todo: get a better sampling scheme
    const samples: array<vec2f, 4> = array<vec2f, 4>(
        vec2f(0.25, 0.25),
        vec2f(0.25, 0.75),
        vec2f(0.75, 0.25),
        vec2f(0.75, 0.75),
    );
    
    let img: SampleImage = make_sample_image(correlations[feature_idx].correlation);
    let src_feature: FeaturePair = src_features[feature_idx];
    for (let i: u32 = 0; i < uniforms.correlation_samples; i++) {
        let st: vec2f = samples[i];
        let xy: vec2f = sample_interp_image(img, st);
        // displacement from a's window origin to b's window origin, in the feature space
        let feature_disp: vec2f = corr_to_displacement(xy);
        // displacement of the feature in the respective image spaces
        let new_a_img_disp: vec2f = src_feature.a.st + src_feature.a.basis * feature_disp;
        let new_b_img_disp: vec2f = src_feature.b.st + src_feature.b.basis * feature_disp;
        
        // symmetrically update the locations and covariances of the image features.
        // todo: refine the covariance with the sample covariance
        let a_prior: Estimate2D = Estimate2D(
            new_a_img_disp,
            src_feature.a.sqrt_cov,
        );
        let b_prior: Estimate2D = Estimate2D(
            new_b_img_disp,
            src_feature.b.sqrt_cov,
        );
        let a_post: Estimate2D = update(a_prior, b_prior);
        let b_post: Estimate2D = update(b_prior, a_prior);
        
        // updated feature pair which incorporates the previous best estimate of
        // the displacement, and the new estimate based on image registration quality
        let feature: FeaturePair(
            uniforms.tree_depth + 1,
            feature_idx,
            ImageFeature(
                a_post.x,
                a_post.sqrt_sigma,
                src_feature.a.basis,
            ),
            ImageFeature(
                b_post.x,
                b_post.sqrt_sigma,
                src_feature.b.basis,
            ),
        );
        dst_features[feature_idx * uniforms.correlation_samples + i] = feature;
    }
}
