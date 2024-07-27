struct Estimate2D {
    x:          vec2f,
    sqrt_sigma: mat2x2f,
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
