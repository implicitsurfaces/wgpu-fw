const Tau: f32 = 6.28318530717958647692;


// todo: we might want to use the log of bhattacharyya distance;
//  many formulations appear to write it that way, and it might have
//  better numerical properties.

struct Estimate2D {
    x:          vec2f,
    sqrt_sigma: mat2x2f,
}

struct QuantifiedEstimate2D {
    estimate: Estimate2D,
    // use a (modified) Bhattacharyya distance to quantify the overlap
    // between the update & prior distributions
    quality:  f32,
}

fn sqrt_2x2(m: mat2x2f) -> mat2x2f {
    let trace: f32 = m[0][0] + m[1][1];
    let det:   f32 = determinant(m);
    // pick the sign of s to match the sign of the trace, so they don't cancel
    let s:     f32 = (trace != 0 ? sign(trace) : 1.) * sqrt(det);
    let t:     f32 = sqrt(trace + 2. * s);
    return (m + mat2x2f(s)) / t;
}

fn transform(a: Estimate2D, dx: vec2f, F: mat2x2f) -> Estimate2D {
    return Estimate2D(
        dx + a.x,
        F  * a.sqrt_sigma,
    );
}

fn update(a: Estimate2D, b: Estimate2D) -> Estimate2D {
    let S0 = a.sqrt_sigma * transpose(a.sqrt_sigma);
    let S1 = b.sqrt_sigma * transpose(b.sqrt_sigma);
    let K: mat2x2f = S0 * (S0 + S1).inverse();
    let J: mat2x2f = mat2x2f(1.) - K;
    let L: mat2x2f = sqrt_2x2(J);
    
    return Estimate2D(
        a.x + K * (b.x - a.x), // update mean
        L * a.sqrt_sigma,      // update sqrt covariance
    );
}

fn update_quantified(a: Estimate2D, b: Estimate2D) -> QuantifiedEstimate2D {
    let S0 = a.sqrt_sigma * transpose(a.sqrt_sigma);
    let S1 = b.sqrt_sigma * transpose(b.sqrt_sigma);
    let s01_inv: mat2x2f = (S0 + S1).inverse();
    let K: mat2x2f = S0 * s01_inv;
    let J: mat2x2f = mat2x2f(1.) - K;
    let L: mat2x2f = sqrt_2x2(J);
    let dx: vec2f = b.x - a.x;
    
    let e = Estimate2D(
        a.x + K * dx,     // update mean
        L * a.sqrt_sigma, // update sqrt covariance
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
