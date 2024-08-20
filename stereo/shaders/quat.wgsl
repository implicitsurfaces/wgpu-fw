// quat.wgsl
// functions for quaternion operations

fn qconj(q: vec4f) -> vec4f {
    return vec4f(-q.xyz, q.w);
}

fn qmult(q0: vec4f, q1: vec4f) -> vec4f {
    return vec4f(
        dot(q0.wxy, q1.xwz) - q0.z * q1.y,
        dot(q0.wyz, q1.ywz) - q0.x * q1.z,
        dot(q0.wxz, q1.zyw) - q0.y * q1.x,
        q0.w * q1.w - dot(q0.xyz, q1.xyz)
    );
}

// matrix representation of a quaternion multiplication
// on the left
fn qmat(q: vec4f) -> mat4x4f {
    return mat4x4f(
        q.wzyx * vec4f( 1., -1.,  1., -1.),
        q.zwxy * vec4f( 1.,  1., -1., -1.),
        q.yxwz * vec4f(-1.,  1.,  1., -1.),
        q
    );
}

// matrix representation of a quaternion multiplication
// on the right
fn qmat_T(q: vec4f) -> mat4x4f {
    return mat4x4f(
        q.wzyx * vec4f( 1.,  1., -1., -1.),
        q.zwxy * vec4f(-1.,  1.,  1., -1.),
        q.yxwz * vec4f( 1., -1.,  1., -1.),
        q
    );
}

fn qvmult(q: vec4f, v: vec3f) -> vec3f {
    // euler-rodrigues formula
    let t: vec3f = 2. * cross(q.xyz, v);
    return q.w * (q.w * v + t + cross(q.xyz, t));
}

fn qvmat(q: vec4f) -> mat3x3f {
    let M: mat4x4f = qmat_T(q) * qmat(q);
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
