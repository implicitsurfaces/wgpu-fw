// dual.wgsl

struct Dual {
    x:  f32,
    dx: f32,
}

fn d_add(a: Dual, b: Dual) -> Dual {
    return Dual(
        a.x  + b.x,
        a.dx + b.dx,
    );
}

fn d_addf(a: Dual, b: f32) -> Dual {
    return Dual(
        a.x  + b,
        a.dx,
    );
}

fn d_sub(a: Dual, b: Dual) -> Dual {
    return Dual(
        a.x  - b.x,
        a.dx - b.dx,
    );
}

fn d_subf(a: Dual, b: f32) -> Dual {
    return Dual(
        a.x  - b,
        a.dx,
    );
}

fn d_fsub(a: f32, b: Dual) -> Dual {
    return Dual(
        a  - b.x,
        b.dx,
    );
}

fn d_mul(a: Dual, b: Dual) -> Dual {
    return Dual(
        a.x * b.x,
        a.x * b.dx + a.dx * b.x,
    );
}

fn d_neg(a: Dual) -> Dual {
    return Dual(
        -a.x,
        -a.dx,
    );
}

fn d_fscale(a: f32, b: Dual) -> Dual {
    return Dual(
        a * b.x,
        a * b.dx,
    );
}

fn d_recip(a: Dual) -> Dual {
    let c = 1.0 / a.x;
    return Dual(
        c,
        -c * c * a.dx,
    );
}

fn d_div(a: Dual, b: Dual) -> Dual {
    return Dual(
        a.x / b.x,
        (a.dx * b.x - a.x * b.dx) / (b.x * b.x),
    );
}

fn d_max(a: Dual, b: Dual) -> Dual {
    let c = a.x > b.x;
    return Dual(
        select(b.x,  a.x,  c),
        select(b.dx, a.dx, c),
    );
}

fn d_min(a: Dual, b: Dual) -> Dual {
    let c = a.x < b.x;
    return Dual(
        select(b.x,  a.x,  c),
        select(b.dx, a.dx, c),
    );
}

fn d_clamp(e: Dual, lo: Dual, hi: Dual) -> Dual {
    return d_max(lo, d_min(hi, e));
}

fn d_select(f: Dual, t: Dual, cond: bool) -> Dual {
    return Dual(
        select(f.x,  t.x,  cond),
        select(f.dx, t.dx, cond),
    );
}

fn d_abs(x: Dual) -> Dual {
    let c = x.x > 0.0;
    return Dual(
        select(-x.x,  x.x,  c),
        select(-x.dx, x.dx, c),
    );
}

fn d_ceil(x: Dual) -> Dual {
    return Dual(
        ceil(x.x),
        x.dx,
    );
}

fn d_floor(x: Dual) -> Dual {
    return Dual(
        floor(x.x),
        x.dx,
    );
}

fn d_sin(x: Dual) -> Dual {
    return Dual(
        sin(x.x),
        x.dx * cos(x.x),
    );
}

fn d_cos(x: Dual) -> Dual {
    return Dual(
        cos(x.x),
        -x.dx * sin(x.x),
    );
}

fn d_tan(x: Dual) -> Dual {
    let c = cos(x.x);
    return Dual(
        tan(x.x),
        x.dx / (c * c),
    );
}

fn d_asin(x: Dual) -> Dual {
    return Dual(
        asin(x.x),
        x.dx / sqrt(1.0 - x.x * x.x),
    );
}

fn d_acos(x: Dual) -> Dual {
    return Dual(
        acos(x.x),
        -x.dx / sqrt(1.0 - x.x * x.x),
    );
}

fn d_atan(x: Dual) -> Dual {
    return Dual(
        atan(x.x),
        x.dx / (1.0 + x.x * x.x),
    );
}

fn d_atan2(y: Dual, x: Dual) -> Dual {
    return Dual(
        atan2(y.x, x.x),
        (x.x * y.dx - y.x * x.dx) / (x.x * x.x + y.x * y.x),
    );
}

fn d_exp(x: Dual) -> Dual {
    let e = exp(x.x);
    return Dual(
        e,
        x.dx * e,
    );
}

fn d_log(x: Dual) -> Dual {
    return Dual(
        log(x.x),
        x.dx / x.x,
    );
}

fn d_pow(base: Dual, xp: Dual) -> Dual {
    // here we use the chain rule for partial derivatives:
    //   f(x,y) = x ^ y
    //   x = g(t); y = h(t)
    // then:
    //   df / dt = (df / dx) * (dx / dt) + (df / dy) * (dy / dt)
    //           = (df / dx) * g`(t)     + (df / dy) * h`(t)
    // and:
    //   df / dx = d/dx (x ^ y) = y * x ^ (y - 1)
    //   df / dy = d/dy (x ^ y) = log(x) * (x ^ y)
    let a_c = pow(base.x, xp.x);
    return Dual(
        a_c,
        base.dx * xp.x * a_c / xp.x +
        xp.dx * a_c * log(base.x),
    );
}

fn d_sqrt(x: Dual) -> Dual {
    let s = sqrt(x.x);
    return Dual(
        s,
        x.dx / (2.0 * s),
    );
}
