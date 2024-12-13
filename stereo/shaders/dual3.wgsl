// dual3.wgsl
// functions for 3D ops with dual numbers

// #include "mat_helpers.wgsl"
// #include "dual.wgsl"
// #include "quat.wgsl"

// function naming convention (wgsl does not permit overloading):
// [d]ual
// [f]loat
// [v]ector
// [3]d
// [m]atrix
// [q]uaternion
// x_[op]y() = x op y
// x_y[op]() = y op x

struct DualV3 {
    x:  vec3f,
    dx: vec3f,
}

struct DualM3 {
    x:  mat3x3f,
    dx: mat3x3f,
}

struct DualQ {
    x:  vec4f,
    dx: vec4f,
}

fn dv3_add(a: DualV3, b: DualV3) -> DualV3 {
    return DualV3(
        a.x  + b.x,
        a.dx + b.dx,
    );
}

fn dv3_addd(a: DualV3, b: Dual) -> DualV3 {
    return DualV3(
        a.x  + b.x,
        a.dx + b.dx,
    );
}

fn dv3_addf(a: DualV3, b: f32) -> DualV3 {
    return DualV3(
        a.x  + b,
        a.dx,
    );
}

fn dv3_sub(a: DualV3, b: DualV3) -> DualV3 {
    return DualV3(
        a.x  - b.x,
        a.dx - b.dx,
    );
}

fn dv3_subd(a: DualV3, b: Dual) -> DualV3 {
    return DualV3(
        a.x  - b.x,
        a.dx - b.dx,
    );
}

fn dv3_subf(a: DualV3, b: f32) -> DualV3 {
    return DualV3(
        a.x - b,
        a.dx,
    );
}

fn dv3_mul(a: DualV3, b: DualV3) -> DualV3 {
    return DualV3(
        a.x  * b.x,
        a.dx * b.x + a.x * b.dx,
    );
}

fn dv3_neg(a: DualV3) -> DualV3 {
    return DualV3(
        -a.x,
        -a.dx,
    );
}

fn dv3_fscale(a: f32, b: DualV3) -> DualV3 {
    return DualV3(
        a * b.x,
        a * b.dx,
    );
}

fn dv3_dscale(a: Dual, b: DualV3) -> DualV3 {
    return DualV3(
        a.x * b.x,
        a.dx * b.x + a.x * b.dx, // product rule
    );
}

fn dv3_dot(a: DualV3, b: DualV3) -> Dual {
    return Dual(
        dot(a.x, b.x),
        dot(a.dx, b.x) + dot(a.x, b.dx),
    );
}

fn dv3_div(a: DualV3, b: DualV3) -> DualV3 {
    return DualV3(
        a.x / b.x,
        (a.dx * b.x - a.x * b.dx) / (b.x * b.x),
    );
}

fn dv3_divf(a: DualV3, b: f32) -> DualV3 {
    return DualV3(
        a.x  / b,
        a.dx / b,
    );
}

fn dv3_divd(a: DualV3, b: Dual) -> DualV3 {
    return DualV3(
        a.x  / b.x,
        (a.dx * b.x - a.x * b.dx) / (b.x * b.x),
    );
}

fn dv3_cross(a: DualV3, b: DualV3) -> DualV3 {
    return DualV3(
        cross(a.x, b.x),
        cross(a.x, b.dx) + cross(a.dx, b.x),
    );
}

fn dv3_project_to_segment(p: DualV3, v: DualV3) -> DualV3 {
    let s: Dual = d_div(dv3_dot(p, v), dv3_dot(v, v));
    return dv3_dscale(
        d_clamp(s, Dual(0., 0.), Dual(1., 0.)),
        v
    );
}

fn dv3_length(a: DualV3) -> Dual {
    let len: f32 = length(a.x);
    return Dual(
        len,
        dot(a.x, a.dx) / len,
    );
}

fn dv3_length2(a: DualV3) -> Dual {
    return dv3_dot(a, a);
}

fn dv3_normalize(a: DualV3) -> DualV3 {
    let len: Dual = dv3_length(a);
    return dv3_divd(a, len);
}

fn dv3_abs(a: DualV3) -> DualV3 {
    return DualV3(
        abs(a.x),
        select(a.dx, -a.dx, a.x < vec3f(0.)),
    );
}

fn dv3_select(f: DualV3, t: DualV3, cond: bool) -> DualV3 {
    return DualV3(
        select(f.x,  t.x,  cond),
        select(f.dx, t.dx, cond),
    );
}

fn dv3_selectv(f: DualV3, t: DualV3, cond: vec3<bool>) -> DualV3 {
    return DualV3(
        select(f.x,  t.x,  cond),
        select(f.dx, t.dx, cond),
    );
}

// matrix ops

fn dm3_identity() -> DualM3 {
    return DualM3(
        I_3x3,
        mat3x3f(),
    );
}

fn dmv3_dot(m: DualM3, v: DualV3) -> DualV3 {
    return DualV3(
        m.x * v.x,
        m.x * v.dx + m.dx * v.x,
    );
}

fn dvm3_dot(m: DualM3, v: DualV3) -> DualV3 {
    return DualV3(
        v.x * m.x,
        v.x * m.dx + v.dx * m.x,
    );
}

fn dm3_dot(m_a: DualM3, m_b: DualM3) -> DualM3 {
    return DualM3(
        m_a.x * m_b.x,
        m_a.x * m_b.dx + m_a.dx * m_b.x,
    );
}

fn dm3_transpose(m: DualM3) -> DualM3 {
    return DualM3(
        transpose(m.x),
        transpose(m.dx),
    );
}

fn dm3_fscale(a: f32, m: DualM3) -> DualM3 {
    return DualM3(
        a * m.x,
        a * m.dx,
    );
}

fn dm3_dscale(a: Dual, m: DualM3) -> DualM3 {
    return DualM3(
        a.x * m.x,
        a.dx * m.x + a.x * m.dx,
    );
}

// quaternion ops

fn dq_mul(a: DualQ, b: DualQ) -> DualQ {
    return DualQ(
        qmult(a.x, b.x),
        qmult(a.x, b.dx) + qmult(a.dx, b.x),
    );
}

fn dq_conj(q: DualQ) -> DualQ {
    return DualQ(
        qconj(q.x),
        qconj(q.dx),
    );
}

fn dqv_mul(q: DualQ, v: DualV3) -> DualV3 {
    return DualV3(
        qrot(q.x, v.x),
        qrot(q.x, v.dx) + qrot(q.dx, v.x),
    );
}

fn dq_mat(q: DualQ) -> DualM3 {
    return DualM3(
        qvmat(q.x),
        qvmat(q.dx),
    );
}

fn dq_length(q: DualQ) -> Dual {
    let len: f32 = length(q.x);
    return Dual(
        len,
        dot(q.x, q.dx) / len,
    );
}

fn dq_normalize(q: DualQ) -> DualQ {
    let len: Dual = dq_length(q);
    return DualQ(
        q.x / len.x,
        (q.dx * len.x - q.x * len.dx) / (len.x * len.x),
    );
}

fn dq_fscale(a: f32, q: DualQ) -> DualQ {
    return DualQ(
        a * q.x,
        a * q.dx,
    );
}


fn dq_dscale(a: Dual, q: DualQ) -> DualQ {
    return DualQ(
        a.x * q.x,
        a.dx * q.x + a.x * q.dx,
    );
}

// transform ops

// produce a new transformation representing
// the application of tx_b followed by tx_a
fn dtx_apply(tx_a: RigidTransformD, tx_b: RigidTransformD) -> RigidTransformD {
    return RigidTransformD(
        dq_mul(tx_a.q, tx_b.q),
        dv3_add(dqv_mul(tx_a.q, tx_b.tx), tx_a.tx),
    );
}

fn dtx_inverse(tx: RigidTransformD) -> RigidTransformD {
    let q_inv: DualQ = dq_conj(tx.q);
    return RigidTransformD(
        q_inv,
        dqv_mul(q_inv, dv3_neg(tx.tx)),
    );
}

fn dtxv_apply(tx: RigidTransformD, v: DualV3) -> DualV3 {
    return dv3_add(dqv_mul(tx.q, v), tx.tx);
}
