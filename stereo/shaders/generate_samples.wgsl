// #include "structs.wgsl"
// #include "sample.wgsl"
// #include "inverse.wgsl"
// #include "mat_helpers.wgsl"

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

alias SampleMode = u32;

const SampleMode_Joint:       SampleMode = 0;
const SampleMode_Correlation: SampleMode = 1;

struct Covariance {
    sqrt_cov:     mat2x2f,
    inv_sqrt_cov: mat2x2f,
    i_sqrt_det:   f32, // 1 / determinant(sqrt_cov)
}

struct SampleUniforms {
    multiple: u32,
    mode:     SampleMode,
}

// todo: wgpu support for constants is still in progress
// number of invocations for samples
// @id(1000) override Sample_Invocations: u32 = 4;

const Sample_Invocations: u32 = 4;
const Wg_Width:           u32 = 64 / Sample_Invocations;
const Tau:                f32 = 6.28318530717958647692;

@group(0) @binding(0) var<storage,read>       correlations: array<CorrelationKernel>;
@group(0) @binding(1) var<storage,read_write> dst_samples:  array<WeightedSample>;
@group(0) @binding(2) var<uniform>            uniforms:     SampleUniforms;

@group(1) @binding(0) var<storage,read> src_features: array<FeaturePair>;

@group(2) @binding(0) var<storage,read> uniform_2d_samples:       array<vec2f>;
@group(2) @binding(1) var<storage,read> unit_gaussian_2d_samples: array<vec2f>;

// xy is in [0, 1]^2 -> [-1, 1]^2
fn corr_to_displacement(xy: vec2f) -> vec2f {
    // cut up the square into 4 quadrants
    let uv = modf(xy * 2);
    // map to [-1, 1]
    return uv.fract - vec2f(uv.whole);
}

// xy is in [-1, 1]^2 -> [0, 1]^2
fn displacement_to_corr(xy: vec2f) -> vec2f {
    let st = xy + 1.;
    let uv = modf(st);
    return 0.5 * (uv.fract - vec2f(uv.whole)) + 0.5;
}

fn gauss_pdf(x: vec2f, cov: Covariance) -> f32 {
    let dx: vec2f = cov.inv_sqrt_cov * x; // put x in the space of the unit gaussian
    let r2: f32 = dot(dx, dx);
    return exp(-0.5 * r2) * cov.i_sqrt_det / Tau;
}

@compute @workgroup_size(Wg_Width, Sample_Invocations)
fn main(
    @builtin(global_invocation_id)   global_id: vec3u,
    @builtin(local_invocation_id)    local_id:  vec3u,
    @builtin(local_invocation_index) local_index: u32)
{
    let feature_idx: u32 = global_id.x;
    let invoc_idx:   u32 = global_id.y;
    if feature_idx >= arrayLength(&src_features) { return; }
    
    // we do this redundantly in each invocation, but we _have_ to spend the time
    // to perform it once; it doesn't matter if multiple cores are doing it simultaneously
    let img: SampleImage = make_sample_image(correlations[feature_idx].correlation);
    let src_feature: FeaturePair = src_features[feature_idx];
    // put the texture-space covariances in the kernel space
    let a_tex2kern:   mat2x2f = inverse2x2(src_feature.a.basis);
    let b_tex2kern:   mat2x2f = inverse2x2(src_feature.b.basis);
    let a_cov_kernel: mat2x2f = a_tex2kern * src_feature.a.cov * transpose(a_tex2kern);
    let b_cov_kernel: mat2x2f = b_tex2kern * src_feature.b.cov * transpose(b_tex2kern);
    // difference and sqrt (sign has no effect on covariance):
    let sqrt_cov_dx:  mat2x2f = sqrt_2x2(a_cov_kernel + b_cov_kernel);
    let cov: Covariance = Covariance(
        sqrt_cov_dx,
        inverse2x2(sqrt_cov_dx),
        1. / determinant(sqrt_cov_dx),
    );
    
    let sample_count: u32 = uniforms.multiple * Sample_Invocations * 2;
    let samples_per_invocation: u32 = uniforms.multiple * 2;
    let population_base: u32 = feature_idx * sample_count;
    if uniforms.mode == SampleMode_Joint {
        for (var i: u32 = 0u; i < uniforms.multiple; i++) {
            // we do a kind of "MIS" here— we want the mean and covariance of the joint
            // probability distributions. distribution A is already "weighted" by nature
            // of being importance sampled, so we need only multiply in the pdf of the
            // other distribution.
            let feature_sample_id: u32 = invoc_idx * uniforms.multiple + i;
            let sample_base:       u32 = population_base + invoc_idx * samples_per_invocation + i * 2;
            let gaus_sample:     vec2f = cov.sqrt_cov * unit_gaussian_2d_samples[feature_sample_id];
            var xcor_sample:     vec2f = sample_interp_image(img, uniform_2d_samples[feature_sample_id]).st;
            xcor_sample                = corr_to_displacement(xcor_sample);
            
            let pdf_gaus_at_xcor:  f32 = gauss_pdf(xcor_sample, cov);
            let pdf_xcor_at_gaus:  f32 = eval_interp_image(img.image, displacement_to_corr(gaus_sample));
            // write the samples in feature.basis coordinates
            dst_samples[sample_base]     = WeightedSample(xcor_sample, pdf_gaus_at_xcor);
            dst_samples[sample_base + 1] = WeightedSample(gaus_sample, pdf_xcor_at_gaus);
        }
    } else if uniforms.mode == SampleMode_Correlation {
        for (var i: u32 = 0u; i < uniforms.multiple * 2; i++) {
            let feature_sample_id: u32 = invoc_idx * uniforms.multiple + i;
            let sample_base:       u32 = population_base + invoc_idx * samples_per_invocation + i;
            var xcor_sample:     vec2f = sample_interp_image(img, uniform_2d_samples[feature_sample_id]).st;
            xcor_sample                = corr_to_displacement(xcor_sample);
            dst_samples[sample_base]   = WeightedSample(xcor_sample, 1.);
        }
    }
}
