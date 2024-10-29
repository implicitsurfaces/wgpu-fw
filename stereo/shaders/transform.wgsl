// #include "mat_helpers.wgsl"
// #include "quat.wgsl"
// #include "inverse.wgsl"

// todo: when the points have uncertainty, this creates uncertainty in the
//   rotation and translation. how do we propagate this uncertainty?
// todo: this is incorrect in cases where the matrices are degenerate.
//   an SVD based solution would be more robust (but we'd need to implement it)
// todo: it is more convenient to write the matrices as their transposes
//   (so that data lives in columns, and we eliminate explicit transpose ops)

struct Xf {
    q: vec4f,
    t: vec3f,
}

fn fit_transform(dst_pts: mat3x4f, src_pts: mat3x4f) -> Xf {
    // use the kabsch algorithm to find the optimal rotation and translation.
    // first compute the centroids:
    let s_c: vec3f = (vec4f(1.) * src_pts) / 4.;
    let d_c: vec3f = (vec4f(1.) * dst_pts) / 4.;
    let P: mat3x4f = src_pts - transpose(mat4x3f(s_c, s_c, s_c, s_c));
    let Q: mat3x4f = dst_pts - transpose(mat4x3f(d_c, d_c, d_c, d_c));
    let H: mat3x3f = transpose(P) * Q; // cross covar

    let R: mat3x3f = sqrt_3x3(transpose(H) * H) * inverse3x3(H);
    let dx: vec3f = d_c - s_c;

    return Xf(
        mat2q(R),
        dx,
    );
}
