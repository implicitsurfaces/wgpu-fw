// #include "mat_helpers.wgsl"
// #include "inverse.wgsl"

const Tau: f32 = 6.28318530717958647692;


// todo: we might want to use the log of bhattacharyya distance;
//  many formulations appear to write it that way, and it might have
//  better numerical properties.

struct Estimate2D {
    x:          vec2f,
    sigma: mat2x2f,
}

struct Estimate3D {
    x:          vec3f,
    sigma: mat3x3f,
}

struct QuantifiedEstimate2D {
    estimate: Estimate2D,
    // use a (modified) Bhattacharyya distance to quantify the overlap
    // between the update & prior distributions
    quality:  f32,
}

fn regularize_posdef_2x2(m: mat2x2f) -> mat2x2f {
    return 0.5 * (m + transpose(m));
}

fn regularize_posdef_3x3(m: mat3x3f) -> mat3x3f {
    return 0.5 * (m + transpose(m));
}

fn transform2d(a: Estimate2D, dx: vec2f, F: mat2x2f) -> Estimate2D {
    return Estimate2D(
        dx + a.x,
        F  * a.sigma * transpose(F),
    );
}

fn transform3d(a: Estimate3D, dx: vec3f, F: mat3x3f) -> Estimate3D {
    return Estimate3D(
        dx + a.x,
        F  * a.sigma * transpose(F),
    );
}

fn update(a: Estimate2D, b: Estimate2D) -> Estimate2D {
    let S0 = a.sigma;
    let S1 = b.sigma;
    let K: mat2x2f = S0 * inverse2x2(S0 + S1);
    let J: mat2x2f = I_2x2 - K;
    let E: mat2x2f = J * a.sigma * transpose(J);
    
    return Estimate2D(
        a.x + K * (b.x - a.x),    // update mean
        regularize_posdef_2x2(E), // update covariance
    );
}

fn update_quantified(a: Estimate2D, b: Estimate2D) -> QuantifiedEstimate2D {
    let S0 = a.sigma;
    let S1 = b.sigma;
    let s01_inv: mat2x2f = inverse2x2(S0 + S1);
    let K: mat2x2f = S0 * s01_inv;
    let J: mat2x2f = I_2x2 - K;
    let dx:  vec2f = b.x - a.x;
    let E: mat2x2f = J * a.sigma * transpose(J);
    
    let e = Estimate2D(
        a.x + K * dx,             // update mean
        regularize_posdef_2x2(E), // update covariance
    );
    
    // normalization factors of the two estimates;
    // the "characteristic areas" of the two distributions.
    // we omit the factor of inverse tau from all the measures below.
    let a_norm: f32 = sqrt(determinant(S0));
    let b_norm: f32 = sqrt(determinant(S1));
    
    // overlap area of the two distributions
    let ln_q:   f32 = -dot(dx, s01_inv * dx) / 2.;
    let c_norm: f32 = sqrt(determinant(s01_inv));
    
    return QuantifiedEstimate2D(
        e,
        // "overlap area" over "characteristic area"
        exp(ln_q) / (a_norm * b_norm * c_norm)
        // log of the above:
        // ln_q - log(k_inv) - log(a_norm) - log(b_norm),
    );
}

fn update_ekf_unproject_2d_3d(
    prior:       Estimate3D,
    prior_2d:    vec2f,
    measurement: Estimate2D,
    H:           mat3x2f) -> Estimate3D
{
    // recall that because of the column major convention, the matrix
    // row and column counts are reversed from the usual convention.
    let P:  mat3x3f = prior.sigma;
    let R:  mat2x2f = measurement.sigma;
    let HT: mat2x3f = transpose(H);
    let S0: mat2x2f = inverse2x2(H * P * HT + R);
    //  (3x2) = (3x3) * (3x2) * (2x2)
    let K:  mat2x3f = P * HT * S0;
    let M:  mat3x3f = I_3x3 - K * H;
    
    let P1: mat3x3f = M * P;
    let mu: vec3f   = prior.x + K * (measurement.x - prior_2d);
    
    return Estimate3D(
        mu,
        regularize_posdef_3x3(P1)
    );
}

// xxx: todo: I think we need three estimates: a, b, and b-a.
//   > could we fanagle that into a 4x4 so we can invert?
//     it all comes down to how to generate that covariance matrix.
//     > I think yes. the Q is: if I have E[A], E[B], and E[B-A],
//       what is E[A,B]?
//       > this is called cross-covariance. I'm not sure that we actually can measure
//         anything about it?
//       > but I think maybe if we really don't know anything about it, it's valid to
//         set it to zero, and let the unprojection handle any variable interrelationships
//       > we may be able to use the formula: cov(X + Y) = cov(X) + cov(Y) + 2 * cov(X,Y)
//         - dimensionality on this seems whack? what's up here
//     > this is a 6x6 matrix, so we need to code up the inverse for that
//       (a.xy, b.xy, b.xy - a.xy). note that it's a positive definite matrix,
//       so this is probably a simpler/cheaper operation than a general inverse.
fn update_ekf_unproject_initial_2d_3d(
    measure_a: Estimate2D,
    measure_b: Estimate2D,
    H_a:       mat3x2f,
    H_b:       mat3x2f) -> Estimate3D
{
    // see 'initialization of the kalman filter without assumptions
    // on the initial state' by linderoth et al.; equation 5
    let H_aT = transpose(H_a);
    let H_bT = transpose(H_b);
    let HT: mat4x3f = mat4x3f(
        H_aT[0], H_aT[1], H_bT[0], H_bT[1]
    );
    let H: mat3x4f = transpose(HT);
    
    // xxx: what does R look like? it should be 4x4, and I think it's not block-diagonal
    let R: mat4x4f = mat4x4f(); // ...
    
    let R_inv:  mat4x4f = inverse4x4(R);
    var S:      mat3x3f = inverse3x3(HT * R_inv * H);
    S = regularize_posdef_3x3(S);
    
    return Estimate3D(
        S * HT * R_inv * vec4f(measure_a.x, measure_b.x),
        S
    );
}

fn update_unproject_2d_3d(
        prior:       Estimate3D,
        measurement: Estimate2D,
        H:           mat3x2f) -> Estimate3D
{
    let p_2d: vec2f = H * prior.x;
    return update_ekf_unproject_2d_3d(prior, p_2d, measurement, H);
}

// todo / questions:
// - can we block-diagonalize the update?
//   - does the math factor nicely?
//     I'm thinking we assume uncorrelated between the blocks, and then
//     update accordingly. does this introduce off-diagonal terms?
//     > yes.
//   - if yes, what do we get if try to project the correlation down
//     to block-diagonal state again?
//     > you just extract the diagonal block and discard the off diagonals.
// - can we discard the off-diagonal terms in the update step early if we know
//   we will not store them?
//   - write this in terms of a proj mtx; see if you can absorb it into
//     the internal calc?
//   - this is extremely useful if possible, because we want three 7x7 diagonal blocks
//     (cam A, cam B, feature), and it would be great not to have to multiply
//     21x21 matrices (400 elements).
//     > even in the best case, this is 84 elements, which is still a lot;
//       we may have to split up this step into smaller passes or multiple invocations.
// - write 3d backproject over difference betw. camera views
// - how do we do a blockified matrix mult?
//   - I suspect it's the same as the mxmul algorithm, but with
//     the sub-blocks instead of the elements.
// - we how shall we handle the camera Q.T coviariance? (upper tri 7x7 = 28 elements)
//   - should we write an upper tri matmul?
//     these are needed:
//     - 2x7 * (7x7 upper or lower tri) -> 2x7
//     - (7x7 upper or lower tri) * 7x2 -> 7x2
//     - 2x7 * 7x2 -> 2x2
//     - 7x2 * 2x7 outer product -> full 7x7
//     - 7x7 * (7x7 upper or lower tri) -> full 7x7
//     - 7x7 mtx sqrt
//    > 7x7 update toolset is critical because we can also use it to update
//      the "augmented" feature state (xyz + q).
// - Q: what happens when you cholesky factorize a lower triangular matrix?
//   > A: you can't. cholesky only operates on symmetric positive definite matrices.
// - Q: can we keep our Q mean at 0? what would that update look like?
//   - what does it mean to "observe" q?
//   - how do we "renormalize" Q to force it to have the correct covariance for
//     points on the unit sphere?
