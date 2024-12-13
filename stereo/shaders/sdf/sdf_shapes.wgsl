// #include "sdf_structs.wgsl"

fn sdf_sphere(sphere: SphereD, d: SdfDomain) -> SdfRange {
    let r: DualV3 = dv3_sub(d.p, sphere.center);
    let f: Dual   = dv3_length(r);
    let n: DualV3 = dv3_divd(r, f);
    return SdfRange(f, n);
}

fn sdf_box(box: BoxD, x: SdfDomain) -> SdfRange {
    // todo: normal calculation is degenerate on the surface
    let d_lo: vec3f = x.p.x - box.lo.x; // distance from lo extreme to p
    let d_hi: vec3f = x.p.x - box.hi.x; // ...
    let surface_pt: DualV3 = dv3_selectv(box.lo, box.hi, abs(d_lo) < abs(d_hi));
    let v: DualV3 = dv3_sub(x.p, surface_pt); // vector from the surface to p
    let d: Dual = dv3_length(v);
    let inside: bool = all(d_lo > vec3f(0)) && all(d_hi < vec3f(0));
    let f: Dual = d_select(d, d_neg(d), inside);
    return SdfRange(f, dv3_divd(v, d));
}

fn sdf_cylinder(cylinder: CylinderD, x: SdfDomain) -> SdfRange {
    // cylinder axis:
    let a: DualV3 = dv3_sub(cylinder.p1, cylinder.p0);
    // vector from p0 to p:
    let b: DualV3 = dv3_sub(x.p, cylinder.p0);
    let b2: Dual = dv3_length2(b);
    let a2: Dual = dv3_length2(a);
    let d:  Dual = dv3_dot(a, b);
    // fractional distance along axis in [0,1]:
    var s:  Dual = d_div(d, a2);
    // square of distance from the base to the projected axial point
    let x2: Dual = d_mul(d, s);
    var r_dist: Dual = d_sqrt(d_sub(b2, x2));
    // signed distance to the surface of the cylinder:
    var r:  Dual = d_sub(r_dist, cylinder.radius);
    // signed fractional distance along the axis to nearest cap:
    var t:  Dual = d_max(d_neg(s), d_subf(s, 1.0));
    // radial vector 
    let r_vec: DualV3 = dv3_sub(dv3_sub(x.p, cylinder.p0), dv3_dscale(s, a));
    let r_hat: DualV3 = dv3_divd(r_vec, r_dist);
    // sdf is negative (inside shape) iff (s, r) both negative:
    let sign: f32 = select(1., -1., s.x < 0 && r.x < 0);
    let zero: Dual = Dual(0., 0.);
    let one:  Dual = Dual(1., 0.);
    // clamped coordinates, to project orthogonally to wall/cap
    t = d_max(t, zero);
    r = d_max(r, zero);
    // squared axis-parallel distance to cap
    let y2: Dual = d_mul(d_mul(t, t), a2);
    // squared distance to surface point
    let z2: Dual = d_add(d_mul(r, r), y2);
    let dist: Dual = d_sqrt(z2);
    
    // compute normal
    var n: DualV3;
    if s.x < 0 || s.x > 1 {
        // p projects to the cap; compute vector to the cap point and normalize it
        // clamp s to [0,1]:
        s = d_clamp(s, zero, one);
        if r_dist.x <= cylinder.radius.x {
            // projects to cap face
            n = dv3_select(a, dv3_neg(a), s.x < 0.5);
        } else {
            // projects to cap rim
            let surf_pt: DualV3 = dv3_add(
                dv3_add(cylinder.p0, dv3_dscale(s, a)),
                dv3_dscale(r_dist, r_hat)
            );
            let v: DualV3 = dv3_sub(x.p, surf_pt);
            n = dv3_divd(v, dist);
        }
    } else if
            r_dist.x < cylinder.radius.x &&
            sqrt(a2.x) * min(s.x, 1. - s.x) < cylinder.radius.x - r_dist.x
    {
        // p is interior and the cap is closer than the wall.
        // normal points along the axis, toward the cap
        n = dv3_fscale(select(-1., 1., s.x < 0.5), a);
    } else {
        // p projects to the cylinder wall
        n = r_hat;
    }
    
    return SdfRange(d_fscale(sign, dist), n);
}

fn sdf_capsule(capsule: CapsuleD, x: SdfDomain) -> SdfRange {
    // capsule axis:
    let a: DualV3 = dv3_sub(capsule.p1, capsule.p0);
    // vector from p0 to p:
    let b: DualV3 = dv3_sub(x.p, capsule.p0);
    let b2: Dual = dv3_length2(b);
    let a2: Dual = dv3_length2(a);
    let d:  Dual = dv3_dot(a, b);
    // fractional distance along axis in [0,1]:
    var s:  Dual = d_div(d, a2);
    s = d_clamp(s, Dual(0., 0.), Dual(1., 0.));
    // project p orthogonally to the axis:
    let p_a: DualV3 = dv3_add(capsule.p0, dv3_dscale(s, a));
    // vector from the projected point to p:
    let v: DualV3 = dv3_sub(x.p, p_a);
    return SdfRange(
        d_sub(dv3_length(v), capsule.radius),
        dv3_normalize(v)
    );
}

/*
fn sdf_cone(cone: ConeD, d: SdfDomain) -> SdfRange {
    // xxx todo
}

fn sdf_torus(torus: TorusD, d: SdfDomain) -> SdfRange {
    // xxx todo
}
*/

fn sdf_plane(plane: PlaneD, d: SdfDomain) -> SdfRange {
    return SdfRange(
        d_add(dv3_dot(d.p, plane.n), plane.d),
        plane.n
    );
}

fn sdf_triangle(triangle: TriD, x: SdfDomain) -> SdfRange {
    let v0: DualV3 = dv3_sub(triangle.p1, triangle.p0);
    let v1: DualV3 = dv3_sub(triangle.p2, triangle.p1);
    let v2: DualV3 = dv3_sub(triangle.p0, triangle.p2);
    let b0: DualV3 = dv3_sub(x.p, triangle.p0);
    let b1: DualV3 = dv3_sub(x.p, triangle.p1);
    let b2: DualV3 = dv3_sub(x.p, triangle.p2);
    let n:  DualV3 = dv3_cross(v0, v2); // tri normal
    
    let inside: bool = 
            sign(dot(cross(v0.x, n.x), b0.x)) +
            sign(dot(cross(v1.x, n.x), b1.x)) +
            sign(dot(cross(v2.x, n.x), b2.x)) < 2.0;
    var d: Dual;
    var normal: DualV3;
    if inside {
        // outside the triangle
        // find the projected point on each edge
        let v_e0: DualV3 = dv3_sub(dv3_project_to_segment(b0, v0), b0);
        let v_e1: DualV3 = dv3_sub(dv3_project_to_segment(b1, v1), b1);
        let v_e2: DualV3 = dv3_sub(dv3_project_to_segment(b2, v2), b2);
        // distance to each edge
        let d0: Dual = dv3_length(v_e0);
        let d1: Dual = dv3_length(v_e1);
        let d2: Dual = dv3_length(v_e2);
        // find closest edge point
        d      = d0;
        normal = v_e0; // sdf normal
        if d1.x < d.x {
            d      = d1;
            normal = v_e1;
        }
        if d2.x < d.x {
            d      = d2;
            normal = v_e2;
        }
        normal = dv3_divd(normal, d_sqrt(d));
    } else {
        // projection inside the triangle face
        let x: Dual = dv3_dot(n, b0);
        d = d_div(d_mul(x, x), dv3_length2(n));
        normal = dv3_select(dv3_neg(n), n, dot(n.x, b0.x) > 0.);
    }
    return SdfRange(d, normal);
}
