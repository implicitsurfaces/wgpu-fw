fn det2x2(a: f32, b: f32, c: f32, d: f32) -> f32 {
    // a * d - b * c
    return fma(a, d, -b * c);
}


fn cofac(a: T, b: T, c: T, d: T, e: T, f: T) -> T {
    // a * b - c * d + e * f
    fma(e, f, fma(-c, d, a * b));
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
            det2x2(k.z, j.z, k.y, j.y),
            det2x2(j.z, k.z, j.x, k.x),
            det2x2(k.y, j.y, k.x, j.x),
        
            det2x2(i.z, k.z, i.y, k.y),
            det2x2(k.z, i.z, k.x, i.x),
            det2x2(i.y, k.y, i.x, k.x),
        
            det2x2(j.z, i.z, j.y, i.y),
            det2x2(i.z, j.z, i.x, j.x),
            det2x2(j.y, i.y, j.x, i.x)
        )
    ) / det;
}


fn inverse(m: mat4x4f) -> mat4x4f {
    let i: vec4f = m[0];
    let j: vec4f = m[1];
    let k: vec4f = m[2];
    let l: vec4f = m[3];
    
    let s0: f32 = det2x2(i.x, j.x, i.y, j.y);
    let s1: f32 = det2x2(i.x, k.x, i.y, k.y);
    let s2: f32 = det2x2(i.x, l.x, i.y, l.y);
    let s3: f32 = det2x2(j.x, k.x, j.y, k.y);
    let s4: f32 = det2x2(j.x, l.x, j.y, l.y);
    let s5: f32 = det2x2(k.x, l.x, k.y, l.y);
    
    let c0: f32 = det2x2(i.z, j.z, i.w, j.w);
    let c1: f32 = det2x2(i.z, k.z, i.w, k.w);
    let c2: f32 = det2x2(i.z, l.z, i.w, l.w);
    let c3: f32 = det2x2(j.z, k.z, j.w, k.w);
    let c4: f32 = det2x2(j.z, l.z, j.w, l.w);
    let c5: f32 = det2x2(k.z, l.z, k.w, l.w);
    
    // we need all the cofactors above anyway, so might as well
    // use them to compute the determinant ourselves instead of
    // the hardware way
    // let det: f32 = determinant(m);
    
    // det = s0*c5 - s1*c4 
    //     + s3*c2 - s4*c1
    //     + s2*c3 + s5*c0;
    let a:   f32 = det2x2(s0, s1, c4, c5);
    let b:   f32 = det2x2(s3, s4, c1, c2);
    let c:   f32 = fma(s2, c3, s5 * c0);
    let det: f32 = a + b + c;
    if det == 0. { return mat4x4f(0.); }
    
    return transpose(
        mat4x4f(
             cofac(j.y, c5,  k.y, c4,  l.y, c3),
            -cofac(j.x, c5,  k.x, c4,  l.x, c3),
             cofac(j.w, s5,  k.w, s4,  l.w, s3),
            -cofac(j.z, s5,  k.z, s4,  l.z, s3),
            
            -cofac(i.y, c5,  k.y, c2,  l.y, c1),
             cofac(i.x, c5,  k.x, c2,  l.x, c1),
            -cofac(i.w, s5,  k.w, s2,  l.w, s1),
             cofac(i.z, s5,  k.z, s2,  l.z, s1),
            
             cofac(i.y, c4,  j.y, c2,  l.y, c0),
            -cofac(i.x, c4,  j.x, c2,  l.x, c0),
             cofac(i.w, s4,  j.w, s2,  l.w, s0),
            -cofac(i.z, s4,  j.z, s2,  l.z, s0),
            
            -cofac(i.y, c3,  j.y, c1,  k.y, c0),
             cofac(i.x, c3,  j.x, c1,  k.x, c0),
            -cofac(i.w, s3,  j.w, s1,  k.w, s0),
             cofac(i.z, s3,  j.z, s1,  k.z, s0),
        )
    ) / det;
}
