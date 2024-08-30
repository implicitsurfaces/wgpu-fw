const I_2x2 = mat2x2f(1.0, 0.0, 0.0, 1.0);
const I_3x3 = mat3x3f(
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0,
);
const I_4x4 = mat4x4f(
    1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0,
);

fn I2x2(s: f32) -> mat2x2f {
    return s * I_2x2;
}

fn I3x3(s: f32) -> mat3x3f {
    return s * I_3x3;
}

fn I4x4(s: f32) -> mat4x4f {
    return s * I_4x4;
}

fn outer2x2(v: vec2f) -> mat2x2f {
    return mat2x2f(
        v * v.x,
        v * v.y,
    );
}

fn outer3x3(v: vec3f) -> mat3x3f {
    return mat3x3f(
        v * v.x,
        v * v.y,
        v * v.z,
    );
}

fn outer4x4(v: vec4f) -> mat4x4f {
    return mat4x4f(
        v * v.x,
        v * v.y,
        v * v.z,
        v * v.w,
    );
}

fn sqrt_2x2(m: mat2x2f) -> mat2x2f {
    let trace: f32 = m[0][0] + m[1][1];
    let det:   f32 = determinant(m);
    // pick the sign of s to match the sign of the trace, so they don't cancel
    let s:     f32 = select(1., sign(trace), trace != 0.);
    let t:     f32 = sqrt(trace + 2. * s);
    return (m + I2x2(s)) * (1. / t);
}
