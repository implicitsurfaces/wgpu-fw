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
const INTEGRATE_CUBIC: vec4f =  vec4f(1., 1. / 2., 1. / 3., 1. / 4.);
// dot this with a vec4 of samples to get the integral of the interpolating cubic
// over the unit interval.
const CUBIC_AREA: vec4f = (vec4f(-1, 1, 1, -1) / 12. + vec4f(0, 1, 1, 0)) / 2.;
// dot this with a vec4 of samples to get the areas of each cell in the bicubics
// which intperpolate the cyclical signal. the cell at index 1 contains area of
// the un-rotated signal's unit cell.
const AREA_TRANSFORM: mat4x4f = mat4x4f(
    CUBIC_AREA.wxyz,
    CUBIC_AREA.xyzw,
    CUBIC_AREA.yzwx,
    CUBIC_AREA.zwxy,
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

struct SampleCoordinate {
    x:           i32,
    x_remainder: f32,
}

struct SampleCoordinate2d {
    x:           vec2i,
    x_remainder: vec2f,
}

struct SampleImage {
    // the source samples
    image:            mat4x4f,
    // columnwise cdf at points in (0, 3)^2.
    // each cell has the accumulated area of the cells before it in the same row.
    marginal_col_cdf: mat4x4f,
    // rowwise cdf.
    // each entry has the accumulated area of the row before it.
    row_cdf:          vec4f,
}

// rootfind the normalized integral of a cubic polynomial over the unit interval.
// `coeffs` is the coefficients of the cubic function. `y` is in [0, 1].
fn invert_normed_cubic_integral(coeffs: vec4f, y: f32) -> f32 {
    coeffs *= INTEGRATE_CUBIC; // integrate the cubic
    // bound the root on the left and the right
    let x:    f32 = y;
    let lo:   f32 = 0;
    let hi:   f32 = 1;
    let f_lo: f32 = 0;                      // f(0)
    let f_hi: f32 = dot(coeffs, vec4f(1.)); // f(1)
    y  *= f_hi; // scale y to the integral of the cubic
    // subtract y so we can find the root of f(x) = 0
    f_lo -= y;
    f_hi -= y;
    // error ~halves each iteration, and we start with ~1 bit of error.
    // this function is pretty fuzzy in the 0,1 interval, so we don't
    // need a huge amount of precision. 6 iters oughtta get us to <=1% error
    for (int i = 0; i < 6; i++) {
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

fn sample_cdf_vector(cdf: vec4f, x_01: f32) -> SampleCoordinate {
    if cdf[2] == 0. {
        let z = modf(x_01 * 3., i);
        return SampleCoordinate(i32(z.whole), z.frac);
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
    let rem: f32 = x_01 - y0;
    return SampleCoordinate(i, rem / (cdf[i] - y0));
}

// distribute the uniform sample `uv` over the bicubic interpolation on the unit square
// with probability proportional to the value of that interpolation.
// `c` are the coefficients of the interpolation. `uv` should be in [0, 1]^2
fn sample_bicubic(c: mat4x4f, uv: vec2f) -> vec2f {
    // let G(x,t) = integral(f(x,y) dy) from 0 to t.
    // that is, integrate the area in "columns".
    // then obtain a polynomial for g_1(s) = G(s, 1)
    // that is, evaluate the area in each column, yielding a polynomial `g`.
    let g: vec4f = INTEGRATE_CUBIC * c;
    // find s such that g_1(s) = uv.s
    // that is, take the integral of the per-column area, and rootfind
    // for where it equals `uv.s`; pick a column.
    let s:  f32 = invert_normed_cubic_integral(g, uv.s);
    let s2: f32 = s * s;
    // let h(t) = f(s, t) for the chosen s.
    let h: vec4f = c * vec4(1., s, s2, s2 * s);
    // find t such that integral(h(t) dt) = uv.y; pick a "row".
    let t: f32 = invert_normed_cubic_integral(h, uv.y);
    return vec2f(s, t);
}

fn sample_discrete_image(image: SampleImage, st: vec2f) -> SampleCoordinate2d {
    let smp_y: SampleCoordinate = sample_cdf_vector(image.row_cdf, st.y);
    let row: i32 = smp_y.x;
    let y:   f32 = smp_y.x_remainder;
    
    // get this row's cdf for choosing columns
    // (access a row as a vec3— wgsl is column-major!)
    let col_cdf: vec4f = transpose(image.marginal_col_cdf)[row];
    
    let smp_x = sample_cdf_vector(col_cdf, st.x);
    let col: i32 = smp_x.x;
    let x:   f32 = smp_x.x_remainder;
    
    return SampleCoordinate2d(
        vec2i(col, row),
        vec2f(x, y),
    );
}

fn bicubic(samples: mat4x4f) -> mat4x4f {
    return cubic_mtx * samples * transpose(cubic_mtx);
}

fn rotated_bicubic(d: u32) -> mat4x4f {
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
    return rotated_bicubic(xy.y) * samples * transpose(rotated_bicubic(xy.x));
}

fn sample_interp_image(image: SampleImage, st: vec2f) -> vec2f {
    let smp2d: SampleCoordinate2d = sample_discrete_image(image, st);
    let bicubic_coeffs: mat4x4f = bicubic_cell(image.image, smp2d.x);
    let uv: vec3f = sample_bicubic(bicubic_coeffs, smp2d.x_remainder);
    return (vec2f(smp2d.x) + uv) / 3.;
}

fn make_sample_image(image: mat4x4f) -> SampleImage {
    let areas:    mat4x4f = dot(transpose(AREA_TRANSFORM), image * AREA_TRANSFORM);
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
