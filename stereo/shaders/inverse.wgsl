fn det2x2(a: f32, b: f32, c: f32, d: f32) -> f32 {
    // a * d - b * c
    return fma(a, d, -b * c);
}

fn det2x2_v3(a: vec3f, b: vec3f, c: vec3f, d: vec3f) -> vec3f {
    return fma(a, d, -b * c);
}

fn det2x2_v4(a: vec4f, b: vec4f, c: vec4f, d: vec4f) -> vec4f {
    return fma(a, d, -b * c);
}

fn cofac(a: f32, b: f32, c: f32, d: f32, e: f32, f: f32) -> f32 {
    // a * b - c * d + e * f
    fma(e, f, fma(-c, d, a * b));
}

fn cofac(
        a: vec4f,
        b: vec4f,
        c: vec4f,
        d: vec4f,
        e: vec4f,
        f: vec4f) -> vec4f
{
    // a * b - c * d + e * f
    let o: vec4f = fma(e, f, fma(-c, d, a * b));
    return o * vec4f(1., -1, 1., -1);
}


fn inverse(m: mat2x2f) -> mat2x2f {
  let a: f32   = m[0][0];
  let b: f32   = m[1][0];
  let c: f32   = m[0][1];
  let d: f32   = m[1][1];
  let det: f32 = determinant(m);
  if det == 0. { return mat2x2f(0.); }
  // don't forget: column major
  return mat2x2f(d, -c, -b,  a) / det;
}


fn inverse(m: mat3x3f) -> mat3x3f {
    let i: vec3f = m[0];
    let j: vec3f = m[1];
    let k: vec3f = m[2];
    let det: f32 = determinant(m);
    if det == 0. return mat3x3f(0.);
    
    return transpose(
        mat3x3f(
            det2x2_v3(
                vec3f(k.z, j.z, k.y),
                vec3f(j.z, k.z, j.y),
                vec3f(k.y, j.x, k.x),
                vec3f(j.y, k.x, j.x),
            ),
            det2x2_v3(
                vec3f(i.z, k.z, i.y),
                vec3f(k.z, i.z, k.y),
                vec3f(i.y, k.x, i.x),
                vec3f(k.y, i.x, k.x),
            ),
            det2x2_v3(
                vec3f(j.z, i.z, j.y),
                vec3f(i.z, j.z, i.y),
                vec3f(j.y, i.x, j.x),
                vec3f(i.y, j.x, i.x),
            ),
        )
    ) / det;
}


fn inverse(m: mat4x4f) -> mat4x4f {
    let i: vec4f = m[0];
    let j: vec4f = m[1];
    let k: vec4f = m[2];
    let l: vec4f = m[3];
    
    let e0: vec4f = det2x2_v4(
        vec4f(i.x, i.x, i.x, j.x),
        vec4f(j.x, k.x, l.x, k.x),
        vec4f(i.y, i.y, i.y, j.y),
        vec4f(j.y, k.y, l.y, k.y),
    );
    let e1: vec4f = det2x2_v4(
        vec4f(j.x, k.x, i.z, i.z),
        vec4f(l.x, l.x, j.z, k.z),
        vec4f(j.y, k.y, i.w, i.w),
        vec4f(l.y, l.y, j.w, k.w),
    );
    let e2: vec4f = det2x2_v4(
        vec4f(i.z, j.z, j.z, k.z),
        vec4f(l.z, k.z, l.z, l.z),
        vec4f(i.w, j.w, j.w, k.w),
        vec4f(l.w, k.w, l.w, l.w),
    );
    
    // we need all the cofactors above anyway, so might as well
    // use them to compute the determinant ourselves instead of
    // the hardware way
    // let det: f32 = determinant(m);
    
    // det = e0.x*e2.w - e0.y*e2.z 
    //     + e0.w*e2.x - e1.x*e1.w
    //     + e0.z*e2.y + e1.y*e1.z;
    let a:   f32 = det2x2(e0.x, e0.y, e2.z, e2.w);
    let b:   f32 = det2x2(e0.w, e1.x, e1.w, e2.x);
    let c:   f32 = fma(e0.z, e2.y, e1.y * e1.z);
    let det: f32 = a + b + c;
    if det == 0. { return mat4x4f(0.); }
    
    let e2we1y: vec4f = vec4f(e2.ww, e1.yy);
    let e2ze1x: vec4f = vec4f(e2.zz, e1.xx);
    let e2ye0w: vec4f = vec4f(e2.yy, e0.ww);
    let e2xe0z: vec4f = vec4f(e2.xx, e0.zz);
    let e1we0y: vec4f = vec4f(e1.ww, e0.yy);
    let e1ze0x: vec4f = vec4f(e1.zz, e0.xx);
    
    return transpose(
        mat4x4f(
             cofac(j.yxwz, e2we1y, k.yxwz, e2ze1x, l.yxwz, e2ye0w),
            -cofac(i.yxwz, e2we1y, k.yxwz, e2xe0z, l.yxwz, e1we0y),
             cofac(i.yxwz, e2ze1x, j.yxwz, e2xe0z, l.yxwz, e1ze0x),
            -cofac(i.yxwz, e2ye0w, j.yxwz, e1we0y, k.yxwz, e1ze0x),
        )
    ) / det;
}
