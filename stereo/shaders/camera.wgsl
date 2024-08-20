// #include "structs.wgsl"
// #include "kalman.wgsl"
// #include "quat.wgsl"

// todo: account for lens distortion.
//   best way to do this:
//   - use the forward distortion model to resample the source
//     image into rectilinear or stereographic space.
//   - apply the map distortion as part of the projection, and apply its
//     jacobian by the chain rule.

// R^2 -> R^2 P and jacobian
struct Dx2x2 {
    f_x: vec2f,
    J_f: mat2x2f,
}

// R^3 -> R^2 P and jacobian
struct Dx3x2 {
    f_x: vec2f,
    J_f: mat3x2f,
}

// R^3 -> R^3 P and jacobian
struct Dx3x3 {
    f_x: vec3f,
    J_f: mat3x3f,
}

// R^4 -> R^2 P and jacobian
struct Dx4x2 {
    f_x: vec2f,
    J_f: mat4x2f,
}

fn pinhole_projection(lens: LensParameters) -> mat4x4f {
    // xxx todo
}

// compute the world-to-camera matrix from a camera state.
world_to_cam(cam: CameraState) -> mat4x4f {
    let q:  vec4f  = qconj(cam.q); // invert cam2world for world2cam
    let Q:  mat3x3 = qvmat(q);
    // world2cam transform; i.e. puts a world point in camera space
    let T:  vec3f  = -cam.x;
    let M:  mat4x4 = mat4x4f(
        vec4f(Q[0],  0.),
        vec4f(Q[1],  0.), 
        vec4f(Q[2],  0.),
        vec4f(Q * T, 1.),
    );
    return M;
}

// take an X in camera space and project it to the stereographic image plane.
// return the projected point and its jacobian about the camera space point.
fn J_project_stereographic_dp(x: vec3f) -> Dx3x2 {
    let m:     f32 = length(x);
    let p:   vec3f = x / m;
    let uv:  vec2f = p.xy / (1. - p.z);
    let k:     f32 = 1. / (m * (x.z - m));
    let zzp:   f32 = x.z * (x.z - m);
    let q:     f32 = 1. / dot(uv, uv);
    let J: mat3x2f = mat3x2f(
        k * vec2f(x.y * x.y + zzp, x.x * x.y),
        k * vec2f(x.x * x.y, x.x * x.x + zzp),
        q * vec2f(x + x.y * x.z / m, y + x.y * x.z / m),
    );
    return Dx3x2(x, J);
}

// take a point in camera space and project it to the image plane
// using the given camera state. return the jacobian of the projection
// about the camera space point.
fn J_project_dp(cam: CameraState, p: vec3f) -> Dx3x2 {
    let M: mat4x4f = world_to_cam(cam);            // world-to-camera matrix
    let P: mat4x4  = pinhole_projection(cam.lens); // projection matrix
    let Q: mat4x4  = P * M;
    let x_clip: vec4f = Q * vec4f(p, 1.);
    let x: vec2f = x_clip.xy / x_clip.w;
    
    let J: mat3x2f = ( 
        transpose(mat2x3f(x.x * Q[3].xyz, x.y * Q[3].xyz))
        + mat3x2f(Q[0].xyz, Q[1].xyz, Q[2].xyz)
    ) / x_clip.w;
    
    return Dx3x2(x, J);
}

fn J_world_to_cam_dp(cam: CameraState) {
    return world_to_cam(cam);
}

// projection with jacobian with respect to the camera position
fn J_world_to_cam_dtx(cam: CameraState) -> mat3x3f {
    const one: vec3f = vec3f(1., 0., 0.);
    // inverse bc camera point is negated in the world_to_cam transform
    return mat3x3f(-one.xyy, -one.yxy, -one.yyx);
}

// projection with jacobian with respect to the camera orientation
fn J_world_to_cam_dq(cam: CameraState, p: vec3f) -> mat3x4f {
    return J_qvmult_dq(qconj(cam.q), p - cam.x);
}

fn project(cam: CameraState, p: vec3f) -> vec2f {
    let P: mat4x4f = pinhole_projection(cam.lens);
    let x:   vec3f = qvmult(qconj(cam.q), p - cam.x);
    let x_clip: vec4f = P * vec4f(x, 1.);
    return x_clip.xy / x_clip.w;
}

fn unproject_kalman_view_difference(
    prior:         Estimate3D,
    measured_diff: Estimate2D,
    cam_a:         CameraState,
    cam_b:         CameraState) -> Estimate3D
{
    // we have only an estimate of the difference of the projections of the
    // point in the two camera views. we can incorporate this difference
    // into the projection matrix though, and treat the whole "project twice
    // and difference" as a single projection of the 3D state.
    let p_a: Dx3x2 = J_project_dp(cam_a, prior.x);
    let p_b: Dx3x2 = J_project_dp(cam_b, prior.x);
    // transpose so we can vector-select rows
    let a_H: mat2x3f = transpose(p_a.J_f);
    let b_H: mat2x3f = transpose(p_b.J_f);
    let prior_diff: vec2f = p_b.f_x - p_a.f_x;
    // u_diff = u_b.uv - u_a.uv
    let P_H: mat3x2f = transpose(
        mat2x3f(b_H[0] - a_H[0], b_H[1] - a_H[1])
    );
    return update_ekf_unproject_2d_3d(prior, prior_diff, measured_diff, P_H);
}
