// #include "sdf_structs.wgsl"

fn sdf_union(f_a: SdfRange, f_b: SdfRange) -> SdfRange {
    let b: bool = f_a.f.x > f_b.f.x;
    return SdfRange(
        // min of A and B
        d_select(  f_a.f,      f_b.f,      b),
        dv3_select(f_a.grad_f, f_b.grad_f, b),
    );
}

fn sdf_intersection(f_a: SdfRange, f_b: SdfRange) -> SdfRange {
    let b: bool = f_a.f.x < f_b.f.x;
    return SdfRange(
        // max of A and B
        d_select(  f_a.f,      f_b.f,      b),
        dv3_select(f_a.grad_f, f_b.grad_f, b),
    );
}

fn sdf_subtract(f_a: SdfRange, f_b: SdfRange) -> SdfRange {
    let b: bool = f_a.f.x < -f_b.f.x;
    return SdfRange(
        // max of A and -B
        d_select(  f_a.f,        d_neg(f_b.f),      b),
        dv3_select(f_a.grad_f, dv3_neg(f_b.grad_f), b),
    );
}

/*
fn sdf_xor(f_a: SdfRange, f_b: SdfRange) -> SdfRange {
    // xxx todo
}
*/

fn sdf_dilate(f: SdfRange, r: Dual) -> SdfRange {
    return SdfRange(
        d_sub(f.f, r),
        f.grad_f,
    );
}

fn sdf_transform(xf: RigidTransformD, p: SdfDomain) -> SdfDomain {
    let xf_inv: RigidTransformD = dtx_inverse(xf);
    return SdfDomain(
        dtxv_apply(xf_inv, p.p),
        dm3_dot(dq_mat(xf_inv.q), p.J),
    );
}
