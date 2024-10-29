// quat.wgsl
// functions for quaternion operations

fn qconj(q: vec4f) -> vec4f {
    return vec4f(-q.xyz, q.w);
}

// this is correct
fn qmult(q0: vec4f, q1: vec4f) -> vec4f {
    return vec4f(
        dot(q0.wxy, q1.xwz) - q0.z * q1.y,
        dot(q0.wyz, q1.ywx) - q0.x * q1.z,
        dot(q0.wxz, q1.zyw) - q0.y * q1.x,
        q0.w * q1.w - dot(q0.xyz, q1.xyz)
    );
}

// matrix representation of a quaternion multiplication
// on the left
fn qmat(q: vec4f) -> mat4x4f {
    return mat4x4f(
        q.wzyx * vec4f( 1.,  1., -1.,  1.),
        q.zwxy * vec4f(-1.,  1.,  1.,  1.),
        q.yxwz * vec4f( 1., -1.,  1.,  1.),
        q      * vec4f(-1., -1., -1.,  1.)
    );
}

// matrix representation of a quaternion multiplication
// on the right
fn qmat_T(q: vec4f) -> mat4x4f {
    return mat4x4f(
        q.wzyx * vec4f( 1., -1.,  1.,  1.),
        q.zwxy * vec4f( 1.,  1., -1.,  1.),
        q.yxwz * vec4f(-1.,  1.,  1.,  1.),
        q      * vec4f(-1., -1., -1.,  1.)
    );
}

// quaternion rotation of a vector
fn qrot(q: vec4f, v: vec3f) -> vec3f {
    // euler-rodrigues formula
    // this is from wikipedia; afaict it's completely wrong.
    // let t: vec3f = 2. * cross(q.xyz, v);
    // return q.w * (q.w * v + t) + cross(q.xyz, t);

    // chatgpt gives this, and it works:
    let b: vec3f = 2.0 * cross(q.xyz, v);
    return v + q.w * b + cross(q.xyz, b);
}

// convert a 3x3 matrix into a quaternion. result will be a pure rotation.
fn mat2q(m: mat3x3f) -> vec4f {
    // "alternate method" vectorized from:
    // https://www.euclideanspace.com/maths/geometry/rotations/conversions/matrixToQuaternion/
    let s:   f32 = 1.; // pow(determinant(m), 1. / 3.); // scaling of M
    let m00: f32 = m[0][0];
    let m11: f32 = m[1][1];
    let m22: f32 = m[2][2];
    let d: vec4f = vec4f(m00, m11, m22, s);
    let A: mat4x4f = mat4x4f(
        vec4f( 1,  1,  1, 1), // q.x coeffs
        vec4f( 1, -1, -1, 1), // q.y
        vec4f(-1,  1, -1, 1), // ...
        vec4f(-1, -1,  1, 1),
    );
    var q: vec4f = d * A;
    q   = sqrt(max(q, vec4f(0))) / 2.;
    q.x = abs(q.x) * sign(m[1][2] - m[2][1]);
    q.y = abs(q.y) * sign(m[2][0] - m[0][2]);
    q.z = abs(q.z) * sign(m[0][1] - m[1][0]);
    return normalize(q);
}

fn qvmat(q: vec4f) -> mat3x3f {
    let M: mat4x4f = transpose(qmat_T(q)) * qmat(q);
    return mat3x3f(M[0].xyz, M[1].xyz, M[2].xyz);
}

// jacobian of a quaternion rotation with respect to the quaternion
fn J_qvmult_dq(q: vec4f, pv: vec3f) -> mat3x4f {
    // slight refactoring from:
    //   https://math.stackexchange.com/a/2713077
    //   (conj(pq) = conj(q)conj(p))
    let p:   vec4f = vec4f(pv, 0.);
    let Q: mat4x4f = qmat(qconj(qmult(p, q))) + qmat_T(qmult(p, qconj(q)));
    return mat3x4f(Q[0], Q[1], Q[2]);
}

// jacobian of a quaternion rotation with respect to the point.
// the same as a quaternion rotation matrix for q.
fn J_qvmult_dp(q: vec4f) -> mat3x3f {
    return qvmat(q);
}
