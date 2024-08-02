// #include "sample.wgsl"
// #include "structs.wgsl"

// todo: get rigorous about the feature offset
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


// okay, here's what we're doing:
// - "multiple target" sampling is an initialization problem.
//   - we're doing that when we create new features
//   - it's a particle filter-like step
//   - we have a buffer of correllograms, so we can probably figure out
//     how to do this without redundant computation
// - we're maintaining a new invariant: all image feature-pairs
//   in a tree back-project to a single "coherent" feature in _scene space_
// x we'll update the parent only at this step
//   x we'll generate N samples from the correllogram, and N from the (sigma pts?)
//     of the estimate covariance
//   x we'll get the joint pr for each sample and treat that as a weighted
//     ptcloud for a new mean and covariance.
//     - bc the importance sampled distro already has its own "weighting",
//       it's the other distro's pdf that is the weight. double check this.
//   ~ we could factor sample generation across cores and then do the aggregation
//     quickly at the end
//     > there is enough unique "end stuff" that this should get its own stage
// x the covariance of the _disparity_ in correllogram space comes by subtracting
//   the means of the two features, which are defined to be at the center of the window,
//   so it's just the sum of the respective covariances.
// x the image features get the symmetric update we figured out below
// x the distributions of the child features should be considered as relative
//   to their parent. we need this so that children recieve updated
//   info about their positioning extracted from the coarser level.
//   - this update needs to happen BEFORE image sampling, so
//     likely as a final step when the parent is updated.
//   ~ this could be a very light "parent buffer" of v2ds
//     > no, bc of the above requirement that this happen before sampling.
//       the fuse stage is the last stage to touch a Feature before it gets sampled.
//   - if there were a window space update, this would also propagate here
//   > we'll unproject back to feature space, then predict children based on parent.
// x we can discard the "patch sampling". double check that the old algorithm
//   isn't missing some updates / bug discoveries from our latest work
// - quality estimate is the normalization factor of the sample pool
// > question: when and how do shitty samples get pruned?
// > this work turned out to be irrelevant for this step:
//   - gaussian dot product
//   - patch sampling (goodbye two days of work :|)
//   - all NxN sampling logic
//   > all of these might be useful for the resampling step, though

// todo: update the children with the parent's delta
// todo: check that max samples not exceeded
// todo: precomputed sample pts do not need to have their pdfs stored

// - thing I don't like: we could move the mean a lot, until the relatively tiny child
//   covars don't overlap their actual positions
//   - should we do something like propagate updated parent covar to child?
//   > the fix for this is that those covariances should never be that small.
//     or if they are that small, that means we should _expect_ to find them
//     at their locations relative to the moved mean
//   > if this gets away from us, that's a tree conditioning / process noise step
//     make a note of this, test/observe, and address if it causes issues.

struct Covariance {
    sqrt_cov:     mat2x2f,
    inv_sqrt_cov: mat2x2f,
    i_sqrt_det:   f32, // 1 / determinant(sqrt_cov)
}
  
// todo: precompute a longer array in an LD sequence, length 32 probably
// see https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
const uniform_2d_samples: array<vec2f, 32> = array<vec2f, 32>(
    vec2f(0.25, 0.25),
    vec2f(0.25, 0.75),
    vec2f(0.75, 0.25),
    vec2f(0.75, 0.75),
    // xxx do this
);

// xxx: todo: this
const unit_gaussian_2d_samples: array<Sample, 32> = array<Sample, 32>(
    // xxx: pls
);

// number of invocations for samples
@id(1001) override Sample_Invocations: u32 = 4;
// number of samples per invocation
@id(1002) override Sample_Multiple: u32 = 4;

const Sample_Count: u32 = Sample_Invocations * Sample_Multiple * 2;
const Wg_Width:     i32 = 64 / Sample_Invocations;
const Tau:          f32 = 6.28318530717958647692;

@group(0) @binding(0) var<storage,read>  correlations: array<CorrelationWindow>;
@group(0) @binding(1) var<storage,write> dst_samples:  array<WeightedSample>;

@group(1) @binding(0) var<storage,read>  src_features: array<FeaturePair>;

// xy is in [0, 1]^2 -> [-1, 1]^2
fn corr_to_displacement(xy: vec2f) -> vec2f {
    // cut up the square into 4 quadrants
    let uv = modf(xy * 2);
    // map to [-1, 1]
    return vec2f(uv.x.fract, uv.y.fract) - vec2f(uv.x.whole, uv.y.whole);
}

fn gauss_sample(sample_index: u32, cov: Covariance) -> Sample {
    let sample: Sample = unit_gaussian_2d_samples[sample_index];
    return Sample(
        cov.sqrt_cov * sample.st,
        sample.pdf * cov.i_sqrt_det / Tau,
    );
}

fn gauss_pdf(x: vec2f, cov: Covariance) -> f32 {
    let dx: vec2f = cov.inv_sqrt_cov * x; // put x in the space of the unit gaussian
    let r: f32 = dot(dx, dx);
    return exp(-0.5 * r * r) * cov.i_sqrt_det / Tau;
}

@compute @workgroup_size(Wg_Width, Sample_Invocations)
fn main(
    @builtin(global_invocation_id)   global_id: vec3u,
    @builtin(local_invocation_id)    local_id:  vec3u,
    @builtin(local_invocation_index) local_index: u32)
{
    let feature_idx: u32 = global_id.x;
    let chunk_idx:   u32 = global_id.y;
    if feature_idx >= u32(src_features.length()) { return; }
    
    // we do this redundantly in each invocation, but we _have_ to spend the time
    // to perform it once; it doesn't matter if multiple cores are doing it simultaneously
    let img: SampleImage = make_sample_image(correlations[feature_idx].correlation);
    let src_feature: FeaturePair = src_features[feature_idx];
    let sqrt_cov_dx: mat2x2f = src_feature.a.sqrt_cov + src_feature.b.sqrt_cov;
    let cov: Covariance = Covariance(
        sqrt_cov_dx,
        inverse(sqrt_cov_dx),
        1. / determinant(sqrt_cov_dx),
    );
    
    let pop_base: u32 = feature_idx * Sample_Count;
    for (let i: i32 = 0; i < Sample_Multiple; i++) {
        // we do a kind of "MIS" here— we want the mean and covariance of the joint
        // probability distributions. distribution A is already "weighted" by nature
        // of being importance sampled, so we need only multiply in the pdf of the
        // other distribution.
        let sample_id:         u32 = pop_base + chunk_idx * Sample_Multiple * 2 + i;
        let gaus_sample:    Sample = gauss_sample(sample_id, cov);
        let xcor_sample:    Sample = sample_interp_image(img, uniform_2d_samples[sample_id]);
        xcor_sample.st             = corr_to_displacement(xcor_sample.st);
        let pdf_xcor_at_gaus:  f32 = gauss_pdf(xcor_sample.st, cov);
        let pdf_gaus_at_xcor:  f32 = eval_interp_image(img, gaus_sample.st);
        dst_samples[sample_id]     = WeightedSample(xcor_sample.st, pdf_gaus_at_xcor);
        dst_samples[sample_id + 1] = WeightedSample(gaus_sample.st, pdf_xcor_at_gaus);
    }
}
