// sample.wgsl
// Functions for sampling 2D discrete and continuous patches.

// in this file, polynomials are represented as vec4s, with the coefficients
// in increasing order of power. e.g. ax^3 + bx^2 + cx + d is represented as
// vec4(d, c, b, a). this is because the dot product of a vec4 with a vec4
// of powers of x gives the value of the polynomial at that point:
//   dot(vec4(d, c, b, a), vec4(1, x, x^2, x^3)) = ax^3 + bx^2 + cx + d

// likewise, a bicubic interpolation can be represented as a 4x4 matrix M.
// t * M gives a column polynomial; M * s gives a row polynomial, with s and
// t "power vectors" as above. t * M * s gives the value of the bicubic
// interpolation at (s, t).

// integral of ax^3 + bx^2 + cx + d is
//   g(x) = (a/4)x^4 + (b/3)x^3 + (c/2)x^2 + (d)x + e
// because we're dealing with cdfs, we know that g(0) = 0; therefore e = 0.
// therefore we can get the coefficients of the integral by multiplying the
// coefficients of the cubic polynomial by (1, 1/2, 1/3, 1/4) respectively.
// to evaluate the integral, we use the updated coefficients with an x-vector
// raised to one higher power; i.e. (x, x^2, x^3, x^4), instead of (1, x, x^2, x^3).
// nb: i find it very pleasing that integrals in this application are closed in vec4. :)
const INTEGRATE_CUBIC: vec4f = vec4f(1., 1. / 2., 1. / 3., 1. / 4.);
// dot this with a vec4 of samples to get the integral of the interpolating cubic
// over the unit interval.
const CUBIC_AREA: vec4f = (vec4f(-1, 1, 1, -1) / 12. + vec4f(0, 1, 1, 0)) / 2.;
// dot this with a vec4 of samples to get the areas of each cell in the bicubics
// which intperpolate the cyclical signal. the cell at index 1 contains area of
// the un-rotated signal's unit cell.
const AREA_TRANSFORM: mat4x4f = transpose(
    mat4x4f(
        CUBIC_AREA.wxyz,
        CUBIC_AREA.xyzw,
        CUBIC_AREA.yzwx,
        CUBIC_AREA.zwxy,
    )
);
// the matrix which transforms a vector of samples to the coefficients of the
// interpolating cubic.
const CUBIC_MTX: mat4x4f = transpose(
    mat4x4f(
        0., 2., 0., 0.,
       -1., 0., 1., 0.,
        2.,-5., 4.,-1.,
       -1., 3.,-3., 1.
    ) / 2.
);

struct Coordinate {
    x:           i32,
    x_remainder: f32,
    df_dx:       f32,
}


struct Coordinate2d {
    x:           vec2i,
    x_remainder: vec2f,
}

struct CoordinatePatch2d {
    x:           vec2i,
    x_remainder: vec2f,
    J:           mat2x2f,
}

struct Sample {
    st:  vec2f,
    pdf: f32,
}

struct SamplePatch {
    st:  vec2f,
    pdf: f32,
    J:   mat2x2f,
}

struct SampleImage {
    // the source samples
    image:            mat4x4f,
    // columnwise cdf at points in [0, 3]^2.
    // each cell has the accumulated area of the cells before it in the same row.
    marginal_col_cdf: mat4x4f,
    // rowwise cdf.
    // each entry has the accumulated area of the row before it.
    row_cdf:          vec4f,
}


fn value_or(v: f32, alt: f32) -> f32 {
    return select(v, alt, v == 0.);
}

fn bicubic(samples: mat4x4f) -> mat4x4f {
    return cubic_mtx * samples * transpose(cubic_mtx);
}

/*
fn eval_cubic_derivative(coeffs: vec4f, x: f32) -> f32 {
    let p: vec4f = vec4f(0., 1., 2. * x, 3. * x * x);
    return dot(p, coeffs);
}
*/

fn eval_bicubic(coeffs: mat4x4f, xy: vec2f) -> f32 {
    let x2: f32 = xy.x * xy.x;
    let y2: f32 = xy.y * xy.y;
    let x: vec4f = vec4f(1., xy.y, y2, y2 * xy.y);
    let y: vec4f = vec4f(1., xy.y, y2, y2 * xy.y);
    return dot(y, coeffs * x);
}

fn eval_bicubic_gradient(coeffs: mat4x4f, xy: vec2f) -> vec2f {
    let x2: f32   = xy.x * xy.x;
    let y2: f32   = xy.y * xy.y;
    let x:  vec4f = vec4f(1., xy.y, y2, y2 * xy.y);
    let y:  vec4f = vec4f(1., xy.y, y2, y2 * xy.y);
    let dx: vec4f = vec4f(0., 1., 2. * xy.y, 3. * y2);
    let dy: vec4f = vec4f(0., 1., 2. * xy.y, 3. * y2);
    
    return vec2f(
        dot(y,  coeffs * dx), // df / dx
        dot(dy, coeffs * x ), // df / dy
    );
}

fn cdf_bilinear_gradient(
    marginal_col_cdf: mat4x4f,
    smp_x: Coordinate,
    smp_y: Coordinate) -> vec2f
{
    let i:    i32 = smp_x.x;
    let j:    i32 = smp_y.x;
    let x:    f32 = smp_x.x_remainder;
    let y:    f32 = smp_y.x_remainder;
    let F_11: f32 = marginal_col_cdf[i][j];
    let F_00: f32 = select(0., marginal_col_cdf[i - 1][j - 1], i > 0 && j > 0);
    let F_01: f32 = select(0., marginal_col_cdf[i - 1][j    ], i > 0);
    let F_10: f32 = select(0., marginal_col_cdf[i    ][j - 1], j > 0);
    let dy_0: f32 = F_01;
    let dy_1: f32 = F_11;
    let dx_0: f32 = F_10 - F_00;
    let dx_1: f32 = dx_0 + (F_11 - F_01);
    
    return mix(
        vec2f(dx_0, dy_0),
        vec2f(dx_1, dy_1),
        vec2f(x, y)
    );
}

fn cycled_bicubic(d: u32) -> mat4x4f {
    const j: vec4u          = vec4ui(0, 1, 2, 3);
    const J: array<vec4u,4> = array<vec4u,4>(
        j.wxyz,
        j.xyzw,
        j.yzwx,
        j.zwxy,
    );
    const i: vec4u = J[d];
    return mat4x4f(
        CUBIC_MTX[i.x],
        CUBIC_MTX[i.y],
        CUBIC_MTX[i.z],
        CUBIC_MTX[i.w],
    );
}

fn bicubic_cell(samples: mat4x4f, xy: vec2i) -> f32 {
    return cycled_bicubic(xy.y) * samples * transpose(cycled_bicubic(xy.x));
}

// rootfind the normalized integral of a cubic polynomial over the unit interval.
// `coeffs` is the coefficients of the cubic function. `y` is in [0, 1].
// the range of the polynomial will be scaled to [0,1] before inverting.
fn invert_normed_cubic_integral(coeffs: vec4f, y: f32) -> f32 {
    coeffs *= INTEGRATE_CUBIC; // integrate the cubic
    // bound the root on the left and the right
    let x:    f32 = y;
    let lo:   f32 = 0;
    let hi:   f32 = 1;
    let f_lo: f32 = 0;                      // f(0)
    let f_hi: f32 = dot(coeffs, vec4f(1.)); // f(1)
    y    *= f_hi; // scale y to the integral of the cubic
    // subtract y so we can find the root of f(x) = 0
    f_lo -= y;
    f_hi -= y;
    // error ~halves each iteration, and we start with ~1 bit of error.
    // this function is pretty fuzzy in the 0,1 interval, so we don't
    // need a huge amount of precision. 6 iters oughtta get us to <=1% error
    for (let i: u32 = 0; i < 6; i++) {
        let x2: f32 = x * x;
        // coeffs are a quartic:
        let  sv: vec4f = vec4(x, x2, x2 * x, x2 * x2);
        let f_x: f32   = dot(sv, coeffs) - y;
        if f_x < 0. {
            lo   = x;
            f_lo = f_x;
        } else {
            hi   = x;
            f_hi = f_x;
        }
        let dy: f32 = f_hi - f_lo;
        if abs(dy) > 1e-5 {
            // double false position method (will not diverge).
            // linearly interpolate the two bracket points to find f(x) = 0
            x = -f_lo * (hi - lo) / dy + lo;
        } else {
            // delta too smol. fall back to bisection method.
            // this could happen if f is uniformly zero, e.g.;
            // or just very flat here
            x = mix(hi, lo, 0.5);
        }
    }
    return x;
}

// sample the piecewise linear cdf `cdf` at `x_01` in [0, 1].
// return the sampled x-coordinate of the function in [0, 4].
fn sample_cdf_vector(cdf: vec4f, x_01: f32) -> Coordinate {
    if cdf[3] == 0. {
        let z = modf(x_01 * 4., i);
        return Coordinate(i32(z.whole), z.frac, 0.25 * dx);
    }
    cdf /= cdf[3];
    let i: i32;
    if x_01 >= cdf[1] {
        if x_01 >= cdf[2] {
            i = 3;
        } else {
            i = 2;
        }
    } else {
        if x_01 >= cdf[0] {
            i = 1;
        } else {
            i = 0;
        }
    }
    let y0:  f32 = i > 0 ? cdf[i - 1] : 0.;
    let rem: f32 = x_01   - y0;
    let d_i: f32 = cdf[i] - y0;
    return Coordinate(i, rem / d_i, df_dx, 1. / d_i);
}

// distribute the uniform sample `uv` over the bicubic interpolation on the unit square
// with probability proportional to the value of that interpolation.
// `c` are the coefficients of the interpolation. `uv` should be in [0, 1]^2
fn sample_bicubic(c: mat4x4f, uv: vec2f) -> Sample {
    // let G(x,t) = integral(f(x,y) dy) from 0 to t.
    // that is, integrate the area in "columns".
    // then obtain a polynomial for g_1(s) = G(s, 1)
    // that is, evaluate the area in each column, yielding a polynomial `g`.
    let g: vec4f = INTEGRATE_CUBIC * c;
    // find s such that g_1(s) = uv.s
    // that is, take the integral of the per-column area, and rootfind
    // for where it equals `uv.s`; pick a column.
    let s:   f32 = invert_normed_cubic_integral(g, uv.u);
    let s2:  f32 = s * s;
    //  let h(t) = f(s, t) for the chosen s.
    let h: vec4f = c * vec4(1., s, s2, s2 * s);
    // find t such that integral(h(t) dt) = uv.y; pick a "row".
    let t:   f32 = invert_normed_cubic_integral(h, uv.v);
    let t2:  f32 = t * t;
    
    return Sample(
        vec2f(s, t),
        // eval bicubic at (s, t) (recall h = c * sv)
        dot(vec4(1., t, t2, t * t2), h)
    );
}

// perform a sampling of the given bicubic,
// recording the jacobian and value at the sample point.
fn sample_bicubic_patch(c: mat4x4f, uv: vec2f) -> SamplePatch {
    // let G(x,t) = integral(f(x,y) dy) from 0 to t.
    // that is, integrate the area in "columns".
    // then obtain a polynomial for g_1(s) = G(s, 1)
    // that is, evaluate the area in each column, yielding a polynomial `g`.
    let g:   vec4f = INTEGRATE_CUBIC * c;
    // find s such that g_1(s) = uv.s
    // that is, take the integral of the per-column area, and rootfind
    // for where it equals `uv.s`; pick a column.
    let s:     f32 = invert_normed_cubic_integral(g, uv.u);
    let s2:    f32 = s * s;
    let sv:  vec4f = vec4(1., s, s2, s2 * s);
    
    // compute derivatives. we inverted the _integral_ of our polynomial, so
    // its derivative is (the reciprocal of) the polynomial itself, evaluated at the root.
    let g_1:   f32 = value_or(dot(g, INTEGRATE_CUBIC), 1.);
    let ds_du: f32 = g_1 / value_or(dot(sv, g), 1.);
    let ds_dv: f32 = 0.; // s is independent of v
    
    // let h(t) = f(s, t) for the chosen s. (this is a column slice)
    let h:   vec4f = c * sv; // <-- column slice polynomial at s
    // find t such that integral(h(t) dt) = uv.y; pick a "row".
    let t:     f32 = invert_normed_cubic_integral(h, uv.v);
    
    // find the derivatives at (s,t)
    let t2:    f32 = t * t;
    let tv:  vec4f = vec4(1., t, t2, t * t2);
    let k:   vec4f = tv * c; // <-- row slice polynomial at t
    // scaling factor (total area) for each polynomial we inverted:
    let h1:    f32 = value_or(dot(INTEGRATE_CUBIC, h), 1.);
    let k1:    f32 = value_or(dot(INTEGRATE_CUBIC, k), 1.);
    // again, we're taking derivative[integral[f]], so it's just f:
    let dt_du: f32 = ds_du * k1 / value_or(dot(k, sv), 1.); // derivative along row slice
    let dt_dv: f32 =         h1 / value_or(dot(tv, h), 1.); // derivative along column slice
    
    // eval bicubic at (s, t) (recall h = c * sv)
    let val:   f32 = dot(tv, h);
    
    return SamplePatch(
        vec2f(s, t),
        val,
        mat2x2f(
            vec2f(ds_du, dt_du),
            vec2f(ds_dv, dt_dv),
        )
    );
}

fn sample_discrete_image(image: SampleImage, uv: vec2f) -> Coordinate2d {
    let smp_y: Coordinate = sample_cdf_vector(image.row_cdf, uv.v);
    let row: i32 = smp_y.x;
    let y:   f32 = smp_y.x_remainder;
    
    // get this (interpolated) row's cdf for choosing columns
    // (access a row as a vec3— wgsl is column-major!)
    let mT:    mat4x4f = transpose(image.marginal_col_cdf);
    let c1:      vec4f = mT[row];
    let c0:      vec4f = select(vec4f(0.), mT[row - 1], row > 0);
    let col_cdf: vec4f = mix(c0, c1, y);
    
    let smp_x: Coordinate = sample_cdf_vector(col_cdf, uv.u);
    let col:   i32 = smp_x.x;
    let x:     f32 = smp_x.x_remainder;
    
    return Coordinate2d(
        vec2i(col, row),
        vec2f(x, y)
    );
}

fn sample_discrete_image_patch(image: SampleImage, uv: vec2f) -> CoordinatePatch2d {
    let smp_y: Coordinate = sample_cdf_vector(image.row_cdf, uv.v);
    let row:   i32 = smp_y.x;
    let y:     f32 = smp_y.x_remainder;
    let dy_dv: f32 = smp_y.df_dx;
    let dy_du: f32 = 0.; // y is independent of u
    
    // get this (interpolated) row's cdf for choosing columns
    // (access a row as a vec3— wgsl is column-major!)
    let mT:    mat4x4f = transpose(image.marginal_col_cdf);
    let c1:      vec4f = mT[row];
    let c0:      vec4f = select(vec4f(0.), mT[row - 1], row > 0);
    let col_cdf: vec4f = mix(c0, c1, y);
    
    let smp_x: Coordinate = sample_cdf_vector(col_cdf, uv.u);
    let col:   i32 = smp_x.x;
    let x:     f32 = smp_x.x_remainder;
    // todo: we could also get the derivative right out of smp_x, and
    //   only compute the y derivative here. if there is an issue with
    //   the derivative, we could check if they're the same.
    let Jf:    vec2f = cdf_bilinear_gradient(image.marginal_col_cdf, smp_x, smp_y);
    let row_max: f32 = value_or(col_cdf[3],       1.);
    let col_max: f32 = value_or(image.row_cdf[3], 1.);
    let dx_du:   f32 =         row_max / value_or(Jf.x, 1.);
    let dx_dv:   f32 = dy_dv * col_max / value_or(Jf.y, 1.);
    
    return CoordinatePatch2d(
        vec2i(col, row),
        vec2f(x, y),
        mat2x2f(
            vec2f(dx_du, dy_du),
            vec2f(dx_dv, dy_dv),
        ),
    );
}

// xy is in [0, 1]^2
// returns a sample in [0, 1]^2.
fn sample_interp_image(image: SampleImage, st: vec2f) -> Sample {
    let smp2d: Coordinate2d = sample_discrete_image(image, st);
    let bicubic_coeffs: mat4x4f = bicubic_cell(image.image, smp2d.x);
    let sample: Sample = sample_bicubic(bicubic_coeffs, smp2d.x_remainder);
    return Sample(
        // sample point on [0,1]^2
        (vec2f(smp2d.x) + sample.st) / 4.,
        // (normalized) pdf at the point
        sample.pdf / image.row_cdf[3],
    );
}

// xy is in [0, 1]^2
// returns a sample in [0, 1]^2.
fn sample_interp_image_patch(image: SampleImage, xy: vec2f) -> SamplePatch {
    let smp2d: CoordinatePatch2d = sample_discrete_image_patch(image, xy);
    let bicubic_coeffs:  mat4x4f = bicubic_cell(image.image, smp2d.x);
    let sample:      SamplePatch = sample_bicubic_patch(bicubic_coeffs, smp2d.x_remainder);
    let st:                vec2f = (vec2f(smp2d.x) + sample.st) / 4.;
    
    return SamplePatch(
        // sample point on [0,1]^2
        st,
        // pdf at the sample point. normalize the cdf
        sample.pdf / image.row_cdf[3],
        // jacobian of the remapping at the sample point
        // (using multivariate chain rule)
        sample.J * smp2d.J / 4.,
    );
}

fn eval_interp_image(image: SampleImage, st: vec2f) -> f32 {
    let xy: vec2f = st * 4.;
    let ij: vec2f = min(max(vec2i(0), floor(xy)), vec2i(3));
    let bicubic_coeffs:  mat4x4f = bicubic_cell(image.image, vec2i(ij));
    return eval_bicubic(bicubic_coeffs, xy - ij);
}

fn make_sample_image(image: mat4x4f) -> SampleImage {
    let areas:    mat4x4f = dot(AREA_TRANSFORM, image * transpose(AREA_TRANSFORM));
    let marginal: mat4x4f = areas;
    areas[1] += areas[0];
    areas[2] += areas[1];
    areas[3] += areas[2];
    let cdf: vec4f = areas[3];
    cdf[1] += cdf[0];
    cdf[2] += cdf[1];
    cdf[3] += cdf[2];
    
    return SampleImage(
        image,
        marginal,
        cdf,
    );
}
