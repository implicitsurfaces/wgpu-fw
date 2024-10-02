const I_2x2 = mat2x2f(
    1., 0.,
    0., 1.
);

const I_3x3 = mat3x3f(
    1., 0., 0.,
    0., 1., 0.,
    0., 0., 1.,
);

const I_4x4 = mat4x4f(
    1., 0., 0., 0.,
    0., 1., 0., 0.,
    0., 0., 1., 0.,
    0., 0., 0., 1.,
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

fn outer2x2(v0: vec2f, v1: vec2f) -> mat2x2f {
    return mat2x2f(
        v0 * v1.x,
        v0 * v1.y,
    );
}

fn outer3x3(v0: vec3f, v1: vec3f) -> mat3x3f {
    return mat3x3f(
        v0 * v1.x,
        v0 * v1.y,
        v0 * v1.z,
    );
}

fn outer4x4(v0: vec4f, v1: vec4f) -> mat4x4f {
    return mat4x4f(
        v0 * v1.x,
        v0 * v1.y,
        v0 * v1.z,
        v0 * v1.w,
    );
}

fn sqrt_2x2(m: mat2x2f) -> mat2x2f {
    // cholesky decomposition
    let a: f32 = sqrt(m[0][0]);
    let b: f32 = m[0][1] / a;
    return mat2x2f(
        vec2f(a, b),
        vec2f(0., sqrt(m[1][1] - b * b)),
    );
}
