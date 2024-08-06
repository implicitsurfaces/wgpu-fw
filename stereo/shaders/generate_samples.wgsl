// #include "sample.wgsl"
// #include "structs.wgsl"

// todo: quality estimate
// todo: figure out how to estimate an update to the feature bases
// todo: check that max samples not exceeded
// todo: precomputed sample pts do not need to have their pdfs stored

// okay, here's what we're doing:
// - "multiple target" sampling is an initialization problem.
//   - we're doing that when we create new features
//   - it's a particle filter-like step
//   - we have a buffer of correllograms, so we can probably figure out
//     how to do this without redundant computation
// x we're maintaining a new invariant: all image feature-pairs
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
//   > no, we also need to know how good the correlation is
// > question: when and how do shitty samples get pruned?
// > this work turned out to be irrelevant for this step:
//   - gaussian dot product
//   - patch sampling (goodbye two days of work :|)
//   - all NxN sampling logic
//   > all of these might be useful for the resampling step, though


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
    vec2f(0.25487765, 0.06984026),
    vec2f(0.00975529, 0.63968052),
    vec2f(0.76463294, 0.20952078),
    vec2f(0.51951058, 0.77936104),
    vec2f(0.27438823, 0.34920130),
    vec2f(0.02926587, 0.91904155),
    vec2f(0.78414352, 0.48888181),
    vec2f(0.53902116, 0.05872207),
    vec2f(0.29389881, 0.62856233),
    vec2f(0.04877645, 0.19840259),
    vec2f(0.80365410, 0.76824285),
    vec2f(0.55853174, 0.33808311),
    vec2f(0.31340939, 0.90792337),
    vec2f(0.06828703, 0.47776363),
    vec2f(0.82316468, 0.04760389),
    vec2f(0.57804232, 0.61744415),
    vec2f(0.33291997, 0.18728440),
    vec2f(0.08779761, 0.75712466),
    vec2f(0.84267526, 0.32696492),
    vec2f(0.59755290, 0.89680518),
    vec2f(0.35243055, 0.46664544),
    vec2f(0.10730819, 0.03648570),
    vec2f(0.86218584, 0.60632596),
    vec2f(0.61706348, 0.17616622),
    vec2f(0.37194113, 0.74600648),
    vec2f(0.12681877, 0.31584674),
    vec2f(0.88169642, 0.88568700),
    vec2f(0.63657406, 0.45552726),
    vec2f(0.39145171, 0.02536751),
    vec2f(0.14632935, 0.59520777),
    vec2f(0.90120700, 0.16504803),
    vec2f(0.65608464, 0.73488829),
);

const unit_gaussian_2d_samples: array<Sample, 32> = array<Sample, 32>(
    vec2f( 1.49680485,  0.70250879),
    vec2f(-1.94438832, -2.34077875),
    vec2f( 0.18432872,  0.70904280),
    vec2f( 0.20993119, -1.12501782),
    vec2f(-0.93875604,  1.30581763),
    vec2f( 2.32109218, -1.29429425),
    vec2f(-0.69567017,  0.04867707),
    vec2f( 1.03693719,  0.40095157),
    vec2f(-1.08153675, -1.13106932),
    vec2f( 0.78293964,  2.32981132),
    vec2f( 0.07562191, -0.65685157),
    vec2f(-0.56730032,  0.91818190),
    vec2f( 1.27541851, -0.83294516),
    vec2f(-2.29433307,  0.32265550),
    vec2f( 0.59615865,  0.18382840),
    vec2f(-0.77464361, -0.70437466),
    vec2f( 0.56942901,  1.36947322),
    vec2f( 0.09870982, -2.20356491),
    vec2f(-0.27204781,  0.51801276),
    vec2f( 0.80885454, -0.61284520),
    vec2f(-1.41263111,  0.30046092),
    vec2f( 2.05756548,  0.48013004),
    vec2f(-0.42750802, -0.33735111),
    vec2f( 0.43967841,  0.87877736),
    vec2f(-0.03528644, -1.40598516),
    vec2f(-0.81701037,  1.86077577),
    vec2f( 0.37784344, -0.33022613),
    vec2f(-0.91356122,  0.26213528),
    vec2f( 1.35223314,  0.21737472),
    vec2f(-1.62010006, -1.10411344),
    vec2f( 0.23206277,  0.39266713),
    vec2f(-0.08704357, -0.91397722),
);

// todo: wgpu support for constants is still in progress
// number of invocations for samples
// @id(1000) override Sample_Invocations: u32 = 4;
// number of samples per invocation
// @id(1001) override Sample_Multiple: u32 = 4;

const Sample_Invocations: u32 = 4;
const Sample_Multiple:    u32 = 4;
const Sample_Count:       u32 = Sample_Invocations * Sample_Multiple * 2;
const Wg_Width:           i32 = 64 / Sample_Invocations;
const Tau:                f32 = 6.28318530717958647692;

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
    // if feature_idx >= arrayLength(src_features) { return; } // xxx why not work?
    
    // we do this redundantly in each invocation, but we _have_ to spend the time
    // to perform it once; it doesn't matter if multiple cores are doing it simultaneously
    let img: SampleImage = make_sample_image(correlations[feature_idx].correlation);
    let src_feature: FeaturePair = src_features[feature_idx];
    let sqrt_cov_dx: mat2x2f = src_feature.a.sqrt_cov + src_feature.b.sqrt_cov;
    let cov: Covariance = Covariance(
        sqrt_cov_dx,
        inverse2x2(sqrt_cov_dx),
        1. / determinant(sqrt_cov_dx),
    );
    
    let pop_base: u32 = feature_idx * Sample_Count;
    for (var i: i32 = 0; i < Sample_Multiple; i++) {
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
